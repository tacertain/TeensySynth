#ifndef FRAMEBUFFER_GFX_H
#define FRAMEBUFFER_GFX_H

#include <Adafruit_GFX.h>

// RGB565 Color definitions for Adafruit GFX
#define BLACK       0x0000
#define NAVY        0x000F
#define DARKGREEN   0x03E0
#define DARKCYAN    0x03EF
#define MAROON      0x7800
#define PURPLE      0x780F
#define OLIVE       0x7BE0
#define LIGHTGREY   0xC618
#define DARKGREY    0x7BEF
#define BLUE        0x001F
#define GREEN       0x07E0
#define CYAN        0x07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define WHITE       0xFFFF
#define ORANGE      0xFD20
#define GREENYELLOW 0xAFE5
#define PINK        0xF81F

class FramebufferGFX : public Adafruit_GFX {
public:
    FramebufferGFX(uint16_t *framebuffer, int16_t w, int16_t h)
        : Adafruit_GFX(w, h), _framebuffer(framebuffer) {}

    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (x < 0 || x >= _width || y < 0 || y >= _height)
            return;
        _framebuffer[y * _width + x] = color;
        _updated = true;
    }

    uint16_t *getFramebuffer() { return _framebuffer; }

    bool updated() { return _updated; }
    void clearUpdate() { _updated = false;  }

private:
    uint16_t *_framebuffer;
    bool _updated = false;
};

#endif