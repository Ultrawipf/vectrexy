#pragma once

#include <cstdint>

// A single-stroke ("engraving") vector font.
//
// The Vectrex BIOS font is a bitmap: 8 raster rows per character, drawn by sweeping the beam across
// each row while the shift register blanks and unblanks it. That's what gives BIOS text its
// characteristic scan-line look, and it's unusable on a laser projector. This font describes the
// same characters as polylines instead, so each glyph is a handful of long strokes.
//
// The charset deliberately matches the BIOS's own table exactly - ASCII $20 to $5B, uppercase only
// - so substituting it can't lose characters that the BIOS was able to draw.
namespace VectorFont {

    // Glyphs are authored on an integer grid, x in [0, CellW], y in [0, CellH], with y pointing UP
    // and the origin at the bottom-left of the character cell.
    constexpr int8_t CellW = 4;
    constexpr int8_t CellH = 6;

    constexpr uint8_t FirstChar = 0x20; // ' '
    constexpr uint8_t LastChar = 0x5B;  // '['

    // No glyph in this charset needs more than this many separate strokes.
    constexpr uint8_t MaxPolylinesPerGlyph = 4;

    struct Polyline {
        const int8_t* xy = nullptr; // Interleaved x0,y0,x1,y1,... on the cell grid
        uint8_t numPoints = 0;
    };

    // Writes the strokes making up `c` into `out` and returns how many were written. Returns 0 for
    // space and for any character outside the charset, which therefore renders as blank.
    uint8_t GetGlyph(char c, Polyline* out, uint8_t maxOut);

} // namespace VectorFont
