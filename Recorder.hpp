File recFile;
const char * csvPath = "/rec.csv";

#include "apps/sntp/sntp.h"

#define MAX_REC_SIZE 60*36*4
static void initialize_sntp(void);
static void printTime();
static uint64_t startRecordTime =0;


static bool isRecording = false;

float getTemp();
int sendNotification(String title,String msg);


#include <Preferences.h>
#include "TempPoint.hpp"
#include <vector>

using std::vector;

Preferences preferences;
static time_t global_time = {0};
void updateTime() { time(&global_time); }

std::vector<TempPoint> recValues;


void recorder_setup(){
  Serial.printf("max rec size = %.2f ko\n",sizeof(TempPoint)*MAX_REC_SIZE / 1000.0) ;

  recFile = SPIFFS.open(csvPath);
  if(!recFile){
    Serial.println("Failed to open file for creating one now");
    recFile=SPIFFS.open(csvPath, FILE_WRITE);
    if(!recFile){
      Serial.println("Failed to open file for writing");
      return;
    }
  }
  else{
    if(TempPoint::fillValuesFromFile(recFile,recValues)){
      recFile = SPIFFS.open(csvPath,FILE_APPEND);
    }
    else{
      Serial.println("can't read saved file");
      recFile = SPIFFS.open(csvPath,FILE_WRITE);
    }
    if(!recFile){
      Serial.println("Failed to open file for append");
      return;
    }
  }

  Serial.println("init time");
  initialize_sntp();
    // do not rec before interval (alpha filter warms up)
  lastRecTime = global_time+8;

  preferences.begin("pref", false);
  isRecording = preferences.getBool("isRec", false);
  startRecordTime = preferences.getULong64("startT",0);  
  if(isRecording){
    updateTime();
    int recI = recIntervalMs/1000;
    float numInts = (global_time - startRecordTime)*1.0/recI;
    lastRecTime= startRecordTime+floor(numInts)*recI;
      //Serial.println("try to keep phase : "+String(lastRecTime) + ","+String(global_time));
  }
}


std::vector<TempPoint> getRecordedForInterval(int from){
  if(from<0){return recValues;}
  else{
    std::vector<TempPoint> res;
    for (auto & v:recValues){if(v.t>from){res.push_back(v);}}
      return res;
  }
}

static void initialize_sntp(void)
{
  Serial.println( "Initializing SNTP");
  configTime(3600, 3600,"pool.ntp.org");

  struct tm timeinfo = {0};
  const uint32_t timeOut = 50000;
  if(getLocalTime(&timeinfo,timeOut)){
    updateTime();
    Serial.println("get time succeeded");
    printTime();
  }
  else{
    Serial.println("get time failed");
  }
}

static void printTime(){
  Serial.println(global_time);
  struct tm * time_info = localtime(&global_time);
  Serial.println(time_info, "%A, %B %d %Y %H:%M:%S");
}

uint32_t getRecTime(){
  updateTime();
  uint64_t now = global_time;
  if(startRecordTime> now ){
    Serial.printf("error on rec time : %i,%i",now,startRecordTime);
    return -1;
  }
  
  return now-startRecordTime;
}

void writeTemp(const float temp){
  if(!isRecording){Serial.println("trying to write but not recording");return;}
  if(recFile){
    if(recValues.size()>MAX_REC_SIZE){
      Serial.println("rec file full");
      broadcastMsg("err:recfull");
      return;
    }
    else{
      uint32_t recTime = getRecTime();
      if(recTime!=(uint32_t)-1){
        TempPoint tp ( {recTime,temp});
        recValues.push_back(tp);
        tp.appendToFile(recFile);
        recFile.flush();
      }
      else{
       Serial.println("invalid rec time");
       broadcastMsg("err:invalid rec time");
     }
   }
   
 }
 else{
  Serial.println("rec file not opened");
}

}

bool updateRec(bool force=false){
  updateTime();
  if( isRecording && (force || (global_time-lastRecTime>=recIntervalMs/1000.0))){
    lastRecTime =global_time;
    float temp = getTemp();
    writeTemp(temp);
      //sendNotification("smartpotier","new temp value");
    return true;
  }
  return false;
}

void startRec(){
  Serial.println("startrec");
  updateTime();
  startRecordTime=global_time;
  recFile=SPIFFS.open(csvPath, FILE_WRITE);
  isRecording = true;
  recValues.clear();
  updateRec(true);
  preferences.putBool("isRec", isRecording);
  preferences.putULong64("startT",startRecordTime);
  broadcastMsg("r:1");
  //webSocket.sendTXT(num, "p:"+String((unsigned long)(startRecordTime)));
  
}


void stopRec(){
  Serial.println("stoprec");
  updateTime();
  isRecording = false;
  preferences.putBool("isRec", isRecording);
  recFile.close();
  broadcastMsg("r:0");
}


// refs


vector<String> getRefNames(){
  vector<String> refNames;
  File dir = SPIFFS.open("/");
  File file = dir.openNextFile();
  String str;
  while (file) {
    str += file.name();
    str += " /// ";
    str += file.size();
    str += "\r\n";
    String cFile = file.name();
    if(cFile.endsWith(".ref")){
      refNames.push_back(cFile);
    } 
    file = dir.openNextFile();
  }
  Serial.print(str);
  return refNames;
}



bool deleteRefNamed(const String & n){
  if( n.length()>3 && SPIFFS.exists(n)){
    SPIFFS.remove(n);
    return true;
  }

  return false;
  
}
String normalizeRefPath(String n){
    n.trim();
  if(!n.endsWith(".ref")){
    n+=".ref";
  }
  if(n[0]!='/'){
    n = "/"+n;
  }
  return n;
}
bool createRefNamed(String n,String refV){
  n =  normalizeRefPath(n);
  refV.trim();
  refV.replace(",","\n");
  // refV.replace(":",",");
  if(refV[refV.length()-1]!='\n'){
    refV+="\n";
  }
  File dF  = SPIFFS.open(n,FILE_WRITE);
  if(dF){
    dF.print(refV);
    dF.flush();
    dF.close();
    return true;
  }
  else{
    Serial.println("can't create file : "+n);
    return false;
  }
  
  
}

vector<TempPoint> getRefNamed(String n){
  n =  normalizeRefPath(n);
  vector<TempPoint> v;
  File dF  = SPIFFS.open(n,FILE_READ);
  if(dF){
    TempPoint::fillValuesFromFile(dF,v);
    dF.close();
  }
  else{
    Serial.println("can't get ref : " + n );
  }
  return v;
}







