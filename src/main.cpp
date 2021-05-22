/*----(LIBRARIES)----*/
#include <esp8266httpclient.h>
//time
#include <WiFiUdp.h>
#include <NTPClient.h>
//mqtt and data
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
//lcd
#include <U8g2lib.h>
//sensor
#include <Adafruit_Sensor.h>
#include <Adafruit_AM2320.h>
//Icons
#include "weather_icons.h"

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
    float uvi = 0.0;
    float pressure = 0.0;
    float wind_speed = 0.0;
    String icon = "";
} DayData;

/*----(CONSTANTS)----*/
const String ssid = "Konrad_2.4GHz";
const String passw = "FD053449D3";
const String mqtt_addr = "192.168.1.176";
const String ntp_addr = "192.168.1.1";
const String city = "Horoměřice";
const String device_name = "WeatherBug";

/*----(VARIABLES)----*/
//init
WiFiClient espClient;                               //create wificlient
PubSubClient client(espClient);                     //setup mqtt client
WiFiUDP ntpUDP;                                     //create wifiudp
NTPClient time_client(ntpUDP, ntp_addr.c_str());    //setup time client width server on 192.168.1.1
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R2, 15, 16);  //LCD config
Adafruit_AM2320 am2320 = Adafruit_AM2320();         //am2320 sensor

//weekdays for time screen and forecast
char days_of_week[7][10] = {"Neděle", "Pondělí", "Úterý", "Středa", "Čtvrtek", "Pátek", "Sobota"};
char days_of_week_short[7][4] = {"NE", "PO", "UT", "ST", "CT", "PA", "SO"};

bool startup = true;    //startup bool

DayData curr_day;       //current day data storage
DayData forecast[4];    //next 4 days forecast
float inside_temp;      //inside temperature

int screen = 0;         //current screen to show

//timers
unsigned long screen_timer = 0;
unsigned long footer_timer = 0;
unsigned long sync_timer = 0;



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
    //clear footer area
    LCD_CLEAR_AREA(0, 55, 30, 9);           //time
    LCD_CLEAR_AREA(98, 55, 30, 9);           //temp
    u8g2.setFont(u8g2_font_profont11_tf);   //set font
    u8g2.drawHLine(0, 54, 128);             //draw horizontal line (start x, y, width)
    //time
    u8g2.setCursor(2, 64);                          //move cursor to time
    u8g2.printf("%02d", time_client.getHours());    //print hours
    u8g2.setCursor(19, 64);
    u8g2.printf("%02d", time_client.getMinutes());  //print minutes
    u8g2.setCursor(13, 63);
    if (separator) u8g2.printf(":");                //print separator every other call
    separator = !separator;                         //flip separator
    //temp
    u8g2.setCursor(91, 64);                         //move cursor to temp
    u8g2.printf("%02.1f\xb0%c", inside_temp, 'C');    //print temperature
}

void timeScreen() {
    LCD_CLEAR_AREA(0, 0, 128, 54);  //clear screen part 
    //variables
    char tmp[10];
    int txt_width;
    int start_width;
    String day =   time_client.getFormattedDate().substring(8, 10);
    String month = time_client.getFormattedDate().substring(5, 7);
    String year =  time_client.getFormattedDate().substring(0, 4);
    //weekday
    u8g2.setFont(u8g2_font_6x12_te);                        //set font (for diacritics)
    sprintf(tmp, "%s", days_of_week[time_client.getDay()]); //get weekday from array
    txt_width = u8g2.getStrWidth(tmp);                      //get text width
    start_width = LCD_WIDTH/2 - txt_width/2;                //get center
    u8g2.drawUTF8(start_width, 12, tmp);                    //print day in center (UTF!!!)
    //date
    u8g2.setFont(u8g2_font_profont15_tr);
    sprintf(tmp, "%s.%s.%s", day.c_str(), month.c_str(), year.c_str());
    txt_width = u8g2.getStrWidth(tmp);
    start_width = LCD_WIDTH/2 - txt_width/2;
    u8g2.drawStr(start_width, 26, tmp);

    //time
    u8g2.setFont(u8g2_font_profont22_tr);
    sprintf(tmp, "%s", time_client.getFormattedTime().c_str());
    txt_width = u8g2.getStrWidth(tmp);
    start_width = LCD_WIDTH/2 - txt_width/2;
    u8g2.drawStr(start_width, 46, tmp);
}

void weatherScreen() {
    LCD_CLEAR_AREA(0, 0, 128, 54);
    char tmp[16];   //variable to store strings

    //get correct bitmap
    bool day = (curr_day.icon[2] == 'd') ? true : false;    //get the icon letter (night or day)
    int type = curr_day.icon.substring(0, 2).toInt();   //convert icon number from string to int
    Serial.printf("Icon: %s Day: %d type: %d\n", curr_day.icon.c_str(), day, type);

    //draw the bitmap (64x42)
    switch (type){
        case 1:  u8g2.drawXBM(0-12, 0, 64, 42, (day) ? clear_sky3_day_bits : clear_sky_night_bits);   break;    //12-12 0^ 2 | 17-18 1^ 1
        case 2:  u8g2.drawXBM(0-8,  0, 64, 42, (day) ? few_clouds_day_bits : few_clouds1_night_bits); break;    //8-5   0^ 4 | 8-10  6^ 4
        case 3:  u8g2.drawXBM(0-8,  0, 64, 42, scattered_clouds_bits);                                break;    //8-8   5^ 7
        case 4:  u8g2.drawXBM(0-8,  0, 64, 42, broken_clouds_bits);                                   break;    //8-8   5^ 7
        case 9:  u8g2.drawXBM(0-8,  0, 64, 42, shower_rain_bits);                                     break;    //8-8   0^ 1
        case 10: u8g2.drawXBM(0-5,  0, 64, 42, (day) ? rain_day_bits : rain_night_bits);              break;    //10-10 0^ 0 | 10-8  0^0
        case 11: u8g2.drawXBM(0-8,  0, 64, 42, thunderstorm_bits);                                    break;    //8-8   0^ 1
        case 13: u8g2.drawXBM(0-8,  0, 64, 42, snow1_bits);                                           break;    //8-8   0^ 0
        case 50: u8g2.drawXBM(0-8,  0, 64, 42, mist_bits);                                            break;    //8-8   5^ 5
        default: break;
    }

    //print temperature
    u8g2.setFont(u8g2_font_profont15_tf);
    sprintf(tmp, "%.1f\xb0", curr_day.temp);
    int txt_start = 54/2 - u8g2.getStrWidth(tmp)/2;
    u8g2.drawStr(txt_start, 53, tmp);
    //humidity
    u8g2.setFont(u8g2_font_profont11_tf);
    sprintf(tmp, "%d%%", curr_day.humidity);
    u8g2.drawXBM(58, 12, 11, 12, humidity2_bits);
    u8g2.drawStr(75, 22, tmp);
    //pressure
    sprintf(tmp, "%.3fbar", curr_day.pressure);
    u8g2.drawXBM(56, 28, 15, 7, pressure2_bits);
    u8g2.drawStr(75, 36, tmp);
    //wind speed
    sprintf(tmp, "%.1fm/s", curr_day.wind_speed);
    u8g2.drawXBM(58, 40, 11, 11, speed_bits);
    u8g2.drawStr(75, 50, tmp);
    //city name and gps icon
    u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
    u8g2.drawStr(56, 9, "\x47");
    u8g2.setFont(u8g2_font_6x12_te);
    u8g2.drawUTF8(65, 8, city.c_str());
    //draw lines
    u8g2.drawVLine(54, 0, 54);
    u8g2.drawHLine(54, 10, 74);
}

void forecastScreen() {
    LCD_CLEAR_AREA(0, 0, 128, 54);
     char tmp[10];

    //u8g2.setFont(u8g2_font_6x12_te);
    u8g2.setFont(u8g2_font_profont10_tf);

    for (int i = 0; i < LEN(forecast); i++){
        bool day = forecast[i].icon[2];    //get the icon letter (night or day)
        int type = forecast[i].icon.substring(0, 2).toInt();   //convert icon number from string to int
        Serial.printf("Icon: %s Day: %d type: %d\n", forecast[i].icon.c_str(), day, type);

        int column_offset = (i % 3) * (43);
        u8g2.drawStr(column_offset + 16, 6, days_of_week_short[(time_client.getDay() + i + 1) % 7]);
        u8g2.drawXBM(column_offset + 3, 39, 6, 6, sun_tiny_bits);
        u8g2.drawXBM(column_offset + 3, 47, 6, 6, moon_tiny_bits);
        sprintf(tmp, "%.1f\xb0", forecast[i].day_temp);
        u8g2.drawStr(column_offset + 12, 45, tmp);
        sprintf(tmp, "%.1f\xb0", forecast[i].night_temp);
        u8g2.drawStr(column_offset + 12, 53, tmp);

        //draw the bitmap (40x30)
        switch (type){
            case 1:  u8g2.drawXBM(column_offset + 1, 7, 40, 30, (day) ? clear_sky3_day_small_bits : clear_sky_night_small_bits);  break;    //12-12 0^ 2 | 17-18 1^ 1
            case 2:  u8g2.drawXBM(column_offset + 1, 7, 40, 30, (day) ? few_clouds_day_small_bits : few_clouds_night_small_bits); break;    //8-5   0^ 4 | 8-10  6^ 4
            case 3:  u8g2.drawXBM(column_offset + 1, 7, 40, 30, scattered_clouds_small_bits);                                     break;    //8-8   5^ 7
            case 4:  u8g2.drawXBM(column_offset + 1, 7, 40, 30, broken_clouds_small_bits);                                        break;    //8-8   5^ 7
            case 9:  u8g2.drawXBM(column_offset + 1, 7, 40, 30, shower_rain_small_bits);                                          break;    //8-8   0^ 1
            case 10: u8g2.drawXBM(column_offset + 1, 7, 40, 30, (day) ? rain_day_small_bits : rain_night_small_bits);             break;    //10-10 0^ 0 | 10-8  0^0
            case 11: u8g2.drawXBM(column_offset + 1, 7, 40, 30, thunderstorm_small_bits);                                         break;    //8-8   0^ 1
            case 13: u8g2.drawXBM(column_offset + 1, 7, 40, 30, snow_small_bits);                                                 break;    //8-8   0^ 0
            case 50: u8g2.drawXBM(column_offset + 1, 7, 40, 30, mist_small_bits);                                                 break;    //8-8   5^ 5
            default: break;
        }

    }

    u8g2.drawVLine(42, 0, 54);
    u8g2.drawVLine(85, 0, 54);
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
    curr_day.uvi        = (float)(root["current"]["uvi"]);
    curr_day.icon       =        (root["current"]["weather"][0]["icon"].as<String>());

    //save forecast
    for (unsigned int i = 0; i < LEN(forecast); i++) {
        forecast[i].day_temp   = (float)(root["daily"][i+1]["temp"]["day"]) - 273.15;
        forecast[i].night_temp = (float)(root["daily"][i+1]["temp"]["night"]) - 273.15;
        forecast[i].uvi        = (float)(root["daily"][i+1]["uvi"]);
        forecast[i].icon       =         root["daily"][i+1]["weather"][0]["icon"].as<String>();
    }
    updateStatus("weather update");
}

void setup() {
    delay(500);
    //LCD
    u8g2.begin();
    u8g2.setFont(u8g2_font_bitcasual_tr);

    //AM2320 sensor
    am2320.begin();                 //initialize am2320

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
        inside_temp = am2320.readTemperature()*0.95;    //read temperature
        footer();                                       //update footer
        u8g2.sendBuffer();                              //print it
    }

    //change screen every x seconds
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

    //sync time every minute
    if (millis() - sync_timer >= MINUTE) {
        sync_timer = millis();
        updateTime();
        updateStatus("time sync");
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
