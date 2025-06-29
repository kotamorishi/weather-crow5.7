#include "config.h"
#include <esp_sleep.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <sys/time.h>
#include "EPD.h"
#include "weatherIcons.h"

// Communication and data constants
#define BAUD_RATE 115200
#define HTTP_NOT_REQUESTED_YET -999
#define STRING_BUFFER_SIZE 256
#define JSON_CAPACITY 32768

// Display and UI constants
#define TEMPERATURE_GRAPH_LINE_WIDTH 2
#define UVI_DISPLAY_THRESHOLD 4.0
#define WIND_SPEED_DISPLAY_THRESHOLD 4.0

// LED
#define PWR_LED_PIN 41

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
  bool ledState = false;

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
      delay(300);
      logPrint(".");
      ledToggle();
    }

    if (LOW_POWER_MODE)
    {
      // Restore CPU frequency to 240MHz for faster WiFi connection
      setCpuFrequencyMhz(240);
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
   * Safely retrieves an icon from the icon map, returning a fallback if not found
   * @param iconKey The key to look up in icon_map
   * @param fallbackKey Optional fallback key if the main key is not found (defaults to "error_sm")
   * @return Reference to the icon data, or fallback icon if not found
   */
  const unsigned char *getIcon(const char *iconKey, const char *fallbackKey = "na_md")
  {
    // First check if the requested icon exists
    if (icon_map.count(iconKey) > 0)
    {
      return icon_map[iconKey];
    }

    // If not found, try to use the fallback
    if (icon_map.count(fallbackKey) > 0)
    {
      Serial.print("Icon not found: ");
      Serial.print(iconKey);
      Serial.print(", using fallback: ");
      Serial.println(fallbackKey);
      return icon_map[fallbackKey];
    }

    // If even fallback isn't available, return the first available icon
    Serial.print("Neither requested icon nor fallback found: ");
    Serial.println(iconKey);
    return icon_map.begin()->second;
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
   * Converts Unix timestamp to specified formatted date-time string
   */
  String convertUnixTimeToSpecifiedDateTimeString(long unixTime, const char *format)
  {
    if (unixTime == 0)
    {
      return "N/A";
    }

    time_t rawtime = unixTime;
    struct tm *dt;
    char buffer[30];
    dt = gmtime(&rawtime);
    strftime(buffer, sizeof(buffer), format, dt);
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

    char buffer[30];
    String monthStr = convertUnixTimeToSpecifiedDateTimeString(unixTime, "%b");
    String dayStr = convertUnixTimeToSpecifiedDateTimeString(unixTime, "%d");
    dayStr = formatDay(dayStr);
    String weekOfDayStr = convertUnixTimeToSpecifiedDateTimeString(unixTime, "%a");
    snprintf(buffer, sizeof(buffer), "%s %s, %s", monthStr.c_str(), dayStr.c_str(), weekOfDayStr.c_str());
    return String(buffer);
  }

  /**
   * Formats a date string by removing leading zeros and adding suffix for 1st, 2nd, 3rd days
   * @param dateStr Date string (e.g., "01", "08")
   * @return Formatted date string (e.g., "1", "2", "3", "4", "5", etc.)
   */
  String formatDay(String dateStr)
  {
    int day = dateStr.toInt();
    return String(day);
  }

  /**
   * Displays an error message on the screen
   */
  void displayErrorMessage(char *title, char *description)
  {
    clearScreen();

    uint16_t baseXpos = 258;
    char buffer[STRING_BUFFER_SIZE];

    // Display error icon - with safety check
    const char *icons[] = {"emma_cupcake_lg", "emma_mon1_lg", "leo_face_lg", "leo_gator_lg"};
    int randomIndex = random(0, sizeof(icons) / sizeof(icons[0]));
    const char *selectedIcon = icons[randomIndex];

    // Safely get the icon
    EPD_drawImage(30, 20, getIcon(selectedIcon));
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

  String constructApiEndpointUrl()
  {
    return "https://api.openweathermap.org/data/3.0/onecall?lat=" +
           apiParamLatitude + "&lon=" + apiParamLongitude +
           "&exclude=minutely" +
           "&APPID=" + openWeatherMapApiKey +
           "&units=" + UNITS;
  }

  void setRTC(long unixTime)
  {
    if (unixTime == 0)
      return;

    struct timeval now = {.tv_sec = unixTime, .tv_usec = 0};
    settimeofday(&now, NULL);
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

    // set RTC to set current time (if RTC is available)
    setRTC(weatherInfo.currentDateTime);

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
  bool getWeatherInfo(uint8_t maxRetries = 3)
  {
    uint8_t currentRetry = 0;
    bool success = false;

    while (currentRetry <= maxRetries && !success)
    {
      if (currentRetry > 0)
      {
        logPrint("Retry attempt ");
        logPrint(currentRetry);
        logPrintln(" of weather data fetch...");
        // Exponential backoff: 1s, 2s, 4s...
        delay(1000 * (1 << (currentRetry - 1)));
      }

      errorMessageBuffer = "getWeatherInfo() failed.";
      httpResponseCode = HTTP_NOT_REQUESTED_YET;
      if (WiFi.status() != WL_CONNECTED)
      {
        errorMessageBuffer = "Wireless network not available.";
        return false;
      }

      // Construct API endpoint URL and make request
      String apiEndPoint = constructApiEndpointUrl();
      jsonBuffer = httpsGETRequest(apiEndPoint.c_str());

      // Wait for response to complete
      // Add timeout to prevent infinite loop
      const unsigned long HTTP_TIMEOUT_MS = 5000; // Match HTTP timeout (5 seconds)
      unsigned long startTime = millis();
      while (httpResponseCode < 0 && (millis() - startTime) < HTTP_TIMEOUT_MS)
      {
        delay(500);
        ledToggle();
      }
      ledOn();

      // Check if timeout occurred or non-success status code
      if (httpResponseCode < 0)
      {
        logPrintln("HTTP request timed out.");
        currentRetry++;
        continue;
      }

      logPrint("HTTP response code: ");
      logPrintln(httpResponseCode);

      // Check for HTTP errors
      if (httpResponseCode != HTTP_CODE_OK)
      {
        errorMessageBuffer = "Weather API failed with HTTP code: " + String(httpResponseCode) +
                             "\n\nThis typically happens due to a weather API server issue or an invalid API key.";

        // Only retry on server errors (5xx) or certain client errors, don't retry on 4xx errors
        if (httpResponseCode >= HTTP_CODE_INTERNAL_SERVER_ERROR || httpResponseCode == HTTP_CODE_TOO_MANY_REQUESTS)
        {
          currentRetry++;
          continue;
        }
        return false; // Don't retry for other error codes
      }

      if (LOW_POWER_MODE)
      {
        // Turn off wireless as soon as possible.
        wirelessOff();
      }

      // Parse JSON response
      DeserializationError error = deserializeJson(weatherApiResponse, jsonBuffer);
      if (error)
      {
        logPrint(F("deserializeJson() failed: "));
        logPrintln(error.c_str());
        errorMessageBuffer = "JSON parsing error: " + String(error.c_str());
        currentRetry++;
        continue;
      }

      success = processWeatherData();
      if (!success)
      {
        currentRetry++;
      }
    }

    if (!success)
    {
      logPrintln("Failed to get weather info after all retry attempts");
    }
    return success;
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
    EPD_drawImage(x + 2, y, getIcon(buffer));

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
    EPD_DrawLine(x + 35, y + 75, x + 35, y + lineLength, BLACK);
    EPD_DrawLine(x + 36, y + 75, x + 36, y + lineLength, BLACK);
    EPD_DrawLine(x + 37, y + 75, x + 37, y + lineLength, BLACK);

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
      snprintf(buffer, sizeof(buffer), "%s", "Error: API not responding with hourly forecast data.");
      EPD_ShowString(x, y + 60, buffer, FONT_SIZE_16, BLACK, true);
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
      x = x + 107;
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

    // Alert, Snow, rain, UVI, or wind speed
    y += 40;
    displayAdditionalInfoLine(centerX, y, unitOffsetX, unitOffsetY);
  }

  /**
   * truncate the string to a maximum length
   * @param str The string to truncate
   * @param maxLength The maximum length of the string
   * @return The truncated string
   */
  String truncateString(const String& str, size_t maxLength)
  {
    if (str.length() > maxLength)
    {
      return str.substring(0, maxLength) + "...";
    }
    return str;
  }

  /**
   * Displays weather alerts information
   * @param baseX X position for alert display
   * @param baseY Y position for alert display
   * @return true if alerts were displayed, false otherwise
   */
  bool displayAlerts(uint16_t baseX, uint16_t baseY)
  {
    uint16_t x = baseX;
    uint16_t y = baseY;
    char buffer[STRING_BUFFER_SIZE];

    // Get the first/most recent alert
    JsonObject alert = weatherApiResponse["alerts"][0];

    // Display alert icon
    EPD_drawImage(x, y + 5, getIcon("warning_sm", "na_md"));

    y += 30;
    x += 75;

    // Display event type
    if (alert.containsKey("event"))
    {
      String event = alert["event"].as<String>();
      event = truncateString(event, 10);

      event[0] = toupper(event[0]);
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", event.c_str());
      EPD_ShowString(x - 10, y - 5, buffer, FONT_SIZE_36, BLACK, true);
      y += 45;
    }

    if (alert.containsKey("sender_name") && alert.containsKey("start") && alert.containsKey("end"))
    {
      String senderName = alert["sender_name"].as<String>();
      senderName = truncateString(senderName, 28);
      long startTime = alert["start"].as<long>();
      long endTime = alert["end"].as<long>();

      String startTimeStr = convertUnixTimeToShortDateTimeString(startTime);
      String startStr = convertUnixTimeToSpecifiedDateTimeString(startTime, "%a ") + startTimeStr;
      String endTimeStr = convertUnixTimeToShortDateTimeString(endTime);
      String endStr = convertUnixTimeToSpecifiedDateTimeString(endTime, "%a ") + endTimeStr;

      // check if startTime and EndTime are the same day, we don't need to put %a twice
      if (startStr.substring(0, 3) == endStr.substring(0, 3))
      {
        endStr = endStr.substring(4);
      }

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s to %s %s", startStr.c_str(), endStr.c_str(), senderName.c_str());
      EPD_ShowString(x - 60, y, buffer, FONT_SIZE_16, BLACK, true);
      y += 25;
    }

    if (alert.containsKey("description"))
    {
      String desc = removeWarningDescription(alert["description"].as<String>());

      if (desc.length() > STRING_BUFFER_SIZE - 5)
      {
        desc = desc.substring(0, STRING_BUFFER_SIZE - 5) + "...";
      }

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", desc.c_str());
      EPD_ShowString(x - 60, y, buffer, FONT_SIZE_16, BLACK, false);
    }

    return true;
  }

  String removeWarningDescription(String description)
  {
    // Remove "What:\n", "\n\nHazard:\" from the description
    const char *toBeRemoved[] = {
      "What:\n",
      "\n\nHazard:\n",
      "\n\nTiming:\n",
      "\n\nAdditional information:\n",
      "\n\n###\n",
      "\n" // replace all line breaks
    };
    const int toBeRemovedCount = sizeof(toBeRemoved) / sizeof(toBeRemoved[0]);
    for (int i = 0; i < toBeRemovedCount; ++i) {
      int idx;
      while ((idx = description.indexOf(toBeRemoved[i])) != -1) {
        description.remove(idx, strlen(toBeRemoved[i]));
      }
    }

    return description;
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
      snprintf(buffer, sizeof(buffer), "mm snow");
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
      snprintf(buffer, sizeof(buffer), "mm rain");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if ((weatherApiResponse["current"].containsKey("uvi")) &&
             (weatherApiResponse["current"]["uvi"].as<float>() > UVI_DISPLAY_THRESHOLD))
    {
      // UVI
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%.1f", weatherApiResponse["current"]["uvi"].as<float>());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "UVI");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if (weatherInfo.windSpeed.toFloat() > WIND_SPEED_DISPLAY_THRESHOLD)
    {
      // Wind speed
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%.1f", weatherApiResponse["current"]["wind_speed"].as<float>());
      EPD_ShowStringRightAligned(centerX, y, buffer, FONT_SIZE_36, BLACK);

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "m/s wind");
      EPD_ShowString(centerX + unitOffsetX, y + unitOffsetY, buffer, FONT_SIZE_16, BLACK, false);
    }
  }

  void displayTemperature(uint16_t x, uint16_t y, bool isLarge)
  {
    char buffer[STRING_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    if (isLarge)
    {
      snprintf(buffer, sizeof(buffer), "%s", weatherInfo.tempIntegerPart.c_str());
      EPD_ShowStringRightAligned(x, y, buffer, FONT_SIZE_92, BLACK);
      memset(buffer, 0, sizeof(buffer));
      if (UNITS == "metric")
      {
        snprintf(buffer, sizeof(buffer), "C");
        EPD_drawImage(x, y, degrees_lg);
      }
      else
      {
        snprintf(buffer, sizeof(buffer), "F");
      }
      EPD_ShowString(x + 10, y + 24, buffer, FONT_SIZE_36, BLACK);
    }
    else
    {
      if (UNITS == "metric")
      {
        snprintf(buffer, sizeof(buffer), "C");
        EPD_drawImage(x - 2, y - 20, degrees_lg);
      }
      else
      {
        snprintf(buffer, sizeof(buffer), "F");
      }
      EPD_ShowString(x + 12, y, buffer, FONT_SIZE_36, BLACK);

      snprintf(buffer, sizeof(buffer), "%s", weatherInfo.tempIntegerPart.c_str());
      EPD_ShowStringRightAligned(x, y, buffer, FONT_SIZE_36, BLACK);
    }
  }

  void displayWeatherForecast()
  {
    clearScreen();
    char buffer[STRING_BUFFER_SIZE];

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", convertUnixTimeToDisplayFormat(weatherInfo.currentDateTime).c_str());
    //snprintf(buffer, sizeof(buffer), "５月４日　土");
    EPD_ShowStringRightAligned(790, 25, buffer, FONT_SIZE_36, BLACK);

    // Display weather icon
    String iconName = "icon_" + weatherInfo.icon + "_lg";
    EPD_drawImage(10, 1, getIcon(iconName.c_str()));

    if (
        (ENABLE_ALERT_DISPLAY == true)
        && weatherApiResponse.containsKey("alerts")
        && weatherApiResponse["alerts"].size() > 0
      )
    {
      displayAlerts(270, 0);
      displayTemperature(740, 70, false);
    }
    else
    {
      displayCurrentInfo(380, 30);
      displayTemperature(740, 105, true);
    }

    // Display future forecast
    drawWeatherFutureForecast(270, 160, 5);

    // clock frequency is reduced to 80MHz to save power
    if (LOW_POWER_MODE)
    {
      setCpuFrequencyMhz(80);
    }

    ledOff();
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

  void displayTypographyTest()
  {
    clearScreen();
    char buffer[STRING_BUFFER_SIZE];

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789");
    EPD_ShowStringRightAligned(790, 10, buffer, FONT_SIZE_8, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789");
    EPD_ShowStringRightAligned(790, 30, buffer, FONT_SIZE_16, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    EPD_ShowStringRightAligned(790, 80, buffer, FONT_SIZE_36, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "abcdefghijklmnopqrstuvwxyz");
    EPD_ShowStringRightAligned(790, 120, buffer, FONT_SIZE_36, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "Pixel perfection");
    EPD_ShowStringRightAligned(790, 210, buffer, FONT_SIZE_92, BLACK);

    // Update display
    EPD_Display(imageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  void displayCustomFontTest()
  {
    clearScreen();
    char buffer[STRING_BUFFER_SIZE];

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "Hello.");
    EPD_ShowString(60, 85, buffer, FONT_SIZE_92, BLACK, true);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "Included font file converter tool.");
    EPD_ShowString(80, 160, buffer, FONT_SIZE_36, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "You don't need to build the font from scratch. Just convert it.");
    EPD_ShowString(80, 200, buffer, FONT_SIZE_16, BLACK);

    // Update display
    EPD_Display(imageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  void displayMonsterIconsTest()
  {
    clearScreen();

    int ypos = 0;

    // Use safe icon retrieval for all icons
    EPD_drawImage(0, 0, getIcon("emma_cupcake_lg"));
    EPD_drawImage(170, 0, getIcon("emma_mon1_lg"));
    EPD_drawImage(350, 0, getIcon("leo_face_lg"));
    EPD_drawImage(550, 0, getIcon("leo_gator_lg"));

    char buffer[STRING_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "Included monster icons too!");
    EPD_ShowStringRightAligned(790, 250, buffer, FONT_SIZE_36, BLACK);

    // Update display
    EPD_Display(imageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  void displayIconsTest()
  {
    clearScreen();

    int ypos = 0;

    // Update all icon accesses to use getSafeIcon
    EPD_drawImage(0, ypos, getIcon("icon_01d_sm"));
    EPD_drawImage(60, ypos, getIcon("icon_01n_sm"));
    EPD_drawImage(120, ypos, getIcon("icon_02d_sm"));
    EPD_drawImage(180, ypos, getIcon("icon_02n_sm"));
    EPD_drawImage(240, ypos, getIcon("icon_03d_sm"));
    EPD_drawImage(300, ypos, getIcon("icon_03n_sm"));
    EPD_drawImage(360, ypos, getIcon("icon_04d_sm"));
    EPD_drawImage(420, ypos, getIcon("icon_04n_sm"));
    EPD_drawImage(480, ypos, getIcon("icon_09d_sm"));
    EPD_drawImage(540, ypos, getIcon("icon_09n_sm"));
    EPD_drawImage(600, ypos, getIcon("icon_10d_sm"));
    EPD_drawImage(660, ypos, getIcon("icon_10n_sm"));
    EPD_drawImage(720, ypos, getIcon("icon_11d_sm"));

    ypos = 60;
    EPD_drawImage(0, ypos, getIcon("icon_11n_sm"));
    EPD_drawImage(60, ypos, getIcon("icon_13d_sm"));
    EPD_drawImage(120, ypos, getIcon("icon_13n_sm"));

    const char *icons[] = {
        "wind_sm", "raindrop_sm", "sunrise_sm", "snow_sm", "dust_sm",
        "rain_sm", "sunset_sm", "humidity_sm", "barometer_sm", "degrees_sm", "na_md"};

    for (int i = 0; i < sizeof(icons) / sizeof(icons[0]); ++i)
    {
      EPD_drawImage(200 + i * 30, ypos, getIcon(icons[i]));
    }

    EPD_drawImage(550, 15, getIcon("icon_04n_lg"));
    EPD_drawImage(0, 200, getIcon("error_sm"));

    char buffer[STRING_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "Weather Icons.");
    EPD_ShowStringRightAligned(790, 230, buffer, FONT_SIZE_92, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "Build-in small and large");
    EPD_ShowString(100, 178, buffer, FONT_SIZE_38, BLACK, true);

    // Update display
    EPD_Display(imageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  void wirelessOff()
  {
    WiFi.disconnect(true); // disconnect from WiFi
    WiFi.mode(WIFI_OFF);   // Disable WiFi
    btStop();              // Disable Bluetooth(Just in case)
  }

  static const uint8_t MAX_WEATHER_API_RETRIES = 3;

  /**
   * Calculates the sleep duration based on regular refresh time, time until midnight,
   * and alert start/end times
   * @return sleep duration in microseconds
   */
  uint64_t calculateSleepDuration()
  {
    // Standard refresh time calculation
    uint64_t minSleepDuration = 1000000ULL * 60ULL * 10; // 10 minutes
    uint64_t requestedSleepDuration = 1000000ULL * 60ULL * REFRESH_MINUTES;
    uint64_t regularSleepDuration = (requestedSleepDuration < minSleepDuration) ? minSleepDuration : requestedSleepDuration;

    // Default to regular sleep duration
    uint64_t sleepDuration = regularSleepDuration;
    String wakeReason = "Regular refresh interval";

    // Get current time from RTC
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
      time_t currentTime = mktime(&timeinfo);

      // Calculate time until next date change (midnight)
      int seconds_to_midnight = (23 - timeinfo.tm_hour) * 3600 +
                                (59 - timeinfo.tm_min) * 60 +
                                (60 - timeinfo.tm_sec);

      // Add 30 seconds delay after midnight
      uint64_t timeToNextDateChange = (seconds_to_midnight + 30) * 1000000ULL;

      // Check if date change is sooner than regular refresh
      if (timeToNextDateChange < sleepDuration)
      {
        sleepDuration = timeToNextDateChange;
        wakeReason = "Date change (midnight + 30s)";
      }

      // Check for weather alerts
      if (weatherApiResponse.containsKey("alerts") && weatherApiResponse["alerts"].size() > 0)
      {
        // Current time in unix timestamp
        time_t now = currentTime;

        // Iterate through alerts to find closest upcoming start or end time
        for (size_t i = 0; i < weatherApiResponse["alerts"].size(); i++)
        {
          if (weatherApiResponse["alerts"][i].containsKey("start"))
          {
            time_t alertStartTime = weatherApiResponse["alerts"][i]["start"].as<long>();

            // If alert starts in the future
            if (alertStartTime > now)
            {
              uint64_t timeToAlertStart = (alertStartTime - now) * 1000000ULL;

              // If this alert starts sooner than our current wake time
              if (timeToAlertStart < sleepDuration)
              {
                sleepDuration = timeToAlertStart;
                wakeReason = "Upcoming alert start";
              }
            }
          }

          if (weatherApiResponse["alerts"][i].containsKey("end"))
          {
            time_t alertEndTime = weatherApiResponse["alerts"][i]["end"].as<long>();

            // If alert ends in the future
            if (alertEndTime > now)
            {
              uint64_t timeToAlertEnd = (alertEndTime - now) * 1000000ULL;

              // If this alert ends sooner than our current wake time
              if (timeToAlertEnd < sleepDuration)
              {
                sleepDuration = timeToAlertEnd;
                wakeReason = "Alert end time";
              }
            }
          }
        }
      }

      // Print current date and time in yyyy-mm-dd hh:mm:ss format
      char currentTimeBuffer[30];
      strftime(currentTimeBuffer, sizeof(currentTimeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
      Serial.print("Current date and time: ");
      Serial.println(currentTimeBuffer);

      Serial.print("Sleep duration: ");
      Serial.print((uint32_t)(sleepDuration / 1000000ULL));
      Serial.print(" seconds (");
      Serial.print(wakeReason);
      Serial.println(")");

      return sleepDuration;
    }
    else
    {
      // If we can't get the time, use the regular sleep duration
      Serial.println("Failed to get RTC time, using regular sleep duration");
      return regularSleepDuration;
    }
  }

  void ledToggle()
  {
    if (LOW_POWER_MODE)
      return;

    if (ledState)
    {
      digitalWrite(PWR_LED_PIN, LOW);
      ledState = false;
    }
    else
    {
      digitalWrite(PWR_LED_PIN, HIGH);
      ledState = true;
    }
  }

  void ledOff()
  {
    if (LOW_POWER_MODE)
      return;

    digitalWrite(PWR_LED_PIN, LOW);
    ledState = false;
  }

  void ledOn()
  {
    if (LOW_POWER_MODE)
      return;

    digitalWrite(PWR_LED_PIN, HIGH);
    ledState = true;
  }

public:
  uint64_t getSleepDuration()
  {
    return calculateSleepDuration();
  }

  void begin()
  {
    errorMessageBuffer = "";
    delay(1000);
    Serial.begin(BAUD_RATE);
    screenPowerOn();
    EPD_GPIOInit();
    if (LOW_POWER_MODE == false)
    {
      // when not in low power mode, set output to avoid leakage current.
      pinMode(PWR_LED_PIN, OUTPUT);
    }
  }

  bool run()
  {
    try
    {

      // displayIconsTest();
      // displayTypographyTest();
      // displayCustomFontTest();
      // displayMonsterIconsTest();
      // delay(500000);

      ledOn();

      connectToWiFi();

      if (!getWeatherInfo(MAX_WEATHER_API_RETRIES))
      {
        char title[] = "Weather API failed.";
        char msg[256];
        memset(msg, 0, sizeof(msg));
        snprintf(msg, sizeof(msg), "%s\n\n(After %d retry attempts)",
                 errorMessageBuffer.c_str(), MAX_WEATHER_API_RETRIES);
        displayErrorMessage(title, msg);
        return false;
      }
      else
      {
        logPrintln("Weather information retrieved successfully.");
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

  if (LOW_POWER_MODE)
  {
    setCpuFrequencyMhz(80);
    btStop(); // Disable Bluetooth, as it is not used in this project
  }
  weatherCrow.begin();
}

void loop()
{

  Serial.println("Starting weatherCrow.run()");
  if (weatherCrow.run() == true)
  {
    Serial.println("weatherCrow.run() completed successfully");
    uint64_t sleepDuration = weatherCrow.getSleepDuration();
    esp_sleep_enable_timer_wakeup(sleepDuration);
    esp_deep_sleep_start();
  }
  else
  {
    // refresh failed, wait for 5 minutes and try again.
    esp_sleep_enable_timer_wakeup(1000000ULL * 60ULL * 5);
    esp_deep_sleep_start();
  }
}
