#include "DisplayController.h"

// Static DMAMEM buffer for TFT internal framebuffer
DMAMEM static uint16_t fb_internal_buffer[TFT_WIDTH * TFT_HEIGHT];

DisplayController::DisplayController()
    : tft(TFT_CS, TFT_DC, TFT_SCK, TFT_MOSI, TFT_MISO, TFT_RST, TFT_TCS, TFT_TIRQ)
    , fb_internal(fb_internal_buffer)
    , gfx(fb, TFT_WIDTH, TFT_HEIGHT)
    , available(false)
    , lastUpdateTime(0) {
}

bool DisplayController::begin() {
#ifdef TFT_DISPLAY
    Serial.println("Initializing TFT display...");
    if (!tft.begin(SPI_SPEED)) {
        Serial.println("TFT initialization failed");
        available = false;
        return false;
    }
    
    tft.setRotation(3);
    tft.setFramebuffer(fb_internal);
    tft.setDiffBuffers(&diff1, &diff2);
    tft.setRefreshRate(60);
    tft.setVSyncSpacing(2);

    gfx.fillScreen(BLACK);
    gfx.setTextColor(WHITE);
    gfx.setCursor(10, 10);
    gfx.print("Hello from GFX!");
    Serial.println("TFT display initialized");
    
    available = true;
    return true;
#else
    Serial.println("TFT disabled by compile flag - skipping initialization");
    available = false;
    return false;
#endif
}

void DisplayController::update() {
    if (!available) return;
    
    if (gfx.updated()) {
        uint32_t now = micros();
        Serial.println("Start frame update");
        tft.update(fb);
        lastUpdateTime = micros();
        Serial.printf("Frame update took %dus\n", lastUpdateTime - now);
        gfx.clearUpdate();
    }
}
