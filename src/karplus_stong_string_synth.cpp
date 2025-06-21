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

void KarplusStrongStringSynth::fillBuffer(uint16_t fromBuffer, uint16_t attenuation, uint16_t filter)
{
	int16_t *from = buffers[fromBuffer % 2];
	int16_t *to = buffers[(fromBuffer + 1) % 2];

	int16_t prior = to[bufferLen - 1];
	for (int i = 0; i < bufferLen; ++i, ++from, ++to)
	{
		int16_t in = *from;

		int16_t interpolated = (((int32_t)in * filter) + ((int32_t)prior * ((1 << 16) - filter))) >> 16;

		int16_t out = ((int32_t)interpolated * attenuation) >> 16;

		prior = in;
		*to = out;
	}
	bufferGeneration[(fromBuffer + 1) % 2] = bufferGeneration[fromBuffer % 2] + fromBuffer % 2;
}

void KarplusStrongStringSynth::update(void)
{
	audio_block_t *block;

	if (state == 0)
		return;

	// We want the decay to be the same in time,
	// not the same in cycles.
	float timeBasedAttenuation = pow((float)attenuation / 128.0f, 7.0f / frequency);
	uint16_t attenuationScaled = (uint16_t)(timeBasedAttenuation * 65535.0f);

	float timeBasedFilter = pow((float)filterStrength / 127.0f, 100.0f / frequency);
	uint16_t filterScaled = 65535 - (uint16_t)(timeBasedFilter * 32767.0f);

	uint16_t num_iter = (uint16_t)(-1.0f / log2f((float)filterScaled));

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
		whichBuffer = 0;
		bufferGeneration[0] = 0;
		bufferGeneration[1] = -1;
		fillBuffer(0, attenuationScaled, filterScaled);

		Serial.printf("bufferLen = %d\n", bufferLen);
		Serial.printf("attentuation = %d, pow(%.3f, %.3f) = %.3f\n",
					  attenuation, (float)attenuation / 128.0f, 
					  (float)bufferLen / AUDIO_SAMPLE_RATE_EXACT, timeBasedAttenuation);
		Serial.printf("attenuationScaled = %x\n", attenuationScaled);
		Serial.printf("filterStrength = %d, pow(%.3f, %.3f) = %.3f\n",
					  filterStrength, (float)filterStrength / 127.0f, 
					  (float)bufferLen / AUDIO_SAMPLE_RATE_EXACT, timeBasedFilter);
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
		// bufferPosition is in 16.16 fixed-point format
		// High 16 bits = integer part, low 16 bits = fractional part
		uint32_t intPos = bufferPosition >> 16;		 // Integer part
		uint32_t fracPart = bufferPosition & 0xFFFF; // Fractional part (0-65535)

		// Get the two samples to interpolate between
		int16_t sample1 = buffers[whichBuffer][intPos];

		int16_t sample2;
		// Sample2 might be beyond the buffer end, so we will need to recompule
		if (intPos + 1 >= bufferLen)
		{
			// fillIfNecessary(attenuationScaled, filterScaled);
			sample2 = buffers[(whichBuffer + 1) % 2][intPos + 1];
		}
		else
			sample2 = buffers[whichBuffer][intPos + 1];

		// Linear interpolation using fixed-point math
		// interpolated = sample1 + (sample2 - sample1) * fracPart / 65536
		int32_t diff = sample2 - sample1;
		int32_t interpolated = sample1 + ((diff * fracPart) >> 16);

		*data++ = (int16_t)interpolated;

		// Advance fractional position by delayIncrementFixed (16.16 format)
		bufferPosition += delayIncrementFixed;

		// Check if we've completed a full buffer cycle
		if ((bufferPosition >> 16) >= bufferLen)
		{
			fillIfNecessary(attenuationScaled, filterScaled);
			whichBuffer = (whichBuffer ^ 0x1) & 0x1;
			bufferPosition -= (bufferLen << 16); // Subtract buffer length in fixed-point
		}
	}
	transmit(block);
	release(block);
}

void KarplusStrongStringSynth::fillIfNecessary(uint16_t attenuationScaled, uint16_t filterScaled)
{
	if ((whichBuffer == 0 && bufferGeneration[1] < bufferGeneration[0]) ||
		(whichBuffer == 1 && bufferGeneration[0] <= bufferGeneration[1]))
	{
		fillBuffer(whichBuffer, attenuationScaled, filterScaled);
	}
}

uint32_t KarplusStrongStringSynth::seed = 1;
