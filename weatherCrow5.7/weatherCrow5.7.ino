#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "EPD.h"
#include "weatherIcons.h"
#include <esp_sleep.h>

#define BAUD_RATE 115200
#define HTTP_NOT_REQUESTED_YET -999
#define STRING_BUFFER_SIZE 256
#define JSON_CAPACITY 32768 // 32KB

class WeatherCrow
{
private:
  uint8_t ImageBW[27200];
  const char *ssid = WIFI_SSID;
  const char *password = WIFI_PASSWORD;
  String openWeatherMapApiKey = OPEN_WEATHER_MAP_API_KEY;
  String apiParamLatitude = LATITUDE;
  String apiParamLongtitude = LONGITUDE;
  String jsonBuffer;
  String errorMessageBuffer;
  int httpResponseCode = HTTP_NOT_REQUESTED_YET;
  // Replace JSONVar with DynamicJsonDocument
  DynamicJsonDocument weatherApiResponse = DynamicJsonDocument(JSON_CAPACITY);

  // Weather information structure
  // You are still able to API response data via weatherApiResponse variable.
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
    String wind_speed;
    String timezone;
    String icon;
  } weatherInfo;

  void logPrint(const char *msg) { Serial.print(msg); }
  void logPrint(int msg) { Serial.print(msg); }
  void logPrint(String msg) { Serial.print(msg); }

  void logPrintln(const char *msg) { Serial.println(msg); }
  void logPrintln(int msg) { Serial.println(msg); }
  void logPrintln(String msg) { Serial.println(msg); }

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

  void connectToWiFi()
  {
    WiFi.begin(ssid, password);
    Serial.println("Wifi Connecting");
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 180000)
    {             // 3 minutes timeout
      delay(500); // wait for wireless connection
      Serial.print(".");
    }
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("Failed to connect to WiFi. Retrying...");
      errorMessageBuffer = "Failed to connect to the wifi.";
    }
    Serial.println("");
  }

  void screenPowerOn()
  {
    pinMode(7, OUTPUT);
    digitalWrite(7, HIGH);
  }

  void UI_clear_screen()
  {
    Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
    Paint_Clear(WHITE);
    EPD_FastMode1Init();
    EPD_Display_Clear();
    EPD_Update();
    EPD_Clear_R26A6H();
  }

  // Display system information
  // Call this function during the drawing process, this funtion will update the screen buffer only.
  void UI_system_info(uint16_t base_x, uint16_t base_y)
  {
    uint16_t x = base_x;
    uint16_t y = base_y;
    uint16_t y_offset = 8;
    char buffer[STRING_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "WIFI SSID: %s", String(WIFI_SSID));
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y = y + y_offset;

    // memset(buffer, 0, sizeof(buffer));
    // snprintf(buffer, sizeof(buffer), "Password: %s", String(WIFI_PASSWORD));
    // EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    // y = y + y_offset;

    memset(buffer, 0, sizeof(buffer));
    if (WL_CONNECTED == WiFi.status())
    {
      snprintf(buffer, sizeof(buffer), "DEVICE IS CONNECTED TO WIFI. IP : %s", WiFi.localIP().toString().c_str());
    }
    else
    {
      snprintf(buffer, sizeof(buffer), "WIFI NOT CONNECTED. CHECK WIFI SSID AND CREDENTIALS. ROUTER REBOOT MAY BE WORKS.");
    }
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y = y + y_offset;

    // memset(buffer, 0, sizeof(buffer));
    // snprintf(buffer, sizeof(buffer), "API key: %s", String(OPEN_WEATHER_MAP_API_KEY));
    // EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    // y = y + y_offset;

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "LOCATION LATITUDE: %s, LONGITUDE: %s", String(LATITUDE), String(LONGITUDE));
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
    y = y + y_offset;

    // print current date time
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "LATEST SUCCESSFUL API CALL: %s", convertUnixTimeToDateTimeString(weatherInfo.currentDateTime).c_str());
    EPD_ShowString(x, y, buffer, FONT_SIZE_8, BLACK, false);
  }

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
    strftime(buffer, sizeof(buffer), "%I %p", dt); // it seems %-I option is not working on the ESP32
    // if the buffer starts with '0' remove it
    if (buffer[0] == '0')
    {
      memmove(buffer, buffer + 1, strlen(buffer));
    }
    return String(buffer);
  }

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

  void UI_error_message(char *titile, char *description)
  {
    UI_clear_screen();

    uint16_t baseXpos = 258;
    char buffer[STRING_BUFFER_SIZE];

    // icon
    EPD_drawImage(30, 30, leo_face_lg);
    EPD_DrawLine(baseXpos, 110, 740, 110, BLACK);

    // title
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", titile);
    EPD_ShowString(baseXpos, 70, buffer, FONT_SIZE_36, BLACK);

    // description
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", description);
    EPD_ShowString(baseXpos, 140, buffer, FONT_SIZE_16, BLACK);

    UI_system_info(baseXpos, 230);

    EPD_Display(ImageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  bool getWeatherInfo(uint8_t retry_count = 0)
  {
    httpResponseCode = HTTP_NOT_REQUESTED_YET;
    if (WiFi.status() == WL_CONNECTED)
    {
      String apiEndPoint = "https://api.openweathermap.org/data/3.0/onecall?lat=" +
                           apiParamLatitude + "&lon=" + apiParamLongtitude +
                           "&exclude=minutely"
                           "&APPID=" +
                           openWeatherMapApiKey + "&units=" + UNITS;

      jsonBuffer = httpsGETRequest(apiEndPoint.c_str());

      while (httpResponseCode < 0) // when the response is negative, it means in progress.(I guess)
      {
        delay(100);
      }

      Serial.print("HTTP response code: ");
      Serial.println(httpResponseCode);

      if (httpResponseCode != HTTP_CODE_OK)
      {
        errorMessageBuffer = "Weather API failed with HTTP code : " + String(httpResponseCode) + " \n\nThis typically happens when the weather API server side issue or the API key is invalid.";
        return false;
      }

      // Parse JSON using ArduinoJson
      DeserializationError error = deserializeJson(weatherApiResponse, jsonBuffer);
      if (error)
      {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        errorMessageBuffer = "JSON parsing error: " + String(error.c_str());
        return false;
      }

      // Extract values using ArduinoJson syntax
      weatherInfo.weather = weatherApiResponse["current"]["weather"][0]["main"].as<String>();
      weatherInfo.icon = weatherApiResponse["current"]["weather"][0]["icon"].as<String>();
      weatherInfo.currentDateTime = weatherApiResponse["current"]["dt"].as<long>() + weatherApiResponse["timezone_offset"].as<long>();
      weatherInfo.sunrise = weatherApiResponse["current"]["sunrise"].as<long>();
      weatherInfo.sunset = weatherApiResponse["current"]["sunset"].as<long>();
      weatherInfo.temperature = String(weatherApiResponse["current"]["temp"].as<float>(), 1);
      weatherInfo.humidity = String(weatherApiResponse["current"]["humidity"].as<int>());
      weatherInfo.pressure = String(weatherApiResponse["current"]["pressure"].as<int>());
      weatherInfo.wind_speed = String(weatherApiResponse["current"]["wind_speed"].as<float>(), 1);
      weatherInfo.timezone = weatherApiResponse["timezone"].as<String>();

      // Find the position of the decimal point
      int decimalPos = weatherInfo.temperature.indexOf('.');
      weatherInfo.tempIntegerPart = String(weatherInfo.temperature.substring(0, decimalPos).toInt());
      weatherInfo.tempDecimalPart = String(weatherInfo.temperature.substring(decimalPos + 1).toInt());
      if (weatherInfo.tempDecimalPart.length() > 1)
      {
        weatherInfo.tempDecimalPart = weatherInfo.tempDecimalPart.substring(0, 1);
      }

      return true;
    }
    else
    {
      errorMessageBuffer = "Wireless network not established.";
      return false;
    }
  }

  void UI_weather_future_forecast(uint16_t base_x, uint16_t base_y, uint16_t length)
  {
    uint16_t x = base_x;
    uint16_t y = base_y;
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

      String icon;
      if (hourly["weather"][0].containsKey("icon"))
      {
        icon = "icon_" + hourly["weather"][0]["icon"].as<String>();
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

      // Time 10 AM, 9 PM or 12 PM
      long forecastTimeInt = hourly["dt"].as<long>() + weatherApiResponse["timezone_offset"].as<long>();
      String forecastTime = convertUnixTimeToShortDateTimeString(forecastTimeInt);
      // split the time string and get the hour part and AM/PM part

      int splitIndex = forecastTime.indexOf(' ');
      String hour = forecastTime.substring(0, splitIndex);    // number like 10, 9, 12
      String period = forecastTime.substring(splitIndex + 1); // AM or PM

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", hour.c_str());
      EPD_ShowStringRightAligned(x + 24, y + 80, buffer, FONT_SIZE_16, BLACK);

      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", period.c_str());
      EPD_ShowStringRightAligned(x + 26, y + 96, buffer, FONT_SIZE_16, BLACK);

      // vertical line 3px width
      u_int16_t line_length = 103;
      EPD_DrawLine(x + 35, y + 70, x + 35, y + line_length, BLACK);
      EPD_DrawLine(x + 36, y + 70, x + 36, y + line_length, BLACK);
      EPD_DrawLine(x + 37, y + 70, x + 37, y + line_length, BLACK);

      // temperature
      memset(buffer, 0, sizeof(buffer));
      float temp = hourly["temp"].as<float>();
      int tempInt = (int)temp; // Get the integer part
      snprintf(buffer, sizeof(buffer), "%d", tempInt);
      EPD_ShowString(x + 46, y + 87, buffer, FONT_SIZE_38, BLACK, true);

      // offset for the next icon
      x = x + 105;
    }
  }

  void UI_graph(uint16_t base_x, uint16_t base_y, uint16_t graph_width, uint16_t graph_height, uint8_t line_width = 1)
  {
    uint16_t x = base_x;
    uint16_t y = base_y;
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
    { // Need at least 2 points to draw a line
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", "Error: Insufficient forecast data");
      EPD_ShowString(x, y, buffer, FONT_SIZE_16, BLACK, true);
      return;
    }

    // Determine how many hours to show based on available space
    uint16_t hours_to_show = min((uint16_t)24, (uint16_t)availableHours);
    uint16_t point_spacing = graph_width / (hours_to_show - 1);

    // Collect temperature data
    float temps[hours_to_show];
    float min_temp = 100.0;
    float max_temp = -100.0;

    // Find min and max temperatures for scaling
    for (uint16_t i = 0; i < hours_to_show; i++)
    {
      if (i >= availableHours)
        break;

      temps[i] = weatherApiResponse["hourly"][i]["temp"].as<float>();

      if (temps[i] < min_temp)
        min_temp = temps[i];
      if (temps[i] > max_temp)
        max_temp = temps[i];
    }

    // Add padding to min/max for visual appeal
    float temp_range = max_temp - min_temp;
    if (temp_range < 5.0)
    { // If range is too small, expand it
      float padding = (5.0 - temp_range) / 2;
      min_temp -= padding;
      max_temp += padding;
      temp_range = max_temp - min_temp;
    }
    else
    {
      // Add 10% padding on top and bottom
      min_temp -= temp_range * 0.1;
      max_temp += temp_range * 0.1;
      temp_range = max_temp - min_temp;
    }

    // Calculate scaling factor
    float scale = graph_height / temp_range;

    // Draw graph title
    // memset(buffer, 0, sizeof(buffer));
    // snprintf(buffer, sizeof(buffer), "24-Hour Temperature Forecast");
    // EPD_ShowString(x, y - 20, buffer, FONT_SIZE_16, BLACK, true);

    // Draw baseline
    // EPD_DrawLine(x, y + graph_height, x + graph_width, y + graph_height, BLACK);

    // Draw temperature points and connecting lines
    for (uint16_t i = 1; i < hours_to_show; i++)
    {
      // Calculate positions
      uint16_t x1 = x + (i - 1) * point_spacing;
      uint16_t y1 = y + graph_height - (uint16_t)((temps[i - 1] - min_temp) * scale);
      uint16_t x2 = x + i * point_spacing;
      uint16_t y2 = y + graph_height - (uint16_t)((temps[i] - min_temp) * scale);

      // Draw thicker line by drawing multiple lines if line_width > 1
      for (int w = 0; w < line_width; w++)
      {
        EPD_DrawLine(x1, y1 + w, x2, y2 + w, BLACK);
      }

      // Draw dots at data points
      EPD_DrawCircle(x1, y1, 2, BLACK, true);
      if (i == hours_to_show - 1)
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
        EPD_ShowString(x2 - 10, y + graph_height + 5, buffer, FONT_SIZE_8, BLACK, true);
      }
    }

    // Draw temperature scale on right side
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%.1f°", max_temp);
    EPD_ShowString(x + graph_width + 5, y, buffer, FONT_SIZE_8, BLACK, true);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%.1f°", min_temp);
    EPD_ShowString(x + graph_width + 5, y + graph_height - 10, buffer, FONT_SIZE_8, BLACK, true);
  }

  void UI_currentInfo(uint16_t base_x, uint16_t base_y)
  {

    uint16_t center_x = base_x;
    uint16_t y = base_y;
    uint16_t unit_offset_x = 4;
    uint16_t unit_offset_y = 5;

    char buffer[STRING_BUFFER_SIZE];
    // ===================== Pressure
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.pressure.c_str());
    EPD_ShowStringRightAligned(center_x, y, buffer, FONT_SIZE_36, BLACK);

    // hPa
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "hPa");
    EPD_ShowString(center_x + unit_offset_x, y + unit_offset_y, buffer, FONT_SIZE_16, BLACK, false);

    // ===================== Humidity
    y += 40;
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.humidity.c_str());
    EPD_ShowStringRightAligned(center_x, y, buffer, FONT_SIZE_36, BLACK);

    // %RH
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%%RH");
    EPD_ShowString(center_x + unit_offset_x, y + unit_offset_y, buffer, FONT_SIZE_16, BLACK, false);

    // Snow, rain, uvi and high wind speed
    y += 40;
    if ((weatherApiResponse["current"].containsKey("snow")) &&
        (weatherApiResponse["current"]["snow"].containsKey("1h")))
    {
      // snow
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["snow"]["1h"].as<String>().c_str());
      EPD_ShowStringRightAligned(center_x, y, buffer, FONT_SIZE_36, BLACK);
      // mm
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "mm");
      EPD_ShowString(center_x + unit_offset_x, y + unit_offset_y, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if ((weatherApiResponse["current"].containsKey("rain")) &&
             (weatherApiResponse["current"]["rain"].containsKey("1h")))
    {
      // rain
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["rain"]["1h"].as<String>().c_str());
      EPD_ShowStringRightAligned(center_x, y, buffer, FONT_SIZE_36, BLACK);
      // mm
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "mm");
      EPD_ShowString(center_x + unit_offset_x, y + unit_offset_y, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if ((weatherApiResponse["current"].containsKey("uvi")) && (weatherApiResponse["current"]["uvi"].as<float>() > UVI_THRESHOLD))
    {
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["uvi"].as<String>().c_str());
      EPD_ShowStringRightAligned(center_x, y, buffer, FONT_SIZE_36, BLACK);
      // uvi
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "UVI");
      EPD_ShowString(center_x + unit_offset_x, y + unit_offset_y, buffer, FONT_SIZE_16, BLACK, false);
    }
    else if (weatherInfo.wind_speed.toFloat() > 4.0)
    {
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["wind_speed"].as<String>().c_str());
      EPD_ShowStringRightAligned(center_x, y, buffer, FONT_SIZE_36, BLACK);

      // m/s
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "m/s");
      EPD_ShowString(center_x + unit_offset_x, y + unit_offset_y, buffer, FONT_SIZE_16, BLACK, false);
    }
  }

  // =================================================================================================
  // @brief Display the weather forecast on the screen
  //
  //
  //
  // =================================================================================================
  void UI_weather_forecast()
  {
    UI_clear_screen();
    char buffer[STRING_BUFFER_SIZE];

    // EPD_DrawLine(600, 42, 775, 42, BLACK);
    // EPD_DrawLine(600, 43, 775, 43, BLACK);

    // Date in formet of "Jan 01, Fri"
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", convertUnixTimeToDisplayFormat(weatherInfo.currentDateTime).c_str());
    EPD_ShowStringRightAligned(790, 25, buffer, FONT_SIZE_36, BLACK);

    // Temperature, position adjusted for metric, imperial may need to adjust the offset.
    u_int16_t yPos = 105;
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.tempIntegerPart.c_str());
    EPD_ShowStringRightAligned(740, yPos, buffer, FONT_SIZE_92, BLACK);

    // memset(buffer, 0, sizeof(buffer));
    // snprintf(buffer, sizeof(buffer), ".%s", weatherInfo.tempDecimalPart.c_str());
    // EPD_ShowString(730, yPos + 8, buffer, FONT_SIZE_16, BLACK, false);

    memset(buffer, 0, sizeof(buffer));
    // if the UNITS is metric, the degree symbol is 'C'
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

    // Main weather icon
    String icon_name = "icon_" + weatherInfo.icon + "_lg";
    EPD_drawImage(10, 1, icon_map[icon_name.c_str()]);

    // description
    // memset(buffer, 0, sizeof(buffer));
    // snprintf(buffer, sizeof(buffer), "%s", weatherApiResponse["current"]["weather"][0]["description"].as<String>().c_str());
    // EPD_ShowString(72, 240, buffer, FONT_SIZE_16, BLACK, false);

    UI_currentInfo(360, 30);

    // draw graph
    // UI_graph(280, 100, 440, 150, 2); //  x=280, y=80, width=300, height=200, line_width=2

    UI_weather_future_forecast(260, 160, 5);

    // UI_system_info(400, 50);

    // latest updated time.
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "LATEST API CALL : %s ", convertUnixTimeToDateTimeString(weatherInfo.currentDateTime).c_str());
    // EPD_ShowString(0, 270, buffer, FONT_SIZE_8, BLACK);

    EPD_Display(ImageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  void UI_error()
  {
    char title[] = "Someting went wrong.";
    char description[STRING_BUFFER_SIZE];
    memset(description, 0, sizeof(description));
    strncpy(description, errorMessageBuffer.c_str(), sizeof(description) - 1);
    UI_error_message(title, description);
  }

public:
  void begin()
  {
    errorMessageBuffer = "";
    delay(1000); // Delay to make sure serial monitor is ready
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
        UI_error_message(title, msg);
        return false;
      }
      else
      {
        UI_weather_forecast();
        return true;
      }
    }
    catch (const std::exception &e)
    {
      errorMessageBuffer = String("Exception: ") + e.what();
      UI_error();
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