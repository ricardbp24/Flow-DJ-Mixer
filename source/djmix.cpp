#define _USE_MATH_DEFINES
// Flow DJ Dual Mixer - VST3 (Deck A = main input, Deck B = sidechain input)
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <atomic>
#include "public.sdk/source/common/pluginview.h"
#include "pluginterfaces/gui/iplugview.h"

using namespace Steinberg;
using namespace Steinberg::Vst;

enum P { kXfade, kAHigh, kAMid, kALow, kAGain, kBHigh, kBMid, kBLow, kBGain, kMaster,
         kAKillL, kAKillM, kAKillH, kBKillL, kBKillM, kBKillH, kAutoEQ, kAutoLen, kAVol, kBVol, kAutoDetect, kDetBar, kBars, kNumParams };

struct PDef { const char16* name; const char16* unit; double mn, mx, def; int steps; };
static const PDef kDefs[kNumParams] = {
 {u"Crossfader",u"%",0,100,50,0},
 {u"Deck A High",u"dB",-48,6,0,0},{u"Deck A Mid",u"dB",-48,6,0,0},{u"Deck A Low",u"dB",-48,6,0,0},{u"Deck A Gain",u"dB",-24,6,0,0},
 {u"Deck B High",u"dB",-48,6,0,0},{u"Deck B Mid",u"dB",-48,6,0,0},{u"Deck B Low",u"dB",-48,6,0,0},{u"Deck B Gain",u"dB",-24,6,0,0},
 {u"Master Volume",u"dB",-36,6,0,0},
 {u"Deck A Kill Low",u"",0,1,0,1},{u"Deck A Kill Mid",u"",0,1,0,1},{u"Deck A Kill High",u"",0,1,0,1},
 {u"Deck B Kill Low",u"",0,1,0,1},{u"Deck B Kill Mid",u"",0,1,0,1},{u"Deck B Kill High",u"",0,1,0,1},
 {u"Auto EQ Blend",u"",0,1,0,1},
 {u"Auto Mix Length (old)",u"",0,4,2,4},
 {u"Deck A Volume",u"%",0,100,100,0},{u"Deck B Volume",u"%",0,100,100,0},
 {u"Auto Detect",u"",0,1,0,1},
 {u"Detect Start On Bar",u"",0,1,0,1},
 {u"Auto Mix Bars",u"bars",1,64,8,63},
};
static const int kLenBars[5]={2,4,8,16,32};
static double toNorm(int i,double v){return (v-kDefs[i].mn)/(kDefs[i].mx-kDefs[i].mn);}
static double toPlain(int i,double n){return kDefs[i].mn+n*(kDefs[i].mx-kDefs[i].mn);}
// Auto EQ blend: per-deck, per-band (0=low,1=mid,2=high) multiplier driven by crossfader x (0=A .. 1=B)
static double ramp(double x,double a,double b){ if(x<=a) return 0; if(x>=b) return 1; double t=(x-a)/(b-a); return t*t*(3-2*t); }
static double autoFactor(int deck,int band,double x){
  if(deck==0){ // Deck A goes out: highs/mids leave late, bass swaps at the middle
    static const double a[3][2]={{0.40,0.50},{0.50,0.85},{0.55,0.95}};
    return 1.0-ramp(x,a[band][0],a[band][1]);
  } else {     // Deck B comes in: highs first, then mids, bass swap at the middle
    static const double b[3][2]={{0.40,0.50},{0.10,0.45},{0.00,0.30}};
    return ramp(x,b[band][0],b[band][1]);
  }
}

// 2nd-order Butterworth biquad, cascaded twice = Linkwitz-Riley 4th order
struct Biquad { double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
  void set(bool lp,double f,double fs){double w=2*M_PI*f/fs,c=cos(w),s=sin(w),al=s/(2*0.70710678),a0=1+al;
    if(lp){b0=(1-c)/2;b1=1-c;b2=(1-c)/2;}else{b0=(1+c)/2;b1=-(1+c);b2=(1+c)/2;}
    b0/=a0;b1/=a0;b2/=a0;a1=-2*c/a0;a2=(1-al)/a0;}
  inline double p(double x){double y=b0*x+z1;z1=b1*x-a1*y+z2;z2=b2*x-a2*y;return y;}
  void reset(){z1=z2=0;} };
struct LR4 { Biquad a,b; void set(bool lp,double f,double fs){a.set(lp,f,fs);b.set(lp,f,fs);} inline double p(double x){return b.p(a.p(x));} void reset(){a.reset();b.reset();} };

// 3-band isolator: low = LP250, rest = HP250 -> split at 3.5k
struct Isolator { LR4 lp1,hp1,lp2,hp2;
  void set(double fs){lp1.set(true,250,fs);hp1.set(false,250,fs);lp2.set(true,3500,fs);hp2.set(false,3500,fs);}
  void reset(){lp1.reset();hp1.reset();lp2.reset();hp2.reset();}
  inline double p(double x,double gl,double gm,double gh){double l=lp1.p(x),r=hp1.p(x);double m=lp2.p(r),h=hp2.p(r);return l*gl+m*gm+h*gh;} };

// DJ-style channel fader curve: incoming deck is up early, outgoing deck leaves late (EQ does the blending)
static double djVol(int deck,double x){ return deck==0 ? 1.0-ramp(x,0.70,1.0) : ramp(x,0.0,0.15); }
static inline double db(double d){return pow(10.0,d/20.0);}
struct Smooth{double v=0,t=0; inline double next(double k){v+= (t-v)*k; return v;}};

static const FUID kUID(0x6F1D2A11,0x4C2B4E8A,0x9D1E77A3,0x5B0C2F01);

class DJMixer : public SingleComponentEffect {
public:
  std::atomic<float> meter[3]={{0},{0},{0}};
  IPlugView* PLUGIN_API createView(FIDString name) SMTG_OVERRIDE;
  // ---- AUTO MIX: the audio thread runs the transition and publishes the moving control values;
  //      the editor writes them back into the real parameters so knobs/faders physically move.
  std::atomic<int> transReq{0};            // GUI -> audio: +1 = A->B, -1 = B->A, 2 = stop
  std::atomic<float> transProg{0.f};
  std::atomic<float> transPos{-1.f};       // visual crossfader position while mixing (-1 = idle)
  std::atomic<int> transDone{0};           // audio -> GUI: 1 = finished/stopped, write final values
  std::atomic<float> live[9];              // A hi,mid,low, B hi,mid,low (dB), A vol, B vol (%), xfade (%)
  bool transOn=false; int tDir=1; double tT=0, tDur=1, tP=0, tXf0=0;
  double home[2][3]={{0,0,0},{0,0,0}}, homeVol[2]={100,100}, mem[2][3]={{0,0,0},{0,0,0}}, memVol[2]={100,100}, g0[2]={1,1};
  double sr=44100;
  // AUTO DETECT: watch the deck that is not playing; when its input goes from silence to signal, mix to it
  std::atomic<int> detState{0};            // 0 off, 1 waiting (watching), 2 detected -> mixing at next bar
  double silentT[2]={0,0}, sigT[2]={0,0}; bool armed[2]={false,false}; int pending=0;
  void resetDetect(){ for(int d=0;d<2;d++){ silentT[d]=0; sigT[d]=0; armed[d]=false; } pending=0; }
  static int eqId(int d,int b){ return (d?kBHigh:kAHigh)+(2-b); }   // b: 0=low 1=mid 2=high
  double xfGain(int d,double x){ if(val[kAutoEQ]>0.5) return djVol(d,x); return d? sin(x*M_PI/2) : cos(x*M_PI/2); }
  double tX(){ return tDir>0? tP : 1.0-tP; }                         // DJ position: 0 = all A, 1 = all B
  double curEQ(int d,int b){ double f=autoFactor(d,b,tX()); double v=home[d][b]<=-47.9? -48.0 : (f<=1e-4? -48.0 : home[d][b]+20*log10(f)); return std::max(-48.0,v); }
  double curVol(int d){ return homeVol[d]*djVol(d,tX()); }
  double curXf(){ return tXf0+((tDir>0?1.0:0.0)-tXf0)*tP; }
  void publish(){ for(int d=0;d<2;d++) for(int b=0;b<3;b++) live[d*3+(2-b)].store((float)curEQ(d,b));
                  live[6].store((float)curVol(0)); live[7].store((float)curVol(1)); live[8].store((float)(curXf()*100)); }
  void startTrans(int dir){
    tDir=dir; int in=dir>0?1:0, out=1-in;
    for(int b=0;b<3;b++){ double v=val[eqId(out,b)]; home[out][b]=v; mem[out][b]=v;
                          double w=val[eqId(in,b)]; home[in][b]= w<=-47.9? mem[in][b] : w; }
    homeVol[out]=val[out?kBVol:kAVol]; memVol[out]=homeVol[out];
    { double w=val[in?kBVol:kAVol]; homeVol[in]= w<1? memVol[in] : w; if(homeVol[in]<1) homeVol[in]=100; }
    tXf0=val[kXfade]/100.0; g0[0]=xfGain(0,tXf0); g0[1]=xfGain(1,tXf0);
    tT=0; tP=0; transOn=true; publish(); transPos.store((float)curXf()); transProg.store(0);
  }
  void endTrans(){ // freeze the current values into the parameters
    for(int d=0;d<2;d++) for(int b=0;b<3;b++) val[eqId(d,b)]=curEQ(d,b);
    val[kAVol]=curVol(0); val[kBVol]=curVol(1); val[kXfade]=curXf()*100;
    publish(); transOn=false; transPos.store(-1.f); transDone.store(1); updateTargets(false); resetDetect();
  }
  double val[kNumParams]; Isolator iso[2][2]; Smooth sm[2][4]; Smooth smX[2], smM; double smK=0.001;
  DJMixer(){ for(int i=0;i<kNumParams;i++) val[i]=kDefs[i].def; for(auto& l:live) l.store(0.f); }
  static FUnknown* create(void*){ return (IAudioProcessor*)new DJMixer; }

  tresult PLUGIN_API initialize(FUnknown* ctx) SMTG_OVERRIDE {
    tresult r=SingleComponentEffect::initialize(ctx); if(r!=kResultOk) return r;
    addAudioInput(STR16("Deck A (Main)"),SpeakerArr::kStereo);
    addAudioInput(STR16("Deck B (Sidechain)"),SpeakerArr::kStereo,kAux,0);
    addAudioOutput(STR16("Master Out"),SpeakerArr::kStereo);
    for(int i=0;i<kNumParams;i++){
      auto* p=new RangeParameter(kDefs[i].name,i,kDefs[i].unit,kDefs[i].mn,kDefs[i].mx,kDefs[i].def,kDefs[i].steps,ParameterInfo::kCanAutomate);
      p->setPrecision(i==kBars?0:1); if(i==kAutoLen) p->getInfo().flags=ParameterInfo::kIsReadOnly|ParameterInfo::kIsHidden; parameters.addParameter(p);
    }
    return kResultOk;
  }
  tresult PLUGIN_API setBusArrangements(SpeakerArrangement* in,int32 nIn,SpeakerArrangement* out,int32 nOut) SMTG_OVERRIDE {
    if(nIn==2&&nOut==1&&in[0]==SpeakerArr::kStereo&&in[1]==SpeakerArr::kStereo&&out[0]==SpeakerArr::kStereo) return SingleComponentEffect::setBusArrangements(in,nIn,out,nOut);
    return kResultFalse;
  }
  tresult PLUGIN_API canProcessSampleSize(int32 s) SMTG_OVERRIDE { return (s==kSample32||s==kSample64)?kResultTrue:kResultFalse; }
  tresult PLUGIN_API setupProcessing(ProcessSetup& s) SMTG_OVERRIDE {
    for(int d=0;d<2;d++)for(int c=0;c<2;c++){iso[d][c].set(s.sampleRate);iso[d][c].reset();}
    sr=s.sampleRate; smK=1.0-exp(-1.0/(0.02*s.sampleRate)); updateTargets(true);
    return SingleComponentEffect::setupProcessing(s);
  }
  void updateTargets(bool snap){
    for(int d=0;d<2;d++){int o=d?4:0,k=d?13:10;
      for(int b=0;b<3;b++){ double v= transOn? curEQ(d,b) : val[eqId(d,b)];
        sm[d][b].t = val[k+b]>0.5?0.0:(v<=-47.9?0.0:db(v)); }
      double vol = transOn? curVol(d) : val[d?kBVol:kAVol];
      sm[d][3].t = db(val[4+o])*vol/100.0; }
    if(transOn){ int in=tDir>0?1:0; smX[in].t = g0[in]+(1.0-g0[in])*std::min(1.0,tP*8); smX[1-in].t = g0[1-in]; }
    else { double x=val[kXfade]/100.0; smX[0].t=xfGain(0,x); smX[1].t=xfGain(1,x);
      if(val[kAutoEQ]>0.5) for(int d=0;d<2;d++) for(int b=0;b<3;b++) sm[d][b].t*=autoFactor(d,b,x); }
    smM.t = val[kMaster]<=-35.9?0.0:db(val[kMaster]);
    if(snap){for(auto&a:sm)for(auto&b:a)b.v=b.t; smX[0].v=smX[0].t; smX[1].v=smX[1].t; smM.v=smM.t;}
  }
  template<typename T> void run(ProcessData& data){
    int n=data.numSamples; T** A=(T**)data.inputs[0].channelBuffers32; T** O=(T**)data.outputs[0].channelBuffers32;
    if(sizeof(T)==8){A=(T**)data.inputs[0].channelBuffers64; O=(T**)data.outputs[0].channelBuffers64;}
    T** B=nullptr; if(data.numInputs>1&&data.inputs[1].numChannels>=2) B=(T**)(sizeof(T)==8?(void*)data.inputs[1].channelBuffers64:(void*)data.inputs[1].channelBuffers32);
    double pa=0,pb=0,pm=0;
    for(int i=0;i<n;i++){
      double g[2][4]; for(int d=0;d<2;d++)for(int b=0;b<4;b++) g[d][b]=sm[d][b].next(smK);
      double xa=smX[0].next(smK), xb=smX[1].next(smK), m=smM.next(smK);
      for(int c=0;c<2;c++){
        double a=A?iso[0][c].p((double)A[c][i],g[0][0],g[0][1],g[0][2])*g[0][3]:0;
        double b=B?iso[1][c].p((double)B[c][i],g[1][0],g[1][1],g[1][2])*g[1][3]:0;
        double o=(a*xa+b*xb)*m; O[c][i]=(T)o;
        pa=std::max(pa,std::fabs(a*xa*m)); pb=std::max(pb,std::fabs(b*xb*m)); pm=std::max(pm,std::fabs(o));
      }
    }
    auto up=[](std::atomic<float>&m,double v){ if(v>m.load()) m.store((float)v); };
    up(meter[0],pa); up(meter[1],pm); up(meter[2],pb);
  }
  static bool managed(int id){ return (id>=kAHigh&&id<=kALow)||(id>=kBHigh&&id<=kBLow)||id==kAVol||id==kBVol; }
  tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE {
    if(auto* pc=data.inputParameterChanges){ int32 np=pc->getParameterCount(); bool cancel=false;
      for(int32 i=0;i<np;i++){ auto* q=pc->getParameterData(i); if(!q) continue; int32 npt=q->getPointCount(); if(npt<=0) continue;
        ParamValue v; int32 off; int id=q->getParameterId(); if(id<0||id>=kNumParams) continue;
        if(q->getPoint(npt-1,off,v)!=kResultOk) continue;
        if(transOn && managed(id)) continue;                 // echo of our own moving controls
        if(transOn && id==kXfade && fabs(toPlain(id,v)-curXf()*100)>2) cancel=true;   // user grabbed the crossfader
        val[id]=toPlain(id,v); }
      if(cancel) endTrans();
      updateTargets(false); }
    int req=transReq.exchange(0);
    if(req==2 && transOn) endTrans();
    else if((req==1||req==-1) && !transOn) startTrans(req);
    if(val[kAutoDetect]>0.5 && !transOn && data.numSamples>0 && data.numInputs>1){
      double dt=data.numSamples/sr;
      int cur = val[kXfade]>=50? 1 : 0, watch = 1-cur;                  // deck currently on air / deck to watch
      double ms=0; int nc=std::min(2,(int)data.inputs[watch].numChannels);
      for(int c=0;c<nc;c++) for(int i=0;i<data.numSamples;i++){
        double v = data.symbolicSampleSize==kSample64? data.inputs[watch].channelBuffers64[c][i] : data.inputs[watch].channelBuffers32[c][i];
        ms+=v*v; }
      double rmsDb = nc? 10*log10(ms/(nc*data.numSamples)+1e-12) : -120;
      bool onBar = val[kDetBar]>0.5;                                     // INSTANT: start on the very first block with signal
      if(rmsDb < (onBar? -50 : -60)){ silentT[watch]+=dt; sigT[watch]=0; if(silentT[watch]>1.0) armed[watch]=true; }
      else { sigT[watch]+=dt; silentT[watch]=0; if(armed[watch] && sigT[watch]>=(onBar? 0.25 : 0.0) && !pending) pending = watch? 1 : -1; }
      if(pending){
        bool go=true; auto* pcx=data.processContext;
        if(onBar && pcx && (pcx->state&ProcessContext::kProjectTimeMusicValid) && (pcx->state&ProcessContext::kBarPositionValid) && (pcx->state&ProcessContext::kTempoValid) && pcx->tempo>0){
          double barLen = (pcx->state&ProcessContext::kTimeSigValid) && pcx->timeSigDenominator>0 ? pcx->timeSigNumerator*4.0/pcx->timeSigDenominator : 4.0;
          double inBar = pcx->projectTimeMusic - pcx->barPositionMusic, blockQ = data.numSamples/sr*pcx->tempo/60.0;
          go = (inBar < blockQ) || (inBar > barLen - 1e-6);
        }
        if(go){ int dir=pending; pending=0; startTrans(dir); }
      }
      detState.store(pending? 2 : 1);
    } else if(val[kAutoDetect]<=0.5){ detState.store(0); resetDetect(); }
    else if(transOn) detState.store(2);
    if(transOn){
      double tempo=120; if(data.processContext && (data.processContext->state&ProcessContext::kTempoValid) && data.processContext->tempo>0) tempo=data.processContext->tempo;
      int bars=std::min(64,std::max(1,(int)lround(val[kBars])));
      tDur=std::max(0.5, bars*4*60.0/tempo);
      bool playing = !data.processContext || (data.processContext->state&ProcessContext::kPlaying);
      if(playing && data.numSamples>0) tT += data.numSamples/sr;
      tP=std::min(1.0,tT/tDur); publish(); transPos.store((float)curXf()); transProg.store((float)tP);
      updateTargets(false);
      if(tP>=1.0) endTrans();
    }
    if(data.numSamples<=0||data.numOutputs<1||data.outputs[0].numChannels<2) return kResultOk;
    if(data.symbolicSampleSize==kSample64) run<double>(data); else run<float>(data);
    data.outputs[0].silenceFlags=0;
    return kResultOk;
  }
  tresult PLUGIN_API setState(IBStream* s) SMTG_OVERRIDE {
    IBStreamer st(s,kLittleEndian); int nread=0;
    for(int i=0;i<kNumParams;i++){double v; if(!st.readDouble(v)) break; val[i]=v; nread++; setParamNormalized(i,toNorm(i,v));}
    if(nread<=kBars){ // project saved by an older version: convert the old 5-step length to bars
      val[kBars]=kLenBars[std::min(4,std::max(0,(int)lround(val[kAutoLen])))]; setParamNormalized(kBars,toNorm(kBars,val[kBars])); }
    updateTargets(true); return kResultOk; }
  tresult PLUGIN_API getState(IBStream* s) SMTG_OVERRIDE {
    IBStreamer st(s,kLittleEndian); for(int i=0;i<kNumParams;i++) st.writeDouble(val[i]); return kResultOk; }
};

#include "gui.inc"

BEGIN_FACTORY_DEF("Flow DJ","https://www.flowmusic.app","mailto:info@flowmusic.app")
  DEF_CLASS2(INLINE_UID_FROM_FUID(kUID),PClassInfo::kManyInstances,kVstAudioEffectClass,"Flow DJ Dual Mixer",
             Vst::kDistributable,"Fx|Mixer|EQ","1.0.0",kVstVersionString,DJMixer::create)
END_FACTORY
