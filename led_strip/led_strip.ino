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

// HomeSpan Addressable RGB LED Example

// Demonstrates use of HomeSpan Pixel Class that provides for control of single-wire
// addressable RGB LEDs, such as the SK68xx or WS28xx models found in many devices,
// including the Espressif ESP32, ESP32-S2, and ESP32-C3 development boards.

// Also demonstrates how to take advantage of the Eve HomeKit App's ability to render
// a generic custom Characteristic.  The sketch uses a custom Characterstic to create
// a "selector" button that enables to the user to select which special effect to run

// Onboard led pin = 32
// NEOPIXEL pin = 17
// Mosfet pin = 18
// Button pin = 0
int angle	= 0;
int counter = 0;

#define REQUIRED VERSION(1, 5, 0)

#include "HomeSpan.h"
#include "extras/Pixel.h" // include the HomeSpan Pixel class
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#if defined(CONFIG_IDF_TARGET_ESP32)

#define NEOPIXEL_RGBW_PIN 17

#elif defined(CONFIG_IDF_TARGET_ESP32S2)

#define NEOPIXEL_RGBW_PIN 38

#elif defined(CONFIG_IDF_TARGET_ESP32C3)

#define NEOPIXEL_RGBW_PIN 2

#endif

// clang-format off
CUSTOM_CHAR(Selector, 00000001-0001-0001-0001-46637266EA00, PR + PW + EV, UINT8, 1, 1, 3, false); // create Custom Characteristic to "select" special effects via Eve App
// clang-format on

// declare function
int *Wheel(byte WheelPos);
///////////////////////////////
AsyncWebServer server(80);
struct Pixel_Strand : Service::LightBulb { // Addressable RGBW Pixel Strand of nPixel Pixels

	struct SpecialEffect {
		Pixel_Strand *px;
		const char   *name;

		SpecialEffect(Pixel_Strand *px, const char *name) {
			this->px   = px;
			this->name = name;
			Serial.printf("Adding Effect %d: %s\n", px->Effects.size() + 1, name);
		}

		virtual void	 init() {}
		virtual uint32_t update() { return (60000); }
		virtual int		 requiredBuffer() { return (0); }
	};

	Characteristic::On		   power{0, true};
	Characteristic::Hue		   H{0, true};
	Characteristic::Saturation S{100, true};
	Characteristic::Brightness V{100, true};
	Characteristic::Selector   effect{1, true};

	vector<SpecialEffect *> Effects;

	Pixel		  *pixel;
	int			  nPixels;
	Pixel::Color *colors;
	uint32_t	  alarmTime;

	Pixel_Strand(int pin, int nPixels) : Service::LightBulb() {

		pixel		  = new Pixel(pin, false); // creates RGBW pixel LED on specified pin using default timing parameters suitable for most SK68xx LEDs
		this->nPixels = nPixels;			   // store number of Pixels in Strand

		Effects.push_back(new ManualControl(this));
		// Effects.push_back(new KnightRider(this));
		// Effects.push_back(new Random(this));
		Effects.push_back(new RainbowSolid(this));
		Effects.push_back(new Rainbow(this));

		effect.setUnit(""); // configures custom "Selector" characteristic for use with Eve HomeKit
		effect.setDescription("Color Effect");
		effect.setRange(1, Effects.size(), 1);

		V.setRange(5, 100, 1); // sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1%

		int bufSize = 0;

		for (int i = 0; i < Effects.size(); i++)
			bufSize = Effects[i]->requiredBuffer() > bufSize ? Effects[i]->requiredBuffer() : bufSize;

		colors = (Pixel::Color *)calloc(bufSize, sizeof(Pixel::Color)); // storage for dynamic pixel pattern

		Serial.printf("\nConfigured Pixel_Strand on pin %d with %d pixels and %d effects.  Color buffer = %d pixels\n\n", pin, nPixels, Effects.size(), bufSize);

		update();
	}

	boolean update() override {

		if (!power.getNewVal()) {
			pixel->set(Pixel::Color().RGB(0, 0, 0, 0), nPixels);
		} else {
			Effects[effect.getNewVal() - 1]->init();
			alarmTime = millis() + Effects[effect.getNewVal() - 1]->update();
			if (effect.updated())
				Serial.printf("Effect changed to: %s\n", Effects[effect.getNewVal() - 1]->name);
		}

		return (true);
	}

	void loop() override {

		if (millis() > alarmTime && power.getVal())
			alarmTime = millis() + Effects[effect.getNewVal() - 1]->update();
	}

	//////////////

	struct KnightRider : SpecialEffect {

		int phase = 0;
		int dir	  = 1;

		KnightRider(Pixel_Strand *px) : SpecialEffect{px, "KnightRider"} {}

		void init() override {
			float level = px->V.getNewVal<float>();
			for (int i = 0; i < px->nPixels; i++, level *= 0.8) {
				px->colors[px->nPixels + i - 1].HSV(px->H.getNewVal<float>(), px->S.getNewVal<float>(), level);
				px->colors[px->nPixels - i - 1] = px->colors[px->nPixels + i - 1];
			}
		}

		uint32_t update() override {
			px->pixel->set(px->colors + phase, px->nPixels);
			if (phase == px->nPixels - 1)
				dir = -1;
			else if (phase == 0)
				dir = 1;
			phase += dir;
			return (20);
		}

		int requiredBuffer() override { return (px->nPixels * 2 - 1); }
	};

	//////////////

	struct ManualControl : SpecialEffect {

		ManualControl(Pixel_Strand *px) : SpecialEffect{px, "Manual Control"} {}

		void init() override { px->pixel->set(Pixel::Color().HSV(px->H.getNewVal<float>(), px->S.getNewVal<float>(), px->V.getNewVal<float>()), px->nPixels); }
	};

	//////////////

	struct Random : SpecialEffect {

		Random(Pixel_Strand *px) : SpecialEffect{px, "Random"} {}

		uint32_t update() override {
			for (int i = 0; i < px->nPixels; i++)
				px->colors[i].HSV((esp_random() % 6) * 60, 100, px->V.getNewVal<float>());
			px->pixel->set(px->colors, px->nPixels);
			return (1000);
		}

		int requiredBuffer() override { return (px->nPixels); }
	};

	///////////////////////////////

	struct RainbowSolid : SpecialEffect {

		int8_t *dir;

		RainbowSolid(Pixel_Strand *px) : SpecialEffect{px, "Rainbow Solid"} {
			dir = (int8_t *)calloc(px->nPixels, sizeof(int8_t));
		}

		void init() override {
			for (int i = 0; i < px->nPixels; i++) {
				px->colors[i].RGB(0, 0, 0, 0);
				dir[i] = 0;
			}
		}

		uint32_t update() override {
			for (int i = 0; i < px->nPixels; i++) {
				int red, green, blue;

				if (angle < 60) {
					red	  = 255;
					green = round(angle * 4.25 - 0.01);
					blue  = 0;
				} else if (angle < 120) {
					red	  = round((120 - angle) * 4.25 - 0.01);
					green = 255;
					blue  = 0;
				} else if (angle < 180) {
					red = 0, green = 255;
					blue = round((angle - 120) * 4.25 - 0.01);
				} else if (angle < 240) {
					red = 0, green = round((240 - angle) * 4.25 - 0.01);
					blue = 255;
				} else if (angle < 300) {
					red = round((angle - 240) * 4.25 - 0.01), green = 0;
					blue = 255;
				} else {
					red = 255, green = 0;
					blue = round((360 - angle) * 4.25 - 0.01);
				}
				px->colors[i] = Pixel::Color().RGB(red, green, blue, 0);
			}
			px->pixel->set(px->colors, px->nPixels);
			angle++;
			if (angle == 360) angle = 0;
			return (100);
		}

		int requiredBuffer() override { return (px->nPixels); }
	};

	///////////////////////////////

	struct Rainbow : SpecialEffect {

		int8_t *dir;

		Rainbow(Pixel_Strand *px) : SpecialEffect{px, "Rainbow"} {
			dir = (int8_t *)calloc(px->nPixels, sizeof(int8_t));
		}

		void init() override {
			for (int i = 0; i < px->nPixels; i++) {
				px->colors[i].RGB(0, 0, 0, 0);
				dir[i] = 0;
			}
		}

		uint32_t update() override {
			for (int i = 0; i < px->nPixels; i++) {
				int *rgb;
				rgb			  = Wheel(((i * 256 / px->nPixels) + counter) & 255);
				px->colors[i] = Pixel::Color().RGB(*rgb, *(rgb + 1), *(rgb + 2), 0);
			}
			px->pixel->set(px->colors, px->nPixels);
			counter++;
			return (100);
		}

		int requiredBuffer() override { return (px->nPixels); }
	};
	///////////////////////////////
};

///////////////////////////////

void setup() {

	Serial.begin(115200);

	homeSpan.setLogLevel(0);
	homeSpan.setStatusPin(32);
	homeSpan.setStatusAutoOff(10);
	homeSpan.setWifiCallback(setupWeb);
	homeSpan.setControlPin(0);
	homeSpan.setPortNum(81);
	homeSpan.enableAutoStartAP();

	homeSpan.begin(Category::Lighting, "Holiday Lights");

	new SpanAccessory();
	new Service::AccessoryInformation();
	new Characteristic::Name("Holiday Lights");
	new Characteristic::Manufacturer("HomeSpan");
	new Characteristic::SerialNumber("123-ABC");
	new Characteristic::Model("NeoPixel 60 RGBW LEDs");
	new Characteristic::FirmwareRevision("1.0");
	new Characteristic::Identify();

	new Service::HAPProtocolInformation();
	new Characteristic::Version("1.1.0");

	new Pixel_Strand(NEOPIXEL_RGBW_PIN, 90);
}

///////////////////////////////

void loop() {
	homeSpan.poll();
}

///////////////////////////////

int *Wheel(byte WheelPos) {
	static int rgb[3];
	WheelPos = 255 - WheelPos;
	if (WheelPos < 85) {
		rgb[0] = 255 - WheelPos * 3;
		rgb[1] = 0;
		rgb[2] = WheelPos * 3;
		return rgb;
	}
	if (WheelPos < 170) {
		WheelPos -= 85;
		rgb[0] = 0;
		rgb[1] = WheelPos * 3;
		rgb[2] = 255 - WheelPos * 3;
		return rgb;
	}
	WheelPos -= 170;
	rgb[0] = WheelPos * 3;
	rgb[1] = 255 - WheelPos * 3;
	rgb[2] = 0;
	return rgb;
}

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