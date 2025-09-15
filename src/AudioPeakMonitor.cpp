
#include <Arduino.h>
#include "AudioPeakMonitor.h"

AudioPeakMonitor::AudioPeakMonitor()
    : AudioStream(1, inputQueueArray), minVal(32767), maxVal(-32768) {}

void AudioPeakMonitor::update(void) {
    audio_block_t *block = receiveReadOnly(0);
    if (!block) return;
    int16_t localMin = 32767, localMax = -32768;
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
        if (block->data[i] < localMin) localMin = block->data[i];
        if (block->data[i] > localMax) localMax = block->data[i];
    }
    minVal = min(minVal, localMin);
    maxVal = max(maxVal, localMax);
    // Optionally: print or store values here
    release(block);
}

int16_t AudioPeakMonitor::getMin() const { return minVal; }
int16_t AudioPeakMonitor::getMax() const { return maxVal; }

void AudioPeakMonitor::reset() {
    minVal = 32767;
    maxVal = -32768;
}
