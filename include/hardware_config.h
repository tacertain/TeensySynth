#pragma once

/**
 * Hardware Pin Configuration for TeensySynth
 * 
 * This file centralizes all hardware pin definitions for the Teensy 4.1 board.
 * Modify these values to adapt to different hardware configurations.
 */

// ============================================================================
// TFT Display Pins (ILI9341)
// ============================================================================
#define TFT_CS      10      // Chip Select
#define TFT_DC      9       // Data/Command
#define TFT_MOSI    11      // Master Out Slave In (SDI)
#define TFT_MISO    12      // Master In Slave Out (SDO)
#define TFT_SCK     13      // Serial Clock
#define TFT_RST     255     // Reset (255 = not used)
#define TFT_TCS     8       // Touch Chip Select
#define TFT_TIRQ    6       // Touch Interrupt

// TFT Display Configuration
#define TFT_WIDTH   320
#define TFT_HEIGHT  240
#define SPI_SPEED   40000000  // 40 MHz SPI speed

// ============================================================================
// Audio Interface Pins (I2S)
// ============================================================================
// Note: These are fixed by Teensy hardware and cannot be changed
// WSEL/LRCLK: 20
// BCLK:       21
// DIN:        7

// ============================================================================
// Feature Flags
// ============================================================================
// Uncomment to enable TFT display - will hang if enabled but not attached
// #define TFT_DISPLAY
