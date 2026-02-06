![Weather Crow](weatherCrow.jpg)

# weather-crow5.7
Weather station using [CrowPanel ESP32 E-Paper HMI 5.79-inch Display](https://www.elecrow.com/crowpanel-esp32-5-79-e-paper-hmi-display-with-272-792-resolution-black-white-color-driven-by-spi-interface.html)

This code based on the CrowPanel ESP32 E-Paper HMI 5.79-inch Display [example](https://www.elecrow.com/wiki/CrowPanel_ESP32_E-paper_5.79-inch_HMI_Display.html). I've added and modified the code to display custom weather icons and font rendering.
It is a weather station that displays the current weather and forecast for the next 5 to 25 hours depending on the configuration.
The weather data is fetched from the [OpenWeatherMap](https://openweathermap.org/) API.

# What you need
- Hardware : [CrowPanel ESP32 E-Paper HMI 5.79-inch Display](https://www.elecrow.com/crowpanel-esp32-5-79-e-paper-hmi-display-with-272-792-resolution-black-white-color-driven-by-spi-interface.html)
- API Key : [OpenWeatherMap API key](https://openweathermap.org/)
- Software : [PlatformIO](https://platformio.org/) (CLI or VS Code extension)

# Setup

1. **Install PlatformIO**
   - Install via [VS Code extension](https://platformio.org/install/ide?install=vscode) (recommended), or
   - Install via Homebrew: `brew install platformio`, or
   - Install via pip: `pip install platformio`

2. **Clone the repository**
   ```bash
   git clone https://github.com/your-username/weather-crow5.7.git
   cd weather-crow5.7
   ```

3. **Configure settings**
   - Copy `src/config.example.h` to `src/config.h`
   - Update the values in `config.h` with your WiFi credentials and OpenWeatherMap API key

4. **Build the project**
   ```bash
   pio run
   ```

5. **Upload to device**
   - Connect the CrowPanel ESP32 E-Paper HMI 5.79-inch Display to your computer
   ```bash
   pio run --target upload
   ```

6. **Monitor serial output** (optional)
   ```bash
   pio device monitor
   ```

## Useful Commands

| Command | Description |
|---------|-------------|
| `pio run` | Build the project |
| `pio run --target upload` | Build and upload to device |
| `pio run --target clean` | Clean build artifacts |
| `pio device monitor` | Open serial monitor (115200 baud) |


# Credits
- Weather data:
  - Weather data provided by [OpenWeather](https://openweathermap.org/)

- Icons:
  - [Weather Icons](https://erikflowers.github.io/weather-icons/)
  Weather Icons licensed under SIL OFL 1.1
  The Weather Icons project created and maintained by Erik Flowers. v1.0 artwork by Lukas Bischoff. v1.1 - 2.0 artwork by Erik Flowers


- 8px font:
Copyright (c) YUJI OSHIMOTO.
  - [04b-03](http://www.04.jp.org/)

- Poppins:
This project uses the "Poppins" font, licensed under the SIL Open Font License, Version 1.1.
Copyright (c) Indian Type Foundry.
  - [Poppins](https://fonts.google.com/specimen/Poppins)
  - [Poppins License](https://fonts.google.com/specimen/Poppins/license)
