// ---------- Native Win32 / GDI+ editor replicating the Flow DJ Dual Mixer design ----------
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
#include <string>
#include <vector>

namespace GUI {
using namespace Gdiplus;
static const int W = 1000, H = 800;
static ULONG_PTR gdipToken = 0; static int gdipRefs = 0;

static Color C(DWORD rgb, BYTE a = 255) { return Color(a, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255); }
static const DWORD BG = 0x0A0C10, PANEL = 0x111419, PANEL2 = 0x0D1014, BORDER = 0x232A33, TEXT = 0xE6EAF0,
                   DIM = 0x7D8794, CYAN = 0x1CC8EE, AMBER = 0xF5B700, KNOBBG = 0x1A1F26;

enum CtlType { kKnob, kButton, kHSlider, kXfader, kXPreset, kReset, kAutoBtn, kMixGo, kLenBtn, kVFader, kDetBtn, kStartBtn };
struct Ctl { CtlType type; int param; RectF r; DWORD color; float preset; int deck; };

static void roundRect(GraphicsPath& p, RectF r, float rad) {
	float d = rad * 2; p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
	p.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90); p.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90); p.CloseFigure();
}
static void fillRR(Graphics& g, RectF r, float rad, Color fill, Color border) {
	GraphicsPath p; roundRect(p, r, rad); SolidBrush b(fill); g.FillPath(&b, &p); Pen pn(border, 1); g.DrawPath(&pn, &p);
}
static void text(Graphics& g, const wchar_t* s, float x, float y, float size, Color c, bool bold = false, bool mono = false,
                 StringAlignment al = StringAlignmentNear, float w = 0) {
	FontFamily ff(mono ? L"Consolas" : L"Segoe UI"); Font f(&ff, size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
	SolidBrush b(c); StringFormat fmt; fmt.SetAlignment(al);
	if (w > 0) g.DrawString(s, -1, &f, RectF(x, y, w, size * 2), &fmt, &b); else g.DrawString(s, -1, &f, PointF(x, y), &b);
}

class MixerView : public CPluginView {
public:
	DJMixer* ctl; HWND hwnd = nullptr; std::vector<Ctl> ctls; int drag = -1; float dragY = 0, dragX = 0, dragStart = 0;
	float mdisp[3] = {0, 0, 0}; bool targetB = true; bool autoEditing = false;
	MixerView(DJMixer* c) : CPluginView(nullptr), ctl(c) { ViewRect r(0, 0, W, H); setRect(r); layout(); }

	double norm(int id) { return ctl->getParamNormalized(id); }
	bool mixing() { return ctl->transPos.load() >= 0.f; }
	double dispX() { float p = ctl->transPos.load(); return p >= 0.f ? p : norm(kXfade); }
	bool djMode() { return norm(kAutoEQ) > 0.5 || mixing(); }
	double plain(int id) { return toPlain(id, norm(id)); }
	void setNorm(int id, double v) {
		v = std::min(1.0, std::max(0.0, v)); ctl->setParamNormalized(id, v); ctl->performEdit(id, v);
	}
	void edit1(int id, double v) { ctl->beginEdit(id); setNorm(id, v); ctl->endEdit(id); }

	void layout() {
		ctls.clear();
		// master slider
		ctls.push_back({kHSlider, kMaster, RectF(800, 34, 120, 16), CYAN, 0, 0});
		for (int d = 0; d < 2; d++) {
			float px = d ? 605.f : 20.f; DWORD col = d ? AMBER : CYAN; int o = d ? 4 : 0;
			int ids[4] = {kAGain + o, kAHigh + o, kAMid + o, kALow + o};
			for (int k = 0; k < 4; k++) ctls.push_back({kKnob, ids[k], RectF(px + 22 + k * 88.f, 210, 68, 68), col, 0, d});
			int kills[3] = {d ? kBKillL : kAKillL, d ? kBKillM : kAKillM, d ? kBKillH : kAKillH};
			for (int k = 0; k < 3; k++) ctls.push_back({kButton, kills[k], RectF(px + 92 + k * 62.f, 498, 54, 26), col, 0, d});
			ctls.push_back({kVFader, d ? kBVol : kAVol, RectF(px + 170, 345, 36, 128), col, 0, d});
			ctls.push_back({kReset, -1, RectF(px + 300, 498, 56, 26), col, 0, d});
		}
		ctls.push_back({kAutoBtn, kAutoEQ, RectF(560, 595, 150, 24), AMBER, 0, 0});
		ctls.push_back({kXPreset, kXfade, RectF(718, 595, 66, 24), CYAN, 0.f, 0});
		ctls.push_back({kXPreset, kXfade, RectF(792, 595, 116, 24), CYAN, 0.5f, 0});
		ctls.push_back({kXPreset, kXfade, RectF(916, 595, 60, 24), CYAN, 1.f, 0});
		ctls.push_back({kXfader, kXfade, RectF(80, 632, 840, 30), CYAN, 0, 0});
		ctls.push_back({kMixGo, -1, RectF(150, 734, 120, 28), CYAN, 1.f, 0});   // A -> B
		ctls.push_back({kMixGo, -1, RectF(280, 734, 120, 28), AMBER, 0.f, 0});  // B -> A
		ctls.push_back({kLenBtn, kBars, RectF(410, 734, 34, 28), CYAN, -1.f, 0});   // minus
		ctls.push_back({kLenBtn, kBars, RectF(448, 734, 92, 28), CYAN, 0.f, 0});    // value (wheel / double-click = 8)
		ctls.push_back({kLenBtn, kBars, RectF(544, 734, 34, 28), CYAN, 1.f, 0});    // plus
		ctls.push_back({kDetBtn, kAutoDetect, RectF(590, 734, 150, 28), AMBER, 0, 0});
		ctls.push_back({kStartBtn, kDetBar, RectF(330, 595, 220, 24), AMBER, 0, 0});
	}

	void fmtVal(int id, wchar_t* buf) { swprintf(buf, 32, L"%.1f dB", plain(id)); if (kDefs[id].mn <= -36 && plain(id) <= kDefs[id].mn + 0.05) swprintf(buf, 32, L"-∞ dB"); }

	void drawKnob(Graphics& g, const Ctl& c) {
		RectF r = c.r; float v = (float)norm(c.param);
		SolidBrush bg(C(KNOBBG)); g.FillEllipse(&bg, r.X + 8, r.Y + 8, r.Width - 16, r.Height - 16);
		Pen track(C(0x2A313B), 5); track.SetStartCap(LineCapRound); track.SetEndCap(LineCapRound);
		g.DrawArc(&track, r.X + 3, r.Y + 3, r.Width - 6, r.Height - 6, 135, 270);
		if (v > 0.001f) {
			Pen arc(C(c.color), 5); arc.SetStartCap(LineCapRound); arc.SetEndCap(LineCapRound);
			g.DrawArc(&arc, r.X + 3, r.Y + 3, r.Width - 6, r.Height - 6, 135, 270 * v);
		}
		bool isGain = (c.param == kAGain || c.param == kBGain);
		if (!isGain && norm(kAutoEQ) > 0.5 && !mixing()) {
			int band = 2 - (c.param - (c.deck ? kBHigh : kAHigh)); // high=2, mid=1, low=0
			double f = autoFactor(c.deck, band, dispX());
			double eff = f > 1e-4 ? plain(c.param) + 20 * log10(f) : -48.0;
			float ev = (float)std::min(1.0, std::max(0.0, toNorm(c.param, std::max(-48.0, eff))));
			Pen in(C(0xFFFFFF, 40), 2); g.DrawArc(&in, r.X + 12, r.Y + 12, r.Width - 24, r.Height - 24, 135, 270);
			if (ev > 0.001f) { Pen ie(C(0xFFFFFF, 200), 2); g.DrawArc(&ie, r.X + 12, r.Y + 12, r.Width - 24, r.Height - 24, 135, 270 * ev); }
		}
		float a = (135 + 270 * v) * 3.14159265f / 180, cx = r.X + r.Width / 2, cy = r.Y + r.Height / 2;
		Pen ind(C(c.color), 3); ind.SetStartCap(LineCapRound); ind.SetEndCap(LineCapRound);
		g.DrawLine(&ind, cx + cosf(a) * 10, cy + sinf(a) * 10, cx + cosf(a) * 24, cy + sinf(a) * 24);
		const wchar_t* names[] = {L"HIGH (3.5K)", L"MID (1K)", L"LOW (250HZ)", L"TRIM / GAIN"};
		int idx = (c.param == kAGain || c.param == kBGain) ? 3 : (c.param - (c.deck ? kBHigh : kAHigh));
		text(g, names[idx], r.X - 16, r.Y + r.Height + 8, 11, C(DIM), true, false, StringAlignmentCenter, r.Width + 32);
		wchar_t b[32]; fmtVal(c.param, b);
		text(g, b, r.X - 16, r.Y + r.Height + 26, 11, C(TEXT), false, true, StringAlignmentCenter, r.Width + 32);
	}

	void drawMeter(Graphics& g, float x, float y, float w, float h, float lvl, DWORD col) {
		fillRR(g, RectF(x, y, w, h), 3, C(0x07090C), C(BORDER));
		float db = lvl > 1e-6f ? 20 * log10f(lvl) : -100; float f = std::min(1.f, std::max(0.f, (db + 48) / 54));
		float fh = (h - 4) * f;
		if (fh > 0) {
			LinearGradientBrush lg(PointF(0, y + h), PointF(0, y), C(col), db > -3 ? C(0xFF4D4D) : C(col));
			g.FillRectangle(&lg, x + 2, y + h - 2 - fh, w - 4, fh);
		}
	}

	void paint(HDC hdc) {
		Bitmap bmp(W, H); Graphics g(&bmp);
		g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
		SolidBrush bg(C(BG)); g.FillRectangle(&bg, 0, 0, W, H);
		// header
		Pen ring(C(CYAN), 2.5f); g.DrawEllipse(&ring, 22, 26, 30, 30); SolidBrush cb(C(CYAN)); g.FillEllipse(&cb, 32, 36, 10, 10);
		text(g, L"FLOW DJ DUAL MIXER", 64, 20, 20, C(TEXT), true);
		fillRR(g, RectF(316, 24, 108, 22), 4, C(0x0C2A33), C(CYAN, 120));
		text(g, L"3-BAND ISO EQ", 322, 27, 11, C(CYAN), true, true);
		text(g, L"Deck A (Main Bus) + Deck B (Sidechain Input Bus)", 65, 50, 12, C(DIM));
		fillRR(g, RectF(670, 22, 310, 40), 8, C(PANEL), C(BORDER));
		text(g, L"MASTER", 690, 32, 13, C(TEXT), true);
		{ wchar_t b[32]; fmtVal(kMaster, b); text(g, b, 924, 33, 12, C(CYAN), false, true); }
		Pen sep(C(BORDER), 1); g.DrawLine(&sep, 20.f, 80.f, 980.f, 80.f);

		for (int d = 0; d < 2; d++) {
			float px = d ? 605.f : 20.f; DWORD col = d ? AMBER : CYAN;
			fillRR(g, RectF(px, 95, 375, 460), 10, C(PANEL), C(BORDER));
			SolidBrush dot(C(col)); g.FillEllipse(&dot, px + 16, 115.f, 8.f, 8.f);
			text(g, d ? L"DECK B (CH 2)" : L"DECK A (CH 1)", px + 30, 107, 15, C(col), true);
			text(g, d ? L"SIDECHAIN IN" : L"MAIN STEREO IN", px + 240, 111, 10, C(DIM), false, true, StringAlignmentFar, 120);
			g.DrawLine(&sep, px + 14, 140.f, px + 361, 140.f);
			fillRR(g, RectF(px + 12, 488, 351, 46), 6, C(PANEL2), C(BORDER));
			text(g, L"EQ KILL:", px + 24, 503, 12, C(DIM), true);
		}
		// levels panel
		fillRR(g, RectF(405, 95, 190, 460), 10, C(PANEL), C(BORDER));
		text(g, L"LEVELS", 405, 107, 13, C(CYAN), true, false, StringAlignmentCenter, 190);
		const wchar_t* ml[3] = {L"A", L"MIX", L"B"}; DWORD mc[3] = {CYAN, CYAN, AMBER};
		for (int i = 0; i < 3; i++) {
			float x = 437 + i * 46.f;
			text(g, ml[i], x - 10, 150, 10, C(DIM), false, true, StringAlignmentCenter, 38);
			drawMeter(g, x, 172, 18, 340, mdisp[i], mc[i]);
		}
		text(g, L"24-BIT RMS / PEAK", 405, 524, 10, C(DIM), false, true, StringAlignmentCenter, 190);
		// crossfader panel
		fillRR(g, RectF(20, 570, 960, 210), 10, C(PANEL), C(BORDER));
		text(g, L"DJ CROSSFADER", 40, 594, 15, C(TEXT), true);
		text(g, L"A", 44, 638, 14, C(CYAN), true); text(g, L"B", 946, 638, 14, C(AMBER), true);
		float xf = (float)dispX(); wchar_t b[64];
		float va = djMode() ? (float)djVol(0, xf) : cosf(xf * 1.5708f), vb = djMode() ? (float)djVol(1, xf) : sinf(xf * 1.5708f);
		swprintf(b, 64, L"DECK A: %d%%", (int)lroundf(va * 100)); text(g, b, 40, 684, 11, C(DIM), false, true);
		swprintf(b, 64, L"DECK B: %d%%", (int)lroundf(vb * 100)); text(g, b, 820, 684, 11, C(DIM), false, true, StringAlignmentFar, 140);
		text(g, djMode() ? L"DJ BLEND: HI \u2192 MID \u2192 BASS SWAP \u2192 FADE OUT" : L"CONSTANT POWER CROSSFADE (-3dB CENTER)", 250, 684, 11, djMode() ? C(AMBER) : C(DIM), false, true, StringAlignmentCenter, 500);
		Pen sep2(C(BORDER), 1); g.DrawLine(&sep2, 36.f, 714.f, 964.f, 714.f);
		text(g, L"AUTO MIX", 40, 740, 14, C(TEXT), true);
		if (mixing()) {
			float pr = ctl->transProg.load();
			fillRR(g, RectF(752, 744, 120, 8), 4, C(0x07090C), C(BORDER));
			SolidBrush pb(C(AMBER)); g.FillRectangle(&pb, 754.f, 746.f, 116.f * pr, 4.f);
			swprintf(b, 64, L"MIX %d%%", (int)lroundf(pr * 100)); text(g, b, 882, 740, 12, C(AMBER), true, true);
		} else {
			int ds = ctl->detState.load(); bool onB = norm(kXfade) >= 0.5;
			if (ds == 2) text(g, norm(kDetBar) > 0.5 ? L"DETECTED \u2192 NEXT BAR" : L"DETECTED \u2192 MIXING", 752, 741, 11, C(AMBER), true, true);
			else if (ds == 1) text(g, onB ? L"WATCHING DECK A\u2026" : L"WATCHING DECK B\u2026", 752, 741, 11, C(CYAN), false, true);
			else text(g, L"SYNCED TO HOST TEMPO", 752, 741, 11, C(DIM), false, true);
		}

		for (auto& c : ctls) {
			if (c.type == kKnob) drawKnob(g, c);
			else if (c.type == kButton) {
				bool on = norm(c.param) > 0.5; const wchar_t* lb[] = {L"LOW", L"MID", L"HI"};
				int k = c.param - (c.deck ? kBKillL : kAKillL);
				fillRR(g, c.r, 4, on ? C(0xE0303A) : C(0x1C2129), on ? C(0xFF5A63) : C(BORDER));
				text(g, lb[k], c.r.X, c.r.Y + 5, 12, on ? C(0xFFFFFF) : C(TEXT), true, false, StringAlignmentCenter, c.r.Width);
			} else if (c.type == kReset) {
				text(g, L"RESET", c.r.X, c.r.Y + 5, 12, C(DIM), true, false, StringAlignmentCenter, c.r.Width);
			} else if (c.type == kAutoBtn) {
				bool on = norm(kAutoEQ) > 0.5;
				fillRR(g, c.r, 4, on ? C(0x332600) : C(0x161A20), on ? C(AMBER) : C(BORDER));
				text(g, on ? L"AUTO EQ: ON" : L"AUTO EQ: OFF", c.r.X, c.r.Y + 5, 11, on ? C(AMBER) : C(TEXT), true, true, StringAlignmentCenter, c.r.Width);
			} else if (c.type == kMixGo) {
				bool act = mixing() && ((c.preset > 0.5f) == targetB);
				fillRR(g, c.r, 5, act ? C(c.color, 60) : C(0x161A20), act ? C(c.color) : C(c.color, 140));
				text(g, act ? L"\u25A0 STOP" : (c.preset > 0.5f ? L"A \u25B6 B" : L"B \u25B6 A"), c.r.X, c.r.Y + 6, 13, C(c.color), true, true, StringAlignmentCenter, c.r.Width);
			} else if (c.type == kStartBtn) {
				bool bar = norm(kDetBar) > 0.5;
				fillRR(g, c.r, 4, C(0x161A20), C(BORDER));
				text(g, bar ? L"DETECT START: NEXT BAR" : L"DETECT START: INSTANT", c.r.X, c.r.Y + 5, 11, norm(kAutoDetect) > 0.5 ? C(AMBER) : C(DIM), true, true, StringAlignmentCenter, c.r.Width);
			} else if (c.type == kDetBtn) {
				bool on = norm(kAutoDetect) > 0.5;
				fillRR(g, c.r, 5, on ? C(0x332600) : C(0x161A20), on ? C(AMBER) : C(BORDER));
				text(g, on ? L"AUTO DETECT: ON" : L"AUTO DETECT: OFF", c.r.X, c.r.Y + 7, 11, on ? C(AMBER) : C(TEXT), true, true, StringAlignmentCenter, c.r.Width);
			} else if (c.type == kLenBtn) {
				int bars = (int)lround(plain(kBars));
				if (c.preset == 0.f) {
					fillRR(g, c.r, 4, C(0x0C2A33), C(CYAN));
					swprintf(b, 64, bars == 1 ? L"%d BAR" : L"%d BARS", bars);
					text(g, b, c.r.X, c.r.Y + 6, 13, C(CYAN), true, true, StringAlignmentCenter, c.r.Width);
					text(g, L"LENGTH (BARS)", 410, 718, 10, C(DIM), true, true);
				} else {
					fillRR(g, c.r, 4, C(0x161A20), C(BORDER));
					text(g, c.preset < 0 ? L"\u2212" : L"+", c.r.X, c.r.Y + 3, 17, C(TEXT), true, true, StringAlignmentCenter, c.r.Width);
				}
			} else if (c.type == kXPreset) {
				bool act = !mixing() && fabsf(xf - c.preset) < 0.005f;
				fillRR(g, c.r, 4, act ? C(0x0C2A33) : C(0x161A20), act ? C(CYAN) : C(BORDER));
				const wchar_t* t = c.preset == 0 ? L"100% A" : (c.preset == 1 ? L"100% B" : L"CENTER (50/50)");
				text(g, t, c.r.X, c.r.Y + 5, 11, act ? C(CYAN) : C(TEXT), false, true, StringAlignmentCenter, c.r.Width);
			} else if (c.type == kVFader) {
				float v = (float)norm(c.param), cx = c.r.X + c.r.Width / 2;
				text(g, L"VOLUME", c.r.X - 110, c.r.Y + c.r.Height / 2 - 8, 11, C(DIM), true, false, StringAlignmentFar, 100);
				fillRR(g, RectF(cx - 4, c.r.Y, 8, c.r.Height), 4, C(0x07090C), C(BORDER));
				float fy = c.r.Y + c.r.Height * (1 - v);
				{ SolidBrush f(C(c.color, 170)); g.FillRectangle(&f, cx - 2, fy, 4.f, c.r.Y + c.r.Height - fy); }
				for (int t = 0; t <= 4; t++) { Pen tk(C(BORDER), 1); float ty = c.r.Y + c.r.Height * t / 4; g.DrawLine(&tk, c.r.X - 2, ty, c.r.X + 6, ty); g.DrawLine(&tk, c.r.X + c.r.Width - 6, ty, c.r.X + c.r.Width + 2, ty); }
				fillRR(g, RectF(c.r.X + 2, fy - 9, c.r.Width - 4, 18), 3, C(0x2A313B), C(c.color));
				{ Pen ln(C(c.color), 2); g.DrawLine(&ln, c.r.X + 6, fy, c.r.X + c.r.Width - 6, fy); }
				swprintf(b, 64, L"%d%%", (int)lround(plain(c.param)));
				text(g, b, c.r.X + c.r.Width + 10, c.r.Y + c.r.Height / 2 - 8, 12, C(TEXT), false, true);
			} else if (c.type == kXfader || c.type == kHSlider) {
				float v = c.type == kXfader ? (float)dispX() : (float)norm(c.param), cy = c.r.Y + c.r.Height / 2;
				fillRR(g, RectF(c.r.X, cy - 4, c.r.Width, 8), 4, C(0x07090C), C(BORDER));
				if (c.type == kHSlider) { SolidBrush f(C(CYAN, 160)); g.FillRectangle(&f, c.r.X + 2, cy - 2, (c.r.Width - 4) * v, 4.f); }
				else { Pen mid(C(DIM), 1); g.DrawLine(&mid, c.r.X + c.r.Width / 2, cy - 12, c.r.X + c.r.Width / 2, cy + 12); }
				float hx = c.r.X + c.r.Width * v; float hr = c.type == kXfader ? 11.f : 8.f;
				SolidBrush hb(C(CYAN)); g.FillEllipse(&hb, hx - hr, cy - hr, hr * 2, hr * 2);
				Pen hp(C(0xFFFFFF, 180), 1.5f); g.DrawEllipse(&hp, hx - hr, cy - hr, hr * 2, hr * 2);
			}
		}
		Graphics screen(hdc); screen.DrawImage(&bmp, 0, 0);
	}

	int hit(float x, float y) {
		for (int i = 0; i < (int)ctls.size(); i++) {
			RectF r = ctls[i].r; if (ctls[i].type == kVFader) r = RectF(r.X, r.Y - 10, r.Width, r.Height + 20); if (ctls[i].type == kKnob) r = RectF(r.X - 10, r.Y - 4, r.Width + 20, r.Height + 50);
			if (x >= r.X && x <= r.X + r.Width && y >= r.Y - 6 && y <= r.Y + r.Height + 6) return i;
		}
		return -1;
	}
	void sliderTo(const Ctl& c, float x) { setNorm(c.param, (x - c.r.X) / c.r.Width); }

	void onDown(float x, float y, bool dbl) {
		int i = hit(x, y); if (i < 0) return; Ctl& c = ctls[i];
		switch (c.type) {
		case kKnob: case kHSlider: case kXfader: case kVFader:
			if (dbl) { edit1(c.param, toNorm(c.param, kDefs[c.param].def)); break; }
			drag = i; dragY = y; dragX = x; dragStart = (float)norm(c.param); ctl->beginEdit(c.param);
			if (c.type != kKnob) sliderTo(c, x);
			SetCapture(hwnd); break;
		case kButton: edit1(c.param, norm(c.param) > 0.5 ? 0 : 1); break;
		case kAutoBtn: edit1(kAutoEQ, norm(kAutoEQ) > 0.5 ? 0 : 1); break;
		case kDetBtn: edit1(kAutoDetect, norm(kAutoDetect) > 0.5 ? 0 : 1); break;
		case kStartBtn: edit1(kDetBar, norm(kDetBar) > 0.5 ? 0 : 1); break;
		case kMixGo:
			if (mixing()) ctl->transReq.store(2);
			else { targetB = c.preset > 0.5f; ctl->transReq.store(targetB ? 1 : -1); }
			break;
		case kLenBtn: {
			int bars = (int)lround(plain(kBars));
			if (c.preset == 0.f) { if (dbl) bars = 8; }
			else bars += (int)c.preset * ((GetKeyState(VK_SHIFT) & 0x8000) ? 4 : 1);
			edit1(kBars, toNorm(kBars, std::min(64, std::max(1, bars))));
		} break;
		case kXPreset: edit1(kXfade, c.preset); break;
		case kReset: {
			int o = c.deck ? 4 : 0, kb = c.deck ? kBKillL : kAKillL;
			for (int id : {kAHigh + o, kAMid + o, kALow + o, kAGain + o}) edit1(id, toNorm(id, 0));
			for (int k = 0; k < 3; k++) edit1(kb + k, 0);
		} break;
		}
		InvalidateRect(hwnd, nullptr, FALSE);
	}
	void onMove(float x, float y, bool fine) {
		if (drag < 0) return; Ctl& c = ctls[drag];
		if (c.type == kKnob) setNorm(c.param, dragStart + (dragY - y) / (fine ? 1000.f : 200.f));
		else sliderTo(c, x);
		InvalidateRect(hwnd, nullptr, FALSE);
	}
	void onUp() { if (drag >= 0) { ctl->endEdit(ctls[drag].param); drag = -1; ReleaseCapture(); } }
	void onWheel(float x, float y, int delta) {
		int i = hit(x, y); if (i < 0) return; Ctl& c = ctls[i];
		if (c.type == kLenBtn) { int cur = (int)lround(plain(kBars)) + (delta > 0 ? 1 : -1); edit1(kBars, toNorm(kBars, std::min(64, std::max(1, cur)))); }
		if (c.type == kKnob || c.type == kHSlider || c.type == kXfader || c.type == kVFader) edit1(c.param, norm(c.param) + delta / 120.0 * 0.02);
		InvalidateRect(hwnd, nullptr, FALSE);
	}
	void onTimer() {
		static const int ids[8] = {kAHigh, kAMid, kALow, kBHigh, kBMid, kBLow, kAVol, kBVol};
		bool done = ctl->transDone.exchange(0) != 0;
		if (mixing() || done) {
			if (!autoEditing && drag < 0) { for (int id : ids) ctl->beginEdit(id); autoEditing = true; }
			if (autoEditing) for (int i = 0; i < 8; i++) setNorm(ids[i], toNorm(ids[i], ctl->live[i].load()));
		}
		if (done) {
			if (autoEditing) { for (int id : ids) ctl->endEdit(id); autoEditing = false; }
			edit1(kXfade, toNorm(kXfade, ctl->live[8].load()));
		}
		for (int i = 0; i < 3; i++) { float p = ctl->meter[i].exchange(0.f); mdisp[i] = std::max(p, mdisp[i] * 0.85f); }
		InvalidateRect(hwnd, nullptr, FALSE);
	}

	static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
		auto* v = (MixerView*)GetWindowLongPtrW(h, GWLP_USERDATA);
		if (!v) return DefWindowProcW(h, m, w, l);
		float x = (float)GET_X_LPARAM(l), y = (float)GET_Y_LPARAM(l);
		switch (m) {
		case WM_PAINT: { PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); v->paint(dc); EndPaint(h, &ps); return 0; }
		case WM_ERASEBKGND: return 1;
		case WM_LBUTTONDOWN: SetFocus(h); v->onDown(x, y, false); return 0;
		case WM_LBUTTONDBLCLK: v->onDown(x, y, true); return 0;
		case WM_MOUSEMOVE: v->onMove(x, y, (w & MK_SHIFT) || (w & MK_CONTROL)); return 0;
		case WM_LBUTTONUP: v->onUp(); return 0;
		case WM_MOUSEWHEEL: { POINT p{GET_X_LPARAM(l), GET_Y_LPARAM(l)}; ScreenToClient(h, &p); v->onWheel((float)p.x, (float)p.y, GET_WHEEL_DELTA_WPARAM(w)); return 0; }
		case WM_TIMER: v->onTimer(); return 0;
		}
		return DefWindowProcW(h, m, w, l);
	}

	tresult PLUGIN_API isPlatformTypeSupported(FIDString t) SMTG_OVERRIDE {
		return strcmp(t, kPlatformTypeHWND) == 0 ? kResultTrue : kResultFalse;
	}
	tresult PLUGIN_API attached(void* parent, FIDString t) SMTG_OVERRIDE {
		if (gdipRefs++ == 0) { GdiplusStartupInput in; GdiplusStartup(&gdipToken, &in, nullptr); }
		HINSTANCE inst = GetModuleHandleW(nullptr);
		{ HMODULE me = nullptr; GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&proc, &me); if (me) inst = me; }
		WNDCLASSW wc{}; wc.style = CS_DBLCLKS; wc.lpfnWndProc = proc; wc.hInstance = inst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wc.lpszClassName = L"FlowDJDualMixerView"; RegisterClassW(&wc);
		hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, W, H, (HWND)parent, nullptr, inst, nullptr);
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)this);
		SetTimer(hwnd, 1, 33, nullptr);
		return CPluginView::attached(parent, t);
	}
	tresult PLUGIN_API removed() SMTG_OVERRIDE {
		if (hwnd) { KillTimer(hwnd, 1); SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0); DestroyWindow(hwnd); hwnd = nullptr; }
		if (--gdipRefs == 0) GdiplusShutdown(gdipToken);
		return CPluginView::removed();
	}
	tresult PLUGIN_API canResize() SMTG_OVERRIDE { return kResultFalse; }
};
} // namespace GUI

IPlugView* PLUGIN_API DJMixer::createView(FIDString name) {
	if (name && strcmp(name, ViewType::kEditor) == 0) return new GUI::MixerView(this);
	return nullptr;
}
