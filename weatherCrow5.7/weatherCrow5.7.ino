#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "EPD.h"
#include "pic.h"
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
    int currentDateTime;
    int sunrise;
    int sunset;
    String temperature;
    String humidity;
    String pressure;
    String wind_speed;
    String city;
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
      errorMessageBuffer = "Failed to connect to the wifi.\n\n SSID : " + String(WIFI_SSID);
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

  void UI_error_message(char *titile, char *description)
  {
    UI_clear_screen();

    uint16_t baseXpos = 258;
    char buffer[STRING_BUFFER_SIZE];

    // icon
    EPD_drawImage(60, 20, error_lg);
    EPD_DrawLine(baseXpos, 110, 740, 110, BLACK);

    // title
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", titile);
    EPD_ShowString(baseXpos, 70, buffer, FONT_SIZE_36, BLACK);

    // description
    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", description);
    EPD_ShowString(baseXpos, 140, buffer, FONT_SIZE_16, BLACK);

    EPD_Display(ImageBW);
    EPD_PartUpdate();
    EPD_DeepSleep();
  }

  void UI_weather_forecast()
  {
    UI_clear_screen();
    char buffer[STRING_BUFFER_SIZE];

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s ", weatherInfo.timezone.c_str());
    EPD_ShowString(300, 70, buffer, FONT_SIZE_36, BLACK);

    memset(buffer, 0, sizeof(buffer));
    snprintf(buffer, sizeof(buffer), "%s C", weatherInfo.temperature.c_str());
    EPD_ShowString(300, 130, buffer, FONT_SIZE_36, BLACK);

    // EPD_drawImage(360, 130, wi_celsius_xs);

    // Debug the icon name
    Serial.print("Icon ID from API: '");
    Serial.print(weatherInfo.icon);
    Serial.println("'");

    String icon_name = "icon_" + weatherInfo.icon + "_lg";
    Serial.print("Icon name : '");
    Serial.print(icon_name);
    Serial.println("'");

    EPD_drawImage(20, 30, icon_map[icon_name.c_str()]);

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
                           openWeatherMapApiKey + "&units=metric";
      Serial.print("getWeatherInfo retry count: ");
      Serial.println(retry_count);
      Serial.print(" API endpoint: ");
      Serial.println(apiEndPoint);
      Serial.print("[before] HTTP response code: ");
      Serial.println(httpResponseCode);

      Serial.print("Http get ");

      jsonBuffer = httpsGETRequest(apiEndPoint.c_str());

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
      weatherInfo.currentDateTime = JSON.stringify(weatherApiResponse["current"]["dt"]).toInt();
      weatherInfo.sunrise = JSON.stringify(weatherApiResponse["current"]["sunrise"]).toInt();
      weatherInfo.sunset = JSON.stringify(weatherApiResponse["current"]["sunset"]).toInt();
      weatherInfo.temperature = JSON.stringify(weatherApiResponse["current"]["temp"]);
      weatherInfo.humidity = JSON.stringify(weatherApiResponse["current"]["humidity"]);
      weatherInfo.pressure = JSON.stringify(weatherApiResponse["current"]["pressure"]);
      weatherInfo.wind_speed = JSON.stringify(weatherApiResponse["wind"]["speed"]);
      weatherInfo.city = JSON.stringify(weatherApiResponse["name"]);
      weatherInfo.timezone = JSON.stringify(weatherApiResponse["timezone"]);

      logPrint("String weather: ");
      logPrintln(weatherInfo.weather);
      logPrint("String Temperature: ");
      logPrintln(weatherInfo.temperature);
      logPrint("String humidity: ");
      logPrintln(weatherInfo.humidity);
      logPrint("String pressure: ");
      logPrintln(weatherInfo.pressure);
      logPrint("String wind_speed: ");
      logPrintln(weatherInfo.wind_speed);
      logPrint("String city: ");
      logPrintln(weatherInfo.city);
      logPrint("String timezone: ");
      logPrintln(weatherInfo.timezone);
      return true;
    }
    else
    {
      errorMessageBuffer = "Wireless network not established.\n\nSSID : " + String(WIFI_SSID);
      return false;
    }
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
        char msg[] = "Please check your internet connection.\nThis message typically appears when the device is unable to connect to the wifi or internet.";
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