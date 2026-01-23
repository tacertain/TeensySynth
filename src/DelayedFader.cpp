#include "DelayedFader.h"

void DelayedFader::update(void) {
    audio_block_t *block;
    uint32_t i, pos, inc;
    int32_t sample;
    int32_t dir;  // Must be signed to handle -1 for fade out
    
    // Check if we're in delay period first
    if (delayRemaining > 0) {
        pos = position;
        
        // Handle delay period with appropriate output
        if (pos == 0) {
            // Silent during fadeIn delay
            block = receiveReadOnly();
            if (block) release(block);
        } else if (pos == 0xFFFFFFFF) {
            // Pass through during fadeOut delay
            block = receiveReadOnly();
            if (!block) return;
            transmit(block);
            release(block);
        } else {
            // Shouldn't happen, but handle gracefully
            block = receiveReadOnly();
            if (block) release(block);
        }
        
        delayRemaining -= AUDIO_BLOCK_SAMPLES;
        if (delayRemaining > 0x80000000) delayRemaining = 0; // Handle underflow
        
        // If delay just finished, start the actual fade
        if (delayRemaining == 0) {
            __disable_irq();
            direction = targetDirection;
            if (direction > 0) {
                position = 1;  // Start fade in from 1 (not 0, to avoid special case)
            } else {
                position = 0xFFFFFFFE;  // Start fade out from max-1
            }
            __enable_irq();
        }
        return;
    }
    
    // Normal fade processing (matching AudioEffectFade behavior)
    pos = position;
    
    // Special cases: handle boundaries before processing
    if (pos == 0) {
        // Output is silent
        block = receiveReadOnly();
        if (block) release(block);
        fadeInProgress = false;
        return;
    } else if (pos == 0xFFFFFFFF) {
        // Output is 100%
        block = receiveReadOnly();
        if (!block) return;
        transmit(block);
        release(block);
        fadeInProgress = false;
        return;
    }
    
    block = receiveWritable();
    if (!block) return;
    
    inc = range;
    dir = direction;
    
    // Process each sample with fade (matching AudioEffectFade algorithm)
    for (i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        // Linear fade: use upper 16 bits as gain (0-65535)
        uint32_t gain = pos >> 16;
        sample = block->data[i];
        sample = (sample * (int32_t)gain) >> 16;
        block->data[i] = sample;
        
        if (dir > 0) {
            // Fade in: output is increasing
            if (inc < 0xFFFFFFFF - pos) {
                pos += inc;
            } else {
                pos = 0xFFFFFFFF;
            }
        } else {
            // Fade out: output is decreasing
            if (inc < pos) {
                pos -= inc;
            } else {
                pos = 0;
            }
        }
    }
    
    position = pos;
    transmit(block);
    release(block);
}
