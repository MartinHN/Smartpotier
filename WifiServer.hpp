#define SEND_BIN 1
#define SSL_SERVER 0
#include <WiFi.h>
#include <WiFiMulti.h>
WiFiMulti WiFiMulti;
#include <ESPmDNS.h>
#include <WebSocketsServer.h>
#include <Hash.h>
#include <algorithm>

#define HOSTNAME "smartpotier"
#define SERVED_HTML_PATH "/index.min.html.gz"
WiFiServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);


unsigned short numClients = 0;
bool shouldRestart = false;

const int pingIntervalMs = 500;
unsigned long lastPingTime = 0;


#include "WifiUtils.hpp"

void WiFiEvent(WiFiEvent_t event);

void broadcastRefNames(){
auto rn = getRefNames();
        String names("refNames:");
        bool first = true;
        for(auto & v:rn){
          if(!first){names+=",";}
          first = false;
          names+=v;
        }
        Serial.println("broadcasting : " + names);
        webSocket.broadcastTXT(names);
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  switch(type) {
    case WStype_DISCONNECTED:
    numClients-=1;
    Serial.printf("[%u] Disconnected!\n", num);
    break;
    case WStype_CONNECTED:
    {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            // send message to client
      webSocket.sendTXT(num, "Connected");
      numClients+=1;
    }
    break;
    case WStype_TEXT:
    {
      auto cp = (char*)payload;
      String cps (cp);
      // Serial.println("textSock : " + cps);
      if(cps  == "reset"){
        webSocket.disconnect();
        shouldRestart = true;
      }
      else if(cps == "start"){
        startRec();
      }
      else if(cps == "stop"){
        stopRec();
      }
      else if(cps == "getExistingRefNames"){
        broadcastRefNames();
      }

      else if(length>=2 && cps.indexOf(":")>0){
        int sidx = cps.indexOf(":");
        auto cmd = cps.substring(0,sidx);
        auto remaining = cps.substring(sidx+1);
        
        // get interval
        if(cmd=="i"){

          int from = remaining.toInt();
          Serial.printf("asking for interval %i\n",from);
                //auto itv = getRecordedForInterval(from);
          sendArray(num,'l',recValues,from);
        }
        
        // get all
        else if(cmd=="a"){
          Serial.println("asking for all ");
          for(auto & v:recValues){
            //Serial.println(v.toString());
            webSocket.sendTXT(num,"l:"+v.toString());
          }
        }
        
        // rec
        else if(cmd=="r"){
          if(length==3){
            if(cp[2]=='1'){
              startRec();
            }
            else if (cp[2]=='0'){
              stopRec();
            }
          }
          else if (length==2){
            webSocket.sendTXT(num,"r:"+String((isRecording?"1":"0")));
          }
        }
        // get ellapsed rec time
        else if(cmd=="g"){
          auto t = getRecTime();
          String ts((unsigned long)t,10);
          static String rtPfx("t:");
          webSocket.sendTXT(num, rtPfx+ ts);
        }
        // set interval in seconds
        else if (cmd=="q"){
          String fv (cp+2);
          float interval = fv.toFloat();
          if(interval>=1 && interval <120){
            recIntervalMs = 1000*interval;
            webSocket.broadcastTXT("inf: interval is "+String(interval));
          }
        }
        // get start Rec Time
        else if(cmd=="p"){
          webSocket.sendTXT(num, "p:"+String((unsigned long)(startRecordTime)));
        }
        // select a curve
        else if(cmd=="selectCurve"){
          auto refP =getRefNamed(remaining);
          if(refP.size()){
            sendArray(num,'c',refP,-1);
          }
          else{
            debug("no curve found for selection");
          }
        }
        // delete a curve
        else if(cmd=="deleteCurve"){
          if(deleteRefNamed(remaining)){
            broadcastRefNames();
          }
          else{
            debug("no curve found for deletion");
          }
        }
        // create Curve
        else if(cmd=="createCurve"){
          int nidx = remaining.indexOf(":");
          if(nidx>0){
            auto cName = remaining.substring(0,nidx);
            auto curveString = remaining.substring(nidx+1);

            if(!createRefNamed(cName,curveString)){
              debug("couldnt create Ref File");
            }
            else{
              broadcastRefNames();
            }
          }
          else{
            debug("no name found for creation");
          }
        }

        else{
          notifyError(num,(char*)payload);
        }
      }
      else{
        // send message to client
        notifyError(num,(char*)payload);

        // send data to all connected clients

      }
    }
    break;
    case WStype_BIN:
    Serial.printf("[%u] get binary length: %u\n", num, length);
            // send message to client
            // webSocket.sendBIN(num, payload, length);
    break;
  }

}


void wifiSetup(){
  uint8_t mac_addr[8] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };
    //esp_base_mac_addr_set(mac_addr);
  const char * ssid =WIFI_SSID;
  WiFiMulti.addAP(ssid, WIFI_PASS);
  Serial.printf("Connecting to : %s\n",ssid);
  WiFi.onEvent(WiFiEvent);
  wifi_connect();

  if (!MDNS.begin(HOSTNAME)) {
    Serial.println("Error setting up MDNS responder!");
    while(1) {
      delay(1000);
    }
  }
  Serial.println("mDNS responder started");
    // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);
    #if SSL_SERVER
  openssl_server_init();
    #endif
  Serial.print("connected as ");
  Serial.print(WiFi.localIP() );
  Serial.printf(" (%s)\n", WiFi.getHostname());
  server.begin();


}




void pingIfNeeded(){
  auto ct = millis();
  if( (ct-lastPingTime>pingIntervalMs)){
    lastPingTime =ct;
    static String pingString("pp");
    webSocket.broadcastPing(pingString);
  }
}


void wifi_loop(){
  if(shouldRestart){ 
    webSocket.disconnect();

    ESP.restart();
  }
  WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("New Client.");           // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        
        if (currentLine.startsWith("GET / ")) {
          writeHttp(client,SERVED_HTML_PATH,"text/html");
          break;
        }
        if (currentLine.startsWith("GET /rec")) {
         writeHttp(client,csvPath,"text/anytext");
         break;
       }
     }
   }
    // close the connection:
   client.stop();
   Serial.println("Client Served.");
 }
 webSocket.loop();
   // pingIfNeeded();
}



void WiFiEvent(WiFiEvent_t event){
  switch(event) {
    case SYSTEM_EVENT_AP_START:
    Serial.println("AP Started");
    WiFi.softAPsetHostname(HOSTNAME);
    break;
    case SYSTEM_EVENT_AP_STOP:
    Serial.println("AP Stopped");
    break;
    case SYSTEM_EVENT_STA_START:
    Serial.println("STA Started");
    Serial.println(WiFi.getHostname());
    WiFi.setHostname(HOSTNAME);
    break;
    case SYSTEM_EVENT_STA_CONNECTED:
    Serial.println("STA Connected");
    //WiFi.enableIpV6();
    break;
    case SYSTEM_EVENT_AP_STA_GOT_IP6:
    Serial.print("STA IPv6: ");
    //Serial.println(WiFi.localIPv6());
    break;
    case SYSTEM_EVENT_STA_GOT_IP:
    Serial.print("STA IPv4: ");
    Serial.println(WiFi.localIP());
    break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
    /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
    wifi_connect(); 
    Serial.println("STA Disconnected");
    break;
    case SYSTEM_EVENT_STA_STOP:
    Serial.println("STA Stopped");
    break;
    default:
    break;
  }

  
}

