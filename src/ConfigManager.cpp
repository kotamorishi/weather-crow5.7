/**
 * ConfigManager.cpp
 *
 * Implementation of web-based configuration using WiFiManager and Preferences.
 */

#include "ConfigManager.h"

// Static pointer for callback (WiFiManager doesn't support member function
// callbacks)
static ConfigManager *configManagerInstance = nullptr;
static WiFiManager *currentWM = nullptr;

ConfigManager::ConfigManager()
    : configButtonPin(-1), paramApiKey(nullptr), paramLatitude(nullptr),
      paramLongitude(nullptr), paramLocationName(nullptr),
      paramRefreshMinutes(nullptr), paramLowPowerMode(nullptr),
      paramHourInterval(nullptr) {

  // Set default values
  config.apiKey = "";
  config.latitude = "";
  config.longitude = "";
  config.locationName = "";
  config.refreshMinutes = DEFAULT_REFRESH_MINUTES;
  config.lowPowerMode = DEFAULT_LOW_POWER_MODE;
  config.hourInterval = DEFAULT_HOUR_INTERVAL;

  configManagerInstance = this;
}

bool ConfigManager::begin(int buttonPin) {
  configButtonPin = buttonPin;

  // Setup button pin with internal pullup
  if (configButtonPin >= 0) {
    pinMode(configButtonPin, INPUT_PULLUP);
  }

  // Load existing configuration from NVS
  loadConfig();

  Serial.println("[ConfigManager] Configuration loaded");
  Serial.print("[ConfigManager] API Key: ");
  Serial.println(config.apiKey.length() > 0 ? "(set)" : "(not set)");
  Serial.print("[ConfigManager] Location: ");
  Serial.println(config.locationName);

  // Check if config button is held - force config portal
  if (isConfigButtonHeld()) {
    Serial.println(
        "[ConfigManager] Config button held - starting config portal");
    return startConfigPortal("Manual Setup");
  }

  // Try to connect with saved credentials
  WiFiManager wm;
  wm.setDebugOutput(true);

  // Set timeout for connection attempts
  wm.setConnectTimeout(30);

  // Don't show config portal on connection failure (we'll handle it)
  wm.setEnableConfigPortal(false);

  // Try to connect
  bool connected = wm.autoConnect();

  if (!connected) {
    Serial.println("[ConfigManager] WiFi connection failed");

    // Start config portal when WiFi fails OR no configuration exists
    Serial.println("[ConfigManager] Starting config portal for setup...");

    // Include the SSID that was attempted in the reason
    String ssid = WiFi.SSID();
    String reason = "Opps! WiFi connection failed. Restarting router may help.";
    if (ssid.length() > 0) {
      reason = "WiFi connection failed: " + ssid;
    }
    return startConfigPortal(reason.c_str());
  }

  Serial.println("[ConfigManager] WiFi connected!");
  Serial.print("[ConfigManager] SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("[ConfigManager] IP: ");
  Serial.println(WiFi.localIP());

  return true;
}

bool ConfigManager::startConfigPortal(const char *reason) {
  WiFiManager wm;
  currentWM = &wm; // Store pointer for callback access

  // Setup custom parameters
  setupWiFiManager(wm);

  // Configure portal
  wm.setDebugOutput(true);
  wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT);
  wm.setBreakAfterConfig(true); // Exit after config even if no connection

  // Custom portal title
  wm.setTitle("Weather Crow Configuration");

  // Custom message after save - modify the "Saved" text
  wm.setCustomHeadElement("<script>"
                          "window.onload=function(){"
                          "var m=document.querySelector('.msg');"
                          "if(m && m.textContent=='Saved'){"
                          "m.innerHTML='Saved! Device is restarting...';"
                          "}"
                          "};"
                          "</script>");

  // Start the config portal (blocking)
  Serial.println("[ConfigManager] Starting config portal...");
  Serial.println("[ConfigManager] Connect to WiFi: 'WeatherCrow-Config'");
  Serial.println("[ConfigManager] Then navigate to: http://192.168.4.1");

  // Call the portal start callback to display on e-paper
  if (portalStartCallback) {
    portalStartCallback("WeatherCrow-Config", "192.168.4.1", reason);
  }

  bool result = wm.startConfigPortal("WeatherCrow-Config");

  if (result) {
    Serial.println("[ConfigManager] Portal completed - WiFi connected");

    // Read and save parameters
    readWiFiManagerParams(wm);
    saveConfig();

    Serial.println("[ConfigManager] Configuration saved");
  } else {
    Serial.println("[ConfigManager] Portal timeout or cancelled");
  }

  currentWM = nullptr;

  // Cleanup WiFiManager parameters
  delete paramApiKey;
  delete paramLatitude;
  delete paramLongitude;
  delete paramLocationName;
  delete paramRefreshMinutes;
  delete paramLowPowerMode;
  delete paramHourInterval;
  delete paramUnits;
  delete paramEnableAlerts;

  paramApiKey = nullptr;
  paramLatitude = nullptr;
  paramLongitude = nullptr;
  paramLocationName = nullptr;
  paramRefreshMinutes = nullptr;
  paramLowPowerMode = nullptr;
  paramHourInterval = nullptr;
  paramUnits = nullptr;
  paramEnableAlerts = nullptr;

  return result;
}

void ConfigManager::setupWiFiManager(WiFiManager &wm) {
  // Create custom parameters
  // Note: WiFiManager allocates char buffers, but we need to manage memory

  // API Key (max 64 chars)
  paramApiKey = new WiFiManagerParameter("api_key", "OpenWeatherMap API Key",
                                         config.apiKey.c_str(), 64);

  // Latitude
  paramLatitude = new WiFiManagerParameter(
      "latitude", "Latitude (e.g., 36.14109)", config.latitude.c_str(), 20);

  // Longitude
  paramLongitude = new WiFiManagerParameter(
      "longitude", "Longitude (e.g., 137.20987)", config.longitude.c_str(), 20);

  // Location Name
  paramLocationName =
      new WiFiManagerParameter("location", "Location Name (e.g., Toronto)",
                               config.locationName.c_str(), 32);

  // Refresh Minutes (number input)
  char refreshStr[8];
  snprintf(refreshStr, sizeof(refreshStr), "%d", config.refreshMinutes);
  paramRefreshMinutes =
      new WiFiManagerParameter("refresh", "Refresh Minutes (1-60)", refreshStr,
                               4, "type='number' min='1' max='60'");

  // Hour Interval (number input)
  char hourStr[4];
  snprintf(hourStr, sizeof(hourStr), "%d", config.hourInterval);
  paramHourInterval =
      new WiFiManagerParameter("hour_interval", "Forecast Hour Interval (1-6)",
                               hourStr, 2, "type='number' min='1' max='6'");

  // Low Power Mode (checkbox) - keep HTML minimal
  paramLowPowerMode = new WiFiManagerParameter(
      config.lowPowerMode
          ? "<br><label><input type='checkbox' name='low_power' value='1' "
            "checked> Low Power Mode</label>"
          : "<br><label><input type='checkbox' name='low_power' value='1'> Low "
            "Power Mode</label>");

  // Units dropdown (metric/imperial) - keep HTML minimal
  const char *unitsMetricSelected =
      config.units == "metric"
          ? "<br><label>Units</label><select name='units'><option "
            "value='metric' selected>Celsius</option><option "
            "value='imperial'>Fahrenheit</option></select>"
          : "<br><label>Units</label><select name='units'><option "
            "value='metric'>Celsius</option><option value='imperial' "
            "selected>Fahrenheit</option></select>";
  paramUnits = new WiFiManagerParameter(unitsMetricSelected);

  // Enable Alert Display (checkbox) - keep HTML minimal
  paramEnableAlerts = new WiFiManagerParameter(
      config.enableAlertDisplay
          ? "<br><label><input type='checkbox' name='alerts' value='1' "
            "checked> Show Weather Alerts</label>"
          : "<br><label><input type='checkbox' name='alerts' value='1'> Show "
            "Weather Alerts</label>");

  // Add parameters to WiFiManager
  wm.addParameter(paramApiKey);
  wm.addParameter(paramLatitude);
  wm.addParameter(paramLongitude);
  wm.addParameter(paramLocationName);
  wm.addParameter(paramRefreshMinutes);
  wm.addParameter(paramHourInterval);
  wm.addParameter(paramUnits);
  wm.addParameter(paramEnableAlerts);
  wm.addParameter(paramLowPowerMode);

  // Set save callback - use currentWM to access server args
  wm.setSaveParamsCallback([]() {
    if (configManagerInstance && currentWM) {
      configManagerInstance->saveParamsCallback(*currentWM);
    }
  });
}

void ConfigManager::saveParamsCallback(WiFiManager &wm) {
  Serial.println("[ConfigManager] Save params callback triggered");
  readWiFiManagerParams(wm);
  saveConfig();
}

void ConfigManager::readWiFiManagerParams(WiFiManager &wm) {
  if (paramApiKey) {
    config.apiKey = String(paramApiKey->getValue());
    config.apiKey.trim();
  }

  if (paramLatitude) {
    config.latitude = String(paramLatitude->getValue());
    config.latitude.trim();
  }

  if (paramLongitude) {
    config.longitude = String(paramLongitude->getValue());
    config.longitude.trim();
  }

  if (paramLocationName) {
    config.locationName = String(paramLocationName->getValue());
    config.locationName.trim();
  }

  if (paramRefreshMinutes) {
    int val = String(paramRefreshMinutes->getValue()).toInt();
    config.refreshMinutes = constrain(val, 1, 60);
    if (val == 0)
      config.refreshMinutes = DEFAULT_REFRESH_MINUTES;
  }

  if (paramHourInterval) {
    int val = String(paramHourInterval->getValue()).toInt();
    config.hourInterval = constrain(val, 1, 6);
    if (val == 0)
      config.hourInterval = DEFAULT_HOUR_INTERVAL;
  }

  // Read custom HTML parameters via server args
  if (wm.server) {
    // Units dropdown
    if (wm.server->hasArg("units")) {
      String units = wm.server->arg("units");
      if (units == "metric" || units == "imperial") {
        config.units = units;
      }
    }

    // Low Power Mode checkbox
    config.lowPowerMode = wm.server->hasArg("low_power");

    // Enable Alerts checkbox
    config.enableAlertDisplay = wm.server->hasArg("alerts");
  }

  Serial.println("[ConfigManager] Parameters read from portal:");
  Serial.print("  API Key: ");
  Serial.println(config.apiKey.length() > 0 ? "(set)" : "(empty)");
  Serial.print("  Latitude: ");
  Serial.println(config.latitude);
  Serial.print("  Longitude: ");
  Serial.println(config.longitude);
  Serial.print("  Location: ");
  Serial.println(config.locationName);
  Serial.print("  Refresh: ");
  Serial.println(config.refreshMinutes);
  Serial.print("  Hour Interval: ");
  Serial.println(config.hourInterval);
  Serial.print("  Low Power: ");
  Serial.println(config.lowPowerMode ? "true" : "false");
}

void ConfigManager::loadConfig() {
  preferences.begin(PREFS_NAMESPACE, true); // Read-only mode

  config.apiKey = preferences.getString("api_key", "");
  config.latitude = preferences.getString("latitude", "");
  config.longitude = preferences.getString("longitude", "");
  config.locationName = preferences.getString("location", "");
  config.refreshMinutes =
      preferences.getInt("refresh", DEFAULT_REFRESH_MINUTES);
  config.lowPowerMode =
      preferences.getBool("low_power", DEFAULT_LOW_POWER_MODE);
  config.hourInterval = preferences.getInt("hour_int", DEFAULT_HOUR_INTERVAL);
  config.units = preferences.getString("units", "metric");
  config.enableAlertDisplay = preferences.getBool("alerts", true);

  preferences.end();

  Serial.println("[ConfigManager] Loaded config from NVS");
  Serial.print("  API Key: ");
  Serial.println(config.apiKey.length() > 0 ? "(set)" : "(empty)");
  Serial.print("  Latitude: ");
  Serial.println(config.latitude);
  Serial.print("  Longitude: ");
  Serial.println(config.longitude);
  Serial.print("  Location: ");
  Serial.println(config.locationName);
  Serial.print("  Refresh: ");
  Serial.println(config.refreshMinutes);
  Serial.print("  Hour Interval: ");
  Serial.println(config.hourInterval);
  Serial.print("  Low Power: ");
  Serial.println(config.lowPowerMode ? "true" : "false");
}

void ConfigManager::saveConfig() {
  preferences.begin(PREFS_NAMESPACE, false); // Read-write mode

  preferences.putString("api_key", config.apiKey);
  preferences.putString("latitude", config.latitude);
  preferences.putString("longitude", config.longitude);
  preferences.putString("location", config.locationName);
  preferences.putInt("refresh", config.refreshMinutes);
  preferences.putBool("low_power", config.lowPowerMode);
  preferences.putInt("hour_int", config.hourInterval);
  preferences.putString("units", config.units);
  preferences.putBool("alerts", config.enableAlertDisplay);

  preferences.end();

  Serial.println("[ConfigManager] Saved config to NVS");
}

void ConfigManager::resetConfig() {
  // Clear NVS
  preferences.begin(PREFS_NAMESPACE, false);
  preferences.clear();
  preferences.end();

  // Clear WiFi credentials
  WiFiManager wm;
  wm.resetSettings();

  // Reset to defaults
  config.apiKey = "";
  config.latitude = "";
  config.longitude = "";
  config.locationName = "";
  config.refreshMinutes = DEFAULT_REFRESH_MINUTES;
  config.lowPowerMode = DEFAULT_LOW_POWER_MODE;
  config.hourInterval = DEFAULT_HOUR_INTERVAL;
  config.units = "metric";
  config.enableAlertDisplay = true;

  Serial.println("[ConfigManager] Configuration reset to defaults");
}

bool ConfigManager::isConfigured() const { return config.isValid(); }

bool ConfigManager::isConfigButtonHeld() {
  if (configButtonPin < 0) {
    return false;
  }

  // Check if button is pressed (active LOW with pullup)
  if (digitalRead(configButtonPin) != LOW) {
    return false;
  }

  Serial.println(
      "[ConfigManager] Config button pressed, checking hold duration...");

  // Wait and check if still held
  unsigned long startTime = millis();
  while (millis() - startTime < CONFIG_BUTTON_HOLD_TIME) {
    if (digitalRead(configButtonPin) != LOW) {
      Serial.println("[ConfigManager] Button released before timeout");
      return false;
    }
    delay(50);
  }

  Serial.println("[ConfigManager] Button held for required duration");
  return true;
}
