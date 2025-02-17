#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "EPD.h"
#include "pic.h"
#include "../weatherIcons.h"

#define BAUD_RATE 115200

/**
 * @brief this class works for only 5.7 inch e-paper display.
 */
class WeatherCrow {
  private:
    uint8_t ImageBW[27200];
    const char *ssid = WIFI_SSID;
    const char *password = WIFI_PASSWORD;
    String openWeatherMapApiKey = OPEN_WEATHER_MAP_API_KEY;
    String apiParamLatitude = LATITUDE;
    String apiParamLongtitude = LONGITUDE;
    String jsonBuffer;
    int httpResponseCode;
    JSONVar weatherApiResponse;

    struct WeatherInfo {
      String weather;
      int currentDateTime;
      int sunrise;
      int sunset;
      String temperature;
      String humidity;
      String pressure;
      String wind_speed;
      String city;
      int weather_flag;
    } weatherInfo;

    void logPrint(const char *msg) { Serial.print(msg); }
    void logPrint(int msg) { Serial.print(msg); }
    void logPrint(String msg) { Serial.print(msg); }

    void logPrintln(const char *msg) { Serial.println(msg); }
    void logPrintln(int msg) { Serial.println(msg); }
    void logPrintln(String msg) { Serial.println(msg); }

    String httpsGETRequest(const char* serverName) {
      WiFiClientSecure client;
      HTTPClient http;
      client.setInsecure();
      http.begin(client, serverName);
      httpResponseCode = http.GET();
      String payload = "{}";
      if (httpResponseCode > 0) {
        logPrint("HTTP Response code: ");
        logPrintln(httpResponseCode);
        payload = http.getString();
      } else {
        logPrint("Error code: ");
        logPrintln(httpResponseCode);
      }
      http.end();
      return payload;
    }

    void connectToWiFi() {
      WiFi.begin(ssid, password);
      Serial.println("Wifi Connecting");
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("");
    }

    void screenPowerOn(){
      pinMode(7, OUTPUT);
      digitalWrite(7, HIGH);
    }

    void UI_clear_screen(){
      // Clear image and initialize display
      Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE);
      Paint_Clear(WHITE);
      EPD_FastMode1Init();
      EPD_Display_Clear();
      EPD_Update();
      EPD_Clear_R26A6H();
    }

    void UI_show_message(char *message){
      UI_clear_screen();
      char buffer[128];
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s ", message);
      EPD_ShowString(20, 0, buffer, 8, BLACK);
      EPD_ShowString(20, 20, buffer, 12, BLACK);
      //EPD_ShowString(20, 32, buffer, 16, BLACK);
      // EPD_ShowString(20, 58, buffer, 24, BLACK);
      // EPD_ShowString(20, 128, buffer, 48, BLACK);
      // EPD_ShowString(20, 172, buffer, 64, BLACK);
      logPrint("UI_show_message : ");
      logPrintln(message);
      EPD_Display(ImageBW);
      EPD_PartUpdate();
      EPD_DeepSleep();
    }

    void UI_weather_forecast(){
      UI_clear_screen();
      char buffer[40];
      // Show background image and weather icon
      EPD_ShowPicture(0, 0, 792, 272, pic, WHITE);
      EPD_drawImage(4, 3, WETHER_IMAGE_WIDTH, WETHER_IMAGE_HEIGHT, wi_night_rain_wind);
      //EPD_ShowPicture(4, 3, 432, 128, Weather_Num[weatherInfo.weather_flag], WHITE);
      // Draw partition lines
      // EPD_DrawLine(0, 190, 792, 190, BLACK);
      // EPD_DrawLine(530, 0, 530, 270, BLACK);
      // // Display city, temperature, humidity, wind speed and pressure
      // memset(buffer, 0, sizeof(buffer));
      // snprintf(buffer, sizeof(buffer), "%s ", weatherInfo.city);
      // EPD_ShowString(620, 60, buffer, 24, BLACK);
      // memset(buffer, 0, sizeof(buffer));
      // // Changed the following line to include buffer size as the second argument.
      // snprintf(buffer, sizeof(buffer), "%s C", weatherInfo.temperature);
      // EPD_ShowString(340, 240, buffer, 24, BLACK);
      // memset(buffer, 0, sizeof(buffer));
      // snprintf(buffer, sizeof(buffer), "%s ", weatherInfo.humidity);
      // EPD_ShowString(620, 150, buffer, 24, BLACK);
      // memset(buffer, 0, sizeof(buffer));
      // snprintf(buffer, sizeof(buffer),"%s m/s", weatherInfo.wind_speed);
      // EPD_ShowString(135, 240, buffer, 24, BLACK);
      // memset(buffer, 0, sizeof(buffer));
      // snprintf(buffer, sizeof(buffer),"%s hpa", weatherInfo.pressure);
      // EPD_ShowString(620, 240, buffer, 24, BLACK);
      EPD_Display(ImageBW);
      EPD_PartUpdate();
      EPD_DeepSleep();
    }

    bool getWeatherInfo(){
      if (WiFi.status() == WL_CONNECTED) {
        String apiEndPoint = "https://api.openweathermap.org/data/3.0/onecall?lat=" +
                               apiParamLatitude + "&lon=" + apiParamLongtitude +
                               "&APPID=" + openWeatherMapApiKey + "&units=metric";
        while (httpResponseCode != 200) {
          jsonBuffer = httpsGETRequest(apiEndPoint.c_str());
          logPrintln(jsonBuffer);
          weatherApiResponse = JSON.parse(jsonBuffer);
          if (JSON.typeof(weatherApiResponse) == "undefined") {
            logPrintln("Parsing input failed!");
            return false;
          }
          delay(10000);
        }
        weatherInfo.weather = JSON.stringify(weatherApiResponse["weather"][0]["main"]);
        weatherInfo.currentDateTime = JSON.stringify(weatherApiResponse["current"]["dt"]).toInt();
        weatherInfo.sunrise = JSON.stringify(weatherApiResponse["current"]["sunrise"]).toInt();
        weatherInfo.sunset = JSON.stringify(weatherApiResponse["current"]["sunset"]).toInt();
        weatherInfo.temperature = JSON.stringify(weatherApiResponse["current"]["temp"]);
        weatherInfo.humidity = JSON.stringify(weatherApiResponse["current"]["humidity"]);
        weatherInfo.pressure = JSON.stringify(weatherApiResponse["current"]["pressure"]);
        weatherInfo.wind_speed = JSON.stringify(weatherApiResponse["wind"]["speed"]);
        weatherInfo.city = JSON.stringify(weatherApiResponse["name"]);
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
        if (weatherInfo.weather.indexOf("clouds") != -1 || weatherInfo.weather.indexOf("Clouds") != -1) {
          weatherInfo.weather_flag = 1;
        } else if (weatherInfo.weather.indexOf("clear sky") != -1 || weatherInfo.weather.indexOf("Clear sky") != -1) {
          weatherInfo.weather_flag = 3;
        } else if (weatherInfo.weather.indexOf("rain") != -1 || weatherInfo.weather.indexOf("Rain") != -1) {
          weatherInfo.weather_flag = 5;
        } else if (weatherInfo.weather.indexOf("thunderstorm") != -1 || weatherInfo.weather.indexOf("Thunderstorm") != -1) {
          weatherInfo.weather_flag = 2;
        } else if (weatherInfo.weather.indexOf("snow") != -1 || weatherInfo.weather.indexOf("Snow") != -1) {
          weatherInfo.weather_flag = 4;
        } else if (weatherInfo.weather.indexOf("mist") != -1 || weatherInfo.weather.indexOf("Mist") != -1) {
          weatherInfo.weather_flag = 0;
        }
        return true;
      } else {
        logPrintln("WiFi Disconnected");
        return false;
      }
    }

  public:
    void begin(){
      Serial.begin(BAUD_RATE);
      connectToWiFi();
      Serial.println(WiFi.localIP());
      screenPowerOn();
      EPD_GPIOInit();
    }

    void run(){
      //UI_show_message(" 1234567890 ABCDEFGHIJKLMNOPQRSTUVWXYZ ");
      //delay(1000 * 60 * REFRESH_MINUITES);

      connectToWiFi();
      if (!getWeatherInfo()){
        char msg[] = "Failed to get weather infomation.";
        UI_show_message(msg);
      } else {
        UI_weather_forecast();
      }
      delay(1000 * 60 * REFRESH_MINUITES);
    }
};

WeatherCrow weatherCrow;

void setup(){
  weatherCrow.begin();
}

void loop(){
  weatherCrow.run();
}