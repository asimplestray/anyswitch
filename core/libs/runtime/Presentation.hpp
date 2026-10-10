#pragma once

// How translated guest code becomes visible on the host.
//
// The console's kernel composites framebuffers; here the guest has a linear
// framebuffer in its own memory and something has to put it on screen. This is
// that half. It owns an SDL2 window, a host-side RGBA surface, and a text log
// fed by svcOutputDebugString so guest output is visible even before the
// graphical path exists.
//
// Nothing here emulates anything: it copies pixels and blits glyphs, which is
// what a compositor does.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace anyswitch {

struct RgbaFrame {
    const std::uint8_t* data;
    std::size_t width;
    std::size_t height;
};

class Presentation {
public:
    struct Config {
        std::uint32_t width = 1280;
        std::uint32_t height = 720;
        // The Switch's framebuffer is BGRA and 32bpp; the framebuffer module
        // converts, so this stays plain RGBA.
        std::uint32_t scale = 1;
    };

    static Presentation& Get();

    bool Init(const Config& config);
    void Shutdown();

    // Receives host-side pixels for the whole window.
    void PresentFrame(const RgbaFrame& frame);

    // A guest wrote a string; push it onto the on-screen log.
    void AppendGuestText(const std::string& text);

    // Returns false when the window has been closed.
    bool PollEvents();

    bool IsInitialised() const { return _initialised; }

private:
    Presentation() = default;
    ~Presentation();

    Presentation(const Presentation&) = delete;
    Presentation& operator=(const Presentation&) = delete;

    void Reset();
    void Render();

    bool _initialised = false;
    Config _config{};
    std::vector<std::uint8_t> _surface;
    std::vector<std::uint8_t> _text;
    void* _window = nullptr;
    void* _renderer = nullptr;
    void* _texture = nullptr;
    void* _font = nullptr;
    std::vector<std::string> _lines;
};

} // namespace anyswitch
