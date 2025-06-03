#include <Arduino.h>
#include "karplus_strong_string_synth.h"

static uint32_t pseudorand(uint32_t lo)
{
	uint32_t hi;

	hi = multiply_16bx16t(16807, lo); // 16807 * (lo >> 16)
	lo = 16807 * (lo & 0xFFFF);
	lo += (hi & 0x7FFF) << 16;
	lo += hi >> 15;
	lo = (lo & 0x7FFFFFFF) + (lo >> 31);
	return lo;
}

void KarplusStrongStringSynth::update(void)
{
	audio_block_t *block;

	if (state == 0)
		return;

	// We want the decay to be the same in time,
	// not the same in cycles.
	float timeBasedAttenuation = pow((float)attenuation / 128.0f, 7.0f * (float)bufferLen / AUDIO_SAMPLE_RATE_EXACT);
	uint16_t attenuationScaled = (uint16_t)(timeBasedAttenuation * 65535.0f);

	float timeBasedFilter = pow((float)filterStrength / 127.0f, 100.0f*(float)bufferLen / AUDIO_SAMPLE_RATE_EXACT);
	uint16_t filterScaled = (uint16_t)(timeBasedFilter * 32767.0f) + (1 << 15);

	if (state == 1)
	{
		uint32_t lo = seed;
		for (int i = 0; i < bufferLen; i++)
		{
			lo = pseudorand(lo);
			buffer[i] = signed_multiply_32x16b(initialAmplitude, lo);
		}
		seed = lo;
		state = 2;
		loops = 0;



		Serial.printf("bufferLen = %d\n", bufferLen);
		Serial.printf("attentuation = %d, pow(%.3f, %.3f) = %.3f\n",
			attenuation, (float)attenuation / 128.0f, (float)bufferLen / AUDIO_SAMPLE_RATE_EXACT, timeBasedAttenuation);
		Serial.printf("attenuationScaled = %x\n", attenuationScaled);
		Serial.printf("filterStrength = %d, pow(%.3f, %.3f) = %.3f\n",
			filterStrength, (float)filterStrength / 127.0f, (float)bufferLen / AUDIO_SAMPLE_RATE_EXACT, timeBasedFilter);
		Serial.printf("filterScaled = %x\n", filterScaled);
		Serial.println();
		outputTotalAttenuation();
	}

	block = allocate();
	if (!block)
	{
		state = 0;
		return;
	}

	int16_t prior;
	if (bufferIndex > 0)
	{
		prior = buffer[bufferIndex - 1];
	}
	else
	{
		prior = buffer[bufferLen - 1];
	}

	int16_t *data = block->data;

	for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
	{
		int16_t in = buffer[bufferIndex];

		int16_t interpolated = (((int32_t)in * filterScaled) + ((int32_t)prior * ((1 << 16) - filterScaled))) >> 16;

		int16_t out = ((int32_t)interpolated * attenuationScaled) >> 16;
		if(bufferIndex==0) {
			if(loops % 100 == 0) Serial.printf("in = %x, prior = %x, filterScaled = %x, out = %x, attenuation = %x\n", in, prior, filterScaled, out, attenuationScaled);
			loops++;
		}
		*data++ = out;
		buffer[bufferIndex] = out;
		prior = in;
		if (++bufferIndex >= bufferLen)
			bufferIndex = 0;
	}
	transmit(block);
	release(block);
}

uint32_t KarplusStrongStringSynth::seed = 1;
