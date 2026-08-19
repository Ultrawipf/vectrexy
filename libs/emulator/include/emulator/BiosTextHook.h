#pragma once

#include "core/Vector2.h"
#include <cstddef>
#include <cstdint>

class Cpu;
class MemoryBus;
class Screen;
struct RenderContext;

// Replaces the Vectrex BIOS's bitmap text with single-stroke vector text.
//
// Every BIOS print entry point (Print_Str_d $F37A, Print_Str_yx $F378, Print_Str_hwyx $F373, the
// Print_List/Print_Ships variants) ends up jumping to Print_Str at $F495, so watching that one
// address catches all BIOS text. At that point the string pointer is in U and the beam has already
// been moved to the text origin.
//
// The routine is then allowed to execute completely normally - every instruction runs, so cycle
// timing, registers, RAM and the beam position are all bit-identical to an unhooked run. The only
// thing that changes is that Screen is told to stop emitting lines for the duration, so the bitmap
// glyphs it draws are simply never recorded, and our stroke glyphs take their place.
class BiosTextHook {
public:
    enum class Mode {
        Off = 0,        // Unhooked; authentic BIOS bitmap text
        StrokeOnly = 1, // Bitmap text suppressed, stroke text drawn in its place
        Both = 2,       // Both drawn, for calibrating the placement model against the real thing
    };

    // Trim on top of the measured placement model. The defaults were calibrated against the BIOS's
    // own glyphs (see the constants in the .cpp); use Mode::Both and the debug sliders to adjust.
    struct Tuning {
        float heightScale = 1.f;      // Multiplier on the derived glyph height
        float pitchScale = 1.f;       // Multiplier on the derived per-character advance
        float glyphWidthRatio = 0.7f; // Glyph width as a fraction of the advance
        bool flipVertical = false;    // Inverts the direction derived from Vec_Text_Height
        // Horizontal origin correction. Left at zero, text starts at the beam position. The BIOS's
        // own glyph bounds start further left than that (about -10 units at Vec_Text_Width 96, -7
        // at 56), but those bounds are inflated by beam overshoot, so the true inset isn't directly
        // measurable - nudge this if text sits off to one side.
        float xBias = 0.f;
        float yBias = 0.f;
        float brightness = 1.f; // A laser wants full-on
    };

    // What the most recent hook captured, plus the bounding box of the BIOS's own glyph lines when
    // running in Mode::Both. This is the calibration readout.
    struct Capture {
        char text[65] = {};
        uint8_t length = 0;
        int8_t vecHeight = 0;
        int8_t vecWidth = 0;
        Vector2 origin;
        Vector2 strokeBboxMin;
        Vector2 strokeBboxMax;
        Vector2 biosBboxMin;
        Vector2 biosBboxMax;
        size_t strokeLineCount = 0;
        size_t biosLineCount = 0;
        bool valid = false;
    };

    // Checks that the loaded BIOS actually has Print_Str where we expect it. Any other BIOS turns
    // the hook into a no-op rather than letting it misfire at a wrong address.
    void ValidateBios(const MemoryBus& memoryBus);
    bool IsBiosSupported() const { return m_biosSupported; }

    void SetMode(Mode mode) {
        // An options file edited by hand shouldn't be able to produce an unhandled mode
        m_mode = (mode == Mode::StrokeOnly || mode == Mode::Both) ? mode : Mode::Off;
    }
    Mode GetMode() const { return m_mode; }

    Tuning& GetTuning() { return m_tuning; }
    const Capture& GetLastCapture() const { return m_lastCapture; }

    void Reset();

    // Debug UI for calibrating Tuning against the BIOS's own glyphs (see Mode::Both).
    void FrameUpdate();

    // Called once per instruction, before that instruction executes.
    void PreExecuteInstruction(const Cpu& cpu, const MemoryBus& memoryBus, Screen& screen,
                               RenderContext& renderContext);

private:
    void Release(Screen& screen, RenderContext& renderContext);

    // Verified against data/bios/System.bin (md5 ab082fa8c8e632dd68589a8c7741388f). All three BIOS
    // images shipped in data/bios are byte-identical here.
    static constexpr uint16_t PrintStrAddress = 0xF495;
    static constexpr uint8_t PrintStrSignature[] = {0xFF, 0xC8, 0x2C, 0x8E, 0xF9, 0xD4};
    static constexpr uint16_t VecTextHeightAddress = 0xC82A;
    static constexpr uint16_t VecTextWidthAddress = 0xC82B;
    static constexpr uint8_t StringTerminator = 0x80;
    static constexpr size_t MaxStringLength = 64;

    // Escape hatch: if a game reset the stack pointer mid-print we'd never see the return and would
    // leave the screen suppressed. ~1/15th of a second of emulated time.
    static constexpr uint32_t WatchdogInstructions = 100'000;

    Mode m_mode = Mode::Off;
    bool m_biosSupported = false;
    bool m_active = false;
    uint16_t m_entryStackPointer = 0;
    uint32_t m_watchdog = 0;
    size_t m_biosLinesStartIndex = 0;
    Tuning m_tuning;
    Capture m_lastCapture;
};
