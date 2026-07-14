#pragma once
#include "Display.h"
#include <cstddef>
#include <cstdint>

// Immediate-mode RGB565 renderer with automatic dirty-tile presentation.
// The App is free to clear and redraw the complete UI every frame; present()
// compares that backbuffer with the previously displayed frame and only sends
// changed tile runs to the physical display.
class FramebufferDisplay : public Display {
public:
    FramebufferDisplay(int width, int height, int tileWidth = 16, int tileHeight = 10);
    ~FramebufferDisplay() override;

    // Call after PSRAM is initialized. On ESP32 this prefers ps_malloc(); on a
    // desktop build it uses malloc(). Two 320x170 RGB565 buffers use ~213 KiB.
    bool beginFramebuffer(bool preferPsram = true);
    bool framebufferReady() const { return back_ != nullptr && front_ != nullptr; }

    int width() const override { return width_; }
    int height() const override { return height_; }
    void clear(Color c) override;
    void drawPixel(int x, int y, Color c) override;
    void fillRect(int x, int y, int w, int h, Color c) override;
    void present() override;

    void forceFullRefresh() { firstFrame_ = true; }
    uint16_t* framebuffer() { return back_; }
    const uint16_t* framebuffer() const { return back_; }
    int stride() const { return width_; }

    static uint16_t to565(Color c);
    static Color from565(uint16_t p);

protected:
    // `pixels` points at the full framebuffer; `stridePixels` is normally the
    // display width. Implementations should stream the specified rectangle.
    virtual void flushRect(int x, int y, int w, int h,
                           const uint16_t* pixels, int stridePixels) = 0;

    virtual void* allocateBytes(size_t bytes, bool preferPsram);
    virtual void freeBytes(void* ptr);

private:
    bool tileChanged(int x, int y, int w, int h) const;
    void copyRectToFront(int x, int y, int w, int h);

    int width_;
    int height_;
    int tileWidth_;
    int tileHeight_;
    uint16_t* back_ = nullptr;
    uint16_t* front_ = nullptr;
    bool firstFrame_ = true;
};
