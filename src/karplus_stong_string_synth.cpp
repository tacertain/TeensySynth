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

void KarplusStrongStringSynth::fillBuffer(uint16_t fromBuffer, uint16_t attenuation, uint16_t filter) { 
	int16_t *from = buffers[fromBuffer & 0x1];
	int16_t *to = buffers[(fromBuffer ^ 0x1) & 0x1];

	int16_t prior = to[bufferLen - 1];
	for(int i=0; i<bufferLen; ++i, ++from, ++to) {
		int16_t in = *from;

		int16_t interpolated = (((int32_t)in * filter) + ((int32_t)prior * ((1 << 16) - filter))) >> 16;

		int16_t out = ((int32_t)interpolated * attenuation) >> 16;

		if(i==0) {
			if(loops % 100 == 0) Serial.printf("in = %x, prior = %x, filterScaled = %x, out = %x, attenuation = %x\n", in, prior, filter, out, attenuation);
			loops++;
		}
		prior = in;
		*to = out;
	}
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
			buffers[0][i] = signed_multiply_32x16b(initialAmplitude, lo);
			buffers[1][i] = signed_multiply_32x16b(initialAmplitude, lo);
		}
		seed = lo;
		state = 2;
		loops = 0;
		whichBuffer = 0;
		fillBuffer(0, attenuationScaled, filterScaled);

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

	int16_t *data = block->data;

	for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++)
	{
		*data++ = buffers[whichBuffer][bufferIndex];
		if (++bufferIndex >= bufferLen) {
			fillBuffer(whichBuffer, attenuationScaled, filterScaled);
			whichBuffer = (whichBuffer ^ 0x1) & 0x1;
			bufferIndex = 0;
		}
	}
	transmit(block);
	release(block);
}

uint32_t KarplusStrongStringSynth::seed = 1;
