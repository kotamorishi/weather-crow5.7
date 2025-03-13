#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "EPD.h"
#include "weatherIcons.h"
#include <esp_sleep.h>

// Communication and data constants
#define BAUD_RATE 115200
#define HTTP_NOT_REQUESTED_YET -999
#define STRING_BUFFER_SIZE 256
#define JSON_CAPACITY 32768

// Display and UI constants
#define TEMPERATURE_GRAPH_LINE_WIDTH 2
#define UVI_DISPLAY_THRESHOLD 4.0
#define WIND_SPEED_DISPLAY_THRESHOLD 4.0

class WeatherCrow
{
private:
  // Display buffer
  uint8_t imageBW[27200];

  // Network and API configuration
  const char *ssid = WIFI_SSID;
  const char *password = WIFI_PASSWORD;
  String openWeatherMapApiKey = OPEN_WEATHER_MAP_API_KEY;
  String apiParamLatitude = LATITUDE;
  String apiParamLongitude = LONGITUDE;

  // Data buffers
  String jsonBuffer;
  String errorMessageBuffer;
  int httpResponseCode = HTTP_NOT_REQUESTED_YET;
  DynamicJsonDocument weatherApiResponse = DynamicJsonDocument(JSON_CAPACITY);

  /**
   * Weather information structure to store processed data from API
   */
  struct WeatherInfo
  {
    String weather;
    long currentDateTime;
    long sunrise;
    long sunset;
    String temperature;
    String tempIntegerPart;
    String tempDecimalPart;
    String humidity;
    String pressure;
    String windSpeed;
    String timezone;
    String icon;
  } weatherInfo;

  // Logging helper methods
  void logPrint(const char *msg) { Serial.print(msg); }
  void logPrint(int msg) { Serial.print(msg); }
  void logPrint(String msg) { Serial.print(msg); }

  void logPrintln(const char *msg) { Serial.println(msg); }
  void logPrintln(int msg) { Serial.println(msg); }
  void logPrintln(String msg) { Serial.println(msg); }

  /**
   * Performs HTTPS GET request to the specified server
   */
  String httpsGETRequest(const char *serverName)
  {
    WiFiClientSecure client;
    HTTPClient http;
    client.setInsecure();
    http.begin(client, serverName);
    httpResponseCode = http.GET();
    String payload = "{}";
    if (httpResponseCode > 0)
    {
      payload = http.getString();
    }
    http.end();
    return payload;
  }

  /**
   * Connects to WiFi network using configured credentials
   */
  void connectToWiFi()
  {
    WiFi.begin(ssid, password);
    logPrintln("WiFi Connecting");
    unsigned long startAttemptTime = millis();
    const unsigned long WIFI_TIMEOUT_MS = 180000; // 3 minutes timeout

    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < WIFI_TIMEOUT_MS)
    {
      delay(500);
      logPrint(".");
    }

    if (WiFi.status() != WL_CONNECTED)
    {
      logPrintln("Failed to connect to WiFi. Retrying...");
      errorMessageBuffer = "Failed to connect to the WiFi network.";
    }
    logPrintln("");
  }

  void screenPowerOn()
  {
    const int SCREEN_POWER_PIN = 7;
    pinMode(SCREEN_POWER_PIN, OUTPUT);
    digitalWrite(SCREEN_POWER_PIN, HIGH);
  }

  void clearScreen()
  {
    Paint_NewImage(imageBW, EPD_W, EPD_H, Rotation, WHITE);
    Paint_Clear(WHITE);
    EPD_FastMode1Init();
    EPD_Display_Clear();
    EPD_Update();
    EPD_Clear_R26A6H();
  }

  /**
   * Displays system information on the screen
   */
  void displaySystemInfo(uint16_t baseX, uint16_t baseY)
  {
    uint16_t x = baseX;
    uint16_t y = baseY;
    uint16_t yOffset = 8;
    char buffer[STRING_BUFFER_SIZE];

    // Display WiFi SSID
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "WIFI SSID: %s", String(WIFI_SSID));
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y += yOffset;

    // Display WiFi connection status
    memset(buffer, 0, sizeof(buffer));
    if (WL_CONNECTED == WiFi.status())
    {
      snprintf(buffer, sizeof(buffer), "DEVICE IS CONNECTED TO WIFI. IP: %s", WiFi.localIP().toString().c_str());
    }
    else
    {
      snprintf(buffer, sizeof(buffer), "WIFI NOT CONNECTED. CHECK WIFI SSID AND CREDENTIALS. ROUTER REBOOT MAY HELP.");
    }
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y += yOffset;

    // Display location coordinates
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "LOCATION LATITUDE: %s, LONGITUDE: %s", String(LATITUDE), String(LONGITUDE));
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y += yOffset;

    // Display latest API call time
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "LATEST SUCCESSFUL API CALL: %s", convertUnixTimeToDateTimeString(weatherInfo.currentDateTime).c_str());
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
  }

  /**
   * Converts Unix timestamp to formatted date-time string
   */
  String convertUnixTimeToDateTimeString(long unixTime)
  {
    if (unixTime == 0)
    {
      return "N/A";
    }

    time_t rawtime = unixTime;
    struct tm *dt;
    char buffer[30];
    dt = gmtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", dt);
    return String(buffer);
  }

  /**
   * Converts Unix timestamp to short time format (hour with AM/PM)
   */
  String convertUnixTimeToShortDateTimeString(long unixTime)
  {
    if (unixTime == 0)
    {
      return "N/A";
    }

    time_t rawtime = unixTime;
    struct tm *dt;
    char buffer[30];
    dt = gmtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%I %p", dt);

    // Remove leading zero from hour
    if (buffer[0] == '0')
    {
      memmove(buffer, buffer + 1, strlen(buffer));
    }
    return String(buffer);
  }

  /**
   * Converts Unix timestamp to display format for date
   */
  String convertUnixTimeToDisplayFormat(long unixTime)
  {
    if (unixTime == 0)
    {
      return "N/A";
    }

    time_t rawtime = unixTime;
    struct tm *dt;
    char buffer[30];
    dt = gmtime(&rawtime);
    strftime(buffer, sizeof(buffer), "%b %d, %a", dt);
    return String(buffer);
  }

  /**
   * Displays an error message on the screen
   */
  void displayErrorMessage(char *title, char *description)
  {
    clearScreen();

    uint16_t baseXpos = 258;
    char buffer[STRING_BUFFER_SIZE];

    // Display error icon
    EPD_drawImage(30, 30, leo_face_lg);
    EPD_DrawLine(baseXpos, 110, 740, 110, BLACK);

    // Display error title
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", title);
    EPD_ShowString(baseXpos, 70, buffer, FONT_SIZE_36, BLACK);

    // Display error description
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", description);
    EPD_ShowString(baseXpos, 140, buffer, FONT_SIZE_16, BLACK);

    // Display system information
    displaySystemInfo(baseXpos, 230);

    // Update display and enter deep sleep
    EPD_Display(imageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  /**
   * Constructs API endpoint URL and returns it
   */
  String constructApiEndpointUrl()
  {
    return "https://api.openweathermap.org/data/3.0/onecall?lat=" +
           apiParamLatitude + "&lon=" + apiParamLongitude +
           "&exclude=minutely" +
           "&APPID=" + openWeatherMapApiKey +
           "&units=" + UNITS;
  }

  /**
   * Processes the weather data from the API response
   */
  bool processWeatherData()
  {
    // Extract and process weather data
    weatherInfo.weather = weatherApiResponse["current"]["weather"][0]["main"].as<String>();
    weatherInfo.icon = weatherApiResponse["current"]["weather"][0]["icon"].as<String>();
    weatherInfo.currentDateTime = weatherApiResponse["current"]["dt"].as<long>() + weatherApiResponse["timezone_offset"].as<long>();
    weatherInfo.sunrise = weatherApiResponse["current"]["sunrise"].as<long>();
    weatherInfo.sunset = weatherApiResponse["current"]["sunset"].as<long>();
    weatherInfo.temperature = String(weatherApiResponse["current"]["temp"].as<float>(), 1);
    weatherInfo.humidity = String(weatherApiResponse["current"]["humidity"].as<int>());
    weatherInfo.pressure = String(weatherApiResponse["current"]["pressure"].as<int>());
    weatherInfo.windSpeed = String(weatherApiResponse["current"]["wind_speed"].as<float>(), 1);
    weatherInfo.timezone = weatherApiResponse["timezone"].as<String>();

    // Process temperature for display
    int decimalPos = weatherInfo.temperature.indexOf('.');
    weatherInfo.tempIntegerPart = String(weatherInfo.temperature.substring(0, decimalPos).toInt());
    weatherInfo.tempDecimalPart = String(weatherInfo.temperature.substring(decimalPos + 1).toInt());
    if (weatherInfo.tempDecimalPart.length() > 1)
    {
      weatherInfo.tempDecimalPart = weatherInfo.tempDecimalPart.substring(0, 1);
    }

    return true;
  }

  /**
   * Fetches weather information from OpenWeatherMap API
   */
  bool getWeatherInfo(uint8_t retryCount = 0)
  {
    httpResponseCode = HTTP_NOT_REQUESTED_YET;
    if (WiFi.status() != WL_CONNECTED)
    {
      errorMessageBuffer = "Wireless network not established.";
      return false;
    }

    // Construct API endpoint URL and make request
    String apiEndPoint = constructApiEndpointUrl();
    jsonBuffer = httpsGETRequest(apiEndPoint.c_str());

    // Wait for response to complete
    while (httpResponseCode < 0)
    {
      delay(100);
    }

    logPrint("HTTP response code: ");
    logPrintln(httpResponseCode);

    // Check for HTTP errors
    if (httpResponseCode != HTTP_CODE_OK)
    {
      errorMessageBuffer = "Weather API failed with HTTP code: " + String(httpResponseCode) +
                           "\n\nThis typically happens due to a weather API server issue or an invalid API key.";
      return false;
    }

    // Parse JSON response
    DeserializationError error = deserializeJson(weatherApiResponse, jsonBuffer);
    if (error)
    {
      logPrint(F("deserializeJson() failed: "));
      logPrintln(error.c_str());
      errorMessageBuffer = "JSON parsing error: " + String(error.c_str());
      return false;
    }

    return processWeatherData();
  }

  void drawForecastItem(uint16_t x, uint16_t y, JsonObject hourlyData)
  {
    char buffer[STRING_BUFFER_SIZE];

    String icon;
    if (hourlyData["weather"][0].containsKey("icon"))
    {
      icon = "icon_" + hourlyData["weather"][0]["icon"].as<String>();
    }
    else
    {
      icon = "na_md"; // default fallback icon
    }

    snprintf(buffer, sizeof(buffer), "%s_sm", icon.c_str());
    if (icon_map.count(buffer) > 0)
    {
      EPD_drawImage(x, y, icon_map[buffer]);
    }
    else
    {
      EPD_drawImage(x, y, icon_map["leo_face_sm"]);
    }

    // Time formatting (10 AM, 9 PM, etc.)
    long forecastTimeInt = hourlyData["dt"].as<long>() + weatherApiResponse["timezone_offset"].as<long>();
    String forecastTime = convertUnixTimeToShortDateTimeString(forecastTimeInt);

    int splitIndex = forecastTime.indexOf(' ');
    String hour = forecastTime.substring(0, splitIndex);
    String period = forecastTime.substring(splitIndex + 1);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", hour.c_str());
    EPD_ShowStringRightAligned(x + 24, y + 80, buffer, FONT_SIZE_16, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", period.c_str());
    EPD_ShowStringRightAligned(x + 26, y + 96, buffer, FONT_SIZE_16, BLACK);

    // Vertical separator line
    uint16_t lineLength = 103;
    EPD_DrawLine(x + 35, y + 70, x + 35, y + lineLength, BLACK);
    EPD_DrawLine(x + 36, y + 70, x + 36, y + lineLength, BLACK);
    EPD_DrawLine(x + 37, y + 70, x + 37, y + lineLength, BLACK);

    // Temperature
    memset(buffer, 0, sizeof(buffer));
    float temp = hourlyData["temp"].as<float>();
    int tempInt = (int)temp;
    snprintf(buffer, sizeof(buffer), "%d", tempInt);
    EPD_ShowString(x + 46, y + 87, buffer, FONT_SIZE_38, BLACK, true);
  }

  void drawWeatherFutureForecast(uint16_t baseX, uint16_t baseY, uint16_t length)
  {
    uint16_t x = baseX;
    uint16_t y = baseY;
    char buffer[STRING_BUFFER_SIZE];

    // Check if hourly data exists
    if (!weatherApiResponse.containsKey("hourly"))
    {
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", "Error: API not responding with hourly data.");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    // Get the hourly array length
    size_t availableHours = weatherApiResponse["hourly"].size();
    if (availableHours == 0)
    {
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", "Error: API responding with ZERO forecast data.");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    // Limit to either requested length or available data
    int forecastsToShow = min(length, (uint16_t)availableHours);

    for (uint16_t i = 1, counter = 0; counter < forecastsToShow; i += HOUR_INTERVAL, counter++)
    {
      // Safely check if the hourly entry exists
      if (i >= availableHours)
      {
        Serial.println("Index out of bounds for hourly forecast");
        break;
      }

      JsonObject hourly = weatherApiResponse["hourly"][i];

      if (!hourly.containsKey("weather") || hourly["weather"].size() == 0)
      {
        Serial.println("No weather data for this hour");
        continue;
      }

      drawForecastItem(x, y, hourly);

      // Offset for the next forecast item
      x = x + 105;
    }
  }

  void prepareGraphData(float *temps, float &minTemp, float &maxTemp, int &hoursToShow, const size_t availableHours)
  {
    // Determine how many hours to show based on available space
    hoursToShow = min((int)24, (int)availableHours);

    minTemp = 100.0;
    maxTemp = -100.0;

    // Find min and max temperatures for scaling
    for (int i = 0; i < hoursToShow; i++)
    {
      if (i >= availableHours)
        break;

      temps[i] = weatherApiResponse["hourly"][i]["temp"].as<float>();

      if (temps[i] < minTemp)
        minTemp = temps[i];
      if (temps[i] > maxTemp)
        maxTemp = temps[i];
    }

    // Add padding to min/max for visual appeal
    float tempRange = maxTemp - minTemp;
    if (tempRange < 5.0)
    {
      // If range is too small, expand it
      float padding = (5.0 - tempRange) / 2;
      minTemp -= padding;
      maxTemp += padding;
    }
    else
    {
      // Add 10% padding on top and bottom
      minTemp -= tempRange * 0.1;
      maxTemp += tempRange * 0.1;
    }
  }

  void drawGraph(uint16_t baseX, uint16_t baseY, uint16_t graphWidth, uint16_t graphHeight, uint8_t lineWidth = 1)
  {
    uint16_t x = baseX;
    uint16_t y = baseY;
    char buffer[STRING_BUFFER_SIZE];

    // Check if hourly data exists
    if (!weatherApiResponse.containsKey("hourly"))
    {
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", "Error: No hourly data available");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    // Get the hourly array length
    size_t availableHours = weatherApiResponse["hourly"].size();
    if (availableHours < 2)
    {
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", "Error: Insufficient forecast data");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    // Prepare data for graph
    float temps[24]; // Maximum 24 hours
    float minTemp, maxTemp;
    int hoursToShow;

    prepareGraphData(temps, minTemp, maxTemp, hoursToShow, availableHours);

    // Calculate scaling factor and point spacing
    float tempRange = maxTemp - minTemp;
    float scale = graphHeight / tempRange;
    uint16_t pointSpacing = graphWidth / (hoursToShow - 1);

    // Draw temperature points and connecting lines
    for (int i = 1; i < hoursToShow; i++)
    {
      // Calculate positions
      uint16_t x1 = x + (i - 1) * pointSpacing;
      uint16_t y1 = y + graphHeight - (uint16_t)((temps[i - 1] - minTemp) * scale);
      uint16_t x2 = x + i * pointSpacing;
      uint16_t y2 = y + graphHeight - (uint16_t)((temps[i] - minTemp) * scale);

      // Draw thicker line by drawing multiple lines if lineWidth > 1
      for (int w = 0; w < lineWidth; w++)
      {
        EPD_DrawLine(x1, y1 + w, x2, y2 + w, BLACK);
      }

      // Draw dots at data points
      EPD_DrawCircle(x1, y1, 2, BLACK, true);
      if (i == hoursToShow - 1)
      {
        EPD_DrawCircle(x2, y2, 2, BLACK, true);
      }

      // Label every 6 hours with time
      if (i % 6 == 0 || i == 1)
      {
        long time = weatherApiResponse["hourly"][i]["dt"].as<long>() +
                    weatherApiResponse["timezone_offset"].as<long>();
        String timeStr = convertUnixTimeToShortDateTimeString(time);

        memset(buffer, 0, sizeof(buffer));
        snprintf(buffer, sizeof(buffer), "%s", timeStr.c_str());
        EPD_ShowString(x2 - 10, y + graphHeight + 5, buffer, FONT_SIZE_8, BLACK, true);
      }
    }

    // Draw temperature scale on right side
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%.1f°", maxTemp);
    EPD_ShowString(x + graphWidth + 5, y, buffer, FONT_SIZE_8, BLACK, true);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%.1f°", minTemp);
    EPD_ShowString(x + graphWidth + 5, y + graphHeight - 10, buffer, FONT_SIZE_8, BLACK, true);
  }

  void displayCurrentInfo(uint16_t baseX, uint16_t baseY)
  {
    uint16_t centerX = baseX;
    uint16_t y = baseY;
    uint16_t unitOffsetX = 4;
    uint16_t unitOffsetY = 5;

    char buffer[STRING_BUFFER_SIZE];

    // Pressure
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.pressure.c_str());
    EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "hPa");
    EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);

    // Humidity
    y += 40;
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.humidity.c_str());
    EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%%RH");
    EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);

    // Snow, rain, UVI, or wind speed
    y += 40;
    displayAdditionalInfoLine(centerX, y, unitOffsetX, unitOffsetY);
  }

  void displayAdditionalInfoLine(uint16_t centerX, uint16_t y, uint16_t unitOffsetX, uint16_t unitOffsetY)
  {
    char buffer[STRING_BUFFER_SIZE];

    if ((weatherApiResponse["current"].containsKey("snow")) &&
        (weatherApiResponse["current"]["snow"].containsKey("1h")))
    {
      // Snow
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["snow"]["1h"].as<String>().c_str());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "mm");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if ((weatherApiResponse["current"].containsKey("rain")) &&
             (weatherApiResponse["current"]["rain"].containsKey("1h")))
    {
      // Rain
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["rain"]["1h"].as<String>().c_str());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "mm");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if ((weatherApiResponse["current"].containsKey("uvi")) &&
             (weatherApiResponse["current"]["uvi"].as<float>() > UVI_DISPLAY_THRESHOLD))
    {
      // UVI
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["uvi"].as<String>().c_str());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "UVI");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if (weatherInfo.windSpeed.toFloat() > WIND_SPEED_DISPLAY_THRESHOLD)
    {
      // Wind speed
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["wind_speed"].as<String>().c_str());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "m/s");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
  }

  void displayWeatherForecast()
  {
    clearScreen();
    char buffer[STRING_BUFFER_SIZE];

    // Display date (format: "Jan 01, Fri")
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", convertUnixTimeToDisplayFormat(weatherInfo.currentDateTime).c_str());
    EPD_ShowStringRightAligned(790, 25, buffer, FONT_SIZE_36, BLACK);

    // Display temperature
    uint16_t yPos = 105;
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.tempIntegerPart.c_str());
    EPD_ShowStringRightAligned(740, yPos, buffer, FONT_SIZE_92, BLACK);

    // Display temperature unit
    memset(buffer, 0, sizeof(buffer));
    if (UNITS == "metric")
    {
      snprintf(buffer, sizeof(buffer), "C");
      EPD_drawImage(740, yPos, degrees_sm);
    }
    else
    {
      snprintf(buffer, sizeof(buffer), "F");
    }
    EPD_ShowString(750, yPos + 24, buffer, FONT_SIZE_36, BLACK);

    // Display weather icon
    String iconName = "icon_" + weatherInfo.icon + "_lg";
    EPD_drawImage(10, 1, icon_map[iconName.c_str()]);

    // Display current info (pressure, humidity, etc.)
    displayCurrentInfo(360, 30);

    // Display future forecast
    drawWeatherFutureForecast(260, 160, 5);

    // Update display
    EPD_Display(imageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  void displayError()
  {
    char title[] = "Something went wrong.";
    char description[STRING_BUFFER_SIZE];
    memset(description, 0, sizeof(description));
    strncpy(description, errorMessageBuffer.c_str(), sizeof(description) - 1);
    displayErrorMessage(title, description);
  }

public:
  void begin()
  {
    errorMessageBuffer = "";
    delay(1000);
    Serial.begin(BAUD_RATE);
    screenPowerOn();
    EPD_GPIOInit();
  }

  bool run()
  {
    try
    {
      connectToWiFi();
      if (!getWeatherInfo())
      {
        char title[] = "Weather API failed.";
        char msg[256];
        memset(msg, 0, sizeof(msg));
        strncpy(msg, errorMessageBuffer.c_str(), sizeof(msg) - 1);
        displayErrorMessage(title, msg);
        return false;
      }
      else
      {
        displayWeatherForecast();
        return true;
      }
    }
    catch (const std::exception &e)
    {
      errorMessageBuffer = String("Exception: ") + e.what();
      displayError();
    }
    // never reach here
    return false;
  }
};

WeatherCrow weatherCrow;

void setup()
{
  weatherCrow.begin();
}

void loop()
{
  // This delay is not necessary, however if the consecutive reboot happens due to error,
  // e-paper keep refreshing and it is reducing the life of the display.
  // it is better to have 30+ mins for real production.
  delay(3000);

  if (weatherCrow.run() == true)
  {
    // success wait for the next refresh
    esp_sleep_enable_timer_wakeup(1000000ULL * 60ULL * REFRESH_MINUITES);
    esp_deep_sleep_start();
  }
  else
  {
    // refresh failed, wait for 5 minutes and try again.
    esp_sleep_enable_timer_wakeup(1000000ULL * 60ULL * 5);
    esp_deep_sleep_start();
  }
}
