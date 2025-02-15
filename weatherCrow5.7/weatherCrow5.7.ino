#include "config.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "EPD.h"
#include "pic.h"

#define BAUD_RATE 115200

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

    String httpsGETRequest(const char* serverName) {
      WiFiClientSecure client;
      HTTPClient http;
      client.setInsecure();
      http.begin(client, serverName);
      httpResponseCode = http.GET();
      String payload = "{}";
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        payload = http.getString();
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
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
      EPD_ShowString(20, 20, buffer, 24, BLACK);
      Serial.print("UI_show_message : ");
      Serial.println(message);
      EPD_Display(ImageBW);
      EPD_PartUpdate();
      EPD_DeepSleep();
    }

    void UI_weather_forecast(){
      UI_clear_screen();
      char buffer[40];
      // Show background image and weather icon
      EPD_ShowPicture(0, 0, 792, 272, pic, WHITE);
      EPD_ShowPicture(4, 3, 432, 184, Weather_Num[weatherInfo.weather_flag], WHITE);
      // Draw partition lines
      EPD_DrawLine(0, 190, 792, 190, BLACK);
      EPD_DrawLine(530, 0, 530, 270, BLACK);
      // Display city, temperature, humidity, wind speed and pressure
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s ", weatherInfo.city);
      EPD_ShowString(620, 60, buffer, 24, BLACK);
      memset(buffer, 0, sizeof(buffer));
      // Changed the following line to include buffer size as the second argument.
      snprintf(buffer, sizeof(buffer), "%s C", weatherInfo.temperature);
      EPD_ShowString(340, 240, buffer, 24, BLACK);
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer), "%s ", weatherInfo.humidity);
      EPD_ShowString(620, 150, buffer, 24, BLACK);
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer),"%s m/s", weatherInfo.wind_speed);
      EPD_ShowString(135, 240, buffer, 24, BLACK);
      memset(buffer, 0, sizeof(buffer));
      snprintf(buffer, sizeof(buffer),"%s hpa", weatherInfo.pressure);
      EPD_ShowString(620, 240, buffer, 24, BLACK);
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
          Serial.println(jsonBuffer);
          weatherApiResponse = JSON.parse(jsonBuffer);
          if (JSON.typeof(weatherApiResponse) == "undefined") {
            Serial.println("Parsing input failed!");
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
        Serial.print("String weather: ");
        Serial.println(weatherInfo.weather);
        Serial.print("String Temperature: ");
        Serial.println(weatherInfo.temperature);
        Serial.print("String humidity: ");
        Serial.println(weatherInfo.humidity);
        Serial.print("String pressure: ");
        Serial.println(weatherInfo.pressure);
        Serial.print("String wind_speed: ");
        Serial.println(weatherInfo.wind_speed);
        Serial.print("String city: ");
        Serial.println(weatherInfo.city);
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
        Serial.println("WiFi Disconnected");
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