# LowLatTune: guida per compilare e usare il VST3

Pitch corrector monofonico a bassa latenza (YIN + shifter a linea di ritardo).
Parametri: Retune Speed (ms) e 12 note abilitabili (la scala).

NOTA: il progetto non e' stato compilato con JUCE da chi l'ha scritto (niente rete
nell'ambiente di lavoro). Il nucleo audio (`Source/lowlat_autotune.h`) e' stato
compilato e testato. Se al primo build compare un errore, copia il testo dell'errore
e fattelo correggere.

## 1. Strumenti da installare (una volta sola)

### Windows 10/11
1. Visual Studio 2022 Community (gratis): nell'installer spunta "Sviluppo di applicazioni desktop con C++".
2. CMake: https://cmake.org/download (spunta "Add CMake to PATH").
3. Git: https://git-scm.com/download/win

### macOS
1. Terminale: `xcode-select --install`
2. Homebrew (https://brew.sh), poi `brew install cmake git`

### Linux (Ubuntu/Debian)
```
sudo apt update
sudo apt install build-essential cmake git libasound2-dev libjack-jackd2-dev \
  libfreetype-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev
```

## 2. Compilare

Apri un terminale (su Windows: PowerShell) nella cartella `LowLatTune` (quella con CMakeLists.txt).

### Windows
```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```
Risultato: `build\LowLatTune_artefacts\Release\VST3\LowLatTune.vst3`

### macOS
```
cmake -B build -G Xcode
cmake --build build --config Release
```
Risultato: `build/LowLatTune_artefacts/Release/VST3/LowLatTune.vst3` e `.../AU/LowLatTune.component`

### Linux
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
Risultato: `build/LowLatTune_artefacts/VST3/LowLatTune.vst3`

Il primo build scarica JUCE da GitHub (serve internet) e puo' richiedere diversi minuti.

## 3. Installare

| Sistema | Cartella di destinazione |
|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` (copia come amministratore) |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/` |
| macOS AU | `~/Library/Audio/Plug-Ins/Components/` |
| Linux | `~/.vst3/` |

Copia l'intero `LowLatTune.vst3` (su Windows e' una cartella, copiala intera).

macOS, se il plugin viene bloccato o non appare:
```
xattr -cr ~/Library/Audio/Plug-Ins/VST3/LowLatTune.vst3
codesign --force --deep -s - ~/Library/Audio/Plug-Ins/VST3/LowLatTune.vst3
```

## 4. Farlo comparire nella DAW

Riavvia la DAW e fai uno scan dei plugin:
- Ableton Live: Preferenze > Plug-in > abilita "Usa plug-in VST3" > Rescan.
- FL Studio: Options > Manage plugins > Find installed plugins.
- Cubase / Studio One / Reaper / Bitwig / Cakewalk: Rescan nelle preferenze plugin.
- Logic Pro e GarageBand: leggono solo AU (macOS). Usa il file `.component`.
- Pro Tools: richiede formato AAX, che ha bisogno dell'SDK Avid e della firma PACE. Non incluso.

Compatibilita': VST3 a 64 bit. Niente VST2 e niente 32 bit.

## 5. Come usarlo

- Mettilo come primo effetto su una traccia con UNA voce o strumento monofonico.
- Seleziona le note della scala (spegni quelle fuori tonalita'). Se spegni tutte, torna cromatico.
- Retune Speed 0 ms = effetto "hard tune"; 20-50 ms = correzione piu' naturale.
- Non funziona su accordi o materiale polifonico.
- Il plugin non comunica latenza alla DAW (varia con la nota, circa 2-15 ms).
  In mixaggio, se serve, allinea la traccia a mano.
- Il limite grave e' 70 Hz; per voci piu' basse modifica `prepare(..., 70.f, ...)` in PluginProcessor.cpp.
