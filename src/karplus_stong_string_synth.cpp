#include <Arduino.h>
#include "karplus_strong_string_synth.h"
#include <TeensyThreads.h>

// Global static pointer to hold the instance for the thread
static KarplusStrongStringSynth* threadInstance = nullptr;

// Forward declarations
void displayThreadWrapper();
void displayUpdateThread(KarplusStrongStringSynth* synthInstance);

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
	
	// Request background display update instead of blocking audio
	if (tftInitialized && tft) {
		if (displayThreadId == -1) {
			threadInstance = this; // Set the static instance pointer
			displayThreadId = threads.addThread(displayThreadWrapper);
		}
		displayBufferIndex = (fromBuffer + 1) % 2;
		displayUpdateRequested = true;
	}
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

void KarplusStrongStringSynth::setTFTDisplay(ILI9341_t3* display)
{
	tft = display;
	tftInitialized = (display != nullptr);
}

// Wrapper function for thread creation
void displayThreadWrapper() {
	if (threadInstance) {
		displayUpdateThread(threadInstance);
	}
}

// Non-member function for background display updates
void displayUpdateThread(KarplusStrongStringSynth* synthInstance)
{
	threadInstance = synthInstance; // Set the static instance pointer

	while (1) {
		if (synthInstance && synthInstance->displayUpdateRequested) {
			// Copy the buffer data to avoid race conditions
			int bufferIndex = synthInstance->displayBufferIndex;
			
			// Clear the request flag immediately to avoid missing updates
			synthInstance->displayUpdateRequested = false;
			
			// Now safely draw the buffer graph
			if (synthInstance->tftInitialized && synthInstance->tft && synthInstance->state != 0) {
				synthInstance->drawBufferGraph(bufferIndex);
			}
		}
		threads.delay(5); // Check every 5ms for update requests
	}
}

void KarplusStrongStringSynth::drawBufferGraph(int bufferIndex)
{
	if (!tftInitialized || !tft) return;

	const int graphY = 30;
	const int graphHeight = 180;
	const int graphWidth = tft->width();
	const int centerY = graphY + graphHeight / 2;
	
	// Clear the graph area
	tft->fillRect(0, graphY, graphWidth, graphHeight, ILI9341_BLACK);
	
	// Draw center line
	tft->drawLine(0, centerY, graphWidth-1, centerY, ILI9341_DARKGREY);

	if (state == 0)
		return; 
		
	// Draw buffer contents
	int16_t *buffer = buffers[bufferIndex];
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
			tft->drawLine(x-1, prevY, x, y, ILI9341_GREEN);
		}
		prevY = y;
	}
	
	// Show buffer info
	tft->fillRect(0, graphY + graphHeight + 5, graphWidth, 20, ILI9341_BLACK);
	tft->setCursor(0, graphY + graphHeight + 5);
	tft->setTextColor(ILI9341_WHITE);
	tft->setFont(Arial_10);
	tft->printf("Buf%d Freq:%.1fHz Len:%d Gen:%d", 
		bufferIndex, frequency, bufferLen, bufferGeneration[bufferIndex]);
}
