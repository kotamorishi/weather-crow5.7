/**
 * ConfigManager.h
 *
 * Web-based configuration management using WiFiManager and Preferences.
 * Provides a captive portal for configuring WiFi and custom parameters.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFiManager.h>

// Default configuration values
#define DEFAULT_REFRESH_MINUTES 30
#define DEFAULT_HOUR_INTERVAL 3
#define DEFAULT_LOW_POWER_MODE true

// Button hold duration to trigger config portal (milliseconds)
#define CONFIG_BUTTON_HOLD_TIME 3000

// WiFiManager portal timeout (seconds) - auto-closes if no activity
#define CONFIG_PORTAL_TIMEOUT 300

// Preferences namespace
#define PREFS_NAMESPACE "weather-crow"

/**
 * Configuration structure holding all user-configurable parameters
 */
struct AppConfig {
  String apiKey;
  String latitude;
  String longitude;
  String locationName;
  int refreshMinutes;
  bool lowPowerMode;
  int hourInterval;
  String units; // "metric" or "imperial"
  bool enableAlertDisplay;

  // Default constructor with sensible defaults
  AppConfig()
      : refreshMinutes(15), lowPowerMode(false), hourInterval(1),
        units("metric"), enableAlertDisplay(true) {}

  // Check if config has been set (API key is required)
  bool isValid() const {
    return apiKey.length() > 0 && latitude.length() > 0 &&
           longitude.length() > 0;
  }
};

/**
 * ConfigManager - Handles WiFi configuration and persistent storage
 *
 * Usage:
 *   ConfigManager configManager;
 *   configManager.begin(CONFIG_BUTTON_PIN);
 *
 *   // Access configuration
 *   String apiKey = configManager.getApiKey();
 */
class ConfigManager {
public:
  ConfigManager();

  /**
   * Initialize ConfigManager with button pin for manual config trigger
   * @param buttonPin GPIO pin connected to config button (active LOW)
   * @return true if connected to WiFi, false if in config portal mode
   */
  bool begin(int buttonPin);

  /**
   * Force start the configuration portal
   * Blocks until configuration is complete or timeout
   * @return true if configuration was saved
   */
  bool startConfigPortal(const char *reason);

  /**
   * Load configuration from Preferences (NVS)
   */
  void loadConfig();

  /**
   * Save configuration to Preferences (NVS)
   */
  void saveConfig();

  /**
   * Reset configuration to defaults and clear stored WiFi credentials
   */
  void resetConfig();

  /**
   * Check if valid configuration exists
   */
  bool isConfigured() const;

  // Configuration getters
  String getApiKey() const { return config.apiKey; }
  String getLatitude() const { return config.latitude; }
  String getLongitude() const { return config.longitude; }
  String getLocationName() const { return config.locationName; }
  int getRefreshMinutes() const { return config.refreshMinutes; }
  bool getLowPowerMode() const { return config.lowPowerMode; }
  int getHourInterval() const { return config.hourInterval; }
  String getUnits() const { return config.units; }
  bool getEnableAlertDisplay() const { return config.enableAlertDisplay; }

  // Get the full config struct
  const AppConfig &getConfig() const { return config; }

  /**
   * Set a callback function that will be called before the config portal starts
   * Use this to display instructions on the e-paper screen
   * @param callback Function pointer that takes AP name, IP address, and reason
   */
  void setPortalStartCallback(void (*callback)(const char *apName,
                                               const char *ipAddress,
                                               const char *reason)) {
    portalStartCallback = callback;
  }

private:
  AppConfig config;
  Preferences preferences;
  int configButtonPin;
  void (*portalStartCallback)(const char *, const char *,
                              const char *) = nullptr;

  // WiFiManager custom parameters
  WiFiManagerParameter *paramApiKey;
  WiFiManagerParameter *paramLatitude;
  WiFiManagerParameter *paramLongitude;
  WiFiManagerParameter *paramLocationName;
  WiFiManagerParameter *paramRefreshMinutes;
  WiFiManagerParameter *paramLowPowerMode;
  WiFiManagerParameter *paramHourInterval;
  WiFiManagerParameter *paramUnits;
  WiFiManagerParameter *paramEnableAlerts;

  /**
   * Check if config button is being held during boot
   */
  bool isConfigButtonHeld();

  /**
   * Setup WiFiManager with custom parameters
   */
  void setupWiFiManager(WiFiManager &wm);

  /**
   * Callback when WiFiManager saves parameters
   */
  void saveParamsCallback(WiFiManager &wm);

  /**
   * Read parameters from WiFiManager after portal closes
   */
  void readWiFiManagerParams(WiFiManager &wm);
};

#endif // CONFIG_MANAGER_H
