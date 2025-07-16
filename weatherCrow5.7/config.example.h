// Configuration file for weatherCrow5.7

// Network configuration
#define WIFI_SSID "Your WiFi SSID"
#define WIFI_PASSWORD "Your WiFi Password"

// OpenWeatherMap API key
#define OPEN_WEATHER_MAP_API_KEY "b0123456789012345678901234567890" // Enter your OpenWeatherMap API key here

// choose metric or imperial
#define UNITS "metric"

// Location, use google maps to get the latitude and longitude of your location, https://www.google.com/maps
#define LATITUDE "20.4255911"
#define LONGITUDE "136.0809294"

// Tokyo station
// #define LATITUDE "35.68"
// #define LONGITUDE "139.75"

// Kyoto
// #define LATITUDE "35.09"
// #define LONGITUDE "135.55"

// New York
// #define LATITUDE "40.75"
// #define LONGITUDE "-73.98"


// Refresh rate for the weather data in minutes (min:10)
#define REFRESH_MINUTES 60

// location name
#define LOCATION_NAME "Toronto"

// Warning for the high UV index
#define UVI_THRESHOLD 3

// Forecast hour interval (min:1 to max:8)
#define HOUR_INTERVAL 3

// Low power mode, when you powring the device with battery set to true
#define LOW_POWER_MODE true

// alert display
#define ENABLE_ALERT_DISPLAY false