#include "Presentation.hpp"

#include "BitmapFont.hpp"

#include <SDL2/SDL.h>

#include <algorithm>

namespace anyswitch {

namespace {
constexpr std::size_t kMaxLines = 20;
constexpr std::size_t kLineHeight = kFontHeight + 6;
constexpr std::size_t kLeftMargin = 12;
constexpr std::size_t kTopMargin = 10;
constexpr std::size_t kGlyphSpacing = 1;

void PutPixel(std::vector<std::uint8_t>& surface, std::size_t width,
              std::size_t height, int x, int y, std::uint8_t r, std::uint8_t g,
              std::uint8_t b, std::uint8_t a) {
    if (x < 0 || y < 0 || static_cast<std::size_t>(x) >= width ||
        static_cast<std::size_t>(y) >= height)
        return;
    const auto idx = (static_cast<std::size_t>(y) * width + x) * 4;
    surface[idx + 0] = r;
    surface[idx + 1] = g;
    surface[idx + 2] = b;
    surface[idx + 3] = a;
}

} // namespace

Presentation& Presentation::Get() {
    static Presentation instance;
    return instance;
}

bool Presentation::Init(const Config& config) {
    if (_initialised) {
        Reset();
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return false;

    _config = config;
    _surface.assign(static_cast<std::size_t>(config.width) * config.height * 4, 0);
    _text = _surface; // the log starts blank

    _window = SDL_CreateWindow("AnySwitch", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, config.width, config.height,
                               SDL_WINDOW_SHOWN);
    _renderer = SDL_CreateRenderer(static_cast<SDL_Window*>(_window), -1,
                                   SDL_RENDERER_ACCELERATED);
    _texture = SDL_CreateTexture(static_cast<SDL_Renderer*>(_renderer),
                                 SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                                 config.width, config.height);
    _initialised = _window && _renderer && _texture;
    return _initialised;
}

void Presentation::Reset() {
    if (_texture)
        SDL_DestroyTexture(static_cast<SDL_Texture*>(_texture));
    if (_renderer)
        SDL_DestroyRenderer(static_cast<SDL_Renderer*>(_renderer));
    if (_window)
        SDL_DestroyWindow(static_cast<SDL_Window*>(_window));
    _texture = nullptr;
    _renderer = nullptr;
    _window = nullptr;
    _lines.clear();
    _surface.clear();
    _text.clear();
    SDL_Quit();
    _initialised = false;
}

void Presentation::Shutdown() {
    if (_initialised)
        Reset();
}

Presentation::~Presentation() { Shutdown(); }

void Presentation::AppendGuestText(const std::string& text) {
    if (!text.empty())
        _lines.push_back(text);
    while (_lines.size() > kMaxLines)
        _lines.erase(_lines.begin());
}

void Presentation::PresentFrame(const RgbaFrame& frame) {
    if (!_initialised)
        return;
    const auto expected = static_cast<std::size_t>(_config.width) * _config.height * 4;
    if (frame.data && frame.width == _config.width && frame.height == _config.height)
        std::copy(frame.data, frame.data + expected, _surface.begin());
    Render();
}

bool Presentation::PollEvents() {
    if (!_initialised)
        return true;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT)
            return false;
    }
    Render();
    return true;
}

void Presentation::Render() {
    if (!_initialised)
        return;
    auto* texture = static_cast<SDL_Texture*>(_texture);
    auto* renderer = static_cast<SDL_Renderer*>(_renderer);

    // Composite: the framebuffer first, then the guest's debug text over it.
    std::copy(_surface.begin(), _surface.end(), _text.begin());
    std::size_t y = kTopMargin;
    for (const auto& line : _lines) {
        std::size_t x = kLeftMargin;
        for (const char c : line) {
            const auto* glyph = GlyphFor(c);
            if (glyph != nullptr) {
                for (std::size_t row = 0; row < kFontHeight; ++row) {
                    const std::uint8_t bits = glyph[2 + row];
                    for (std::size_t col = 0; col < kFontWidth; ++col) {
                        if (bits & (1u << (kFontWidth - 1 - col))) {
                            PutPixel(_text, _config.width, _config.height,
                                     static_cast<int>(x + col), static_cast<int>(y + row),
                                     0, 255, 0, 255);
                        }
                    }
                }
            }
            x += kFontWidth + kGlyphSpacing;
        }
        y += kLineHeight;
    }

    SDL_UpdateTexture(texture, nullptr, _text.data(),
                      static_cast<int>(_config.width) * 4);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

} // namespace anyswitch
