#include "emulator/BiosTextHook.h"
#include "core/ConsoleOutput.h"
#include "core/Gui.h"
#include "core/Line.h"
#include "emulator/Cpu.h"
#include "emulator/EngineTypes.h"
#include "emulator/MemoryBus.h"
#include "emulator/Screen.h"
#include "emulator/VectorFont.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace {
    // Reading the VIA has side effects, and the region above it faults, so a stray string pointer
    // must never be followed into either.
    bool IsSafeToRead(uint16_t address) { return !(address >= 0xD000 && address <= 0xDFFF); }

    void AccumulateBounds(const std::vector<Line>& lines, size_t begin, Vector2& boundsMin,
                          Vector2& boundsMax) {
        boundsMin = Vector2{FLT_MAX, FLT_MAX};
        boundsMax = Vector2{-FLT_MAX, -FLT_MAX};
        for (size_t i = begin; i < lines.size(); ++i) {
            for (const auto& point : {lines[i].p0, lines[i].p1}) {
                boundsMin.x = std::min(boundsMin.x, point.x);
                boundsMin.y = std::min(boundsMin.y, point.y);
                boundsMax.x = std::max(boundsMax.x, point.x);
                boundsMax.y = std::max(boundsMax.y, point.y);
            }
        }
    }

    // Beam-space units per unit of the BIOS text metrics, measured by running the boot sequence in
    // Mode::Both and comparing the BIOS's own glyph bounds against Vec_Text_HW.
    //
    // Height came out perfectly linear: bboxHeight / |Vec_Text_Height| was 1.1953 for every string
    // sampled (heights -4, -13 and -15).
    constexpr float HeightUnitsPerMetric = 1.1953f;
    // Pitch was measured differentially - comparing the bounds of strings of different lengths at
    // the same width cancels out the beam overshoot that inflates the raw bounding box. That gives
    // an advance of 11.16 units at Vec_Text_Width 96 and 6.94 at 56, i.e. ~0.116 and ~0.124 units
    // per metric. 0.12 is the minimax fit, leaving about 3% error in opposite directions at those
    // two widths - a couple of units of drift across a long string. An affine fit would nail both
    // samples exactly, but with only two widths available that would be fitting noise as readily
    // as a real fixed inter-character gap, so this stays proportional and pitchScale is exposed.
    constexpr float PitchUnitsPerMetric = 0.12f;

    void EmitStrokeText(std::vector<Line>& lines, const char* text, size_t length, Vector2 origin,
                        int8_t vecHeight, int8_t vecWidth, const BiosTextHook::Tuning& tuning) {
        const float height =
            HeightUnitsPerMetric * std::fabs(static_cast<float>(vecHeight)) * tuning.heightScale;

        float pitch = PitchUnitsPerMetric * static_cast<float>(vecWidth) * tuning.pitchScale;
        // A game that leaves the width at zero would otherwise pile every glyph on one spot.
        if (pitch <= 0.f)
            pitch = height * 0.6f;

        const float width = pitch * tuning.glyphWidthRatio;
        if (height <= 0.f || width <= 0.f)
            return;

        // Measured: the top of the BIOS's text sits exactly on the beam position at hook time
        // (bboxTop - origin.y was 0.00 in every sample), because it steps rows away from the origin
        // in the direction of Vec_Text_Height, which is normally negative. So the origin is the top
        // edge of the text, and the glyph cell hangs below it.
        float direction = (vecHeight <= 0) ? -1.f : 1.f;
        if (tuning.flipVertical)
            direction = -direction;

        Vector2 pen{origin.x + tuning.xBias, origin.y + tuning.yBias};
        const float baselineY = (direction < 0.f) ? pen.y - height : pen.y;

        VectorFont::Polyline polylines[VectorFont::MaxPolylinesPerGlyph];
        for (size_t i = 0; i < length; ++i) {
            const uint8_t numPolylines =
                VectorFont::GetGlyph(text[i], polylines, VectorFont::MaxPolylinesPerGlyph);

            for (uint8_t p = 0; p < numPolylines; ++p) {
                const auto& polyline = polylines[p];

                auto toBeamSpace = [&](uint8_t pointIndex) {
                    const float gx = static_cast<float>(polyline.xy[pointIndex * 2 + 0]);
                    const float gy = static_cast<float>(polyline.xy[pointIndex * 2 + 1]);
                    return Vector2{pen.x + (gx / VectorFont::CellW) * width,
                                   baselineY + (gy / VectorFont::CellH) * height};
                };

                for (uint8_t point = 1; point < polyline.numPoints; ++point) {
                    lines.emplace_back(
                        Line{toBeamSpace(point - 1), toBeamSpace(point), tuning.brightness});
                }
            }

            pen.x += pitch;
        }
    }
} // namespace

void BiosTextHook::ValidateBios(const MemoryBus& memoryBus) {
    m_biosSupported = true;
    for (size_t i = 0; i < sizeof(PrintStrSignature); ++i) {
        if (memoryBus.ReadRaw(static_cast<uint16_t>(PrintStrAddress + i)) != PrintStrSignature[i]) {
            m_biosSupported = false;
            break;
        }
    }

    if (!m_biosSupported) {
        Printf("BiosTextHook: Print_Str not found at $%04x; stroke text disabled for this BIOS\n",
               PrintStrAddress);
    }
}

void BiosTextHook::Reset() {
    m_active = false;
    m_watchdog = 0;
    m_entryStackPointer = 0;
    m_biosLinesStartIndex = 0;
    m_lastCapture = {};
}

void BiosTextHook::FrameUpdate() {
    static bool BiosTextHookImGui = false;
    IMGUI_CALL(Debug, ImGui::Checkbox("<<< BIOS Text Hook >>>", &BiosTextHookImGui));

    IMGUI_CALL_IF(BiosTextHookImGui, Debug,
                  ImGui::Text("BIOS supported: %s", m_biosSupported ? "yes" : "no"));

    IMGUI_CALL_IF(BiosTextHookImGui, Debug,
                  ImGui::SliderFloat("Text heightScale", &m_tuning.heightScale, 0.1f, 4.f));
    IMGUI_CALL_IF(BiosTextHookImGui, Debug,
                  ImGui::SliderFloat("Text pitchScale", &m_tuning.pitchScale, 0.1f, 4.f));
    IMGUI_CALL_IF(BiosTextHookImGui, Debug,
                  ImGui::SliderFloat("Text widthRatio", &m_tuning.glyphWidthRatio, 0.1f, 1.5f));
    IMGUI_CALL_IF(BiosTextHookImGui, Debug,
                  ImGui::Checkbox("Text flipVertical", &m_tuning.flipVertical));
    IMGUI_CALL_IF(BiosTextHookImGui, Debug,
                  ImGui::SliderFloat("Text xBias", &m_tuning.xBias, -32.f, 32.f));
    IMGUI_CALL_IF(BiosTextHookImGui, Debug,
                  ImGui::SliderFloat("Text yBias", &m_tuning.yBias, -32.f, 32.f));

    // In Mode::Both this reports where the BIOS actually drew, which is what the scales and biases
    // above should be tuned to match.
    IMGUI_CALL_IF(BiosTextHookImGui && m_lastCapture.valid, Debug,
                  ImGui::Text("Last: \"%s\" h=%d w=%d", m_lastCapture.text,
                              static_cast<int>(m_lastCapture.vecHeight),
                              static_cast<int>(m_lastCapture.vecWidth)));
    IMGUI_CALL_IF(BiosTextHookImGui && m_lastCapture.valid, Debug,
                  ImGui::Text("  origin %.2f,%.2f", m_lastCapture.origin.x, m_lastCapture.origin.y));
    IMGUI_CALL_IF(BiosTextHookImGui && m_lastCapture.strokeLineCount > 0, Debug,
                  ImGui::Text("  stroke bbox %.2f,%.2f .. %.2f,%.2f (%d lines)",
                              m_lastCapture.strokeBboxMin.x, m_lastCapture.strokeBboxMin.y,
                              m_lastCapture.strokeBboxMax.x, m_lastCapture.strokeBboxMax.y,
                              static_cast<int>(m_lastCapture.strokeLineCount)));
    IMGUI_CALL_IF(BiosTextHookImGui && m_lastCapture.biosLineCount > 0, Debug,
                  ImGui::Text("  bios   bbox %.2f,%.2f .. %.2f,%.2f (%d lines)",
                              m_lastCapture.biosBboxMin.x, m_lastCapture.biosBboxMin.y,
                              m_lastCapture.biosBboxMax.x, m_lastCapture.biosBboxMax.y,
                              static_cast<int>(m_lastCapture.biosLineCount)));
}

void BiosTextHook::Release(Screen& screen, RenderContext& renderContext) {
    // In Both mode the BIOS drew its own glyphs while we were active; their bounding box is what
    // the placement model gets calibrated against.
    if (m_mode == Mode::Both && m_lastCapture.valid &&
        renderContext.lines.size() > m_biosLinesStartIndex) {

        AccumulateBounds(renderContext.lines, m_biosLinesStartIndex, m_lastCapture.biosBboxMin,
                         m_lastCapture.biosBboxMax);
        m_lastCapture.biosLineCount = renderContext.lines.size() - m_biosLinesStartIndex;
    }

    screen.SetSuppressLineOutput(false);
    screen.BreakLineContinuity();
    m_active = false;
}

void BiosTextHook::PreExecuteInstruction(const Cpu& cpu, const MemoryBus& memoryBus, Screen& screen,
                                         RenderContext& renderContext) {
    if (m_mode == Mode::Off || !m_biosSupported) {
        // Covers being switched off part-way through a string
        if (m_active)
            Release(screen, renderContext);
        return;
    }

    const auto& registers = cpu.Registers();

    // This must come before the PC test. If an interrupt is pending on the instruction at
    // Print_Str, the CPU services it first and we see that PC again on return; being active
    // already is what stops us from capturing the same string twice.
    if (m_active) {
        // Interrupts only ever push, so S can only drop below the watermark while inside the
        // routine. Coming back above it means the RTS has popped the return address.
        const bool returned = registers.S >= static_cast<uint16_t>(m_entryStackPointer + 2);
        if (returned || ++m_watchdog > WatchdogInstructions)
            Release(screen, renderContext);
        return;
    }

    if (registers.PC != PrintStrAddress)
        return;

    // Read the string that U points at, terminated by $80.
    char text[MaxStringLength + 1] = {};
    size_t length = 0;
    uint16_t address = registers.U;
    while (length < MaxStringLength && IsSafeToRead(address)) {
        const uint8_t value = memoryBus.ReadRaw(address++);
        if (value == StringTerminator)
            break;
        text[length++] = static_cast<char>(value & 0x7F);
    }

    const auto vecHeight = static_cast<int8_t>(memoryBus.ReadRaw(VecTextHeightAddress));
    const auto vecWidth = static_cast<int8_t>(memoryBus.ReadRaw(VecTextWidthAddress));
    const Vector2 origin = screen.GetBeamPos();

    m_lastCapture = {};
    std::copy(text, text + length, m_lastCapture.text);
    m_lastCapture.length = static_cast<uint8_t>(length);
    m_lastCapture.vecHeight = vecHeight;
    m_lastCapture.vecWidth = vecWidth;
    m_lastCapture.origin = origin;
    m_lastCapture.valid = length > 0;

    const size_t strokeStartIndex = renderContext.lines.size();
    if (length > 0) {
        EmitStrokeText(renderContext.lines, text, length, origin, vecHeight, vecWidth, m_tuning);
        m_lastCapture.strokeLineCount = renderContext.lines.size() - strokeStartIndex;
        if (m_lastCapture.strokeLineCount > 0) {
            AccumulateBounds(renderContext.lines, strokeStartIndex, m_lastCapture.strokeBboxMin,
                             m_lastCapture.strokeBboxMax);
        }
    }

    // Stop the beam's next move from stretching the last stroke we just appended.
    screen.BreakLineContinuity();

    if (m_mode == Mode::StrokeOnly)
        screen.SetSuppressLineOutput(true);

    m_biosLinesStartIndex = renderContext.lines.size();
    m_active = true;
    m_entryStackPointer = registers.S;
    m_watchdog = 0;
}
