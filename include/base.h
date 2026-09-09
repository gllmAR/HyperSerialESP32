/* base.h
*
*  MIT License
*
*  Copyright (c) 2021-2026 awawa-dev
*
*  https://github.com/awawa-dev/HyperSerialESP32
*
*  Permission is hereby granted, free of charge, to any person obtaining a copy
*  of this software and associated documentation files (the "Software"), to deal
*  in the Software without restriction, including without limitation the rights
*  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
*  copies of the Software, and to permit persons to whom the Software is
*  furnished to do so, subject to the following conditions:
*
*  The above copyright notice and this permission notice shall be included in all
*  copies or substantial portions of the Software.

*  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
*  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
*  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
*  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
*  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
*  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
*  SOFTWARE.
 */

#ifndef BASE_H
#define BASE_H

#include "freertos/semphr.h"

#if defined(SECOND_SEGMENT_START_INDEX)
	#if !defined(SECOND_SEGMENT_DATA_PIN)
		#error "Please define SECOND_SEGMENT_DATA_PIN for second segment"
	#elif !defined(SECOND_SEGMENT_CLOCK_PIN) && !defined(NEOPIXEL_RGBW) && !defined(NEOPIXEL_RGB)
		#error "Please define SECOND_SEGMENT_CLOCK_PIN and SECOND_SEGMENT_DATA_PIN for second segment"
	#endif
#endif

// internal LED segment (e.g. the onboard LED of the M5Atom):
// it does NOT consume any pixel from the incoming frame. Instead it shows
// colors sampled from the frame, so HyperHDR can keep its LED layout
// unchanged (full serial compatibility).
//
// Default (single LED, e.g. M5Atom Lite): shows the average color of a
// window of INTERNAL_LED_SAMPLE_WINDOW (default 16) LEDs around the middle
// of the frame.
//
// INTERNAL_LED_MATRIX (e.g. M5Atom Matrix, 25 LEDs): the internal strand
// is a small matrix; each cell shows the average color of one of the
// INTERNAL_LED_COUNT regions the incoming frame is divided into, arranged
// row-major from the top-left. INTERNAL_LED_MATRIX_ROTATE (0-3, 90 degrees
// clockwise steps) and INTERNAL_LED_MATRIX_MIRROR adapt the logical view
// to the physical mounting of the device.
//
// INTERNAL_LED_COUNT defaults to 1 (25 in matrix mode). To disable the
// internal LED, build without INTERNAL_LED_DATA_PIN.
#if defined(INTERNAL_LED_DATA_PIN)
	#if defined(INTERNAL_LED_MATRIX)
		#if !defined(INTERNAL_LED_MATRIX_WIDTH) || !defined(INTERNAL_LED_MATRIX_HEIGHT)
			#undef INTERNAL_LED_MATRIX_WIDTH
			#undef INTERNAL_LED_MATRIX_HEIGHT
			#define INTERNAL_LED_MATRIX_WIDTH 5
			#define INTERNAL_LED_MATRIX_HEIGHT 5
		#endif
		#if defined(INTERNAL_LED_COUNT) && INTERNAL_LED_COUNT != INTERNAL_LED_MATRIX_WIDTH * INTERNAL_LED_MATRIX_HEIGHT
			#error "INTERNAL_LED_COUNT must match the matrix size (width * height)"
		#endif
		#if !defined(INTERNAL_LED_COUNT)
			#define INTERNAL_LED_COUNT (INTERNAL_LED_MATRIX_WIDTH * INTERNAL_LED_MATRIX_HEIGHT)
		#endif
		#if !defined(INTERNAL_LED_MATRIX_ROTATE)
			#define INTERNAL_LED_MATRIX_ROTATE 0
		#endif
		// physical chain order of the M5Atom Matrix 5x5 (determined on-device):
		// row-wise serpentine, chain #0 at the bottom-right corner, running
		// right-to-left on even rows (counted from the bottom), left-to-right on
		// odd rows, ending at #24 = top-left.
		// Indexed by the LOGICAL cell (row-major, origin at the top-left of the
		// logical view); value = physical chain index of that cell.
		#if !defined(INTERNAL_LED_MATRIX_MAP)
			#define INTERNAL_LED_MATRIX_MAP \
				{ 24, 23, 22, 21, 20, \
				  15, 16, 17, 18, 19, \
				  14, 13, 12, 11, 10, \
				   5,  6,  7,  8,  9, \
				   4,  3,  2,  1,  0 }
		#endif
	#else
		#if !defined(INTERNAL_LED_COUNT)
			#define INTERNAL_LED_COUNT 1
		#endif
		#if !defined(INTERNAL_LED_SAMPLE_WINDOW)
			#define INTERNAL_LED_SAMPLE_WINDOW 16
		#endif
	#endif
#endif

class Base
{
	// LED strip number
	int ledsNumber = 0;
	// NeoPixelBusLibrary primary object
	LED_DRIVER* ledStrip1 = nullptr;
	// NeoPixelBusLibrary second object
	LED_DRIVER2* ledStrip2 = nullptr;
	#if defined(INTERNAL_LED_DATA_PIN)
		// internal LED segment object (e.g. the onboard LED of the M5Atom)
		LED_DRIVER_INTERN* ledStripIntern = nullptr;
		#if defined(INTERNAL_LED_MATRIX)
			// per-cell color sample buckets: the incoming frame is divided
			// into INTERNAL_LED_COUNT regions, one per matrix cell
			uint32_t cellSumR[INTERNAL_LED_COUNT] = {0};
			uint32_t cellSumG[INTERNAL_LED_COUNT] = {0};
			uint32_t cellSumB[INTERNAL_LED_COUNT] = {0};
			#if defined(NEOPIXEL_RGBW) || defined(SPILED_APA102)
				uint32_t cellSumW[INTERNAL_LED_COUNT] = {0};
			#endif
			uint16_t cellPixels[INTERNAL_LED_COUNT] = {0};

			// map a logical cell to the physical chain index, honoring the
			// compile-time rotation (90 degrees clockwise steps) and mirroring
			const uint8_t internalLedMatrixMap[INTERNAL_LED_COUNT] = INTERNAL_LED_MATRIX_MAP;

			inline uint8_t internalMatrixChainIndex(uint8_t logicalCell) const
			{
				uint8_t x = logicalCell % INTERNAL_LED_MATRIX_WIDTH;
				uint8_t y = logicalCell / INTERNAL_LED_MATRIX_WIDTH;

				#if defined(INTERNAL_LED_MATRIX_MIRROR)
					x = INTERNAL_LED_MATRIX_WIDTH - 1 - x;
				#endif

				#if INTERNAL_LED_MATRIX_ROTATE == 1 || INTERNAL_LED_MATRIX_ROTATE == 3
					uint8_t swap = x;
					x = y;
					y = swap;
				#endif
				#if INTERNAL_LED_MATRIX_ROTATE == 1
					x = INTERNAL_LED_MATRIX_WIDTH - 1 - x;
				#elif INTERNAL_LED_MATRIX_ROTATE == 2
					x = INTERNAL_LED_MATRIX_WIDTH - 1 - x;
					y = INTERNAL_LED_MATRIX_HEIGHT - 1 - y;
				#elif INTERNAL_LED_MATRIX_ROTATE == 3
					y = INTERNAL_LED_MATRIX_HEIGHT - 1 - y;
				#endif

				return internalLedMatrixMap[y * INTERNAL_LED_MATRIX_WIDTH + x];
			}
		#else
			// running average of the color sampled around the frame middle
			uint32_t sampleSumR = 0, sampleSumG = 0, sampleSumB = 0, sampleSumW = 0;
			uint16_t samplePixels = 0;
			uint16_t sampleWindowStart = 0, sampleWindowEnd = 0;
		#endif
	#endif
	// frame is set and ready to render
	bool readyToRender = false;

	public:
		// static data buffer for the loop
		uint8_t buffer[MAX_BUFFER + 1] = {0};
		// handle to tasks
		TaskHandle_t processDataHandle = nullptr;
		TaskHandle_t processSerialHandle = nullptr;
		// semaphore to synchronize them
		xSemaphoreHandle i2sXSemaphore;
		// current queue position
		volatile int queueCurrent = 0;
		// queue end position
		volatile int queueEnd = 0;

		inline int getLedsNumber()
		{
			return ledsNumber;
		}

		inline LED_DRIVER* getLedStrip1()
		{
			return ledStrip1;
		}

		inline LED_DRIVER2* getLedStrip2()
		{
			return ledStrip2;
		}

		void initLedStrip(int count)
		{
			if (ledStrip1 != nullptr)
			{
				delete ledStrip1;
				ledStrip1 = nullptr;
			}

			if (ledStrip2 != nullptr)
			{
				delete ledStrip2;
				ledStrip2 = nullptr;
			}

			#if defined(INTERNAL_LED_DATA_PIN)
				if (ledStripIntern != nullptr)
				{
					delete ledStripIntern;
					ledStripIntern = nullptr;
				}
			#endif

			ledsNumber = count;

			#if defined(SECOND_SEGMENT_START_INDEX)
				if (ledsNumber > SECOND_SEGMENT_START_INDEX)
				{
					#if defined(NEOPIXEL_RGBW) || defined(NEOPIXEL_RGB)
						ledStrip1 = new LED_DRIVER(SECOND_SEGMENT_START_INDEX, DATA_PIN);
						ledStrip1->Begin();
						ledStrip2 = new LED_DRIVER2(ledsNumber - SECOND_SEGMENT_START_INDEX, SECOND_SEGMENT_DATA_PIN);
						ledStrip2->Begin();
					#else
						ledStrip1 = new LED_DRIVER(SECOND_SEGMENT_START_INDEX);
						ledStrip1->Begin(CLOCK_PIN, 12, DATA_PIN, 15);
						ledStrip2 = new LED_DRIVER2(ledsNumber - SECOND_SEGMENT_START_INDEX);
						ledStrip2->Begin(SECOND_SEGMENT_CLOCK_PIN, 12, SECOND_SEGMENT_DATA_PIN, 15);
					#endif
				}
			#endif

			if (ledStrip1 == nullptr)
			{
				#if defined(NEOPIXEL_RGBW) || defined(NEOPIXEL_RGB)
					ledStrip1 = new LED_DRIVER(ledsNumber, DATA_PIN);
					ledStrip1->Begin();
				#else
					ledStrip1 = new LED_DRIVER(ledsNumber);
					ledStrip1->Begin(CLOCK_PIN, 12, DATA_PIN, 15);
				#endif
			}

			#if defined(INTERNAL_LED_DATA_PIN)
				ledStripIntern = new LED_DRIVER_INTERN(INTERNAL_LED_COUNT, INTERNAL_LED_DATA_PIN);
				ledStripIntern->Begin();

				#if !defined(INTERNAL_LED_MATRIX)
					// sample window centered around the middle LED, clamped to the strip
					uint16_t middle = ledsNumber / 2;
					uint16_t halfWindow = INTERNAL_LED_SAMPLE_WINDOW / 2;
					sampleWindowStart = (middle > halfWindow) ? (middle - halfWindow) : 0;
					sampleWindowEnd = ((sampleWindowStart + INTERNAL_LED_SAMPLE_WINDOW) < ledsNumber) ?
						(sampleWindowStart + INTERNAL_LED_SAMPLE_WINDOW) : ledsNumber;
				#endif
			#endif
		}

		/**
		 * @brief Check if there is already prepared frame to display
		 *
		 * @return true
		 * @return false
		 */
		inline bool hasLateFrameToRender()
		{
			return readyToRender;
		}

		inline void dropLateFrame()
		{
			readyToRender = false;
		}

		inline void renderLeds(bool newFrame)
		{
			#if defined(INTERNAL_LED_DATA_PIN)
				#if defined(INTERNAL_LED_MATRIX)
					if (newFrame && ledStripIntern != nullptr)
					{
						// each matrix cell shows the average color of its sampled region
						for (uint8_t cell = 0; cell < INTERNAL_LED_COUNT; cell++)
						{
							if (cellPixels[cell] == 0)
								continue;
							#if defined(NEOPIXEL_RGBW) || defined(SPILED_APA102)
								ColorDefinition avg(cellSumR[cell] / cellPixels[cell], cellSumG[cell] / cellPixels[cell],
									cellSumB[cell] / cellPixels[cell], cellSumW[cell] / cellPixels[cell]);
							#else
								ColorDefinition avg(cellSumR[cell] / cellPixels[cell], cellSumG[cell] / cellPixels[cell],
									cellSumB[cell] / cellPixels[cell]);
							#endif
							ledStripIntern->SetPixelColor(internalMatrixChainIndex(cell), avg);
						}
					}
				#else
					if (newFrame && ledStripIntern != nullptr && samplePixels > 0)
					{
						// internal LED shows the average color sampled around the frame middle
						#if defined(NEOPIXEL_RGBW) || defined(SPILED_APA102)
							ColorDefinition avg(sampleSumR / samplePixels, sampleSumG / samplePixels,
								sampleSumB / samplePixels, sampleSumW / samplePixels);
						#else
							ColorDefinition avg(sampleSumR / samplePixels, sampleSumG / samplePixels,
								sampleSumB / samplePixels);
						#endif
						for (uint8_t internPix = 0; internPix < INTERNAL_LED_COUNT; internPix++)
							ledStripIntern->SetPixelColor(internPix, avg);
					}
				#endif
			#endif

			if (newFrame)
				readyToRender = true;

			if (readyToRender &&
				(ledStrip1 != nullptr && ledStrip1->CanShow()) &&
				!(ledStrip2 != nullptr && !ledStrip2->CanShow())
				#if defined(INTERNAL_LED_DATA_PIN)
					&& !(ledStripIntern != nullptr && !ledStripIntern->CanShow())
				#endif
			)
			{
				statistics.increaseShow();
				readyToRender = false;

				// display segments
				ledStrip1->Show(false);
				if (ledStrip2 != nullptr)
					ledStrip2->Show(false);
				#if defined(INTERNAL_LED_DATA_PIN)
					if (ledStripIntern != nullptr)
						ledStripIntern->Show(false);
				#endif
			}
		}

		inline bool setStripPixel(uint16_t pix, ColorDefinition &inputColor)
		{
			if (pix < ledsNumber)
			{
				#if defined(INTERNAL_LED_DATA_PIN)
					// reset the sampler on the first pixel of a new frame
					if (pix == 0)
					{
						#if defined(INTERNAL_LED_MATRIX)
							for (uint8_t cell = 0; cell < INTERNAL_LED_COUNT; cell++)
							{
								cellSumR[cell] = cellSumG[cell] = cellSumB[cell] = 0;
								#if defined(NEOPIXEL_RGBW) || defined(SPILED_APA102)
									cellSumW[cell] = 0;
								#endif
								cellPixels[cell] = 0;
							}
						#else
							sampleSumR = sampleSumG = sampleSumB = sampleSumW = 0;
							samplePixels = 0;
						#endif
					}

					#if defined(INTERNAL_LED_MATRIX)
						// each matrix cell shows one of the INTERNAL_LED_COUNT regions
						// the incoming frame is divided into
						uint16_t cell = ((uint32_t)pix * INTERNAL_LED_COUNT) / ledsNumber;
						if (cell < INTERNAL_LED_COUNT)
						{
							cellSumR[cell] += inputColor.R;
							cellSumG[cell] += inputColor.G;
							cellSumB[cell] += inputColor.B;
							#if defined(NEOPIXEL_RGBW) || defined(SPILED_APA102)
								cellSumW[cell] += inputColor.W;
							#endif
							cellPixels[cell]++;
						}
					#else
						// accumulate the color window around the middle LED
						if (pix >= sampleWindowStart && pix < sampleWindowEnd)
						{
							sampleSumR += inputColor.R;
							sampleSumG += inputColor.G;
							sampleSumB += inputColor.B;
							#if defined(NEOPIXEL_RGBW) || defined(SPILED_APA102)
								sampleSumW += inputColor.W;
							#endif
							samplePixels++;
						}
					#endif
				#endif

				#if defined(SECOND_SEGMENT_START_INDEX)
					if (pix < SECOND_SEGMENT_START_INDEX)
						ledStrip1->SetPixelColor(pix, inputColor);
					else
					{
						#if defined(SECOND_SEGMENT_REVERSED)
							ledStrip2->SetPixelColor(ledsNumber - pix - 1, inputColor);
						#else
							ledStrip2->SetPixelColor(pix - SECOND_SEGMENT_START_INDEX, inputColor);
						#endif
					}
				#else
					ledStrip1->SetPixelColor(pix, inputColor);
				#endif
			}

			return (pix + 1 < ledsNumber);
		}
} base;

#endif