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
// the average color of a window of LEDs sampled around the middle of the
// frame, so HyperHDR can keep its LED layout unchanged (full serial
// compatibility).
// INTERNAL_LED_COUNT defaults to 1, INTERNAL_LED_SAMPLE_WINDOW defaults
// to 16 LEDs. To disable the internal LED, build without INTERNAL_LED_DATA_PIN.
#if defined(INTERNAL_LED_DATA_PIN)
	#if !defined(INTERNAL_LED_COUNT)
		#define INTERNAL_LED_COUNT 1
	#endif
	#if !defined(INTERNAL_LED_SAMPLE_WINDOW)
		#define INTERNAL_LED_SAMPLE_WINDOW 16
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
		// running average of the color sampled around the frame middle
		uint32_t sampleSumR = 0, sampleSumG = 0, sampleSumB = 0, sampleSumW = 0;
		uint16_t samplePixels = 0;
		uint16_t sampleWindowStart = 0, sampleWindowEnd = 0;
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

				// sample window centered around the middle LED, clamped to the strip
				uint16_t middle = ledsNumber / 2;
				uint16_t halfWindow = INTERNAL_LED_SAMPLE_WINDOW / 2;
				sampleWindowStart = (middle > halfWindow) ? (middle - halfWindow) : 0;
				sampleWindowEnd = ((sampleWindowStart + INTERNAL_LED_SAMPLE_WINDOW) < ledsNumber) ?
					(sampleWindowStart + INTERNAL_LED_SAMPLE_WINDOW) : ledsNumber;
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
					// reset the sampler on the first pixel of a new frame and
					// accumulate the color window around the middle LED
					if (pix == 0)
					{
						sampleSumR = sampleSumG = sampleSumB = sampleSumW = 0;
						samplePixels = 0;
					}
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