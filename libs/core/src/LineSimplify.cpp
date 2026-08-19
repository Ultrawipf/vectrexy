#include "core/LineSimplify.h"
#include "core/Vector2.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace {
    constexpr float ZeroLengthEpsilon = 1e-6f;

    bool IsDot(const Line& line) { return Magnitude(line.p1 - line.p0) <= ZeroLengthEpsilon; }

    // Can `next` be appended to the run we've accumulated into `acc`?
    bool CanMerge(const Line& acc, const Line& next, const LineSimplify::Params& params) {
        if (std::fabs(acc.brightness - next.brightness) > params.maxBrightnessDelta)
            return false;

        const auto accDelta = acc.p1 - acc.p0;
        const auto nextDelta = next.p1 - next.p0;
        const auto accLength = Magnitude(accDelta);
        const auto nextLength = Magnitude(nextDelta);
        const bool accIsDot = accLength <= ZeroLengthEpsilon;
        const bool nextIsDot = nextLength <= ZeroLengthEpsilon;

        // Two dots in a row are dot artwork, not an edge; never weld those into a line.
        if (accIsDot && nextIsDot)
            return false;
        if ((accIsDot || nextIsDot) && !params.absorbCollinearDots)
            return false;

        if (accIsDot) {
            // A dot immediately behind a run, on its backward extension, is that run's start.
            const auto dir = Normalized(nextDelta);
            if (Magnitude(next.p0 - acc.p0) > params.maxGap)
                return false;
            if (std::fabs(Cross(dir, acc.p0 - next.p0)) > params.maxPerpOffset)
                return false;
            return Dot(acc.p0 - next.p0, dir) <= 0.f; // must actually be behind it
        }

        const auto accDir = Normalized(accDelta);

        if (nextIsDot) {
            // A dot just past the end of a run, on its forward extension, is that run's end.
            if (Magnitude(next.p0 - acc.p1) > params.maxGap)
                return false;
            if (std::fabs(Cross(accDir, next.p0 - acc.p0)) > params.maxPerpOffset)
                return false;
            return Dot(next.p0 - acc.p0, accDir) >= accLength;
        }

        if (Dot(accDir, Normalized(nextDelta)) < params.minCosAngle)
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

    // One endpoint of one line, addressed as index*2 + which end.
    Vector2 GetEndpoint(const std::vector<Line>& lines, size_t id) {
        const auto& line = lines[id / 2];
        return (id % 2 == 0) ? line.p0 : line.p1;
    }

    void SetEndpoint(std::vector<Line>& lines, size_t id, const Vector2& p) {
        auto& line = lines[id / 2];
        ((id % 2 == 0) ? line.p0 : line.p1) = p;
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
                // When the run began as a dot, that dot is the run's start, so only the far end
                // moves - which is what assigning p1 does in every case.
                acc.p1 = IsDot(next) ? next.p0 : next.p1;
                acc.brightness = std::max(acc.brightness, next.brightness);
            } else {
                lines[++write] = next;
            }
        }

        lines.resize(write + 1);
        return originalSize - lines.size();
    }

    size_t JoinCorners(std::vector<Line>& lines, const Params& params) {
        if (params.cornerJoinRadius <= 0.f || lines.size() < 2)
            return 0;

        const float radius = params.cornerJoinRadius;
        const float minSinAngle = std::sin(params.minCornerAngleDeg * 3.14159265f / 180.f);

        // Endpoints worth considering, bucketed into a uniform grid of cell size `radius` so each
        // one only has to look at its own cell and the eight around it.
        std::unordered_map<uint64_t, std::vector<size_t>> grid;
        const auto cellKey = [radius](const Vector2& p) {
            const auto cx = static_cast<int32_t>(std::floor(p.x / radius));
            const auto cy = static_cast<int32_t>(std::floor(p.y / radius));
            return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) |
                   static_cast<uint32_t>(cy);
        };

        for (size_t i = 0; i < lines.size(); ++i) {
            if (Magnitude(lines[i].p1 - lines[i].p0) < params.minCornerSegmentLength)
                continue;
            grid[cellKey(lines[i].p0)].push_back(i * 2);
            grid[cellKey(lines[i].p1)].push_back(i * 2 + 1);
        }

        std::vector<bool> consumed(lines.size() * 2, false);
        size_t joins = 0;

        for (size_t i = 0; i < lines.size(); ++i) {
            const auto delta = lines[i].p1 - lines[i].p0;
            const auto length = Magnitude(delta);
            if (length < params.minCornerSegmentLength)
                continue;
            const auto dirA = delta / length;

            for (size_t end = 0; end < 2; ++end) {
                const size_t idA = i * 2 + end;
                if (consumed[idA])
                    continue;
                const auto pointA = GetEndpoint(lines, idA);

                size_t bestId = 0;
                float bestDistance = radius;
                Vector2 bestPoint{};
                bool found = false;

                const auto baseX = static_cast<int32_t>(std::floor(pointA.x / radius));
                const auto baseY = static_cast<int32_t>(std::floor(pointA.y / radius));
                for (int32_t gx = baseX - 1; gx <= baseX + 1; ++gx) {
                    for (int32_t gy = baseY - 1; gy <= baseY + 1; ++gy) {
                        const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(gx))
                                              << 32) |
                                             static_cast<uint32_t>(gy);
                        const auto iter = grid.find(key);
                        if (iter == grid.end())
                            continue;

                        for (size_t idB : iter->second) {
                            const size_t j = idB / 2;
                            if (j == i || consumed[idB])
                                continue;

                            const auto pointB = GetEndpoint(lines, idB);
                            const float distance = Magnitude(pointB - pointA);
                            if (distance > bestDistance)
                                continue;

                            const auto deltaB = lines[j].p1 - lines[j].p0;
                            const auto dirB = Normalized(deltaB);
                            const float cross = Cross(dirA, dirB);
                            if (std::fabs(cross) < minSinAngle)
                                continue; // near-parallel: intersection is ill-conditioned

                            // Intersection of the two infinite lines.
                            const float t = Cross(pointB - pointA, dirB) / cross;
                            const Vector2 intersection = pointA + dirA * t;

                            // Refuse to fabricate a long spike: the meeting point has to be close
                            // to where both segments already end.
                            if (Magnitude(intersection - pointA) > radius ||
                                Magnitude(intersection - pointB) > radius)
                                continue;

                            bestDistance = distance;
                            bestId = idB;
                            bestPoint = intersection;
                            found = true;
                        }
                    }
                }

                if (found) {
                    SetEndpoint(lines, idA, bestPoint);
                    SetEndpoint(lines, bestId, bestPoint);
                    consumed[idA] = true;
                    consumed[bestId] = true;
                    ++joins;
                }
            }
        }

        return joins;
    }

    void Simplify(std::vector<Line>& lines, const Params& params) {
        MergeCollinearRuns(lines, params);
        JoinCorners(lines, params);
    }

} // namespace LineSimplify
