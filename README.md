## Simple MQTT ESP8266 weather station

MQTT [PubSubClient library](https://github.com/knolleary/pubsubclient) for getting weather data (in openweathermap style json)

[ArduinoJson library](https://github.com/bblanchon/ArduinoJson) for data parsing

[u8g2 library](https://github.com/olikraus/u8g2) for LCD (currently with 128x64 ST7920)

[Adafruit AM2320 library](https://github.com/adafruit/Adafruit_AM2320) for inside temperature sensor

ArduinoOTA for simple updates

[Modified NTP library](https://github.com/taranais/NTPClient) by taranais for easier date acces

Made using [platformio](https://platformio.org/)

### MQTT
By default subscribes to `weather/'city'`, where it expects your where data to be. 

Weather data are expected in **JSON** format from [OpenWeatherMap](https://openweathermap.org/) [One Call API](https://openweathermap.org/api/one-call-api).

On succesfull connection to WiFi and MQTT broker, it sends it's information to topic under  `devices/'device_name'` (ip and time of connection). 
It also periodically update this topic with latest status (time sync, weather data update)
