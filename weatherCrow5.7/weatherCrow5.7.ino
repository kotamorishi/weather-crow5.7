#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include "EPD.h"
#include "pic.h"

// Define the black and white image array as the buffer for the e-paper display
uint8_t ImageBW[27200];  // Define the buffer size according to the resolution of the e-paper display

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
String openWeatherMapApiKey = OPEN_WEATHER_MAP_API_KEY;
String apiParamLatitude = LATITUDE;
String apiParamLongtitude = LONGITUDE;

// Default timer set to 10 seconds for testing
// For the final application, set an appropriate time interval based on the hourly/minute API call limits
unsigned long lastTime = 0;                // Last update time
unsigned long timerDelay = 10000;          // Timer set to 10 seconds (10000)

// Define variables related to JSON data
String jsonBuffer;
int httpResponseCode;
JSONVar myObject;

// Add a struct to encapsulate weather data
struct WeatherInfo {
  String weather;
  String temperature;
  String humidity;
  String sea_level;
  String wind_speed;
  String city;
  int weather_flag;
};
WeatherInfo weatherInfo;

// Function to display weather forecast
void UI_weather_forecast()
{
  char buffer[40];  // Create a character array to store information

  // Clear the image and initialize the e-ink screen
  Paint_NewImage(ImageBW, EPD_W, EPD_H, Rotation, WHITE); // Create a new image
  Paint_Clear(WHITE); // Clear the image content
  EPD_FastMode1Init(); // Initialize the e-ink screen
  EPD_Display_Clear(); // Clear the screen display
  EPD_Update(); // Update the screen
  EPD_Clear_R26A6H(); // Clear the e-ink screen cache

  // Display the image
  EPD_ShowPicture(0, 0, 792, 272, pic, WHITE); // Display the background image

  // Display the corresponding weather icon based on the weather icon flag
  EPD_ShowPicture(4, 3, 432, 184, Weather_Num[weatherInfo.weather_flag], WHITE);

  // Draw partition lines
  EPD_DrawLine(0, 190, 792, 190, BLACK); // Draw a horizontal line
  EPD_DrawLine(530, 0, 530, 270, BLACK); // Draw a vertical line

  // Display the update time
  memset(buffer, 0, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s ", weatherInfo.city); // Format the update time as a string
  EPD_ShowString(620, 60, buffer, 24, BLACK); // Display the update time

  // Display the temperature
  memset(buffer, 0, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s C", weatherInfo.temperature); // Format the temperature as a string
  EPD_ShowString(340, 240, buffer, 24, BLACK); // Display the temperature

  // Display the humidity
  memset(buffer, 0, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s ", weatherInfo.humidity); // Format the humidity as a string
  EPD_ShowString(620, 150, buffer, 24, BLACK); // Display the humidity

  // Display the wind speed
  memset(buffer, 0, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s m/s", weatherInfo.wind_speed); // Format the wind speed as a string
  EPD_ShowString(135, 240, buffer, 24, BLACK); // Display the wind speed

  // Display the sea level pressure
  memset(buffer, 0, sizeof(buffer));
  snprintf(buffer, sizeof(buffer), "%s ", weatherInfo.sea_level); // Format the sea level pressure as a string
  EPD_ShowString(620, 240, buffer, 24, BLACK); // Display the sea level pressure

  // Update the e-ink screen display content
  EPD_Display(ImageBW); // Display the image
  EPD_PartUpdate(); // Partially update the screen
  EPD_DeepSleep(); // Enter deep sleep mode
}

void setup() {
  Serial.begin(115200); // Initialize the serial port

  // Connect to the WiFi network
  WiFi.begin(ssid, password);
  Serial.println("Wifi Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP()); // Print the IP address after successful connection

  Serial.println("Timer set to 10 seconds (timerDelay variable), it will take 10 seconds before publishing the first reading.");

  // Set the screen power pin to output mode and set it to high level to turn on the power
  pinMode(7, OUTPUT);  // Set GPIO 7 to output mode
  digitalWrite(7, HIGH);  // Set GPIO 7 to high level to turn on the power

  // Initialize the e-paper display
  EPD_GPIOInit();  // Initialize the GPIO pins of the e-paper
}

void loop() {
  try {
    js_analysis();
  } catch (const std::exception& e) {
    Serial.print("Exception caught: ");
    Serial.println(e.what());
  }
  UI_weather_forecast(); // Update weather display
  delay(1000 * 60 * REFRESH_MINUITES);
}

bool js_analysis()
{
  // Check if successfully connected to the WiFi network
  if (WiFi.status() == WL_CONNECTED) {
    String apiEndPoint = "https://api.openweathermap.org/data/3.0/onecall?lat=" + apiParamLatitude + "&lon=" + apiParamLongtitude + "&APPID=" + openWeatherMapApiKey + "&units=metric";

    // Loop until a valid HTTP response code of 200 is obtained
    while (httpResponseCode != 200) {
      // Send an HTTP GET request and get the response content
      jsonBuffer = httpGETRequest(apiEndPoint.c_str());
      Serial.println(jsonBuffer); // Print the obtained JSON data
      myObject = JSON.parse(jsonBuffer); // Parse the JSON data

      // Check if JSON parsing was successful
      if (JSON.typeof(myObject) == "undefined") {
        Serial.println("Parsing input failed!"); // Error message when parsing fails
        return false; // Exit the function if parsing fails
      }
      delay(10000); // Wait for 2 seconds before retrying
    }

    // Extract weather information from the parsed JSON data
    weatherInfo.weather = JSON.stringify(myObject["weather"][0]["main"]); // Weather main information
    weatherInfo.temperature = JSON.stringify(myObject["main"]["temp"]); // Temperature
    weatherInfo.humidity = JSON.stringify(myObject["main"]["humidity"]); // Humidity
    weatherInfo.sea_level = JSON.stringify(myObject["main"]["sea_level"]); // Sea level pressure
    weatherInfo.wind_speed = JSON.stringify(myObject["wind"]["speed"]); // Wind speed
    weatherInfo.city = JSON.stringify(myObject["name"]); // City name

    // Print the extracted weather information
    Serial.print("String weather: ");
    Serial.println(weatherInfo.weather);
    Serial.print("String Temperature: ");
    Serial.println(weatherInfo.temperature);
    Serial.print("String humidity: ");
    Serial.println(weatherInfo.humidity);
    Serial.print("String sea_level: ");
    Serial.println(weatherInfo.sea_level);
    Serial.print("String wind_speed: ");
    Serial.println(weatherInfo.wind_speed);
    Serial.print("String city: ");
    Serial.println(weatherInfo.city);

    // Set the weather icon flag based on the weather description
    if (weatherInfo.weather.indexOf("clouds") != -1 || weatherInfo.weather.indexOf("Clouds") != -1) {
      weatherInfo.weather_flag = 1; // Cloudy
    } else if (weatherInfo.weather.indexOf("clear sky") != -1 || weatherInfo.weather.indexOf("Clear sky") != -1) {
      weatherInfo.weather_flag = 3; // Clear sky
    } else if (weatherInfo.weather.indexOf("rain") != -1 || weatherInfo.weather.indexOf("Rain") != -1) {
      weatherInfo.weather_flag = 5; // Rainy
    } else if (weatherInfo.weather.indexOf("thunderstorm") != -1 || weatherInfo.weather.indexOf("Thunderstorm") != -1) {
      weatherInfo.weather_flag = 2; // Thunderstorm
    } else if (weatherInfo.weather.indexOf("snow") != -1 || weatherInfo.weather.indexOf("Snow") != -1) {
      weatherInfo.weather_flag = 4; // Snowy
    } else if (weatherInfo.weather.indexOf("mist") != -1 || weatherInfo.weather.indexOf("Mist") != -1) {
      weatherInfo.weather_flag = 0; // Foggy
    }
  }
  else {
    // Print a message if the WiFi connection is lost
    Serial.println("WiFi Disconnected");
  }

  return true;
}

// Define the HTTP GET request function
String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Initialize the HTTP client and specify the requested server URL
  http.begin(client, serverName);

  // Send an HTTP GET request
  httpResponseCode = http.GET();

  // Initialize the returned response content
  String payload = "{}";

  // Check the response code and process the response content
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode); // Print the response code
    payload = http.getString(); // Get the response content
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode); // Print the error code
  }
  // Release the HTTP client resources
  http.end();

  return payload; // Return the response content
}