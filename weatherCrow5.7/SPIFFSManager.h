#ifndef SPIFFS_MANAGER_H
#define SPIFFS_MANAGER_H

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>

class SPIFFSManager
{
private:
  static const char* WEATHER_DATA_FILE;
  static const char* CONNECTION_LOG_FILE;
  static const char* FAILURE_COUNT_FILE;
  static const int MAX_WEATHER_DATA_HISTORY = 3;
  static const int JSON_CAPACITY = 32768;

public:
  bool initSPIFFS();
  void saveWeatherDataToSPIFFS(const String& weatherData);
  bool loadLatestWeatherDataFromSPIFFS(DynamicJsonDocument& weatherApiResponse);
  void logConnectionAttempt(bool success, const String& errorMessage = "");
  int getConsecutiveFailureCount();
  void updateConsecutiveFailureCount(int count);
};

#endif
