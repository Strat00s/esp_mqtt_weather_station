/*----(LIBRARIES)----*/
#include <WiFiUdp.h>            //esp udp client
#include <NTPClient.h>          //ntp client
#include <ESP8266WiFi.h>        //esp wifi client
#include <PubSubClient.h>       //mqqt client
#include <ArduinoJson.h>        //json parsing for weather data
#include <U8g2lib.h>            //lcd
#include <Adafruit_Sensor.h>    //sensor
#include <Adafruit_AM2320.h>
#include <ArduinoOTA.h>         //OTA
#include "weather_icons.h"      //icons

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

/*----(STRUCT)----*/
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
//EDIT HERE with your information
const String ssid        = "Konrad_2.4GHz";
const String passw       = "FD053449D3";
const String mqtt_addr   = "192.168.1.176";
const String ntp_addr    = "192.168.1.1";
const String city        = "Horoměřice";
const String device_name = "WeatherBug";

/*----(VARIABLES)----*/
//init
//EDIT HERE for your specific 128x64 configuration
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R2, 15, 16);  //LCD config
//U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R2, 2, 0, U8X8_PIN_NONE);
WiFiClient espClient;                               //create wificlient
PubSubClient client(espClient);                     //setup mqtt client
WiFiUDP ntpUDP;                                     //create wifiudp
NTPClient time_client(ntpUDP, ntp_addr.c_str());    //setup time client width server on 192.168.1.1
Adafruit_AM2320 am2320 = Adafruit_AM2320();         //am2320 sensor

//weekdays for time screen and forecast
//EDIT HERE for your language
char days_of_week[7][10] = {"Neděle", "Pondělí", "Úterý", "Středa", "Čtvrtek", "Pátek", "Sobota"};
char days_of_week_short[7][4] = {"NE", "PO", "UT", "ST", "CT", "PA", "SO"};

bool startup = true;    //startup bool

DayData curr_day;       //current day data storage
DayData forecast[3];    //next 4 days forecast
float inside_temp;      //inside temperature

int screen = 0; //current screen to show

//timers
unsigned long screen_timer = 0;
unsigned long footer_timer = 0;
unsigned long sync_timer = 0;

/*----(HELPER FUNCTIONS)----*/
void drawCenteredString(char * text, int y) {
    int width = u8g2.getStrWidth(text);
    width = LCD_WIDTH/2 - width/2;
    u8g2.drawStr(width, y, text);
}

/*----(OTA)----*/
void onStart() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_te);                //set update font
    drawCenteredString("Update in progress", 16);   //print update message
    u8g2.setFontMode(1);                            //set font mode (transparency)
}

void onProgress(size_t progress, size_t total) {
    int bar_width = 100;
    int bar_height = 9;
    int actual_progress = progress / (total / bar_width);
    char buf[3];
    itoa(actual_progress, buf, 10);
    int offset = (LCD_WIDTH - bar_width) / 2;
    
    //draw bar
    u8g2.setDrawColor(1);
    u8g2.drawFrame(offset - 2, 28, bar_width + 4, bar_height + 4);
    u8g2.drawBox(offset, 30, actual_progress, bar_height);
    u8g2.setDrawColor(0);
    u8g2.drawBox(offset + actual_progress, 30, bar_width - actual_progress, bar_height);
    u8g2.setDrawColor(2);
    drawCenteredString(buf, 38);
    
    u8g2.sendBuffer();
}

void onEnd() {
    drawCenteredString("Done", 56);   //print failed message
    u8g2.sendBuffer();
}

void onError(ota_error_t error) {
    drawCenteredString("Failed", 56);   //print failed message
    u8g2.sendBuffer();
}


/*----(UI ELEMENTS)----*/
void bubbleAnimation(int current_bubble, int min, int max, int location, int spread, char *text){
    LCD_CLEAR_AREA(LCD_WIDTH/2 - (spread+max+1), location - max, (spread+max+1)*2, (max+1)*2);  //clear area

    //center text (if there is any)
    if (text != NULL) drawCenteredString(text, 20);

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
    String day =   time_client.getFormattedDate().substring(8, 10);
    String month = time_client.getFormattedDate().substring(5, 7);
    String year =  time_client.getFormattedDate().substring(0, 4);
    
    //weekday
    u8g2.setFont(u8g2_font_6x12_te);                        //set font (for diacritics)
    sprintf(tmp, "%s", days_of_week[time_client.getDay()]); //get weekday from array
    drawCenteredString(tmp, 12);
    
    //date
    u8g2.setFont(u8g2_font_profont15_tr);
    sprintf(tmp, "%s.%s.%s", day.c_str(), month.c_str(), year.c_str());
    drawCenteredString(tmp, 26);

    //time
    u8g2.setFont(u8g2_font_profont22_tr);
    sprintf(tmp, "%s", time_client.getFormattedTime().c_str());
    drawCenteredString(tmp, 46);
}

void weatherScreen() {
    LCD_CLEAR_AREA(0, 0, 128, 54);
    char tmp[16];   //variable to store strings

    //get correct bitmap
    bool day = (curr_day.icon[2] == 'd') ? true : false;    //get the icon letter (night or day)
    int type = curr_day.icon.substring(0, 2).toInt();   //convert icon number from string to int

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

    u8g2.setFont(u8g2_font_profont10_tf);

    for (int i = 0; i < LEN(forecast); i++){
        bool day = forecast[i].icon[2];                         //get the icon letter (night or day)
        int type = forecast[i].icon.substring(0, 2).toInt();    //convert icon number from string to int

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
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), passw.c_str());
    int timer = 0;
    while(WiFi.status() != WL_CONNECTED) {
        bubbleAnimation(timer, 2, 5, 40, 18, "Connecting to WiFi"); //run animation
        u8g2.sendBuffer();                                          //write it to screen
        timer++;                                                    //increase timeout
        delay(500);                                                 //delay
        if (timer >= 20) ESP.restart();                             //reset after 10 seconds
    }
}

//Send status update
void updateStatus(String status) {
    client.publish(("devices/" + device_name).c_str(), (time_client.getFormattedTime() + " " + status).c_str(), true);
}

/*----(MQTT)----*/
//update weather data on new message (as we are listening on single topic)
void onMessage(char* topic, byte* payload, unsigned int length) {
    if (String(topic) != String("weather/" + city)) return;  //skip if not our city

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

    updateStatus("weather update"); //update status
}

/*----(SETUP)----*/
void setup() {
    delay(500); //wait just because

    //LCD
    u8g2.begin();                           //init lcd
    u8g2.setFont(u8g2_font_bitcasual_tr);   //set starting font
    //Wire.begin(0, 2);                       //for esp-01 when i2c is taken by sensor

    //AM2320 sensor
    am2320.begin(); //initialize am2320

    //wifi
    startWifi();    //connect to wifi

    //mqtt
    client.setServer(mqtt_addr.c_str(), 1883);  //set mqtt server
    client.setBufferSize(8192);                 //set buffer for weather data
    client.setCallback(onMessage);              //set message callback

    //NTP
    time_client.begin();                //start ntp
    time_client.setTimeOffset(7200);    //set offset
    delay(500);                         //wait because reasons
    time_client.forceUpdate();          //update time from ntp server

    //OTA
    ArduinoOTA.begin(); //init ota
    ArduinoOTA.onStart(onStart);
    ArduinoOTA.onProgress(onProgress);
    ArduinoOTA.onEnd(onEnd);
    ArduinoOTA.onError(onError);
}

void loop() {

    //update footer every second
    if (millis() - footer_timer >= SECOND) {
        footer_timer = millis();
        //EDIT HERE - inside temperature sensor "calibration"
        inside_temp = am2320.readTemperature() * 0.95;    //read temperature
        //inside_temp = am2320.readTemperature() * 0.8472;  //
        footer();                                       //update footer
        u8g2.sendBuffer();                              //print it
    }

    //change screen every x seconds
    if (millis() - screen_timer >= SECOND*4) {
        screen_timer = millis();

        //select screen
        switch (screen) {
            case 0: timeScreen();     break;
            case 1: weatherScreen();  break;
            case 2: forecastScreen(); break;
        }

        bubbleAnimation(screen, 1, 2, 59, 12, NULL);    //change animation
        screen++;                                       //go to next screen
        if (screen > 2) screen = 0;                     //reset screen if above limit
        u8g2.sendBuffer();                              //draw display
    }

    //sync time every minute
    if (millis() - sync_timer >= MINUTE) {
        sync_timer = millis();
        time_client.forceUpdate();  //update time
        updateStatus("time sync");  //update status
    }

    //loop and ask if connected
    if(!client.loop()) {
        //subscribe to required topic on connect
        if (client.connect(device_name.c_str())) client.subscribe(("weather/" + city).c_str());
        //else retry in 2 seconds
        else delay(2000);
    }
    
    //send device info on startup
    if (startup) {
        startup = false;

        //publish device info
        client.publish(("devices/" + device_name + "/ip").c_str(), WiFi.localIP().toString().c_str(), true);
        client.publish(("devices/" + device_name + "/connected").c_str(), time_client.getFormattedTime().c_str(), true);

        client.publish(("weather/requests/" + city).c_str(), "1", true);    //publish weather request
        updateStatus("connected");                                          //update status
    }

    //try to reconnect if disconnected
    if (!WiFi.isConnected()) {
        startWifi();    //reconnect
        startup = true; //update device data
    }

    ArduinoOTA.handle();    //run ota
}
