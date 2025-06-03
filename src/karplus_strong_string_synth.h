

#ifndef _karplus_strong_string_synth_h_
#define _karplus_strong_string_synth_h_
#include <Arduino.h>     
#include <AudioStream.h>
#include <utility/dspinst.h>

class KarplusStrongStringSynth : public AudioStream
{
public:
	KarplusStrongStringSynth() : AudioStream(0, NULL) {
		state = 0;
	}
	int noteOn(float frequency, float velocity) {
		if (velocity <= 0.0f || velocity > 1.0f) {
			noteOff();
			return -1;
		}
		initialAmplitude = velocity * 65535.0f;
		size_t len = (AUDIO_SAMPLE_RATE_EXACT / frequency) + 0.5f;
		// Frequency is too low for our buffer size
		if (len > sizeof(buffer)/sizeof(buffer[0])) {
			noteOff();
			return -1;
		}
		bufferLen = len;
		bufferIndex = 0;
		state = 1;

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
private:
	uint8_t  state;     // 0=off, 1=begin on next update, 2=playing
	uint16_t bufferLen;
	uint16_t bufferIndex;
	int32_t  initialAmplitude;
	uint16_t attenuation = 506; // Reasonable starting point
	uint16_t filterStrength = 64; // Equal mix of old and new
	static uint32_t seed;  // must start at 1
	int16_t buffer[1024]; 
	uint32_t loops;
};

#endif
