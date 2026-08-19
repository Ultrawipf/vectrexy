#include "core/LineSimplify.h"
#include "core/Vector2.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr float ZeroLengthEpsilon = 1e-6f;

    bool IsZeroLength(const Line& line) {
        return Magnitude(line.p1 - line.p0) <= ZeroLengthEpsilon;
    }

    // Can `next` be appended to the run we've accumulated into `acc`?
    bool CanMerge(const Line& acc, const Line& next, const LineSimplify::Params& params) {
        if (!params.mergeZeroLengthLines && (IsZeroLength(acc) || IsZeroLength(next)))
            return false;

        const auto accDelta = acc.p1 - acc.p0;
        const auto accLength = Magnitude(accDelta);
        const auto nextDelta = next.p1 - next.p0;
        if (accLength <= ZeroLengthEpsilon || Magnitude(nextDelta) <= ZeroLengthEpsilon)
            return false;

        const auto accDir = Normalized(accDelta);
        if (Dot(accDir, Normalized(nextDelta)) < params.minCosAngle)
            return false;

        if (std::fabs(acc.brightness - next.brightness) > params.maxBrightnessDelta)
            return false;

        // The blanked gap between the end of the run and the start of the next dash.
        if (Magnitude(next.p0 - acc.p1) > params.maxGap)
            return false;

        // Being parallel isn't enough: two opposite sides of a shape are parallel and can be close
        // together. Require both endpoints to sit on the run's own infinite extension, so we only
        // ever weld segments that are genuinely part of the same straight line.
        if (std::fabs(Cross(accDir, next.p0 - acc.p0)) > params.maxPerpOffset)
            return false;
        if (std::fabs(Cross(accDir, next.p1 - acc.p0)) > params.maxPerpOffset)
            return false;

        // Only ever extend forwards. Without this a segment that doubles back along the run would
        // be merged and silently shorten it.
        if (Dot(next.p1 - acc.p0, accDir) < accLength)
            return false;

        return true;
    }
} // namespace

namespace LineSimplify {

    size_t MergeCollinearRuns(std::vector<Line>& lines, const Params& params) {
        if (lines.size() < 2)
            return 0;

        const size_t originalSize = lines.size();

        // In-place forward pass: `write` indexes the run currently being accumulated.
        size_t write = 0;
        for (size_t read = 1; read < lines.size(); ++read) {
            const Line& next = lines[read];
            Line& acc = lines[write];

            if (CanMerge(acc, next, params)) {
                acc.p1 = next.p1;
                acc.brightness = std::max(acc.brightness, next.brightness);
            } else {
                lines[++write] = next;
            }
        }

        lines.resize(write + 1);
        return originalSize - lines.size();
    }

} // namespace LineSimplify
