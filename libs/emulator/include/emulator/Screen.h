#pragma once

#include "core/Vector2.h"
#include "emulator/DelayedValueStore.h"

struct RenderContext;

// Models the actual 9" screen that comes with a Vectrex, including hardware delays when moving the
// beam, etc.
class Screen {
public:
    void Init();
    void Update(cycles_t cycles, RenderContext& renderContext);
    void FrameUpdate(double frameTime);

    void ZeroBeam();
    void SetBlankEnabled(bool enabled) { m_blank = enabled; }
    void SetIntegratorsEnabled(bool enabled) { m_integratorsEnabled = enabled; }
    void SetIntegratorX(int8_t value) { m_velocityX = value; }
    void SetIntegratorY(int8_t value) { m_velocityY = value; }
    void SetIntegratorXYOffset(int8_t value) { m_xyOffset = value; }
    void SetBrightness(uint8_t value) { m_brightness = value; }

    void SetBrightnessCurve(float v) { m_brightnessCurve = v; }

    Vector2 GetBeamPos() const { return m_pos; }

    // While suppressed, the beam keeps moving exactly as it would otherwise, but emits no lines.
    // Used to hide the output of a BIOS routine we're replacing with our own vectors, without
    // perturbing emulation in any way.
    void SetSuppressLineOutput(bool enabled) { m_suppressLineOutput = enabled; }
    bool IsLineOutputSuppressed() const { return m_suppressLineOutput; }

    // Prevents the next drawn segment from extending the last line in the render context. Callers
    // that append their own lines must use this, or the beam's next move will stretch one of them.
    void BreakLineContinuity() { m_lastDrawingEnabled = false; }

private:
    bool m_integratorsEnabled{};
    Vector2 m_pos;

    bool m_lastDrawingEnabled{};
    Vector2 m_lastDir;

    DelayedValueStore<float> m_velocityX;
    DelayedValueStore<float> m_velocityY;
    float m_xyOffset = 0.f;
    float m_brightness = 0.f;
    bool m_blank = false;
    enum class RampPhase { RampOff, RampUp, RampOn, RampDown } m_rampPhase = RampPhase::RampOff;
    int32_t m_rampDelay = 0;

    float m_brightnessCurve = 0.f; // Set externally
    bool m_suppressLineOutput = false;
};
