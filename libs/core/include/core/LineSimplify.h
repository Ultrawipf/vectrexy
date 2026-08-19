#pragma once

#include "core/Line.h"
#include <cstddef>
#include <vector>

// Post-processing of the per-frame line list produced by the vector generator.
//
// The Vectrex draws some shapes - most visibly the borders on the startup/Mine Storm screens - as
// dashed lines: the beam sweeps in a straight line while the brightness is pulsed on and off, so
// what comes out is a run of short collinear segments with gaps between them. That looks fine on a
// phosphor screen, but a laser projector exaggerates every gap. Since the beam traverses such a run
// in one continuous sweep, its dashes end up as *adjacent* entries in the line list, which is what
// lets a single forward pass stitch them back into one line.
//
// Stitching the straight runs is not enough on its own. Measured on the startup border, each edge
// stops about 1.7 units short of the true corner - the beam decelerates through the turn - so two
// perpendicular runs never actually meet and every corner is left with an L-shaped notch. No gap
// threshold can close that, because the two runs aren't collinear. JoinCorners handles it by
// snapping endpoints that are near each other onto the intersection of the two lines.
namespace LineSimplify {

    struct Params {
        // All distances are in beam space, i.e. the ~256-unit-wide grid that Screen produces.
        // Largest blanked gap that still counts as one line. Sweeping this against a real startup
        // frame, the line count falls away steeply up to about 12 and is flat past it (1973 raw ->
        // 643 at a gap of 8, 246 at 12, 234 at 16), so 12 is where the dashed border actually
        // becomes whole edges. Leaving it lower merges the dashes only partially, which then
        // starves the corner join below of segments long enough to qualify.
        float maxGap = 12.0f;
        float minCosAngle = 0.999f;       // ~2.5 degrees of direction change allowed
        float maxPerpOffset = 0.75f;      // Keeps parallel-but-offset segments apart
        float maxBrightnessDelta = 0.25f; // Don't weld segments of visibly different brightness

        // The beam leaves a zero-length "dot" at each end of a swept edge, where it comes to rest
        // before turning. Those dots read as part of the edge, and leaving them out stops a run
        // short of its own end. When set, a dot that is adjacent to and collinear with a run is
        // absorbed into it. Dots that aren't part of a run - real dot artwork - are untouched.
        bool absorbCollinearDots = true;

        // Corner closing. Zero disables it.
        float cornerJoinRadius = 4.0f;   // How far an endpoint may be moved to close a corner
        float minCornerAngleDeg = 20.0f; // Below this the intersection is ill-conditioned

        // Only segments at least this long are considered for corner joining. This is what keeps
        // the pass away from text and small game geometry, which have plenty of legitimate near
        // misses that should stay as they are - nudging those is what reads as distortion. The
        // gap being fixed is on the screen borders, whose edges run 148 to 218 units, whereas a
        // glyph stroke is under 18 and Mine Storm's asteroid edges are under 17.
        float minCornerSegmentLength = 24.0f;
    };

    // Merges runs of near-collinear lines in place, preserving order. Returns the number of lines
    // removed.
    size_t MergeCollinearRuns(std::vector<Line>& lines, const Params& params);

    // Snaps pairs of nearby endpoints belonging to non-parallel segments onto the intersection of
    // those segments, closing the notch the beam leaves at a corner. Returns the number of corners
    // joined. Order and line count are unchanged; only endpoints move.
    size_t JoinCorners(std::vector<Line>& lines, const Params& params);

    // MergeCollinearRuns followed by JoinCorners. This is what callers normally want.
    void Simplify(std::vector<Line>& lines, const Params& params);

} // namespace LineSimplify
