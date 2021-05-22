/*----(LIBRARIES)----*/
#include <esp8266httpclient.h>
//time
#include <WiFiUdp.h>
#include <NTPClient.h>
//mqtt
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/*----(MACROS)----*/
#define SSID "Konrad_2.4GHz"
#define PASSWORD "FD053449D3"
#define SERVER_IP "192.168.1.176"
#define CLIENT_ID "WeatherBug_main_station"

#define SECOND 1000

/*----(VARIABLES)----*/
WiFiClient espClient;   //create client
PubSubClient client(espClient); //setup mqtt client with client
WiFiUDP ntpUDP;
NTPClient time_client(ntpUDP, "192.168.1.1");

//owm
String city_id = "3075127";
String lon = "14.34";
String lat = "50.13";
String API_key = "1f4a8316a93acec4af8db5017bcbd678";
String weather_link = "http://api.openweathermap.org/data/2.5/weather?id=" + city_id + "&APPID=" + API_key;
String onecall_link = "http://api.openweathermap.org/data/2.5/onecall?lat=" + lat + "&lon=" + lon + "&exclude=hourly,minutely&appid=" + API_key;

struct device_data {
    String device = "devices/WeatherBug";
    String ip = "devices/WeatherBug/ip";
    String connected = "devices/WeatherBug/connection time"; 
} device_data;

//String device_path = "devices/WeatherBug";
//String date_path = "date";
//String weather_path = "omw";

int value = 0;

//timers
unsigned long time_update = 0;
unsigned long time_print = 0;
unsigned long msg_update = 0;


void startWifi() {
    Serial.printf("[WiFI] Connecting to: %s\n", SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    while(WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.printf(".");
    }
    Serial.printf("\n");
}

void updateTime() {
    time_client.forceUpdate();
    Serial.printf("[NTP] Time update: %s\n", time_client.getFormattedTime().c_str());
}

void updateStatus(String status) {
    client.publish(device_date.device.c_str(), (time_client.getFormattedTime() + " " + status).c_str(), true);
}

void setup() {
    Serial.begin(115200);
    
    startWifi();    //connect to wifi
    //start mqtt client
    client.setServer(SERVER_IP, 1883);
    
    //NTP
    time_client.begin();
    time_client.setTimeOffset(7200);
    delay(2000);
    updateTime();
}

void loop() {

    //update time every minute
    if (millis() - time_update >= SECOND*60) {
        time_update = millis();
        updateTime();
        updateStatus("time update");
    }

    //loop and ask if connected
    if (!client.loop()) {
        Serial.printf("%s ", time_client.getFormattedTime().c_str());
        Serial.printf("[MQTT] Trying to connect... ");
        if (client.connect(CLIENT_ID)) {
            Serial.printf("connected\n");
            client.subscribe("inTopic");
        }
        else {
            Serial.printf("failed (%d). Retrying in 2 seconds\n", client.state());
            delay(2000);
        }
    };

    if (millis() - msg_update > 2000) {
        msg_update = millis();
        ++value;
        String msg = "hello world #" + String(value);
        Serial.printf("%s ", time_client.getFormattedTime().c_str());
        Serial.print("[Client] Publish message: ");
        Serial.println(msg);
        client.publish("outTopic", msg.c_str());
        updateStatus("message sent");
    }

    //try to reconnect if disconnected
    if (!WiFi.isConnected()) {
        Serial.printf("%s ", time_client.getFormattedTime().c_str());
        Serial.printf("[WiFi] Disconnected\n");
        startWifi();
    }
}
