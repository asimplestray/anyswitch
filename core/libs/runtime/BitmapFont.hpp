#pragma once

// A minimal built-in 5x7 font.
//
// Embedded rather than loading a system font because the presentation layer
// should build anywhere: no SDL_ttf, no font files, no distro differences.
// Covers what guest debug strings are made of: A-Z, 0-9, space and common
// punctuation. Each glyph is 5 columns of 7 bits, MSB at the top.
//
// To add a glyph: extend the table and the switch below.

#include <cstdint>

namespace anyswitch {

constexpr std::uint8_t kFontWidth = 5;
constexpr std::uint8_t kFontHeight = 7;

// Returns the 7 bitplanes for a character, or nullptr when unmapped.
const std::uint8_t* GlyphFor(char c);

} // namespace anyswitch
