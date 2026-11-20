# HarmonizerV v2.6.0

Armonizador vocal profesional de código abierto en **C++20 + JUCE 8** con doble motor de tiempo real y estudio, diseñado tanto para presentaciones en vivo como para producción musical.

---

## Características principales

- **Doble motor seleccionable (`LIVE | STUDIO`) con conmutación sin clics y PDC exacto:**
  - **`LIVE` (TD-PSOLA por épocas glotales):** baja latencia configurable (`2.7 ms / 128 smp`, `5.3 ms / 256 smp`, `8.0 ms / 384 smp`, `10.7 ms / 512 smp`), cruce de banda `< 4.5 kHz` para evitar timbre robótico, guarda anti-aliasing en formantes y soporte extendido para voces graves (`< 120 Hz`).
  - **`STUDIO` (Signalsmith Stretch):** procesamiento espectral de alta calidad (`101.3 ms` PDC a `48 kHz`) para mezclas de estudio.
- **Banco de hasta 8 voces (`4V | 8V`):**
  - Paneo e intervalo diatónico/nota arrastrando en el escenario visual, rueda para nivel, doble clic para encender/apagar y selector de arreglos de acordes **`VOICING` / `ACORDES`** en 1 clic.
  - Por voz: `Interval` / **`FIX` (nota pedal fija C2–C7)**, `Detune`, `Formant`, `Delay` (Haas), `HPF`, `LPF`, `Pan`, `Level` y `Vibrato` orgánico desfasado (`VIB`).
- **Modos `AUTO` (escala/tonalidad) y `MIDI` polifónico (hasta 8 notas):**
  - Soporta Note On/Off, Velocity, Sustain Pedal y Pitch Bend en tiempo real.
- **Control vocal global y medidores en vivo:**
  - **`TUNE`** (corrección de afinación de entrada con medidor bipolar en *cents* y afinador en el encabezado), **`GLIDE`**, **`NATURAL`**, **`DE-ESS`** (con medidor de reducción de ganancia en dB), **`SPACE`**, **`HUMANIZE`** y medidor estéreo con *peak-hold*.
- **Control en vivo avanzado:**
  - **Snapshots `A / B / C / D`** (clic izquierdo cambia, Shift+clic guarda, clic derecho menú completo), **`MIDI Learn`** por Snapshot y **Global Bypass** sin clics, **`Undo / Redo`** y 9 presets de fábrica más presets de usuario.
- **Multiidioma (`EN | ES`):**
  - Detección automática del idioma del sistema operativo al primer inicio y cambio inmediato en vivo guardado entre sesiones.
- **Windows Standalone con ASIO (`JUCE_ASIO=1`) + macOS Universal (`arm64` + `x86_64`):**
  - Formatos soportados: `VST3` y `Standalone` en Windows; `VST3`, `AU` y `Standalone` en macOS.

---

## Instalación rápida

### Windows (`x64`)
1. Descarga el paquete `HarmonizerV-v2.6.0-Windows-x64.zip` y extráelo.
2. Cierra tu DAW antes de instalar o actualizar el plugin.
3. Ejecuta en PowerShell dentro de la carpeta extraída:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\install_vst3_windows.ps1
   ```
   Por defecto instala para tu usuario en `%LOCALAPPDATA%\Programs\Common\VST3\HarmonizerV.vst3`. Si quieres instalación global (requiere PowerShell como Administrador), usa `-Scope System`.
4. Para usar sin DAW con baja latencia ASIO, abre **`HarmonizerV.exe`** directamente.

### macOS Universal (`Apple Silicon arm64 + Intel x86_64`)
1. Descarga el artefacto `HarmonizerV-macOS-Universal` desde GitHub Actions o Releases.
2. Copia:
   - `HarmonizerV.vst3` a `~/Library/Audio/Plug-Ins/VST3/` (o `/Library/Audio/Plug-Ins/VST3/`)
   - `HarmonizerV.component` a `~/Library/Audio/Plug-Ins/Components/` (o `/Library/Audio/Plug-Ins/Components/`)
   - `HarmonizerV.app` a `/Applications/`

---

## Compilación desde código fuente y pruebas

Requisitos: **CMake 3.24+**, compilador C++20 (**MSVC 2022/2026** en Windows o **Xcode / Clang** en macOS). JUCE 8, Signalsmith Stretch y Catch2 se gestionan por `FetchContent`.

```bash
# Configurar (Windows x64 con ASIO y pruebas Catch2)
cmake -B build -A x64 -DHARMONIZERV_BUILD_TESTS=ON -DHARMONIZERV_ENABLE_ASIO=ON

# Compilar tests, VST3 y Standalone
cmake --build build --config Release --target HarmonizerV_Tests HarmonizerV_VST3 HarmonizerV_Standalone

# Ejecutar suite de pruebas unitarias
ctest --test-dir build -C Release --output-on-failure
```

En macOS Universal (`VST3` + `AU` + `Standalone`):

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DHARMONIZERV_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Validación de plugin con `pluginval`:

```bash
pluginval --strictness-level 5 --validate-in-process build/HarmonizerV_artefacts/Release/VST3/HarmonizerV.vst3
```

---

## Soporte y Contacto

Si **HarmonizerV** te resulta útil en tus shows en vivo o mezclas, puedes apoyar el desarrollo de futuras actualizaciones aquí:

- **Buy Me a Coffee:** https://buymeacoffee.com/alexvinicio
