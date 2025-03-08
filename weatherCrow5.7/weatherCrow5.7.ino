#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "EPD.h"
#include "weatherIcons.h"
#include <esp_sleep.h>

#define BAUD_RATE 115200
#define HTTP_NOT_REQUESTED_YET 999
#define STRING_BUFFER_SIZE 256

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
  int httpResponseCode = HTTP_NOT_REQUESTED_YET; // This is unbelievably declared as a global variable. Yes, it is used for the http response. why..
  JSONVar weatherApiResponse;

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
    EPD_drawImage(20, 10, leo_face_lg);
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
      Serial.print("getWeatherInfo retry count: ");
      Serial.println(retry_count);
      Serial.print(" API endpoint: ");
      Serial.println(apiEndPoint);

      jsonBuffer = httpsGETRequest(apiEndPoint.c_str());

      while (httpResponseCode == HTTP_NOT_REQUESTED_YET)
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

      weatherApiResponse = JSON.parse(jsonBuffer);
      if (JSON.typeof(weatherApiResponse) == "undefined")
      {
        errorMessageBuffer = "Unexpected Weather API response.\n\nThis typically happens when the weather API server side has issues and it will resolve soon.";
        return false;
      }

      weatherInfo.weather = String((const char *)weatherApiResponse["current"]["weather"][0]["main"]);
      weatherInfo.icon = String((const char *)weatherApiResponse["current"]["weather"][0]["icon"]);
      weatherInfo.currentDateTime = JSON.stringify(weatherApiResponse["current"]["dt"]).toInt() + JSON.stringify(weatherApiResponse["timezone_offset"]).toInt();
      weatherInfo.sunrise = JSON.stringify(weatherApiResponse["current"]["sunrise"]).toInt();
      weatherInfo.sunset = JSON.stringify(weatherApiResponse["current"]["sunset"]).toInt();
      weatherInfo.temperature = JSON.stringify(weatherApiResponse["current"]["temp"]);
      weatherInfo.humidity = JSON.stringify(weatherApiResponse["current"]["humidity"]);
      weatherInfo.pressure = JSON.stringify(weatherApiResponse["current"]["pressure"]);
      weatherInfo.wind_speed = JSON.stringify(weatherApiResponse["wind"]["speed"]);
      weatherInfo.timezone = JSON.stringify(weatherApiResponse["timezone"]);

      // Find the position of the decimal point
      int decimalPos = weatherInfo.temperature.indexOf('.');
      weatherInfo.tempIntegerPart = String(weatherInfo.temperature.substring(0, decimalPos).toInt());  // -0 -> 0
      weatherInfo.tempDecimalPart = String(weatherInfo.temperature.substring(decimalPos + 1).toInt()); // 00 -> 0
      // if the decimal part length is more than 2, cut it to 1
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

    for (uint16_t i = 1, counter = 0; counter < length; i += 2, counter++)
    {
      String icon = String((const char *)weatherApiResponse["hourly"][i]["weather"]["icon"]);
      String icon_name = "icon_" + weatherInfo.icon + "_sm";
      EPD_drawImage(x, y, icon_map[icon_name.c_str()]);

      // temperature
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", JSON.stringify(weatherApiResponse["hourly"][i]["temp"]).c_str());
      EPD_ShowString(x + 14, y - 32, buffer, FONT_SIZE_16, BLACK, true);

      // Time 10 AM, 9 PM or 12 PM
      long forecastTimeInt = JSON.stringify(weatherApiResponse["hourly"][i]["dt"]).toInt() + JSON.stringify(weatherApiResponse["timezone_offset"]).toInt();
      String forecastTime = convertUnixTimeToShortDateTimeString(forecastTimeInt);
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", forecastTime.c_str());
      EPD_ShowString(x + 14, y + 64, buffer, FONT_SIZE_16, BLACK, true);

      EPD_DrawLine(x + 90, y - 32, x + 90, y + 42, BLACK);

      // offset for the next icon
      x = x + 110;
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

    // Temperature, position adjusted for metric, imperial may need to adjust the offset.
    u_int16_t yPos = 65;
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", weatherInfo.tempIntegerPart.c_str());
    EPD_ShowStringRightAligned(730, yPos, buffer, FONT_SIZE_36, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), ".%s", weatherInfo.tempDecimalPart.c_str());
    EPD_ShowString(732, yPos + 8, buffer, FONT_SIZE_16, BLACK, false);

    memset(buffer, 0, sizeof(buffer));
    // if the UNITS is metric, the degree symbol is 'C'
    if (UNITS == "metric") {
      snprintf(buffer, sizeof(buffer), "C");
    } else {
      snprintf(buffer, sizeof(buffer), "F");
    }
    EPD_ShowString(750, yPos, buffer, FONT_SIZE_36, BLACK);


    // Main weather icon
    String icon_name = "icon_" + weatherInfo.icon + "_lg";
    EPD_drawImage(20, 40, icon_map[icon_name.c_str()]);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s", JSON.stringify(weatherApiResponse["current"]["weather"][0]["description"]).c_str());
    EPD_ShowString(72, 240, buffer, FONT_SIZE_16, BLACK, false);

    // uvi > UVI_THRESHOLD
    if (JSON.stringify(weatherApiResponse["current"]["uvi"]).toInt() > UVI_THRESHOLD){
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s", JSON.stringify(weatherApiResponse["current"]["uvi"]));
      EPD_ShowString(72, 10, buffer, FONT_SIZE_16, BLACK, false);
      EPD_drawImage(0, 0, uv_sm);
    }

    /*
    // Air pressure
    EPD_drawImage(260, 0, wi_barometer_sm);
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s hPa", weatherInfo.pressure.c_str());
    EPD_ShowString(330, 32, buffer, FONT_SIZE_16, BLACK, false);
    */

    // Date in formet of "Jan 01, Fri"
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", convertUnixTimeToDisplayFormat(weatherInfo.currentDateTime).c_str());
    EPD_ShowStringRightAligned(790, 20, buffer, FONT_SIZE_36, BLACK);

    UI_weather_future_forecast(260, 160, 5);

    // Debug : latest updated time.
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "LATEST API RESPONSE : %s ", convertUnixTimeToDateTimeString(weatherInfo.currentDateTime).c_str());
    EPD_ShowStringRightAligned(790, 270, buffer, FONT_SIZE_8, BLACK);

    // UI_system_info(300, 230);

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

  void run()
  {
    try
    {
      delay(3000);
      connectToWiFi();
      if (!getWeatherInfo())
      {
        char title[] = "Failed to get weather info.";
        char msg[256];
        errorMessageBuffer = "Please check your internet connection.\nThis message typically appears when the device is unable to connect to the wifi or internet.";
        memset(msg, 0, sizeof(msg));
        strncpy(msg, errorMessageBuffer.c_str(), sizeof(msg) - 1);
        UI_error_message(title, msg);
      }
      else
      {
        UI_weather_forecast();
      }
    }
    catch (const std::exception &e)
    {
      errorMessageBuffer = String("Exception: ") + e.what();
      UI_error();
    }
  }
};

WeatherCrow weatherCrow;

void setup()
{
  weatherCrow.begin();
}

void loop()
{
  weatherCrow.run();
  esp_sleep_enable_timer_wakeup(1000000ULL * 60ULL * REFRESH_MINUITES);
  esp_deep_sleep_start();
}