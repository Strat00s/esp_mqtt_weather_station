/*----(LIBRARIES)----*/
#include <esp8266httpclient.h>
//time
#include <WiFiUdp.h>
#include <NTPClient.h>
//mqtt
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <ArduinoJson.h>

#include <U8g2lib.h>

/*----(MACROS)----*/
#define SECOND 1000
#define MINUTE 60000
#define HOUR 3600000

#define LEN(x) sizeof(x) / sizeof(x[0])

#define LCD_WIDTH 128
#define LCD_HEIGHT 64

#define LCD_CLEAR_AREA(x, y, w, h) u8g2.setDrawColor(0);\
                                   u8g2.drawBox(x, y, w, h);\
                                   u8g2.setDrawColor(1)\

/*----(PROTOTYPES)----*/
typedef struct {
    float day_temp = 0.0;
    float night_temp = 0.0;
    float temp = 0.0;
    int humidity = 0;
    float pressure = 0.0;
    float wind_speed = 0.0;
    String icon = "";
} DayData;

/*----(VARIABLES)----*/
String ssid = "Konrad_2.4GHz";
String passw = "FD053449D3";
String mqtt_addr = "192.168.1.176";
String ntp_addr = "192.168.1.1";
String city = "Horoměřice";

WiFiClient espClient;                               //create wificlient
PubSubClient client(espClient);                     //setup mqtt client
WiFiUDP ntpUDP;                                     //create wifiudp
NTPClient time_client(ntpUDP, ntp_addr.c_str());    //setup time client width server on 192.168.1.1
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R2, 15, 16);  //LCD config

//device data
String device_name = "WeatherBug";
bool startup = true;

DayData curr_day;
DayData forecast[3];

int screen = 0;

float inside_temp;


//timers
unsigned long screen_timer = 0;
unsigned long footer_timer = 0;
unsigned long sync_timer = 0;
//unsigned long time_update = 0;
//unsigned long time_print = 0;
//unsigned long msg_update = 0;

//LCD printing
void bubbleAnimation(int current_bubble, int min, int max, int location, int spread, char *text){
    LCD_CLEAR_AREA(LCD_WIDTH/2 - (spread+max+1), location - max, (spread+max+1)*2, (max+1)*2);  //clear area

    //center text (if there is any)
    if (text != NULL){
        int txt_width = u8g2.getStrWidth(text);     //get text width
        int txt_start = LCD_WIDTH/2 - txt_width/2;  //lcd width/2 - text width/2 -> where we should start to print
        u8g2.drawStr(txt_start, 20, text);          //print the text
    }

    //draw animation animation (center bubble centered)
    switch ((current_bubble) % 3){
        case 0:
            u8g2.drawCircle(64 - spread, location, max);
            u8g2.drawDisc  (64,          location, min);
            u8g2.drawDisc  (64 + spread, location, min);
            break;
        case 1:
            u8g2.drawDisc  (64 - spread, location, min);
            u8g2.drawCircle(64,          location, max);
            u8g2.drawDisc  (64 + spread, location, min);
            break;
        case 2:
            u8g2.drawDisc  (64 - spread, location, min);
            u8g2.drawDisc  (64,          location, min);
            u8g2.drawCircle(64 + spread, location, max);
            break;
    }
}

void footer(){
    static bool separator = true;
    u8g2.setFont(u8g2_font_profont11_tf);   //set font
    u8g2.drawHLine(0, 54, 128);             //draw horizontal line (start x, y, width)
    //time
    LCD_CLEAR_AREA(0, 55, 30, 9);                   //clear area for separator
    u8g2.setCursor(2, 64);                          //move cursor to time
    u8g2.printf("%02d", time_client.getHours());    //print hours
    u8g2.setCursor(19, 64);
    u8g2.printf("%02d", time_client.getMinutes());  //print minutes
    u8g2.setCursor(13, 63);
    if (separator) u8g2.printf(":");                //print separator every other call
    separator = !separator;                         //flip separator
    //temp
    u8g2.setCursor(97, 64);                         //move cursor to temp
    u8g2.printf("%.1f\xb0%c", inside_temp, 'C');    //print temperature
}

void timeScreen() {
    LCD_CLEAR_AREA(0, 0, 128, 54);
    char *text = "time";
    int txt_width = u8g2.getStrWidth(text);     //get text width
    int txt_start = LCD_WIDTH/2 - txt_width/2;  //lcd width/2 - text width/2 -> where we should start to print
    u8g2.drawStr(txt_start, 20, text);          //print the text
}

void weatherScreen() {
    LCD_CLEAR_AREA(0, 0, 128, 54);
    char *text = "weather";
    int txt_width = u8g2.getStrWidth(text);     //get text width
    int txt_start = LCD_WIDTH/2 - txt_width/2;  //lcd width/2 - text width/2 -> where we should start to print
    u8g2.drawStr(txt_start, 20, text);          //print the text
}

void forecastScreen() {
    LCD_CLEAR_AREA(0, 0, 128, 54);
    char *text = "forecast";
    int txt_width = u8g2.getStrWidth(text);     //get text width
    int txt_start = LCD_WIDTH/2 - txt_width/2;  //lcd width/2 - text width/2 -> where we should start to print
    u8g2.drawStr(txt_start, 20, text);          //print the text
}

void startWifi() {
    Serial.printf("[WiFI] Connecting to: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), passw.c_str());
    int timer = 0;
    while(WiFi.status() != WL_CONNECTED) {
        bubbleAnimation(timer, 2, 5, 40, 18, "Connecting to WiFi"); //run bubble animation
        u8g2.sendBuffer();                                      //write it to screen
        timer++;                                                //increase timeout
        delay(500);                                             //delay
        if (timer >= 20) ESP.restart();                         //reset after 10 seconds
    }
}

//TODO change
void updateTime() {
    time_client.forceUpdate();
    Serial.printf("[NTP] Time update: %s\n", time_client.getFormattedTime().c_str());
}

//TODO change
void updateStatus(String status) {
    client.publish(("devices/" + device_name).c_str(), (time_client.getFormattedTime() + " " + status).c_str(), true);
}

//update weather data on new message (as we are listening on single topic)
void onMessage(char* topic, byte* payload, unsigned int length) {
    if (String(topic) != String("weather/Horoměřice")) return;

    Serial.printf("%s ", time_client.getFormattedTime().c_str());
    Serial.printf("[MQTT] Got message on topic '%s':\n", topic);
    
    //get data as json
    DynamicJsonDocument doc(6144);
    deserializeJson(doc, payload);
    JsonObject root = doc.as<JsonObject>();

    //save current day
    curr_day.temp       = (float)(root["current"]["temp"]) - 273.15;
    curr_day.humidity   =   (int)(root["current"]["humidity"]);
    curr_day.pressure   = (float)(root["current"]["pressure"] )/ 1000.0;
    curr_day.wind_speed = (float)(root["current"]["wind_speed"]);
    curr_day.icon       =        (root["current"]["weather"][0]["icon"].as<String>());

    //save forecast
    for (unsigned int i = 0; i < LEN(forecast); i++) {
        forecast[i].day_temp   = (float)(root["daily"][i+1]["temp"]["day"]) - 273.15;
        forecast[i].night_temp = (float)(root["daily"][i+1]["temp"]["night"]) - 273.15;
        forecast[i].icon       =         root["daily"][i+1]["weather"][0]["icon"].as<String>();
    }
}

void setup() {
    delay(500);
    //LCD
    u8g2.begin();
    u8g2.setFont(u8g2_font_bitcasual_tr);

    //serial
    Serial.begin(115200);                       //start serial monitor

    //wifi
    startWifi();                                //connect to wifi

    //mqtt
    client.setServer(mqtt_addr.c_str(), 1883);  //set mqtt server
    client.setBufferSize(8192);                 //set buffer to 8kb for weather data
    client.setCallback(onMessage);              //set message callback
    
    //NTP
    time_client.begin();                        //start ntp
    time_client.setTimeOffset(7200);            //set offset
    delay(500);                                //wait because reasons
    updateTime();                               //update time from ntp server

    u8g2.clearBuffer();                         //clear buffer before starting
    footer();
    u8g2.sendBuffer();
}

void loop() {

    //update footer every second
    if (millis() - footer_timer >= SECOND) {
        footer_timer = millis();
        footer();
        u8g2.sendBuffer();
    }

    if (millis() - screen_timer >= SECOND*4) {
        screen_timer = millis();
        
        switch (screen) {
            case 0: timeScreen();     break;
            case 1: weatherScreen();  break;
            case 2: forecastScreen(); break;
        }
        //footer();
        bubbleAnimation(screen, 1, 2, 59, 12, NULL);
        screen++;
        if (screen > 2) screen = 0;
        u8g2.sendBuffer();
    }

    if (millis() - sync_timer >= MINUTE) {
        sync_timer = millis();
        updateTime();
        updateStatus("time update");
    }

    //loop and ask if connected
    if(!client.loop()) {
        Serial.printf("%s ", time_client.getFormattedTime().c_str());
        Serial.printf("[MQTT] Trying to connect... ");
        if (client.connect(device_name.c_str())) {
            Serial.printf("connected\n");
            //subscribe to topics we want/need
            client.subscribe(("weather/" + city).c_str());
        }
        else {
            Serial.printf("failed (%d). Retrying in 2 seconds\n", client.state());
            delay(2000);
        }
    }
    
    if (startup) {
        startup = false;
        Serial.printf("%s ", time_client.getFormattedTime().c_str());
        Serial.printf("[ESP] Uploading device information\n");
        //publish device info
        client.publish(("devices/" + device_name + "/ip").c_str(), WiFi.localIP().toString().c_str(), true);
        client.publish(("devices/" + device_name + "/connected").c_str(), time_client.getFormattedTime().c_str(), true);
        //publish weather request topic
        client.publish(("weather/requests/" + city).c_str(), "1", true);
        updateStatus("connected");
    }

    //try to reconnect if disconnected
    if (!WiFi.isConnected()) {
        Serial.printf("%s ", time_client.getFormattedTime().c_str());
        Serial.printf("[WiFi] Disconnected\n");
        startWifi();
        //update device data
        startup = true;
    }
}
