#ifndef AUDIO_PEAK_MONITOR_H
#define AUDIO_PEAK_MONITOR_H

#include <Arduino.h>
#include <AudioStream.h>
#include <Audio.h>

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

#endif // AUDIO_PEAK_MONITOR_H
