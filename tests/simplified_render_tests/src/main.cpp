// Verification for the simplified rendering mode.
//
// The central guarantee of the BIOS text hook is that turning it on changes nothing about the
// emulated machine - only which lines get emitted. That's checked here by running the same boot
// sequence in all three modes and comparing a rolling hash of the CPU registers after every
// instruction, plus the total cycle count.
//
// It also pins down the stroke text placement model against the BIOS's own glyph bounds, and
// covers the dashed-line simplifier with synthetic input.
//
// Usage: simplified_render_tests [path/to/System.bin]
#include "core/LineSimplify.h"
#include "emulator/Emulator.h"
#include "emulator/EngineTypes.h"
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>

namespace {
    constexpr size_t InstructionsToRun = 4'000'000;

    struct RunResult {
        uint64_t registerHash = 0;
        uint64_t totalCycles = 0;
        size_t totalLinesEmitted = 0;
        bool hookFired = false;
        char capturedText[65] = {};
        size_t maxSuppressionRun = 0;
        std::vector<BiosTextHook::Capture> captures;
    };

    void Mix(uint64_t& hash, uint64_t value) {
        hash ^= value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2);
    }

    RunResult Run(const char* biosPath, BiosTextHook::Mode mode) {
        RunResult result;

        Emulator emulator;
        emulator.Init(biosPath);
        emulator.Reset();
        // Reset() randomizes RAM from std::random_device, which would make runs incomparable
        emulator.GetRam().Zero();
        emulator.GetBiosTextHook().SetMode(mode);

        Input input;
        RenderContext renderContext;
        AudioContext audioContext{100.f};

        size_t suppressionRun = 0;
        size_t lastBiosLineCount = 0;
        for (size_t i = 0; i < InstructionsToRun; ++i) {
            result.totalCycles += emulator.ExecuteInstruction(input, renderContext, audioContext);

            // biosLineCount goes 0 -> nonzero when a print completes in calibration mode
            const auto& liveCapture = emulator.GetBiosTextHook().GetLastCapture();
            if (liveCapture.biosLineCount != 0 && lastBiosLineCount == 0 &&
                result.captures.size() < 12) {
                result.captures.push_back(liveCapture);
            }
            lastBiosLineCount = liveCapture.biosLineCount;

            const auto& r = emulator.GetCpu().Registers();
            Mix(result.registerHash, r.PC);
            Mix(result.registerHash, (static_cast<uint64_t>(r.S) << 16) | r.U);
            Mix(result.registerHash, (static_cast<uint64_t>(r.X) << 16) | r.Y);
            Mix(result.registerHash, (static_cast<uint64_t>(r.D) << 16) | r.DP);
            Mix(result.registerHash, r.CC.Value);

            if (emulator.GetVia().GetScreen().IsLineOutputSuppressed()) {
                ++suppressionRun;
                if (suppressionRun > result.maxSuppressionRun)
                    result.maxSuppressionRun = suppressionRun;
            } else {
                suppressionRun = 0;
            }

            // Emulate the engine's per-frame clear so the list doesn't grow without bound
            if (renderContext.lines.size() > 100000) {
                result.totalLinesEmitted += renderContext.lines.size();
                renderContext.lines.clear();
            }
        }
        result.totalLinesEmitted += renderContext.lines.size();

        const auto& capture = emulator.GetBiosTextHook().GetLastCapture();
        result.hookFired = capture.valid;
        snprintf(result.capturedText, sizeof(result.capturedText), "%s", capture.text);
        return result;
    }

    int failures = 0;
    void Check(bool condition, const char* what) {
        printf("[%s] %s\n", condition ? " OK " : "FAIL", what);
        if (!condition)
            ++failures;
    }
} // namespace

int main(int argc, char** argv) {
    const char* biosPath = argc > 1 ? argv[1] : "data/bios/System.bin";

    printf("--- Running %zu instructions per mode with BIOS %s\n", InstructionsToRun, biosPath);

    const RunResult off = Run(biosPath, BiosTextHook::Mode::Off);
    const RunResult stroke = Run(biosPath, BiosTextHook::Mode::StrokeOnly);
    const RunResult both = Run(biosPath, BiosTextHook::Mode::Both);

    printf("off:    hash=%016llx cycles=%llu totalLines=%zu fired=%d maxSuppress=%zu\n",
           (unsigned long long)off.registerHash, (unsigned long long)off.totalCycles,
           off.totalLinesEmitted, (int)off.hookFired, off.maxSuppressionRun);
    printf("stroke: hash=%016llx cycles=%llu totalLines=%zu fired=%d maxSuppress=%zu text=\"%s\"\n",
           (unsigned long long)stroke.registerHash, (unsigned long long)stroke.totalCycles,
           stroke.totalLinesEmitted, (int)stroke.hookFired, stroke.maxSuppressionRun,
           stroke.capturedText);
    printf("both:   hash=%016llx cycles=%llu totalLines=%zu fired=%d maxSuppress=%zu text=\"%s\"\n",
           (unsigned long long)both.registerHash, (unsigned long long)both.totalCycles,
           both.totalLinesEmitted, (int)both.hookFired, both.maxSuppressionRun, both.capturedText);

    printf("\n--- Stroke placement vs BIOS glyph bounds\n");
    float worstHeightError = 0.f;
    float worstTopError = 0.f;
    for (const auto& c : both.captures) {
        const float biosH = c.biosBboxMax.y - c.biosBboxMin.y;
        const float strokeH = c.strokeBboxMax.y - c.strokeBboxMin.y;
        const float heightError = biosH > 0.f ? std::abs(strokeH - biosH) / biosH : 0.f;
        const float topError = std::abs(c.strokeBboxMax.y - c.biosBboxMax.y);
        if (heightError > worstHeightError)
            worstHeightError = heightError;
        if (topError > worstTopError)
            worstTopError = topError;

        printf("  \"%s\" h=%d w=%d origin=(%.2f,%.2f)\n", c.text, (int)c.vecHeight, (int)c.vecWidth,
               c.origin.x, c.origin.y);
        printf("      bios   y %.2f..%.2f (h=%.2f) x %.2f..%.2f  %zu lines\n", c.biosBboxMin.y,
               c.biosBboxMax.y, biosH, c.biosBboxMin.x, c.biosBboxMax.x, c.biosLineCount);
        printf("      stroke y %.2f..%.2f (h=%.2f) x %.2f..%.2f  %zu lines   "
               "heightErr=%.1f%% topErr=%.2f\n",
               c.strokeBboxMin.y, c.strokeBboxMax.y, strokeH, c.strokeBboxMin.x, c.strokeBboxMax.x,
               c.strokeLineCount, heightError * 100.f, topError);
    }
    printf("  worst height error %.1f%%, worst top misalignment %.2f units\n",
           worstHeightError * 100.f, worstTopError);

    // The whole point: turning the hook on must not perturb the emulated machine at all.
    Check(off.registerHash == stroke.registerHash, "CPU register trace identical (off vs stroke)");
    Check(off.registerHash == both.registerHash, "CPU register trace identical (off vs both)");
    Check(off.totalCycles == stroke.totalCycles, "Cycle count identical (off vs stroke)");
    Check(off.totalCycles == both.totalCycles, "Cycle count identical (off vs both)");

    Check(stroke.hookFired, "Hook fired at Print_Str in stroke mode");
    Check(!off.hookFired, "Hook did not fire when disabled");
    Check(off.maxSuppressionRun == 0, "No suppression when disabled");
    Check(stroke.maxSuppressionRun > 0, "Suppression engaged in stroke mode");
    // A stuck flag is the main failure mode; a print is thousands of instructions, not millions.
    Check(stroke.maxSuppressionRun < 100000, "Suppression always released (no stuck flag)");
    Check(both.maxSuppressionRun == 0, "No suppression in calibration mode");
    Check(!both.captures.empty(), "Captured calibration samples");
    Check(worstHeightError < 0.10f, "Stroke text height matches BIOS within 10%");
    Check(worstTopError < 1.0f, "Stroke text top edge aligns with BIOS within 1 unit");
    Check(stroke.totalLinesEmitted < off.totalLinesEmitted,
          "Stroke text emits fewer lines than BIOS bitmap text");

    // Line simplification on a synthetic dashed border: 10 dashes along y=0 with 1-unit gaps.
    {
        std::vector<Line> lines;
        for (int i = 0; i < 10; ++i) {
            const float x = static_cast<float>(i) * 4.f;
            lines.push_back(Line{{x, 0.f}, {x + 3.f, 0.f}, 1.f});
        }
        // A parallel run offset in y must NOT be absorbed into the first
        for (int i = 0; i < 10; ++i) {
            const float x = static_cast<float>(i) * 4.f;
            lines.push_back(Line{{x, 20.f}, {x + 3.f, 20.f}, 1.f});
        }
        LineSimplify::Params params;
        const size_t removed = LineSimplify::MergeCollinearRuns(lines, params);
        printf("simplify: 20 dashes -> %zu lines (removed %zu)\n", lines.size(), removed);
        Check(lines.size() == 2, "Two dashed runs merge into exactly two lines");
        Check(lines[0].p0.x == 0.f && lines[0].p1.x == 39.f, "First run spans the full border edge");
        Check(lines[1].p0.y == 20.f, "Offset parallel run kept separate");
    }

    // Dots must survive
    {
        std::vector<Line> lines;
        for (int i = 0; i < 5; ++i) {
            const float x = static_cast<float>(i) * 2.f;
            lines.push_back(Line{{x, 5.f}, {x, 5.f}, 1.f});
        }
        LineSimplify::Params params;
        LineSimplify::MergeCollinearRuns(lines, params);
        Check(lines.size() == 5, "Zero-length dots are preserved");
    }

    printf("\n%s (%d failure(s))\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED",
           failures);
    return failures == 0 ? 0 : 1;
}
