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

// kyoto
// #define LATITUDE "35.09"
// #define LONGITUDE "135.55"


// Refresh rate for the weather data in minuites (min:1)
#define REFRESH_MINUITES 60

// location name
#define LOCATION_NAME "Toronto"

// Warning for the high UV index
#define UVI_THRESHOLD 3

// Forecast hour interval (min:1 to max:8)
#define HOUR_INTERVAL 3