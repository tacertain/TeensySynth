#ifndef _karplus_strong_string_synth_h_
#define _karplus_strong_string_synth_h_
#include <Arduino.h>     
#include <AudioStream.h>
#include <utility/dspinst.h>

#define MAX_NUM_SAMPLES 1024

class KarplusStrongStringSynth : public AudioStream
{
public:
	KarplusStrongStringSynth() : AudioStream(0, NULL) {
		state = 0;
	}
	int noteOn(float freq, float velocity) {
		if (velocity <= 0.0f || velocity > 1.0f) {
			noteOff();
			return -1;
		}
		frequency =	freq;
		initialAmplitude = velocity * 65535.0f;
		size_t len = (AUDIO_SAMPLE_RATE_EXACT / frequency) + 0.5f;

		bufferLen = len > MAX_NUM_SAMPLES ? MAX_NUM_SAMPLES : len;
		bufferIndex = 0;
		state = 1;

		calculateDelayIncrement();

		return 0;
	}
	void noteOff() {
		state = 0;
	}

	void setAttenuation(uint8_t newAttenuation) {
		attenuation = newAttenuation + 1;
		outputTotalAttenuation();
	}

	void setFilterStrength(uint16_t newStrength) {
		filterStrength = newStrength;
		outputTotalAttenuation();
	}

	void outputTotalAttenuation() {
		Serial.print("Total attenuation: ");
		Serial.print(attenuation);
		Serial.print(" * ");
		Serial.print(filterStrength);
		Serial.print(" = ");
		Serial.print(attenuation * filterStrength);
		Serial.println();
	}
    virtual void update(void);
    void fillIfNecessary(uint16_t attenuationScaled, uint16_t filterScaled);
    void updateFrequency(float newFreq) {
        frequency = newFreq;
        
        calculateDelayIncrement();
    }

private:
	uint8_t  state;     // 0=off, 1=begin on next update, 2=playing
	uint16_t bufferLen;
	uint16_t bufferIndex;
	uint16_t whichBuffer;
	int32_t  initialAmplitude;
	uint16_t attenuation = 103; // Reasonable starting point
	uint16_t filterStrength = 0; // Equal mix of old and new
	static uint32_t seed;  // must start at 1
	int16_t buffers[2][MAX_NUM_SAMPLES];
	int32_t bufferGeneration[2];
	uint32_t loops;

	float frequency;                 // Target frequency in Hz
	uint32_t bufferPosition;         // 16.16 fixed-point position in buffer
	uint32_t delayIncrementFixed;    // 16.16 fixed-point increment per sample

	void fillBuffer(uint16_t fromBuffer, uint16_t attenuation, uint16_t filter);
	void calculateDelayIncrement() {
		float exactDelay = AUDIO_SAMPLE_RATE_EXACT / frequency;
		float delayIncrement = (float)bufferLen / exactDelay;
		
		// Convert to 16.16 fixed-point
		delayIncrementFixed = (uint32_t)(delayIncrement * 65536.0f);
	}
};

#endif
