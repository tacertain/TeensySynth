#pragma once

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <ILI9341_t4.h>
#include <SPI.h>
#include "FrameBufferGFX.h"
#include "hardware_config.h"

/**
 * DisplayController
 * 
 * Encapsulates TFT display initialization and update logic.
 * Manages framebuffers, diff buffers, and display updates.
 */
class DisplayController {
public:
    DisplayController();
    
    // Initialization
    bool begin();
    bool isAvailable() const { return available; }
    
    // Update display (call from main loop)
    void update();
    
    // Access to graphics context
    FramebufferGFX* getGraphics() { return &gfx; }
    ILI9341_T4::ILI9341Driver* getTFT() { return &tft; }
    
private:
    // TFT hardware driver
    ILI9341_T4::ILI9341Driver tft;
    
    // Framebuffers
    uint16_t fb[TFT_WIDTH * TFT_HEIGHT];
    uint16_t fb_internal[TFT_WIDTH * TFT_HEIGHT] __attribute__((section(".dmamem")));
    
    // Diff buffers for efficient updates
    ILI9341_T4::DiffBuffStatic<4096> diff1;
    ILI9341_T4::DiffBuffStatic<4096> diff2;
    
    // Graphics context
    FramebufferGFX gfx;
    
    // State
    bool available;
    uint32_t lastUpdateTime;
};
