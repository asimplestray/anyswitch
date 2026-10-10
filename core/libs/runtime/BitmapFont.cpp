#include "BitmapFont.hpp"

namespace anyswitch {

namespace {

struct Glyph { char c; std::uint64_t bits; };

// 7 rows of 5 bits each, packed top row in the high bits.

const Glyph kGlyphs[] = {
    {' ', 0x0000000u},
    {'!', 0x108421004u},
    {'#', 0x2BEA00000u},
    {'(', 0x88842082u},
    {')', 0x208210888u},
    {'*', 0x2AEFBAA0u},
    {'+', 0x84F9080u},
    {'-', 0x00F8000u},
    {'.', 0x000018Cu},
    {'/', 0x42222210u},
    {'0', 0x3A33AE62Eu},
    {'1', 0x11842109Fu},
    {'2', 0x3A211111Fu},
    {'3', 0x7C441062Eu},
    {'4', 0x8CA97C42u},
    {'5', 0x7E1E0862Eu},
    {'6', 0x1910F462Eu},
    {'7', 0x7C2222108u},
    {'8', 0x3A317462Eu},
    {'9', 0x3A317844Cu},
    {':', 0x18C03180u},
    {'=', 0x1F07C00u},
    {'?', 0x3A2111004u},
    {'A', 0x3A31FC631u},
    {'B', 0x7A31F463Eu},
    {'C', 0x3A308422Eu},
    {'D', 0x7A318C63Eu},
    {'E', 0x7E10F421Fu},
    {'F', 0x7E10F4210u},
    {'G', 0x3A30BC62Eu},
    {'H', 0x4631FC631u},
    {'I', 0x7C842109Fu},
    {'J', 0x1C4210A4Cu},
    {'K', 0x4654C5251u},
    {'L', 0x42108421Fu},
    {'M', 0x47758C631u},
    {'N', 0x47359C631u},
    {'O', 0x3A318C62Eu},
    {'P', 0x7A31F4210u},
    {'Q', 0x3A318D64Du},
    {'R', 0x7A31F5251u},
    {'S', 0x3E107043Eu},
    {'T', 0x7C8421084u},
    {'U', 0x46318C62Eu},
    {'V', 0x46318C544u},
    {'W', 0x46318D771u},
    {'X', 0x462A22A31u},
    {'Y', 0x462A21084u},
    {'Z', 0x7C222221Fu},
    {'_', 0x000001Fu},
};

} // namespace

const std::uint8_t* GlyphFor(char c) {
    for (const auto& g : kGlyphs) {
        if (g.c == c) {
            static thread_local std::uint8_t rows[8];
            rows[0] = kFontWidth;
            rows[1] = kFontHeight;
            for (int i = 0; i < 7; ++i)
                rows[2 + i] = static_cast<std::uint8_t>((g.bits >> (5 * (6 - i))) & 0x1F);
            return rows;
        }
    }
    return nullptr;
}

} // namespace anyswitch

