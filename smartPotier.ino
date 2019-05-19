/*
 * WebSocketServer.ino
 *
 *  Created on: 22.05.2015
 *
 */



#include <Arduino.h>
#include "secrets.h"
#include "SPIFFS.h"
 int recIntervalMs=1000*30;
unsigned long lastRecTime = 0;
unsigned long ct=0;
void broadcastMsg(const char *msg);
#include "Recorder.hpp"
#include "WifiServer.hpp"


#include <ESP8266FtpServer.h>
FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial




unsigned long lastTime = 0;


#include "esp_adc_cal.h"
esp_adc_cal_characteristics_t analog_calib;
#define V_REF   1118 //mV
#define ADC1_TEST_CHANNEL (ADC1_CHANNEL_6) // Temp probe

bool serveFTP = true;

const float TEMPMIN=0;
const float TEMPMAX= 1300;
float alpha = .0001;
double lastM = 0;
double curM = 0;

const int sIntervalMs = 1000;
unsigned long lastSampleTime = 0;

double curMeasureMv = 0;
void listDir(fs::FS &fs, const char * dirname, uint8_t levels);

void setup() {
  pinMode(LED_BUILTIN,OUTPUT);
  bool isOn  =true;
  digitalWrite(LED_BUILTIN,isOn);
    
    //delay(1000);
    Serial.begin(115200);
    //delay(1000);

    Serial.println();
    Serial.println();
    Serial.println();
//esp_log_level_set("*", ESP_LOG_INFO);

    if(!SPIFFS.begin()){//true,"/spiffs",10)){
    isOn = !isOn;
    //digitalWrite(LED_BUILTIN,isOn);
    Serial.println("!!!SPIFFS not opened!");
    }
    else{
      Serial.println("SPIFFS opened!");
      listDir(SPIFFS,"/",3);
    }
    
    wifiSetup();
    isOn = !isOn;
    //digitalWrite(LED_BUILTIN,isOn);
      
    ftpSrv.begin("esp32","esp32");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
    recorder_setup();
    isOn = !isOn;
    //digitalWrite(LED_BUILTIN,isOn);
    
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    adc1_config_channel_atten(ADC1_TEST_CHANNEL, ADC_ATTEN_DB_0);
    esp_adc_cal_get_characteristics(V_REF, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, &analog_calib);
    
    digitalWrite(LED_BUILTIN,LOW);
}


void loop() {
    ct= millis();
    if(serveFTP) ftpSrv.handleFTP();
    
    
    // weird inversion + factors to get Voltage/Temp right
    curM  = -1.011 * adc1_to_voltage(ADC1_TEST_CHANNEL,&analog_calib) + 1129;
    lastM += (curM-lastM)*alpha;
    curMeasureMv = lastM;
    
    updateRec();
    broadcastIfNeeded();
    wifi_loop();

    
    
}

float getTemp() { return curMeasureMv*(TEMPMAX-TEMPMIN)/1000.0f+TEMPMIN;}

void broadcastIfNeeded(){
    if(ct-lastSampleTime>sIntervalMs){
      lastSampleTime = ct;
      float temp = getTemp();
      broadcastData('v',temp);
    }
}


void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}




