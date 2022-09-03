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

/*
 *                ESP-WROOM-32 Utilized pins
 *              ╔═════════════════════════════╗
 *              ║┌─┬─┐  ┌──┐  ┌─┐             ║
 *              ║│ | └──┘  └──┘ |             ║
 *              ║│ |            |             ║
 *              ╠═════════════════════════════╣
 *          +++ ║GND                       GND║ +++
 *          +++ ║3.3V                     IO23║ USED_FOR_NOTHING
 *              ║                         IO22║
 *              ║IO36                      IO1║ TX
 *              ║IO39                      IO3║ RX
 *              ║IO34                     IO21║
 *              ║IO35                         ║ NC
 *      RED_LED ║IO32                     IO19║
 *              ║IO33                     IO18║ RELAY
 *              ║IO25                      IO5║
 *              ║IO26                     IO17║ NEOPIXEL
 *              ║IO27                     IO16║
 *              ║IO14                      IO4║
 *              ║IO12                      IO0║ +++, BUTTON
 *              ╚═════════════════════════════╝
 */

float angle		  = 0;
long  count_wheel = 0;

#define MAX_LEDS 120
#define MAXHUE	 360
#define REQUIRED VERSION(1, 6, 0)

#include "HomeSpan.h"
#include "extras/Pixel.h" // include the HomeSpan Pixel class
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "OTA.hpp"

uint16_t relay;

char sNumber[18] = "11:11:11:11:11:11";

#if defined(CONFIG_IDF_TARGET_ESP32)

#define NEOPIXEL_RGBW_PIN 17

#elif defined(CONFIG_IDF_TARGET_ESP32S2)

#define NEOPIXEL_RGBW_PIN 38

#elif defined(CONFIG_IDF_TARGET_ESP32C3)

#define NEOPIXEL_RGBW_PIN 2

#endif

void addSwitch();

// clang-format off
CUSTOM_CHAR(Selector, 00000001-0001-0001-0001-46637266EA00, PR + PW + EV, UINT8, 1, 1, 3, false); // create Custom Characteristic to "select" special effects via Eve App
CUSTOM_CHAR(NumLeds, 00000002-0001-0001-0001-46637266EA00, PR + PW + EV, UINT8, 90, 1, MAX_LEDS, false);
CUSTOM_CHAR(RelayEnabled, 00000003-0001-0001-0001-46637266EA00, PR + PW + EV, BOOL, 0, 0, 1, false);
CUSTOM_CHAR(AnimSpeed, 00000004-0001-0001-0001-46637266EA00, PR + PW + EV, UINT8, 1, 1, 10, false);
// clang-format on

// declare function
int *Wheel(byte WheelPos);
///////////////////////////////
WebServer server(80);
struct Pixel_Strand : Service::LightBulb { // Addressable RGBW Pixel Strand of nPixel Pixels

	struct SpecialEffect {
		Pixel_Strand *px;
		const char	 *name;

		SpecialEffect(Pixel_Strand *px, const char *name) {
			this->px   = px;
			this->name = name;
			Serial.printf("Adding Effect %d: %s\n", px->Effects.size() + 1, name);
		}

		virtual void	 init() {}
		virtual uint32_t update() { return (60000); }
		virtual int		 requiredBuffer() { return (0); }
	};

	Characteristic::On			 power{0, true};
	Characteristic::Hue			 H{0, true};
	Characteristic::Saturation	 S{100, true};
	Characteristic::Brightness	 V{100, true};
	Characteristic::Selector	 effect{1, true};
	Characteristic::NumLeds		 num_leds{90, true};
	Characteristic::RelayEnabled relay_enabled{false, true};
	Characteristic::AnimSpeed	 anim_speed{1, true};

	vector<SpecialEffect *> Effects;

	Pixel		 *pixel;
	int			  nPixels;
	Pixel::Color *colors;
	uint32_t	  alarmTime;

	Pixel_Strand(int pin) : Service::LightBulb() {

		pixel = new Pixel(pin, false); // creates RGBW pixel LED on specified pin using default timing parameters suitable for most SK68xx LEDs

		Effects.push_back(new ManualControl(this));
		Effects.push_back(new Rainbow(this));
		Effects.push_back(new Colorwheel(this));

		effect.setUnit(""); // configures custom "Selector" characteristic for use with Eve HomeKit
		effect.setDescription("Color Effect");
		effect.setRange(1, Effects.size(), 1);

		num_leds.setUnit(""); // configures custom "Selector" characteristic for use with Eve HomeKit
		num_leds.setDescription("Number of LEDs");
		num_leds.setRange(1, MAX_LEDS, 1);

		anim_speed.setUnit(""); // configures custom "Selector" characteristic for use with Eve HomeKit
		anim_speed.setDescription("Animation Speed");
		anim_speed.setRange(1, 10, 1);

		relay_enabled.setDescription("Switch Enabled");

		V.setRange(5, 100, 1); // sets the range of the Brightness to be from a min of 5%, to a max of 100%, in steps of 1%

		this->nPixels = num_leds.getNewVal(); // store number of Pixels in Strand

		int bufSize = 0;

		for (int i = 0; i < Effects.size(); i++)
			bufSize = Effects[i]->requiredBuffer() > bufSize ? Effects[i]->requiredBuffer() : bufSize;

		colors = (Pixel::Color *)calloc(bufSize, sizeof(Pixel::Color)); // storage for dynamic pixel pattern

		Serial.printf("\nConfigured Pixel_Strand on pin %d with %d pixels and %d effects.  Color buffer = %d pixels\n\n", pin, this->nPixels, Effects.size(), bufSize);

		update();
	}

	boolean update() override {

		if (!power.getNewVal()) {
			pixel->set(Pixel::Color().RGB(0, 0, 0, 0), this->nPixels);
		} else {
			Effects[effect.getNewVal() - 1]->init();
			alarmTime = millis() + Effects[effect.getNewVal() - 1]->update();
			if (effect.updated())
				Serial.printf("Effect changed to: %s\n", Effects[effect.getNewVal() - 1]->name);
		}
		if (relay_enabled.updated()) {
			int relay_status = relay_enabled.getNewVal();
			if (relay_enabled.getNewVal()) {
				LOG0("Relay Enabled\n");
				addSwitch();
				homeSpan.updateDatabase();
				LOG0("Accessories Database updated.  New configuration number broadcasted...\n");
			} else {
				LOG0("Relay Disabled\n");
				homeSpan.deleteAccessory(2);
				homeSpan.updateDatabase();
				LOG0("Nothing to update - no changes were made!\n");
			}
		}

		return (true);
	}

	void loop() override {

		if (millis() > alarmTime && power.getVal())
			alarmTime = millis() + Effects[effect.getNewVal() - 1]->update();
	}

	//////////////

	struct ManualControl : SpecialEffect {

		ManualControl(Pixel_Strand *px) : SpecialEffect{px, "Manual Control"} {}

		void init() override { px->pixel->set(Pixel::Color().HSV(px->H.getNewVal<float>(), px->S.getNewVal<float>(), px->V.getNewVal<float>()), px->nPixels); }
	};

	//////////////
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
			float value = px->V.getNewVal<float>();
			for (int i = 0; i < px->nPixels; i++) {
				px->colors[i] = Pixel::Color().HSV(angle, 100, value);
			}
			px->pixel->set(px->colors, px->nPixels);
			if (angle++ == 360) angle = 0;
			int speed = px->anim_speed.getVal();
			return (200 / speed);
		}

		int requiredBuffer() override { return (px->nPixels); }
	};

	///////////////////////////////
	struct Colorwheel : SpecialEffect {

		int8_t *dir;

		Colorwheel(Pixel_Strand *px) : SpecialEffect{px, "Colorwheel"} {
			dir = (int8_t *)calloc(px->nPixels, sizeof(int8_t));
		}

		void init() override {
			for (int i = 0; i < px->nPixels; i++) {
				px->colors[i].RGB(0, 0, 0, 0);
				dir[i] = 0;
			}
		}

		uint32_t update() override {
			float value = px->V.getNewVal<float>();
			for (int i = 0; i < px->nPixels; i++) {
				int h		  = (i * (MAXHUE / px->nPixels) + count_wheel) % MAXHUE;
				px->colors[i] = Pixel::Color().HSV(h, 100, value);
			}
			px->pixel->set(px->colors, px->nPixels);
			if (count_wheel++ == MAXHUE) count_wheel = 0;
			int speed = px->anim_speed.getVal();
			return (200 / speed);
		}

		int requiredBuffer() override { return (px->nPixels); }
	};
	///////////////////////////////
};

struct DEV_Switch : Service::Switch {

	int					ledPin; // relay pin
	SpanCharacteristic *power;

	// Constructor
	DEV_Switch(int ledPin) : Service::Switch() {
		power		 = new Characteristic::On(0, true);
		this->ledPin = ledPin;
		pinMode(ledPin, OUTPUT);

		digitalWrite(ledPin, power->getVal());
	}

	// Override update method
	boolean update() {
		digitalWrite(ledPin, power->getNewVal());

		return (true);
	}
};

///////////////////////////////

Pixel_Strand *STRIP;

void setup() {

	Serial.begin(115200);

	Serial.print("Active firmware version: ");
	Serial.println(FirmwareVer);

	String	   temp			  = FW_VERSION;
	const char compile_date[] = __DATE__ " " __TIME__;
	char	  *fw_ver		  = new char[temp.length() + 30];
	strcpy(fw_ver, temp.c_str());
	strcat(fw_ver, " (");
	strcat(fw_ver, compile_date);
	strcat(fw_ver, ")");

	for (int i = 0; i < 17; ++i) // we will iterate through each character in WiFi.macAddress() and copy it to the global char sNumber[]
	{
		sNumber[i] = WiFi.macAddress()[i];
	}
	sNumber[17] = '\0'; // the last charater needs to be a null

	homeSpan.setLogLevel(0);										// set log level to 0 (no logs)
	homeSpan.setStatusPin(32);										// set the status pin to GPIO32
	homeSpan.setStatusAutoOff(10);									// disable led after 10 seconds
	homeSpan.setWifiCallback(setupWeb);								// Set the callback function for wifi events
	homeSpan.reserveSocketConnections(5);							// reserve 5 socket connections for Web Server
	homeSpan.setControlPin(0);										// set the control pin to GPIO0
	homeSpan.setPortNum(88);										// set the port number to 81
	homeSpan.enableAutoStartAP();									// enable auto start of AP
	homeSpan.enableWebLog(10, "pool.ntp.org", "UTC-2:00", "myLog"); // enable Web Log
	homeSpan.setSketchVersion(fw_ver);

	homeSpan.begin(Category::Lighting, "Holiday Lights");

	new SpanAccessory(1);
	new Service::AccessoryInformation();
	new Characteristic::Name("Holiday Lights");
	new Characteristic::Manufacturer("HomeSpan");
	new Characteristic::SerialNumber(sNumber);
	new Characteristic::Model("NeoPixel RGB LEDs");
	new Characteristic::FirmwareRevision(temp.c_str());
	new Characteristic::Identify();

	new Service::HAPProtocolInformation();
	new Characteristic::Version("1.1.0");

	STRIP = new Pixel_Strand(NEOPIXEL_RGBW_PIN);

	if (STRIP->relay_enabled.getVal()) {
		addSwitch();
	}
}

///////////////////////////////

void loop() {
	homeSpan.poll();
	server.handleClient();
	repeatedCall();
}

///////////////////////////////

void setupWeb() {

	server.on("/metrics", HTTP_GET, []() {
		double uptime		= esp_timer_get_time() / (6 * 10e6);
		double heap			= esp_get_free_heap_size();
		String uptimeMetric = "# HELP uptime LED Strip uptime\nhomekit_uptime{device=\"led_strip\",location=\"home\"} " + String(int(uptime));
		String heapMetric	= "# HELP heap Available heap memory\nhomekit_heap{device=\"led_strip\",location=\"home\"} " + String(int(heap));

		Serial.println(uptimeMetric);
		Serial.println(heapMetric);
		server.send(200, "text/plain", uptimeMetric + "\n" + heapMetric);
	});

	server.on("/reboot", HTTP_GET, []() {
		String content = "<html><body>Rebooting!  Will return to configuration page in 10 seconds.<br><br>";
		content += "<meta http-equiv = \"refresh\" content = \"10; url = /\" />";
		server.send(200, "text/html", content);

		ESP.restart();
	});

	ElegantOTA.begin(&server); // Start ElegantOTA
	server.begin();
	LOG1("HTTP server started");
} // setupWeb

void addSwitch() {

	LOG0("Adding Accessory: Switch\n");

	new SpanAccessory(2);
	new Service::AccessoryInformation();
	new Characteristic::Name("Switch");
	new Characteristic::Identify();
	new DEV_Switch(18);
}