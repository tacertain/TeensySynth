#ifndef _karplus_strong_string_synth_h_
#define _karplus_strong_string_synth_h_
#include <Arduino.h>
#include <AudioStream.h>
#include <utility/dspinst.h>
#include <ILI9341_t3.h>
#include <font_Arial.h>
#include <SPI.h>

#define NUM_SAMPLES 512

// TFT Display pins (from main.cpp comments)
#define TFT_CS   10
#define TFT_DC   9
// MOSI (11), SCK (13), MISO (12) are handled by SPI library

class KarplusStrongStringSynth : public AudioStream
{
public:
	KarplusStrongStringSynth(ILI9341_t3* display = nullptr) : AudioStream(0, NULL), tft(display)
	{
		state = 0;
		if (tft) {
			tftInitialized = true;
		}
	}
	int noteOn(float freq, float velocity)
	{
		if (velocity <= 0.0f || velocity > 1.0f)
		{
			noteOff();
			return -1;
		}
		frequency = freq;
		initialAmplitude = velocity * 65535.0f;

		bufferLen = NUM_SAMPLES;
		bufferIndex = 0;
		state = 1;

		calculateDelayIncrement();

		return 0;
	}
	void noteOff()
	{
		state = 0;
	}

	void setAttenuation(uint8_t newAttenuation)
	{
		attenuation = newAttenuation + 1;
		outputTotalAttenuation();
	}

	void setFilterStrength(uint16_t newStrength)
	{
		filterStrength = newStrength;
		outputTotalAttenuation();
	}

	void outputTotalAttenuation()
	{
		Serial.print("Total attenuation: ");
		Serial.print(attenuation);
		Serial.print(" * ");
		Serial.print(filterStrength);
		Serial.print(" = ");
		Serial.print(attenuation * filterStrength);
		Serial.println();
	}
	virtual void update(void);
	void setTFTDisplay(ILI9341_t3* display);
    void updateLoop();
    void updateFrequency(float newFreq)
    {
		frequency = newFreq;

		calculateDelayIncrement();
	}

	// Public access for threading (needed by non-member thread function)
	volatile int displayBufferIndex = 0;
	int displayThreadId = -1;
	bool tftInitialized = false;
	ILI9341_t3* tft;
	uint8_t state; // 0=off, 1=begin on next update, 2=playing
	
	// Public access for display drawing
	uint16_t bufferLen;
	int16_t buffers[NUM_SAMPLES];
	int32_t bufferGeneration;
	float frequency;			  // Target frequency in Hz

private:
	uint16_t bufferIndex;
	uint16_t whichBuffer;
	int32_t initialAmplitude;
	uint16_t attenuation = 103;	 // Reasonable starting point
	uint16_t filterStrength = 0; // Equal mix of old and new
	static uint32_t seed;		 // must start at 1

	uint32_t bufferPosition;	  // 16.16 fixed-point position in buffer
	uint32_t delayIncrementFixed; // 16.16 fixed-point increment per sample

	// Display update control - removed from here as they are now public

	void fillBuffer(uint16_t attenuation, uint16_t filter);
	void calculateDelayIncrement()
	{
		float exactDelay = AUDIO_SAMPLE_RATE_EXACT / frequency;
		float delayIncrement = (float)bufferLen / exactDelay;

		// Convert to 16.16 fixed-point
		delayIncrementFixed = (uint32_t)(delayIncrement * 65536.0f);
	}
};

// Non-member function for background display updates
void displayUpdateThread(KarplusStrongStringSynth* synthInstance);

// Non-instance function for drawing buffer graph
void drawBufferGraph(ILI9341_t3* tft, int16_t* buffer, uint16_t bufferLen, float frequency, int32_t bufferGeneration, int bufferIndex);

#endif
