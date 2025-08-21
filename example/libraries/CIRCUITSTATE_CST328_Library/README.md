
![CSE_CST328](https://socialify.git.ci/CIRCUITSTATE/CSE_CST328/image?description=1&font=KoHo&forks=1&issues=1&logo=https%3A%2F%2Fwww.circuitstate.com%2Fwp-content%2Fuploads%2F2024%2F05%2FCIRCUITSTATE-R-Emblem-20052024-2.svg&name=1&pattern=Circuit%20Board&pulls=1&stargazers=1&theme=Auto)

# CSE_CST328

**CSE_CST328** is an Arduino library from [*CIRCUITSTATE Electronics*](https://www.circuitstate.com/). It helps you interface the [**CST328**](https://www.cdtech-display.com/wp-content/uploads/2024/07/CST328-DataSheet-V2.2.pdf) touch controller with your Arduino boards. CST328 can be bought as a standalone touch panel or come integrated with LCD screens like the [**Waveshare 2.8" Capacitive Touch LCD**](https://www.waveshare.com/wiki/2.8inch_Capacitive_Touch_LCD). The library uses I2C to communicate with the CST328.

## Installation

This library is available from the official **Arduino Library Manager**. Open the Arduino IDE, search for "CSE_CST328" and install the latest version of the library.

Additionally, you can download the latest release package from the GitHub repository and install it manually. To do so, open the Arduino IDE, go to `Sketch > Include Library > Add .ZIP Library…` and select the downloaded file.

Another method is to clone the GitHub repository directly into your `libraries` folder. The development branch will have the latest features, bug fixes and other changes. To do so, navigate to your `libraries` folder (usually located at `Documents/Arduino/libraries` on Windows and `~/Documents/Arduino/libraries` on macOS) and execute the following command:

```
git clone https://github.com/CIRCUITSTATE/CSE_CST328.git
```

[**Git**](https://git-scm.com) should be installed on your computer.

The library can also be installed via [**PlatformIO**](https://platformio.org). All officially listed Arduino listed libraries are automatically fetched by PlatformIO. Use the [`lib_deps`](https://docs.platformio.org/en/latest/projectconf/sections/env/options/library/lib_deps.html) search option to install the library.

## Dependencies

This library does not depend on any other libraries other than the standard Arduino libraries including [**Wire**](https://github.com/arduino-libraries/Wire).

## Example

Find the examples in the [examples](examples) folder.

  - [**Read-Touch-Polling**](examples/Read-Touch-Polling/Read-Touch-Polling.ino)
  - [**Fast-Read-Touch-Polling**](examples/Fast-Read-Touch-Polling/Fast-Read-Touch-Polling.ino)
  - [**Read-Touch-Interrupt**](examples/Read-Touch-Interrupt/Read-Touch-Interrupt.ino)

## API Reference

Please see the [API.md](/docs/API.md) file for the API reference.

## References

-  [**CSE_UI - CIRCUITSTATE GitHub**](https://github.com/CIRCUITSTATE/CSE_UI) - An Arduino GUI library for common TFT/IPS screens.
-  [**Adafruit_FT6206 Library**](https://github.com/adafruit/Adafruit_FT6206_Library) - Where this library took inspiration from.
-  [**TouchLib**](https://github.com/mmMicky/TouchLib/tree/main) - An alternate library for the CST328.
-  [**Waveshare 2.8" Capacitive Touch LCD**](https://www.waveshare.com/wiki/2.8inch_Capacitive_Touch_LCD)
