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
#include <vector>

Preferences preferences;
 static time_t global_time = {0};
void updateTime() { time(&global_time); }

struct TempPoint{
  uint32_t t;
  float val;

  static TempPoint fromString(String s){
    TempPoint res{0,-1};
    int split = s.indexOf(',');
    if(split>0 && split< s.length()){
      res.t = s.substring(0,split).toInt();
      res.val = s.substring(split+1).toFloat();
    }
    return res;
  }
   String toString(){
    String res((unsigned long)t,10);
    res+=" , ";
    res+=String(val);
    return res;
  }
  bool isValid() const{
    return val!=-1;
  }
  bool operator < (const TempPoint & other)const {
    return t<other.t;
  }


  
  void fillData(uint8_t *d) const{
    const uint8_t* tt = reinterpret_cast<const uint8_t*>(&t);
    d[0]=tt[3];
    d[1]=tt[2];
    d[2]=tt[1];
    d[3]=tt[0];
    const uint8_t * v = reinterpret_cast<const uint8_t*>(&val);
    d[4] = v[3];
    d[5] = v[2];
    d[6] = v[1];
    d[7] = v[0];
    //return {t[3],t[2],t[1],t[0],v[3],v[2],v[1],v[0]};
  }
};

std::vector<TempPoint> recValues;
void fillValuesFromFile();

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
      fillValuesFromFile();
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



void fillValuesFromFile(){
  recValues.clear();
  
  if(recFile.available()){
    Serial.println("restore from saved");
    auto line = recFile.readStringUntil('\n');
    int i = 1;
    while (line.length() && i < MAX_REC_SIZE)
    {

    auto p = TempPoint::fromString(line);
    if(p.isValid()){
      recValues.push_back(p);
      //Serial.print("valid line : ");
      //Serial.println(line);
      //Serial.printf("%u : %.2f",p.t,p.val);
    }
    else{
      Serial.print("non valid line : ");
      Serial.println(line);
    }
    line = recFile.readStringUntil('\n');
    i++;
    }
      recFile.close();
      recFile = SPIFFS.open(csvPath,FILE_APPEND);
  }
  else{
    Serial.println("can't read saved file");
    recFile = SPIFFS.open(csvPath,FILE_WRITE);
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
      auto str = tp.toString();
     // Serial.println(str);
      recFile.println(str);
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







  


