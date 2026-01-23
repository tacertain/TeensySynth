#pragma once

#include <Arduino.h>
#include <AudioStream.h>

/**
 * DelayedFader
 * 
 * A custom audio fader that adds a delay parameter before starting fade operations.
 * Based on the Audio library's AudioEffectFade, but allows waiting at the starting
 * level for a specified delay time before beginning the fade.
 * 
 * Usage:
 *   delayedFader.fadeIn(500, 100);   // Wait 100ms at 0.0, then fade to 1.0 over 500ms
 *   delayedFader.fadeOut(500, 100);  // Wait 100ms at 1.0, then fade to 0.0 over 500ms
 */
class DelayedFader : public AudioStream {
public:
    DelayedFader() : AudioStream(1, inputQueueArray), position(0xFFFFFFFF) {}
    
    /**
     * Fade in from 0.0 to 1.0
     * @param milliseconds Duration of the fade (after delay)
     * @param delayMs Delay time in milliseconds at starting level before fade begins
     */
    void fadeIn(uint32_t milliseconds, uint32_t delayMs = 0) {
        uint32_t samples = milliseconds * (AUDIO_SAMPLE_RATE_EXACT / 1000.0f);
        uint32_t delaySamples = delayMs * (AUDIO_SAMPLE_RATE_EXACT / 1000.0f);
        __disable_irq();
        delayRemaining = delaySamples;
        delaySamples_total = delaySamples;
        if (delaySamples > 0) {
            // During delay, stay at 0
            position = 0;
            direction = 0;  // No change during delay
        } else {
            // No delay, start fade immediately (start at 1, not 0, to avoid special case)
            position = 1;
            direction = 1;
        }
        range = 0xFFFFFFFF / samples;
        fadeInProgress = true;
        targetDirection = 1;
        __enable_irq();
    }
    
    /**
     * Fade out from 1.0 to 0.0
     * @param milliseconds Duration of the fade (after delay)
     * @param delayMs Delay time in milliseconds at starting level before fade begins
     */
    void fadeOut(uint32_t milliseconds, uint32_t delayMs = 0) {
        uint32_t samples = milliseconds * (AUDIO_SAMPLE_RATE_EXACT / 1000.0f);
        uint32_t delaySamples = delayMs * (AUDIO_SAMPLE_RATE_EXACT / 1000.0f);
        __disable_irq();
        delayRemaining = delaySamples;
        delaySamples_total = delaySamples;
        if (delaySamples > 0) {
            // During delay, stay at max
            position = 0xFFFFFFFF;
            direction = 0;  // No change during delay
        } else {
            // No delay, start fade immediately (start at max-1 to avoid special case)
            position = 0xFFFFFFFE;
            direction = -1;
        }
        range = 0xFFFFFFFF / samples;
        fadeInProgress = true;
        targetDirection = -1;
        __enable_irq();
    }
    
    /**
     * Check if a fade operation is currently in progress (including delay period)
     */
    bool isActive() {
        return fadeInProgress;
    }
    
    virtual void update(void);

private:
    audio_block_t *inputQueueArray[1];
    uint32_t position;      // Current position (0x00000000 = silent, 0xFFFFFFFF = pass-through)
    int32_t direction;      // +1 for fade in, -1 for fade out, 0 during delay
    uint32_t range;         // Amount to change position per sample
    uint32_t delayRemaining; // Samples remaining in delay period
    uint32_t delaySamples_total; // Total delay samples (for tracking)
    bool fadeInProgress;    // True if fade or delay is in progress
    int32_t targetDirection; // Direction to use after delay completes
};
