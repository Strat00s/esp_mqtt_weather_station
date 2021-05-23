## Simple MQTT ESP8266 weather station

MQTT for getting weather data (in openweathermap style json)

u8g2 library for LCD (currently with 128x64 ST7920)

Adafruit AM2320 library for inside temperature sensor

ArduinoOTA for simple updates

Made with platformio

### MQTT
By default subscribes to `weather/'city'`, where it expects your where data to be. 

Weather data are expected in **JSON** format from [OpenWeatherMap](https://openweathermap.org/) [One Call API](https://openweathermap.org/api/one-call-api).

On succesfull connection to WiFi and MQTT broker, it sends it's information to topic under  `devices/'device_name'` (ip and time of connection). 
It also periodically update this topic with latest status (time sync, weather data update)
