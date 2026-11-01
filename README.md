# 🎧 Flow DJ Dual Mixer (VST3)

Mezclador DJ de dos decks en formato **VST3** para **FL Studio** (y otros DAW compatibles con sidechain).
Mezcla dos pistas con **EQ isolator de 3 bandas**, **crossfader de potencia constante**, **faders de volumen** y un modo **AUTO MIX** que hace transiciones al estilo DJ de forma automática: sube y baja agudos, medios, graves y volumen poco a poco, sincronizado con el tempo del proyecto.

![Plataforma](https://img.shields.io/badge/plataforma-Windows%20x64-blue)
![Formato](https://img.shields.io/badge/formato-VST3-orange)
![DAW](https://img.shields.io/badge/probado%20en-FL%20Studio-ff7a00)

![Flow DJ Dual Mixer](docs/screenshot.png)

> Interfaz basada en el diseño *DJ Equalizer Mixer* creado en Flow Music.

---

## ✨ Características

- **2 decks**
  - **Deck A**: entrada principal (el insert donde cargas el plugin).
  - **Deck B**: entrada **sidechain** estéreo.
- **EQ isolator de 3 bandas por deck**
  - **Bandas**: Low 250 Hz · Mid 1 kHz · High 3,5 kHz, con filtros Linkwitz-Riley de 4.º orden.
  - **Rango**: de −48 dB (corte total) a +6 dB.
  - **EQ KILL**: botones de corte instantáneo por banda.
- **Trim/Gain** y **fader de volumen** por deck.
- **Crossfader** de potencia constante (−3 dB en el centro), con atajos **100% A**, **CENTER** y **100% B**.
- **Master** de salida.
- **Medidores en tiempo real**: A, MIX y B.
- **AUTO EQ**: el EQ de cada deck sigue al crossfader. Los agudos entran primero, después los medios, y los graves se intercambian en el centro.
- **AUTO MIX**: transición automática **A ▶ B** o **B ▶ A**, de **1 a 64 compases**, sincronizada con el tempo del DAW. Las perillas, los faders y el crossfader **se mueven solos** y quedan grabables como automatización.
- **AUTO DETECT**: lanza la mezcla sola cuando empieza a sonar el siguiente tema en la pista A o B.
- Todos los parámetros son **automatizables** y se guardan con el proyecto.

---

## 📦 Instalación

1. Descarga el `.zip` de la sección **Releases**.
2. Cierra FL Studio.
3. Copia la carpeta `FlowDJMixer.vst3` en:
   ```
   C:\Program Files\Common Files\VST3\
   ```
4. Abre FL Studio → **Options → Manage plugins → Find plugins**.
5. Busca **Flow DJ Dual Mixer**.

> Si actualizas a una versión nueva, cierra FL Studio **antes** de sustituir la carpeta.

---

## 🔌 Enrutado en FL Studio

![Enrutado en FL Studio](docs/routing.png)

1. Envía la **pista A** a un insert del mixer (por ejemplo, el insert **1**) y la **pista B** a otro (por ejemplo, el insert **2**).
2. Carga **Flow DJ Dual Mixer** en el insert **1**, que será el Deck A.
3. Selecciona el insert **2** y haz **clic derecho** en la flecha de envío hacia el insert 1. Elige **"Sidechain solo a esta pista"** (*Sidechain to this track only*).
   - Así la pista B **solo** suena a través del plugin y no va directa al Master.
4. En el plugin, abre ⚙ **→ Processing → Connections** y asigna la entrada **Deck B (Sidechain)** al insert 2.

> ⚠️ Si la pista B sigue enviada al Master, o enviada como audio normal (no sidechain) al insert A, se oirá aunque el crossfader esté en **100% A**.

💡 Guarda este enrutado como plantilla (**Archivo → Guardar como** en la carpeta *Templates*) para no repetirlo en cada proyecto.

---

## 🎛️ Uso

### Controles manuales

| Control | Función |
|---|---|
| Perillas | Arrastra arriba/abajo. **Shift/Ctrl** = ajuste fino. **Doble clic** = valor por defecto. **Rueda** = paso a paso. |
| EQ KILL (LOW / MID / HI) | Corta la banda al instante. |
| RESET | Vuelve el EQ y el gain del deck a 0 dB. |
| Fader VOLUME | Volumen del canal de cada deck. |
| Crossfader | Mezcla entre A y B. Botones 100% A · CENTER · 100% B. |

### AUTO EQ

Con **AUTO EQ: ON**, al mover el crossfader el EQ se ajusta solo como en una transición de DJ:

1. Entran los **agudos** del tema nuevo.
2. Se cruzan los **medios**.
3. **Bass swap**: los graves se intercambian en el centro, así que nunca suenan dos bajos a la vez.
4. Sale el tema anterior.

El anillo blanco interior de cada perilla muestra el valor que está sonando de verdad.

### AUTO MIX

![AUTO MIX en plena transición](docs/automix.png)

*AUTO MIX de A a B al 44%: los graves se están intercambiando y el crossfader avanza solo.*

| Control | Función |
|---|---|
| **A ▶ B / B ▶ A** | Lanza la transición en esa dirección. Mientras mezcla, el botón pasa a **■ STOP**. |
| **− [ 8 BARS ] +** | Duración de la transición en compases (de 1 a 64). **Shift + clic** = de 4 en 4. **Rueda** = paso a paso. **Doble clic** = 8. |

- La duración se ajusta al tempo del proyecto. Por ejemplo, a 128 BPM, 8 compases duran 15 s.
- La transición avanza solo mientras el DAW está reproduciendo.
- Tocar el crossfader durante la mezcla la cancela.
- El plugin recuerda los ajustes de EQ y volumen de cada deck y los restaura cuando el tema vuelve a entrar.

### AUTO DETECT

Con **AUTO DETECT: ON**, el plugin vigila la entrada del deck que **no** está sonando. Cuando pasa del silencio (más de 1 s) a tener señal, lanza AUTO MIX hacia ese deck.

| DETECT START | Comportamiento |
|---|---|
| **INSTANT** (por defecto) | La subida empieza en cuanto llega la señal (umbral de −60 dB). |
| **NEXT BAR** | Espera 0,25 s de señal y arranca al principio del siguiente compás. |

**Flujo típico:** coloca cada tema en su pista (A o B) de la Playlist, con su inicio en el punto donde quieres que empiece la transición, y dale a play.

---

## 🎚️ Parámetros (automatizables)

| Parámetro | Rango |
|---|---|
| Crossfader | 0–100 % |
| Deck A/B High · Mid · Low | −48 a +6 dB |
| Deck A/B Gain | −24 a +6 dB |
| Deck A/B Volume | 0–100 % |
| Deck A/B Kill Low · Mid · High | on/off |
| Master Volume | −36 a +6 dB |
| Auto EQ Blend | on/off |
| Auto Mix Bars | 1–64 compases |
| Auto Detect | on/off |
| Detect Start On Bar | on/off |

---

## 🛠️ Compilar desde el código

El plugin está escrito en C++17 con el **VST 3 SDK** de Steinberg. La interfaz es nativa Win32/GDI+, sin dependencias externas.

### Estructura

```
├── README.md
├── docs/
│   ├── screenshot.png   # Interfaz
│   ├── automix.png      # AUTO MIX en marcha
│   └── routing.png      # Esquema de enrutado
└── source/
    ├── djmix.cpp        # DSP, parámetros, AUTO MIX / AUTO DETECT, fábrica VST3
    └── gui.inc          # Editor gráfico Win32 + GDI+
```

### Requisitos

- [VST 3 SDK](https://github.com/steinbergmedia/vst3sdk). Basta con los submódulos `base`, `pluginterfaces` y `public.sdk`.
- MinGW-w64 (g++ ≥ 13), en Windows o como compilador cruzado desde Linux.

### Compilación (MinGW-w64)

```bash
git clone --depth 1 https://github.com/steinbergmedia/vst3sdk.git
cd vst3sdk && git submodule update --init --depth 1 base pluginterfaces public.sdk && cd ..

SDK=./vst3sdk
SRCS="source/djmix.cpp \
  $SDK/public.sdk/source/common/pluginview.cpp \
  $SDK/public.sdk/source/common/commoniids.cpp \
  $SDK/public.sdk/source/vst/vstsinglecomponenteffect.cpp \
  $SDK/public.sdk/source/vst/vstbus.cpp \
  $SDK/public.sdk/source/vst/vstparameters.cpp \
  $SDK/public.sdk/source/vst/vstcomponentbase.cpp \
  $SDK/public.sdk/source/vst/vstinitiids.cpp \
  $SDK/public.sdk/source/main/pluginfactory.cpp \
  $SDK/public.sdk/source/main/dllmain.cpp \
  $SDK/public.sdk/source/main/moduleinit.cpp \
  $SDK/base/source/*.cpp \
  $SDK/base/thread/source/flock.cpp \
  $SDK/pluginterfaces/base/*.cpp"

x86_64-w64-mingw32-g++ -std=c++17 -O2 -shared -DNDEBUG=1 -DRELEASE=1 \
  -I$SDK -Isource -o FlowDJMixer.vst3.dll $SRCS \
  -static -static-libgcc -static-libstdc++ \
  -lgdiplus -lgdi32 -luser32 -lole32 -luuid -Wl,--kill-at
x86_64-w64-mingw32-strip FlowDJMixer.vst3.dll
```

### Empaquetar como bundle VST3

```bash
mkdir -p FlowDJMixer.vst3/Contents/x86_64-win
cp FlowDJMixer.vst3.dll FlowDJMixer.vst3/Contents/x86_64-win/FlowDJMixer.vst3
```

---

## ⚠️ Limitaciones conocidas

- Solo **Windows x64**. No hay versión macOS/AU.
- La interfaz tiene un tamaño fijo de 1000 × 800 px.
- Con **DETECT START: INSTANT**, cualquier ruido al principio de un clip (un fade-in largo, ruido de fondo) dispara la mezcla.
- Un silencio de más de 1 s **dentro** de un tema que está en el deck vigilado puede disparar AUTO DETECT.
- AUTO MIX no sincroniza el tempo de los temas (beatmatching). Ajusta el BPM de los clips en FL Studio (*Detectar tempo* + modo *Stretch*).

---

## 🗺️ Ideas para próximas versiones

- [ ] Curvas de transición seleccionables (suave / bass swap rápido / corte).
- [ ] Filtro HPF/LPF por deck para transiciones con filtro.
- [ ] Interfaz redimensionable y soporte HiDPI.

> Las capturas de `docs/` están tomadas del plugin real. Las fuentes pueden verse algo distintas a las de Windows.
- [ ] Build para macOS (VST3/AU).

---

## 📄 Licencia

Indica aquí la licencia del proyecto (por ejemplo, MIT o GPL-3.0).
El plugin usa el **VST 3 SDK** de Steinberg: revisa su licencia en el repositorio oficial antes de distribuirlo.
*VST is a registered trademark of Steinberg Media Technologies GmbH.*
