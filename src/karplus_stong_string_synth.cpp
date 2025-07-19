#include <Arduino.h>
#include "karplus_strong_string_synth.h"
#include <TeensyThreads.h>

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

void KarplusStrongStringSynth::updateSamples(uint16_t attenuation, uint16_t filter)
{

	int16_t prior = buffers[bufferLen - 1];
	for (int i = 0; i < bufferLen; ++i)
	{
		int16_t in = buffers[i];
		int16_t next = buffers[(i + 1) % bufferLen];

		int16_t interpolated = (((int32_t)in * filter) + ((((int32_t)prior + (int32_t)next) * ((1 << 16) - filter)) >> 1)) >> 16;

		int16_t out = ((int32_t)interpolated * attenuation) >> 16;

		prior = in;
		buffers[i] = out;
	}
	bufferGeneration++;
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

	if (state == 1)
	{
		uint32_t lo = seed;
		int32_t sum = 0;
		
		// First pass: generate noise and calculate sum for DC removal
		for (int i = 0; i < bufferLen; i++)
		{
			lo = pseudorand(lo);
			buffers[i] = signed_multiply_32x16b(initialAmplitude, lo);
			sum += buffers[i];
		}
		
		// Calculate DC offset
		int16_t dcOffset = sum / bufferLen;
		
		// Second pass: remove DC component
		for (int i = 0; i < bufferLen; i++)
		{
			buffers[i] -= dcOffset;
		}
		
		seed = lo;
		state = 2;
		whichBuffer = 0;
		bufferGeneration = 0;

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
		int16_t sample1 = buffers[intPos];
		int16_t sample2 = buffers[(intPos + 1) % bufferLen];

		// Linear interpolation using fixed-point math
		int32_t diff = sample2 - sample1;
		int32_t interpolated = sample1 + ((diff * fracPart) >> 16);

		*data++ = (int16_t)interpolated;

		// Advance fractional position by delayIncrementFixed (16.16 format)
		bufferPosition += delayIncrementFixed;

		// Check if we've completed a full buffer cycle
		if ((bufferPosition >> 16) >= bufferLen)
		{
			updateSamples(attenuationScaled, filterScaled);
			bufferPosition -= (bufferLen << 16); // Subtract buffer length in fixed-point
		}
	}
	transmit(block);
	release(block);
}

uint32_t KarplusStrongStringSynth::seed = 1;

void displayUpdateWrapper(void *arg);
void displayUpdateThread(void *arg);

void KarplusStrongStringSynth::setTFTDisplay(FramebufferGFX* display)
{
	gfx = display;
	tftInitialized = (display != nullptr);

	// Request background display update instead of blocking audio
	if (tftInitialized && gfx)
	{
		if (displayThreadId == -1)
		{
			displayThreadId = threads.addThread(displayUpdateWrapper, (void *)this, 1<<15);
		}
	}
}

// Non-member function for background display updates
void displayUpdateWrapper(void *arg)
{
	KarplusStrongStringSynth *synthInstance = (KarplusStrongStringSynth *)arg;
	synthInstance->updateLoop();
}

extern uint32_t count;

void KarplusStrongStringSynth::updateLoop()
{
	int16_t *buffer = new int16_t[NUM_SAMPLES];
	while (1)
	{
		if (state != 0) {
			memcpy(buffer, buffers, NUM_SAMPLES * sizeof(buffer[0]));
			drawBufferGraph(gfx,
							buffer,
							bufferLen,
							frequency,
							bufferGeneration);
		}
		threads.yield();
	}
}


// Non-instance function for drawing buffer graph
void drawBufferGraph(FramebufferGFX* gfx, int16_t* buffer, uint16_t bufferLen, float frequency, int32_t bufferGeneration)
{
	if (!gfx || !buffer) return;

	Serial.println("Start drawBufferGraph");
	const int graphY = 0;
	const int graphHeight = gfx->height() - 20;
	const int graphWidth = gfx->width();
	const int centerY = graphY + graphHeight / 2;
	
	// Clear the graph area
	gfx->fillRect(0, graphY, graphWidth, graphHeight, BLACK);
	
	// Draw center line
	gfx->drawLine(0, centerY, graphWidth - 1, centerY, DARKGREY);

	// Draw buffer contents
	int prevY = centerY;
	
	for (int x = 0; x < graphWidth && x < bufferLen; x++)
	{
		// Scale sample value to graph height
		int32_t sample = buffer[x * bufferLen / graphWidth];
		int y = centerY - (sample * graphHeight / 2) / 32768;
		
		// Clamp y to graph bounds
		if (y < graphY) y = graphY;
		if (y > graphY + graphHeight - 1) y = graphY + graphHeight - 1;
		
		// Draw line from previous point
		if (x > 0) {
			gfx->drawLine(x - 1, prevY, x, y, GREEN);
		}
		prevY = y;
	}
	
	// Show buffer info
	gfx->fillRect(0, graphY + graphHeight + 5, graphWidth, 20, BLACK);
	gfx->setCursor(0, graphY + graphHeight + 5);
	gfx->setTextColor(WHITE);
	gfx->printf("Freq:%.1fHz Len:%d Gen:%d", frequency, bufferLen, bufferGeneration);
	Serial.println("Left drawBufferGraph");
}
