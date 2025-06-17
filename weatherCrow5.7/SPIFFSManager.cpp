#include "SPIFFSManager.h"

const char *SPIFFSManager::WEATHER_DATA_FILE = "/weather_data.json";
const char *SPIFFSManager::CONNECTION_LOG_FILE = "/connection_log.json";
const char *SPIFFSManager::FAILURE_COUNT_FILE = "/failure_count.json";

bool SPIFFSManager::initSPIFFS()
{
    if (!SPIFFS.begin(true))
    {
        Serial.println("Failed to initialize SPIFFS");
        return false;
    }
    Serial.println("SPIFFS initialized successfully");
    return true;
}

void SPIFFSManager::saveWeatherDataToSPIFFS(const String &weatherData)
{
    DynamicJsonDocument dataArray(JSON_CAPACITY * 2);

    // Load existing data
    File file = SPIFFS.open(WEATHER_DATA_FILE, "r");
    if (file)
    {
        deserializeJson(dataArray, file);
        file.close();
    }

    // Create new entry
    DynamicJsonDocument newEntry(JSON_CAPACITY);
    deserializeJson(newEntry, weatherData);
    newEntry["saved_timestamp"] = millis();

    // Create new array with the new entry at the beginning
    DynamicJsonDocument newDataArray(JSON_CAPACITY * 2);
    JsonArray newArray = newDataArray.to<JsonArray>();

    // Add new entry first
    newArray.add(newEntry);

    // Add existing entries (up to MAX_WEATHER_DATA_HISTORY - 1)
    if (dataArray.is<JsonArray>())
    {
        JsonArray oldArray = dataArray.as<JsonArray>();
        int entriesToCopy = min((int)oldArray.size(), MAX_WEATHER_DATA_HISTORY - 1);
        for (int i = 0; i < entriesToCopy; i++)
        {
            newArray.add(oldArray[i]);
        }
    }

    // Save back to file
    file = SPIFFS.open(WEATHER_DATA_FILE, "w");
    if (file)
    {
        serializeJson(newDataArray, file);
        file.close();
        Serial.println("Weather data saved to SPIFFS");
    }
    else
    {
        Serial.println("Failed to save weather data to SPIFFS");
    }
}

bool SPIFFSManager::loadLatestWeatherDataFromSPIFFS(DynamicJsonDocument &weatherApiResponse)
{
    File file = SPIFFS.open(WEATHER_DATA_FILE, "r");
    if (!file)
    {
        Serial.println("No saved weather data found");
        return false;
    }

    DynamicJsonDocument dataArray(JSON_CAPACITY * 2);
    if (deserializeJson(dataArray, file) != DeserializationError::Ok)
    {
        file.close();
        Serial.println("Failed to parse saved weather data");
        return false;
    }
    file.close();

    JsonArray array = dataArray.as<JsonArray>();
    if (array.size() == 0)
    {
        Serial.println("No weather data entries found");
        return false;
    }

    // Copy the most recent entry to weatherApiResponse
    weatherApiResponse.clear();
    weatherApiResponse.set(array[0]);
    Serial.println("Loaded weather data from SPIFFS");
    return true;
}

void SPIFFSManager::logConnectionAttempt(bool success, const String &errorMessage)
{
    DynamicJsonDocument logArray(4096);

    // Load existing log
    File file = SPIFFS.open(CONNECTION_LOG_FILE, "r");
    if (file)
    {
        deserializeJson(logArray, file);
        file.close();
    }

    // Create new log entry
    DynamicJsonDocument newEntry(512);
    newEntry["timestamp"] = millis();
    newEntry["success"] = success;
    if (!success && errorMessage.length() > 0)
    {
        newEntry["error"] = errorMessage;
    }

    // Create new array with the new entry at the beginning
    DynamicJsonDocument newLogArray(4096);
    JsonArray newArray = newLogArray.to<JsonArray>();

    // Add new entry first
    newArray.add(newEntry);

    // Add existing entries (up to 9 more)
    if (logArray.is<JsonArray>())
    {
        JsonArray oldArray = logArray.as<JsonArray>();
        int entriesToCopy = min((int)oldArray.size(), 9);
        for (int i = 0; i < entriesToCopy; i++)
        {
            newArray.add(oldArray[i]);
        }
    }

    // Save back to file
    file = SPIFFS.open(CONNECTION_LOG_FILE, "w");
    if (file)
    {
        serializeJson(newLogArray, file);
        file.close();
    }
}

int SPIFFSManager::getConsecutiveFailureCount()
{
    File file = SPIFFS.open(FAILURE_COUNT_FILE, "r");
    if (!file)
    {
        return 0;
    }

    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, file) != DeserializationError::Ok)
    {
        file.close();
        return 0;
    }
    file.close();

    return doc["count"].as<int>();
}

void SPIFFSManager::updateConsecutiveFailureCount(int count)
{
    DynamicJsonDocument doc(256);
    doc["count"] = count;
    doc["last_updated"] = millis();

    File file = SPIFFS.open(FAILURE_COUNT_FILE, "w");
    if (file)
    {
        serializeJson(doc, file);
        file.close();
    }
}

long SPIFFSManager::getLastConnectionTimestamp()
{
    // use loadLatestWeatherDataFromSPIFFS to load latest weather data
    DynamicJsonDocument weatherApiResponse(JSON_CAPACITY);
    if (loadLatestWeatherDataFromSPIFFS(weatherApiResponse))
    {
        if (weatherApiResponse.containsKey("dt"))
        {
            return weatherApiResponse["dt"].as<long>() + weatherApiResponse["timezone_offset"].as<long>();
        }
    }

    return 0;
}
