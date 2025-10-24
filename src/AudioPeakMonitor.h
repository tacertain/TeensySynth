#pragma once

// Third-party libraries
#include <Arduino.h>
#include <Audio.h>
#include <AudioStream.h>

class AudioPeakMonitor : public AudioStream {
public:
    AudioPeakMonitor();
    virtual void update(void);
    int16_t getMin() const;
    int16_t getMax() const;
    void reset();
private:
    audio_block_t *inputQueueArray[1];
    int16_t minVal, maxVal;
};
