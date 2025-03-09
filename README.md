# weather-crow5.7
Weather station using [CrowPanel ESP32 E-Paper HMI 5.79-inch Display](https://www.elecrow.com/crowpanel-esp32-5-79-e-paper-hmi-display-with-272-792-resolution-black-white-color-driven-by-spi-interface.html)

This code based on the CrowPanel ESP32 E-Paper HMI 5.79-inch Display example.
It is a weather station that displays the current weather and forecast for the next 3 days. The weather data is fetched from the OpenWeatherMap API.

# What you need
- [CrowPanel ESP32 E-Paper HMI 5.79-inch Display](https://www.elecrow.com/crowpanel-esp32-5-79-e-paper-hmi-display-with-272-792-resolution-black-white-color-driven-by-spi-interface.html)
- [OpenWeatherMap API key](https://openweathermap.org/)
- [Arduino IDE](https://www.arduino.cc/en/software)

# Setup the environment
1. Install the ESP32 board in Arduino IDE
   - Open Arduino IDE
   - Go to File > Preferences
   - In the Additional Boards Manager URLs field, add the following URL:
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
     ```
   - Click OK
   - Go to Tools > Board > Boards Manager
   - Search for "esp32" and install the board

2. Install the required libraries
    - Open Arduino IDE
    - Go to Sketch > Include Library > Manage Libraries
    - Search for and install the following libraries:
      - Arduino_Json

3. Download the code
    - Clone this repo or download the code as a zip file
    - Open the `weather-crow5.7.ino` file in Arduino IDE

4. Configure the code
    Copy config.example.h to config.h and update the defined values.

I will update here...hopefully soon.


# Note
While the code using JSONVar which is a JSON library to parse weather data.
if you have any issue with the JSONVar library, you can use the ArduinoJson library instead.
