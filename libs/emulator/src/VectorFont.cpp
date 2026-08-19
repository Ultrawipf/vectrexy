#include "emulator/VectorFont.h"

namespace {
    using VectorFont::Polyline;

    // A PenUp,PenUp pair separates one stroke from the next within a glyph.
    constexpr int8_t PenUp = -1;

    // Glyphs are authored left-to-right where possible: starting near the left edge and ending near
    // the right edge keeps the blanked jump to the next character short, which matters to a galvo.

    const int8_t gSpace[] = {0, 0}; // Never drawn; kept so the table entry isn't null-special
    const int8_t gExclam[] = {2, 6, 2, 2, PenUp, PenUp, 2, 0, 2, 1};
    const int8_t gQuote[] = {1, 6, 1, 4, PenUp, PenUp, 3, 6, 3, 4};
    const int8_t gHash[] = {1, 6, 1, 0, PenUp, PenUp, 3, 6, 3, 0, PenUp, PenUp,
                            0, 4, 4, 4, PenUp, PenUp, 0, 2, 4, 2};
    const int8_t gDollar[] = {4, 5, 3, 6, 1, 6, 0, 5, 0, 4, 1, 3, 3,     3,     4, 2,
                              4, 1, 3, 0, 1, 0, 0, 1, PenUp, PenUp, 2, 6, 2, 0};
    const int8_t gPercent[] = {0, 0, 4, 6, PenUp, PenUp, 0, 5, 1, 5, 1, 6, 0, 6, 0, 5,
                               PenUp, PenUp, 3, 0, 4, 0, 4, 1, 3, 1, 3, 0};
    const int8_t gAmp[] = {4, 0, 1, 4, 1, 5, 2, 6, 3, 5, 3, 4, 0, 2, 0, 1, 1, 0, 2, 0, 4, 2};
    const int8_t gApos[] = {2, 6, 2, 4};
    const int8_t gLParen[] = {3, 6, 1, 4, 1, 2, 3, 0};
    const int8_t gRParen[] = {1, 6, 3, 4, 3, 2, 1, 0};
    const int8_t gStar[] = {2, 5, 2, 1, PenUp, PenUp, 0, 4, 4, 2, PenUp, PenUp, 0, 2, 4, 4};
    const int8_t gPlus[] = {2, 5, 2, 1, PenUp, PenUp, 0, 3, 4, 3};
    const int8_t gComma[] = {2, 1, 1, 0};
    const int8_t gMinus[] = {0, 3, 4, 3};
    const int8_t gPeriod[] = {1, 0, 2, 0};
    const int8_t gSlash[] = {0, 0, 4, 6};

    const int8_t g0[] = {1, 0, 3, 0, 4, 1, 4, 5, 3, 6, 1, 6, 0, 5, 0, 1, 1, 0, PenUp, PenUp, 1, 1, 3, 5};
    const int8_t g1[] = {1, 5, 2, 6, 2, 0, PenUp, PenUp, 1, 0, 3, 0};
    const int8_t g2[] = {0, 5, 1, 6, 3, 6, 4, 5, 4, 4, 0, 0, 4, 0};
    const int8_t g3[] = {0, 6, 4, 6, 2, 3, 3, 3, 4, 2, 4, 1, 3, 0, 1, 0, 0, 1};
    const int8_t g4[] = {3, 0, 3, 6, 0, 2, 4, 2};
    const int8_t g5[] = {4, 6, 0, 6, 0, 4, 3, 4, 4, 3, 4, 1, 3, 0, 1, 0, 0, 1};
    const int8_t g6[] = {4, 5, 3, 6, 1, 6, 0, 5, 0, 1, 1, 0, 3, 0, 4, 1, 4, 2, 3, 3, 1, 3, 0, 2};
    const int8_t g7[] = {0, 6, 4, 6, 1, 0};
    const int8_t g8[] = {1, 3, 0, 4, 0, 5, 1, 6, 3, 6, 4, 5, 4, 4, 3, 3,
                         1, 3, 0, 2, 0, 1, 1, 0, 3, 0, 4, 1, 4, 2, 3, 3};
    const int8_t g9[] = {0, 1, 1, 0, 3, 0, 4, 1, 4, 5, 3, 6, 1, 6, 0, 5, 0, 4, 1, 3, 3, 3, 4, 4};

    const int8_t gColon[] = {2, 5, 2, 4, PenUp, PenUp, 2, 2, 2, 1};
    const int8_t gSemi[] = {2, 5, 2, 4, PenUp, PenUp, 2, 2, 1, 0};
    const int8_t gLess[] = {4, 5, 0, 3, 4, 1};
    const int8_t gEqual[] = {0, 4, 4, 4, PenUp, PenUp, 0, 2, 4, 2};
    const int8_t gGreater[] = {0, 5, 4, 3, 0, 1};
    const int8_t gQuestion[] = {0, 5, 1, 6, 3, 6, 4, 5, 4, 4, 2, 3, 2, 2, PenUp, PenUp, 2, 0, 2, 1};
    const int8_t gAt[] = {4, 1, 3, 0, 1, 0, 0, 1, 0, 5, 1, 6, 3, 6,
                          4, 5, 4, 3, 2, 3, 2, 2, 3, 2, 3, 4};

    const int8_t gA[] = {0, 0, 0, 4, 2, 6, 4, 4, 4, 0, PenUp, PenUp, 0, 2, 4, 2};
    const int8_t gB[] = {0, 0, 0, 6, 3, 6, 4, 5, 4, 4, 3, 3, 0, 3, PenUp, PenUp,
                         3, 3, 4, 2, 4, 1, 3, 0, 0, 0};
    const int8_t gC[] = {4, 5, 3, 6, 1, 6, 0, 5, 0, 1, 1, 0, 3, 0, 4, 1};
    const int8_t gD[] = {0, 0, 0, 6, 3, 6, 4, 5, 4, 1, 3, 0, 0, 0};
    const int8_t gE[] = {4, 6, 0, 6, 0, 0, 4, 0, PenUp, PenUp, 0, 3, 3, 3};
    const int8_t gF[] = {0, 0, 0, 6, 4, 6, PenUp, PenUp, 0, 3, 3, 3};
    const int8_t gG[] = {4, 5, 3, 6, 1, 6, 0, 5, 0, 1, 1, 0, 3, 0, 4, 1, 4, 3, 2, 3};
    const int8_t gH[] = {0, 6, 0, 0, PenUp, PenUp, 0, 3, 4, 3, PenUp, PenUp, 4, 6, 4, 0};
    const int8_t gI[] = {1, 6, 3, 6, PenUp, PenUp, 2, 6, 2, 0, PenUp, PenUp, 1, 0, 3, 0};
    const int8_t gJ[] = {3, 6, 3, 1, 2, 0, 1, 0, 0, 1};
    const int8_t gK[] = {0, 6, 0, 0, PenUp, PenUp, 4, 6, 0, 2, 4, 0};
    const int8_t gL[] = {0, 6, 0, 0, 4, 0};
    const int8_t gM[] = {0, 0, 0, 6, 2, 3, 4, 6, 4, 0};
    const int8_t gN[] = {0, 0, 0, 6, 4, 0, 4, 6};
    const int8_t gO[] = {1, 0, 3, 0, 4, 1, 4, 5, 3, 6, 1, 6, 0, 5, 0, 1, 1, 0};
    const int8_t gP[] = {0, 0, 0, 6, 3, 6, 4, 5, 4, 4, 3, 3, 0, 3};
    const int8_t gQ[] = {1, 0, 3, 0, 4, 1, 4, 5, 3, 6, 1, 6, 0, 5, 0, 1, 1, 0,
                         PenUp, PenUp, 2, 2, 4, 0};
    const int8_t gR[] = {0, 0, 0, 6, 3, 6, 4, 5, 4, 4, 3, 3, 0, 3, PenUp, PenUp, 2, 3, 4, 0};
    const int8_t gS[] = {0, 1, 1, 0, 3, 0, 4, 1, 4, 2, 3, 3, 1, 3, 0, 4, 0, 5, 1, 6, 3, 6, 4, 5};
    const int8_t gT[] = {0, 6, 4, 6, PenUp, PenUp, 2, 6, 2, 0};
    const int8_t gU[] = {0, 6, 0, 1, 1, 0, 3, 0, 4, 1, 4, 6};
    const int8_t gV[] = {0, 6, 2, 0, 4, 6};
    const int8_t gW[] = {0, 6, 1, 0, 2, 3, 3, 0, 4, 6};
    const int8_t gX[] = {0, 6, 4, 0, PenUp, PenUp, 0, 0, 4, 6};
    const int8_t gY[] = {0, 6, 2, 3, 4, 6, PenUp, PenUp, 2, 3, 2, 0};
    const int8_t gZ[] = {0, 6, 4, 6, 0, 0, 4, 0};
    const int8_t gLBracket[] = {3, 6, 1, 6, 1, 0, 3, 0};

    struct GlyphEntry {
        const int8_t* data;
        uint8_t numValues; // Number of int8_t entries, i.e. twice the number of points
    };

#define GLYPH(a) GlyphEntry { a, static_cast<uint8_t>(sizeof(a)) }

    // Indexed by (character - VectorFont::FirstChar). Order matters - keep in sync with ASCII.
    const GlyphEntry Glyphs[VectorFont::LastChar - VectorFont::FirstChar + 1] = {
        GlyphEntry{nullptr, 0}, // 0x20 space
        GLYPH(gExclam),         // 0x21 !
        GLYPH(gQuote),          // 0x22 "
        GLYPH(gHash),           // 0x23 #
        GLYPH(gDollar),         // 0x24 $
        GLYPH(gPercent),        // 0x25 %
        GLYPH(gAmp),            // 0x26 &
        GLYPH(gApos),           // 0x27 '
        GLYPH(gLParen),         // 0x28 (
        GLYPH(gRParen),         // 0x29 )
        GLYPH(gStar),           // 0x2A *
        GLYPH(gPlus),           // 0x2B +
        GLYPH(gComma),          // 0x2C ,
        GLYPH(gMinus),          // 0x2D -
        GLYPH(gPeriod),         // 0x2E .
        GLYPH(gSlash),          // 0x2F /
        GLYPH(g0),              // 0x30 0
        GLYPH(g1),              // 0x31 1
        GLYPH(g2),              // 0x32 2
        GLYPH(g3),              // 0x33 3
        GLYPH(g4),              // 0x34 4
        GLYPH(g5),              // 0x35 5
        GLYPH(g6),              // 0x36 6
        GLYPH(g7),              // 0x37 7
        GLYPH(g8),              // 0x38 8
        GLYPH(g9),              // 0x39 9
        GLYPH(gColon),          // 0x3A :
        GLYPH(gSemi),           // 0x3B ;
        GLYPH(gLess),           // 0x3C <
        GLYPH(gEqual),          // 0x3D =
        GLYPH(gGreater),        // 0x3E >
        GLYPH(gQuestion),       // 0x3F ?
        GLYPH(gAt),             // 0x40 @
        GLYPH(gA),              // 0x41 A
        GLYPH(gB),              // 0x42 B
        GLYPH(gC),              // 0x43 C
        GLYPH(gD),              // 0x44 D
        GLYPH(gE),              // 0x45 E
        GLYPH(gF),              // 0x46 F
        GLYPH(gG),              // 0x47 G
        GLYPH(gH),              // 0x48 H
        GLYPH(gI),              // 0x49 I
        GLYPH(gJ),              // 0x4A J
        GLYPH(gK),              // 0x4B K
        GLYPH(gL),              // 0x4C L
        GLYPH(gM),              // 0x4D M
        GLYPH(gN),              // 0x4E N
        GLYPH(gO),              // 0x4F O
        GLYPH(gP),              // 0x50 P
        GLYPH(gQ),              // 0x51 Q
        GLYPH(gR),              // 0x52 R
        GLYPH(gS),              // 0x53 S
        GLYPH(gT),              // 0x54 T
        GLYPH(gU),              // 0x55 U
        GLYPH(gV),              // 0x56 V
        GLYPH(gW),              // 0x57 W
        GLYPH(gX),              // 0x58 X
        GLYPH(gY),              // 0x59 Y
        GLYPH(gZ),              // 0x5A Z
        GLYPH(gLBracket),       // 0x5B [
    };

#undef GLYPH

    static_assert(sizeof(Glyphs) / sizeof(Glyphs[0]) == 60,
                  "Glyph table must cover exactly the BIOS charset $20-$5B");

    // gSpace exists only to keep the array declarations uniform; the table uses a null entry.
    static_assert(sizeof(gSpace) == 2, "");
} // namespace

namespace VectorFont {

    uint8_t GetGlyph(char c, Polyline* out, uint8_t maxOut) {
        const auto uc = static_cast<uint8_t>(c);
        if (uc < FirstChar || uc > LastChar)
            return 0;

        const auto& entry = Glyphs[uc - FirstChar];
        if (entry.data == nullptr)
            return 0;

        uint8_t numPolylines = 0;
        uint8_t i = 0;
        while (i < entry.numValues && numPolylines < maxOut) {
            // Skip any pen-up separators
            while (i < entry.numValues && entry.data[i] == PenUp)
                i += 2;
            if (i >= entry.numValues)
                break;

            const int8_t* start = entry.data + i;
            uint8_t numPoints = 0;
            while (i < entry.numValues && entry.data[i] != PenUp) {
                i += 2;
                ++numPoints;
            }
            out[numPolylines++] = Polyline{start, numPoints};
        }
        return numPolylines;
    }

} // namespace VectorFont
