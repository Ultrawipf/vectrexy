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
namespace LineSimplify {

    struct Params {
        // All distances are in beam space, i.e. the ~256-unit-wide grid that Screen produces.
        float maxGap = 3.0f;               // Largest blanked gap that still counts as one line
        float minCosAngle = 0.999f;        // ~2.5 degrees of direction change allowed
        float maxPerpOffset = 0.75f;       // Keeps parallel-but-offset segments apart
        float maxBrightnessDelta = 0.25f;  // Don't weld segments of visibly different brightness
        bool mergeZeroLengthLines = false; // Vectrex dots are meaningful; leave them be
    };

    // Merges runs of near-collinear lines in place, preserving order. Returns the number of lines
    // removed.
    size_t MergeCollinearRuns(std::vector<Line>& lines, const Params& params);

} // namespace LineSimplify
