#pragma once

#include <algorithm>
#include <array>

namespace harmonizer::dsp
{

enum class Language : int
{
    English = 0,
    Spanish = 1,
    NumLanguages
};

inline const char* languageCode(Language lang) noexcept
{
    return (lang == Language::Spanish) ? "es" : "en";
}

inline Language languageFromCode(const char* code) noexcept
{
    if (code == nullptr)
        return Language::English;

    const char c0 = code[0];
    const char c1 = (c0 != '\0') ? code[1] : '\0';
    if ((c0 == 'e' || c0 == 'E') && (c1 == 's' || c1 == 'S'))
        return Language::Spanish;

    return Language::English;
}

enum class TextKey : int
{
    // Header & Presets
    BypassTooltip = 0,
    MidiPrefix,
    MidiLearnBypass,
    CancelMidiLearn,
    ClearMidiBinding,
    PrevPresetTooltip,
    NextPresetTooltip,
    SaveBtn,
    SavePresetTooltip,
    SaveDialogTitle,
    SaveDialogMessage,
    SaveDialogDefault,
    CancelBtn,
    EngineTooltip,
    HarmonyModeTooltip,
    LangTooltip,
    AboutBtn,
    AboutTooltip,

    // LIVE buffer menu
    BufUltra,
    BufFast,
    BufLow,
    BufSafe,

    // Stage & Snapshots
    StageTooltip,
    VoicingBtn,
    SnapStoreInto,
    SnapCopyFrom,
    SnapClear,
    SnapMidiLearn,
    SnapResetPc,
    MidiLearnSnapHint,
    MidiLearnBypassHint,
    StageMidiHint,
    StageAutoHint,
    LeadAxis,
    CenterAxis,
    BypassedBadge,

    // Feel bar tooltips
    TuneTooltip,
    GlideTooltip,
    NaturalTooltip,
    DeEssTooltip,
    SpaceTooltip,

    // Voice Inspector labels & tooltips
    VoicePrefix,
    FixBtnTooltip,
    KnobInterval,
    KnobFixNote,
    KnobDetune,
    KnobFormant,
    KnobDelay,
    KnobHpf,
    KnobLpf,
    KnobPan,
    KnobLevel,
    KnobVib,
    KnobHumanize,
    KnobMixTooltip,
    KnobOutTooltip,
    KnobIntervalTooltip,
    KnobFixNoteTooltip,
    KnobDetuneTooltip,
    KnobFormantTooltip,
    KnobDelayTooltip,
    KnobHpfTooltip,
    KnobLpfTooltip,
    KnobPanTooltip,
    KnobLevelTooltip,
    KnobVibTooltip,
    KnobHumanizeTooltip,

    // About / Buy Me a Coffee modal
    AboutSubtitle,
    AboutFeatures,
    AboutSupportTitle,
    AboutSupportDesc,
    AboutCoffeeButton,
    AboutPendingLinkNote,
    CloseBtn,

    NumKeys
};

inline constexpr int kNumTextKeys = static_cast<int>(TextKey::NumKeys);

struct BilingualEntry
{
    const char* en;
    const char* es;
};

// Pure ASCII file: UTF-8 Spanish accents are encoded with explicit hex escapes and split whenever
// followed by [0-9a-fA-F] so C++ literal hex parsing never over-consumes characters.
inline constexpr std::array<BilingualEntry, static_cast<size_t>(kNumTextKeys)> kTextTable = {{
    // BypassTooltip
    {
        "Global Bypass (Click: Toggle | Right-Click: MIDI Learn)",
        "Bypass global (Clic: Activar/Desactivar | Clic derecho: MIDI Learn)"
    },
    // MidiPrefix
    { "MIDI: ", "MIDI: " },
    // MidiLearnBypass
    { "MIDI Learn Bypass", "MIDI Learn Bypass" },
    // CancelMidiLearn
    { "Cancel MIDI Learn", "Cancelar MIDI Learn" },
    // ClearMidiBinding
    { "Clear MIDI Binding", "Borrar asignaci\xC3\xB3" "n MIDI" },
    // PrevPresetTooltip
    { "Previous Preset", "Preset anterior" },
    // NextPresetTooltip
    { "Next Preset", "Preset siguiente" },
    // SaveBtn
    { "Save", "Guardar" },
    // SavePresetTooltip
    { "Save Current State as User Preset", "Guardar configuraci\xC3\xB3" "n actual como preset" },
    // SaveDialogTitle
    { "Save User Preset", "Guardar preset de usuario" },
    // SaveDialogMessage
    { "Enter a name for your vocal harmony preset:", "Escribe un nombre para tu preset de armon\xC3\xAD" "a:" },
    // SaveDialogDefault
    { "My Harmony", "Mi Armon\xC3\xAD" "a" },
    // CancelBtn
    { "Cancel", "Cancelar" },
    // EngineTooltip
    {
        "LIVE / STUDIO engine | Click latency badge in LIVE mode to change DSP buffer (2.7-10.7 ms)",
        "Motor LIVE / STUDIO | Clic en la latencia en modo LIVE para cambiar buffer DSP (2.7-10.7 ms)"
    },
    // HarmonyModeTooltip
    {
        "AUTO: Diatonic scale harmonies | MIDI: Play harmony notes from MIDI keyboard",
        "AUTO: Armon\xC3\xAD" "as por escala | MIDI: Toca las voces desde teclado MIDI"
    },
    // LangTooltip
    {
        "Switch language (EN / ES)",
        "Cambiar idioma (EN / ES)"
    },
    // AboutBtn
    { "About", "Acerca" },
    // AboutTooltip
    {
        "Version, credits & support contact (Buy Me a Coffee)",
        "Versi\xC3\xB3" "n, cr\xC3\xA9" "ditos y contacto de soporte (Buy Me a Coffee)"
    },

    // BufUltra .. BufSafe
    { "Ultra - 2.7 ms (128 smp)", "Ultra - 2.7 ms (128 smp)" },
    { "Fast - 5.3 ms (256 smp)",  "R\xC3\xA1" "pido - 5.3 ms (256 smp)" },
    { "Low - 8.0 ms (384 smp)",   "Bajo - 8.0 ms (384 smp)" },
    { "Safe - 10.7 ms (512 smp)", "Seguro - 10.7 ms (512 smp)" },

    // StageTooltip
    {
        "Drag orbs: Pan/Pitch | Wheel: Level | 4V/8V voices | VOICING menu | Undo/Redo | Snapshots A-D",
        "Arrastra orbes: Pan/Altura | Rueda: Nivel | 4V/8V voces | Men\xC3\xBA Acordes | Deshacer/Rehacer | Escenas A-D"
    },
    // VoicingBtn
    { "VOICING", "ACORDES" },
    // SnapStoreInto
    { "Store Current into {0}", "Guardar actual en {0}" },
    // SnapCopyFrom
    { "Copy from {0}", "Copiar desde {0}" },
    // SnapClear
    { "Clear {0}", "Limpiar {0}" },
    // SnapMidiLearn
    { "MIDI Learn {0}", "MIDI Learn {0}" },
    // SnapResetPc
    { "Reset MIDI to PC {0}", "Restablecer MIDI a PC {0}" },
    // MidiLearnSnapHint
    {
        "MIDI LEARN: Send PC, CC, or Note for Snap {0}",
        "MIDI LEARN: Env\xC3\xAD" "a PC, CC o Nota para escena {0}"
    },
    // MidiLearnBypassHint
    {
        "MIDI LEARN: Send PC, CC, or Note for Bypass",
        "MIDI LEARN: Env\xC3\xAD" "a PC, CC o Nota para Bypass"
    },
    // StageMidiHint
    {
        "MIDI | Play Chords | Drag X: Pan | Wheel: Level",
        "MIDI | Toca acordes | Arrastra X: Pan | Rueda: Nivel"
    },
    // StageAutoHint
    {
        "Drag: Pan / Pitch | Wheel: Level | Dbl-Click: On/Off",
        "Arrastra: Pan / Altura | Rueda: Nivel | Doble clic: On/Off"
    },
    // LeadAxis
    { "Lead", "Voz" },
    // CenterAxis
    { "Center", "Centro" },
    // BypassedBadge
    { "BYPASSED", "EN BYPASS" },

    // TuneTooltip
    {
        "Input pitch correction (0-100%) | Live meter shows pitch pull in cents | Double-click to reset",
        "Correcci\xC3\xB3" "n de afinaci\xC3\xB3" "n de entrada (0-100%) | Medidor en cents | Doble clic: reset"
    },
    // GlideTooltip
    {
        "Pitch transition glide time (0-150 ms) | Double-click to reset",
        "Tiempo de deslizamiento entre notas (0-150 ms) | Doble clic: reset"
    },
    // NaturalTooltip
    {
        "Formant follow and high-band vocal air (0-100%) | Double-click to reset",
        "Seguimiento de formantes y aire vocal agudo (0-100%) | Doble clic: reset"
    },
    // DeEssTooltip
    {
        "Harmony bus sibilance control (0-100%) | Live meter shows HF gain reduction (dB) | Double-click to reset",
        "Control de sibilancia en armon\xC3\xAD" "as (0-100%) | Medidor de reducci\xC3\xB3" "n (dB) | Doble clic: reset"
    },
    // SpaceTooltip
    {
        "Studio stereo vocal space on harmony bus (0-100%) | Double-click to reset",
        "Ambiente est\xC3\xA9" "reo de estudio en el bus de armon\xC3\xAD" "as (0-100%) | Doble clic: reset"
    },

    // VoicePrefix
    { "VOICE ", "VOZ " },
    // FixBtnTooltip
    {
        "Lock voice to a fixed pedal note instead of scale interval",
        "Fijar esta voz a una nota pedal en lugar de seguir el intervalo"
    },
    // Knob labels (short uppercase ASCII-compatible display)
    { "INTERVAL", "INTERVALO" },
    { "FIX NOTE", "NOTA FIJA" },
    { "DETUNE",   "DESAFIN." },
    { "FORMANT",  "FORMANTE" },
    { "DELAY",    "RETARDO" },
    { "HPF",      "HPF" },
    { "LPF",      "LPF" },
    { "PAN",      "PAN" },
    { "LEVEL",    "NIVEL" },
    { "VIB",      "VIBRATO" },
    { "HUMANIZE", "HUMANIZAR" },

    // Knob tooltips
    {
        "Dry lead vs harmony bus balance (0-100%)",
        "Mezcla entre voz original y armon\xC3\xAD" "as (0-100%)"
    },
    {
        "Master output trim (-24 to +12 dB)",
        "Ganancia de salida maestra (-24 a +12 dB)"
    },
    {
        "Diatonic scale interval for this voice",
        "Intervalo diat\xC3\xB3" "nico respecto a la voz principal"
    },
    {
        "Fixed pedal MIDI note (C2-C7) held regardless of sung melody",
        "Nota pedal fija (C2-C7) sostenida aunque cambie la melod\xC3\xAD" "a"
    },
    {
        "Fine pitch detune (-50 to +50 cents)",
        "Micro-desafinaci\xC3\xB3" "n fina (-50 a +50 cents)"
    },
    {
        "Vocal formant character shift (-12 to +12 st)",
        "Desplazamiento de formantes vocales (-12 a +12 st)"
    },
    {
        "Per-voice micro-delay for stereo Haas width (0-40 ms)",
        "Micro-retardo por voz para amplitud est\xC3\xA9" "reo (0-40 ms)"
    },
    {
        "Voice high-pass filter cutoff (Hz)",
        "Filtro pasa-altos de la voz (Hz)"
    },
    {
        "Voice low-pass filter cutoff (Hz / kHz)",
        "Filtro pasa-bajos de la voz (Hz / kHz)"
    },
    {
        "Voice stereo panorama (L100 - C - R100)",
        "Paneo est\xC3\xA9" "reo de la voz (L100 - C - R100)"
    },
    {
        "Voice output level (-inf to +6 dB)",
        "Nivel de salida de la voz (-inf a +6 dB)"
    },
    {
        "Staggered organic vibrato depth (0-100%)",
        "Profundidad de vibrato org\xC3\xA1" "nico desfasado (0-100%)"
    },
    {
        "Organic drift in timing, pitch, breath, and stereo spread",
        "Variaci\xC3\xB3" "n humana en tiempo, afinaci\xC3\xB3" "n, aire y est\xC3\xA9" "reo"
    },

    // About modal
    {
        "Real-Time 8-Voice Vocal Harmonizer (VST3 / AU / Standalone ASIO)",
        "Armonizador vocal de hasta 8 voces en tiempo real (VST3 / AU / Standalone ASIO)"
    },
    {
        "Dual LIVE (2.7-10.7 ms TD-PSOLA) & STUDIO (Signalsmith) engines | Polyphonic MIDI | A/B/C/D Snapshots",
        "Doble motor LIVE (2.7-10.7 ms TD-PSOLA) y STUDIO (Signalsmith) | MIDI polif\xC3\xB3" "nico | Escenas A/B/C/D"
    },
    {
        "Support & Contact",
        "Soporte y Contacto"
    },
    {
        "If HarmonizerV helps your live shows or mixes, you can support updates and get in touch here:",
        "Si HarmonizerV te ayuda en vivo o en mezcla, puedes apoyar su desarrollo y contactar aqu\xC3\xAD" ":"
    },
    {
        "Buy Me a Coffee",
        "Buy Me a Coffee"
    },
    {
        "Creator link pending final handle before public release.",
        "Enlace de creador pendiente de URL final antes del empaquetado p\xC3\xBA" "blico."
    },
    {
        "Close",
        "Cerrar"
    }
}};

inline constexpr std::array<BilingualEntry, 9> kScaleNames = {{
    { "Major",            "Mayor" },
    { "Natural Minor",    "Menor natural" },
    { "Harmonic Minor",   "Menor arm\xC3\xB3" "nica" },
    { "Melodic Minor",    "Menor mel\xC3\xB3" "dica" },
    { "Dorian",           "D\xC3\xB3" "rica" },
    { "Mixolydian",       "Mixolidia" },
    { "Major Pentatonic", "Pentat\xC3\xB3" "nica mayor" },
    { "Minor Pentatonic", "Pentat\xC3\xB3" "nica menor" },
    { "Chromatic",        "Crom\xC3\xA1" "tica" }
}};

inline constexpr std::array<BilingualEntry, 8> kVoicingNames = {{
    { "Classic Triad (3V)",        "Tr\xC3\xAD" "ada cl\xC3\xA1" "sica (3V)" },
    { "Open Pop 4-Part (4V)",      "Pop abierto a 4 voces (4V)" },
    { "Gospel 7th Choir (4V)",     "Coro Gospel 7ma (4V)" },
    { "Power 5ths & Octaves (4V)", "Quintas y octavas (4V)" },
    { "Pedal Tonic + Triad (4V)",  "T\xC3\xB3" "nica pedal + Tr\xC3\xAD" "ada (4V)" },
    { "Epic 8-Voice Choir (8V)",   "Coro \xC3\xA9" "pico 8 voces (8V)" },
    { "Wide 8V Worship Wall (8V)", "Muro Worship amplio (8V)" },
    { "Close Jazz 8V Spread (8V)", "Acorde Jazz cerrado (8V)" }
}};

inline const char* lookupUtf8(Language lang, TextKey key) noexcept
{
    const int idx = std::clamp(static_cast<int>(key), 0, kNumTextKeys - 1);
    const auto& entry = kTextTable[static_cast<size_t>(idx)];
    return (lang == Language::Spanish) ? entry.es : entry.en;
}

inline const char* lookupScaleNameUtf8(Language lang, int scaleIndex) noexcept
{
    const int idx = std::clamp(scaleIndex, 0, static_cast<int>(kScaleNames.size()) - 1);
    const auto& entry = kScaleNames[static_cast<size_t>(idx)];
    return (lang == Language::Spanish) ? entry.es : entry.en;
}

inline const char* lookupVoicingNameUtf8(Language lang, int voicingIndex) noexcept
{
    const int idx = std::clamp(voicingIndex, 0, static_cast<int>(kVoicingNames.size()) - 1);
    const auto& entry = kVoicingNames[static_cast<size_t>(idx)];
    return (lang == Language::Spanish) ? entry.es : entry.en;
}

} // namespace harmonizer::dsp
