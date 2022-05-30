# esp32-homekit-led-strip
Native Apple HomeKit via an ESP32 for the NeoPixel LED Strip or any 5V LED Strip

Software is based on [HomeSpan](https://github.com/HomeSpan/HomeSpan) library. Supports RGB or RGBW Neopixel strips and optionally can act like a switch for any 5V electronics (Switch accessory in HomeKit can be shown as a Bulb, Socket or Fan).

_As usual, don't expect any warranties. I am just a hobbyist, not a professional. It works for me, but you take your own risk using it. I am not liable for any damage to your devices._

## Hardware

![render](./images/render.png)

Custom PCB is designed for the project. Currently for regular ESP32 board, however there are plans to do for ESP32-C3, which has smaller footprint.

## Software

Here you can see, which pins are used and pre-defined in the firmware:
```c++
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
```

The firmware can be built and flashed using the Arduino IDE.

For this, you will need to add ESP32 support to it.

Furthermore, you will also need to install the following libraries using the Library Manager:

* HomeSpan

And some libraries manually:

1. Go to this GitHub repo and download it as a ZIP - [AsyncElegantOTA](https://github.com/ayushsharma82/AsyncElegantOTA)
2. In Arduino IDE select "Sketch" -> "Include Library" and "Add .ZIP Library..." and select downloaded ZIP
3. Do previous steps to the following libraries: 
   * [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
   * [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
4. Download and open this repository in Arduino IDE (or VSCode with Arduino extension)
5. Set the upload speed to 115200
6. Build, flash, and you're done

Instead of Arduino IDE OTA, the webserver update was implemented. You can flash binary at `http://[DEVICE IP]/update`.
There is a reboot link. Opening `http://[DEVICE IP]/reboot` will force the device to reboot. 

The device can also be controlled by the FLASH button on the board. More on [HomeSpan docs](https://github.com/HomeSpan/HomeSpan/blob/master/docs/UserGuide.md)

## Connect to HomeKit

1. Plug your LED Controller to power.
2. Press the FLASH button until the LED starts blinking rapidly and release it. Now it is in the configuration mode (More on [HomeSpan docs](https://github.com/HomeSpan/HomeSpan/blob/master/docs/UserGuide.md)). Press it two more times until the LED starts blinking 3 times in a row. This means mode 3 is chosen. Hold the button for 3-4 seconds once again and the WiFi AP will be started.
3. Go to WiFi settings on your iPhone/iPad and connect to the "HomeSpan-Setup" WiFi network.
4. You will choose your WiFi network and set the setup code for the accessory. 
5. Go to your Home app and select "Add Accessory"
6. Select "More Options" and you should see your LED Controller there.

## References and sources

- @HomeSpan for ESP32 HomeKit firmware [GitHub link](https://github.com/HomeSpan/HomeSpan)
