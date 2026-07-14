#include "FramebufferDisplay.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <esp_heap_caps.h>
#endif

FramebufferDisplay::FramebufferDisplay(int width, int height,
                                       int tileWidth, int tileHeight)
    : width_(width), height_(height),
      tileWidth_(std::max(1, tileWidth)),
      tileHeight_(std::max(1, tileHeight)) {}

FramebufferDisplay::~FramebufferDisplay() {
    freeBytes(back_);
    freeBytes(front_);
}

void* FramebufferDisplay::allocateBytes(size_t bytes, bool preferPsram) {
#if defined(ARDUINO_ARCH_ESP32)
    if (preferPsram && psramFound()) {
        void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p) return p;
    }
    return heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
#else
    (void)preferPsram;
    return std::malloc(bytes);
#endif
}

void FramebufferDisplay::freeBytes(void* ptr) {
    if (!ptr) return;
#if defined(ARDUINO_ARCH_ESP32)
    heap_caps_free(ptr);
#else
    std::free(ptr);
#endif
}

bool FramebufferDisplay::beginFramebuffer(bool preferPsram) {
    if (framebufferReady()) return true;
    const size_t bytes = static_cast<size_t>(width_) * height_ * sizeof(uint16_t);
    back_ = static_cast<uint16_t*>(allocateBytes(bytes, preferPsram));
    front_ = static_cast<uint16_t*>(allocateBytes(bytes, preferPsram));
    if (!back_ || !front_) {
        freeBytes(back_); back_ = nullptr;
        freeBytes(front_); front_ = nullptr;
        return false;
    }
    std::memset(back_, 0, bytes);
    std::memset(front_, 0, bytes);
    firstFrame_ = true;
    return true;
}

uint16_t FramebufferDisplay::to565(Color c) {
    return static_cast<uint16_t>(((c.r & 0xF8) << 8) |
                                 ((c.g & 0xFC) << 3) |
                                 (c.b >> 3));
}

Color FramebufferDisplay::from565(uint16_t p) {
    const uint8_t r5 = static_cast<uint8_t>((p >> 11) & 0x1F);
    const uint8_t g6 = static_cast<uint8_t>((p >> 5) & 0x3F);
    const uint8_t b5 = static_cast<uint8_t>(p & 0x1F);
    return Color{
        static_cast<uint8_t>((r5 << 3) | (r5 >> 2)),
        static_cast<uint8_t>((g6 << 2) | (g6 >> 4)),
        static_cast<uint8_t>((b5 << 3) | (b5 >> 2))
    };
}

void FramebufferDisplay::clear(Color c) {
    if (!back_) return;
    std::fill_n(back_, static_cast<size_t>(width_) * height_, to565(c));
}

void FramebufferDisplay::drawPixel(int x, int y, Color c) {
    if (!back_ || x < 0 || y < 0 || x >= width_ || y >= height_) return;
    back_[static_cast<size_t>(y) * width_ + x] = to565(c);
}

void FramebufferDisplay::fillRect(int x, int y, int w, int h, Color c) {
    if (!back_ || w <= 0 || h <= 0) return;
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(width_, x + w);
    const int y1 = std::min(height_, y + h);
    if (x0 >= x1 || y0 >= y1) return;

    const uint16_t p = to565(c);
    for (int row = y0; row < y1; ++row) {
        uint16_t* start = back_ + static_cast<size_t>(row) * width_ + x0;
        std::fill(start, start + (x1 - x0), p);
    }
}

bool FramebufferDisplay::tileChanged(int x, int y, int w, int h) const {
    for (int row = 0; row < h; ++row) {
        const size_t offset = static_cast<size_t>(y + row) * width_ + x;
        if (std::memcmp(back_ + offset, front_ + offset,
                        static_cast<size_t>(w) * sizeof(uint16_t)) != 0)
            return true;
    }
    return false;
}

void FramebufferDisplay::copyRectToFront(int x, int y, int w, int h) {
    for (int row = 0; row < h; ++row) {
        const size_t offset = static_cast<size_t>(y + row) * width_ + x;
        std::memcpy(front_ + offset, back_ + offset,
                    static_cast<size_t>(w) * sizeof(uint16_t));
    }
}

void FramebufferDisplay::present() {
    if (!framebufferReady()) return;

    if (firstFrame_) {
        flushRect(0, 0, width_, height_, back_, width_);
        std::memcpy(front_, back_,
                    static_cast<size_t>(width_) * height_ * sizeof(uint16_t));
        firstFrame_ = false;
        return;
    }

    const int tilesX = (width_ + tileWidth_ - 1) / tileWidth_;
    const int tilesY = (height_ + tileHeight_ - 1) / tileHeight_;
    const int tileCount = tilesX * tilesY;
    int changedCount = 0;

    for (int ty = 0; ty < tilesY; ++ty) {
        const int y = ty * tileHeight_;
        const int h = std::min(tileHeight_, height_ - y);
        for (int tx = 0; tx < tilesX; ++tx) {
            const int x = tx * tileWidth_;
            const int w = std::min(tileWidth_, width_ - x);
            if (tileChanged(x, y, w, h)) ++changedCount;
        }
    }

    if (changedCount == 0) return;

    // Large transitions are more efficient as one transaction. Small dynamic
    // changes (RA/Dec, RMS, battery, joystick direction) use tile runs.
    if (changedCount * 3 > tileCount) {
        flushRect(0, 0, width_, height_, back_, width_);
        std::memcpy(front_, back_,
                    static_cast<size_t>(width_) * height_ * sizeof(uint16_t));
        return;
    }

    for (int ty = 0; ty < tilesY; ++ty) {
        const int y = ty * tileHeight_;
        const int h = std::min(tileHeight_, height_ - y);
        int tx = 0;
        while (tx < tilesX) {
            int x = tx * tileWidth_;
            int w = std::min(tileWidth_, width_ - x);
            if (!tileChanged(x, y, w, h)) { ++tx; continue; }

            const int startTile = tx;
            ++tx;
            while (tx < tilesX) {
                const int nextX = tx * tileWidth_;
                const int nextW = std::min(tileWidth_, width_ - nextX);
                if (!tileChanged(nextX, y, nextW, h)) break;
                ++tx;
            }

            const int rx = startTile * tileWidth_;
            const int rw = std::min(width_, tx * tileWidth_) - rx;
            flushRect(rx, y, rw, h, back_, width_);
            copyRectToFront(rx, y, rw, h);
        }
    }
}
