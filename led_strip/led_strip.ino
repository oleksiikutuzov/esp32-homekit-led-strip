/*********************************************************************************
 *  MIT License
 *
 *  Copyright (c) 2022 Gregg E. Berman
 *
 *  https://github.com/HomeSpan/HomeSpan
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
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 ********************************************************************************/

// HomeSpan Addressable RGB LED Examples.  Demonstrates use of:
//
//  * HomeSpan Pixel Class that provides for control of single-wire addressable RGB and RGBW LEDs, such as the WS2812 and SK6812
//  * HomeSpan Dot Class that provides for control of two-wire addressable RGB LEDs, such as the APA102 and SK9822
//
// IMPORTANT:  YOU LIKELY WILL NEED TO CHANGE THE PIN NUMBERS BELOW TO MATCH YOUR SPECIFIC ESP32/S2/C3 BOARD
//

// Onboard led pin = 32
// NEOPIXEL pin = 17
// Mosfet pin = 18
// Button pin = 0

#if defined(CONFIG_IDF_TARGET_ESP32)

#define NEOPIXEL_RGB_PIN 17
#define DEVICE_SUFFIX	 ""

#elif defined(CONFIG_IDF_TARGET_ESP32S2)

#define NEOPIXEL_RGB_PIN 17
#define DEVICE_SUFFIX	 "-S2"

#elif defined(CONFIG_IDF_TARGET_ESP32C3)

#define NEOPIXEL_RGB_PIN 0
#define DEVICE_SUFFIX	 "-C3"

#endif

#include "HomeSpan.h"
#include "extras/Pixel.h" // include the HomeSpan Pixel class
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

AsyncWebServer server(80);

///////////////////////////////

struct NeoPixel_RGB : Service::LightBulb { // Addressable single-wire RGB LED Strand (e.g. NeoPixel)

	Characteristic::On		   power{0, true};
	Characteristic::Hue		   H{0, true};
	Characteristic::Saturation S{0, true};
	Characteristic::Brightness V{100, true};
	Pixel					  *pixel;
	uint8_t					   nPixels;

	NeoPixel_RGB(uint8_t pin, uint8_t nPixels) : Service::LightBulb() {

		V.setRange(5, 100, 1);			// sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1%
		pixel		  = new Pixel(pin); // creates Pixel LED on specified pin
		this->nPixels = nPixels;		// save number of Pixels in this LED Strand
		update();						// manually call update() to set pixel with restored initial values
	}

	boolean update() override {

		int p = power.getNewVal();

		float h = H.getNewVal<float>(); // range = [0,360]
		float s = S.getNewVal<float>(); // range = [0,100]
		float v = V.getNewVal<float>(); // range = [0,100]

		Pixel::Color color;

		pixel->set(color.HSV(h * p, s * p, v * p), nPixels); // sets all nPixels to the same HSV color

		return (true);
	}
};

void setup() {

	Serial.begin(115200);
	homeSpan.setLogLevel(0);
	homeSpan.setStatusPin(32);
	homeSpan.setStatusAutoOff(10);
	homeSpan.setWifiCallback(setupWeb);
	homeSpan.setControlPin(0);
	homeSpan.setPortNum(81);
	homeSpan.enableAutoStartAP();
	homeSpan.begin(Category::Lighting, "Pixel LEDS" DEVICE_SUFFIX);

	SPAN_ACCESSORY(); // create Bridge (note this sketch uses the SPAN_ACCESSORY() macro, introduced in v1.5.1 --- see the HomeSpan API Reference for details on this convenience macro)

	SPAN_ACCESSORY("Neo RGB");
	new NeoPixel_RGB(NEOPIXEL_RGB_PIN, 60); // create 8-LED NeoPixel RGB Strand with full color control
}

///////////////////////////////

void loop() {
	homeSpan.poll();
}

///////////////////////////////

void setupWeb() {
	server.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
		String content = "<html><body>Rebooting!  Will return to configuration page in 10 seconds.<br><br>";
		content += "<meta http-equiv = \"refresh\" content = \"10; url = /\" />";
		request->send(200, "text/html", content);

		ESP.restart();
	});

	AsyncElegantOTA.begin(&server); // Start AsyncElegantOTA
	server.begin();
	LOG1("HTTP server started");
} // setupWeb