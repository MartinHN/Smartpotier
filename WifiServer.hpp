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



void debug(const String& d){
  if(numClients>0){webSocket.broadcastTXT("err:"+String(d));}
  Serial.println(d);
}
 void WiFiEvent(WiFiEvent_t event);
 
void notifyError(uint8_t num,char * err);
void broadcastMsg(const char *msg);
void broadcastData(char cmd,float f);
void sendData(const uint8_t num,const char cmd,const float f);
void sendArray(const uint8_t num,const char cmd,const std::vector<TempPoint>& itv,const  int from=0);


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
            //Serial.printf("[%u] get Text: %s\n", num, payload);
            auto cp = (char*)payload;
            if(strcmp(cp,"reset")==0){
              webSocket.disconnect();
              shouldRestart = true;
            }
            else if(strcmp(cp,"start")==0){
              startRec();
            }
            else if(strcmp(cp,"stop")==0){
              stopRec();
            }
            else if(length>=2 &&  cp[1]==':'){
              auto cmd = cp[0];
              // get interval
              if(cmd=='i'){
                
                String fv (cp+2);
                int from = fv.toInt();
                Serial.printf("asking for interval %i\n",from);
                //auto itv = getRecordedForInterval(from);
                
                
                sendArray(num,'l',recValues,from);
              }
              // get all
              else if(cmd=='a'){
                Serial.println("asking for all ");
                for(auto & v:recValues){
                  //Serial.println(v.toString());
                  webSocket.sendTXT(num,"l:"+v.toString());
                }
              }
              // rec
              else if(cmd=='r'){
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
              else if(cmd=='g'){
                auto t = getRecTime();
                String ts((unsigned long)t,10);
                static String rtPfx("t:");
                webSocket.sendTXT(num, rtPfx+ ts);
              }
              else if (cmd=='q'){
                String fv (cp+2);
                float interval = fv.toFloat();
                if(interval>=1 && interval <120){
                  recIntervalMs = 1000*interval;
                  webSocket.broadcastTXT("inf: interval is "+String(interval));
                }
              }
              else if(cmd=='p'){
              webSocket.sendTXT(num, "p:"+String((unsigned long)(startRecordTime)));
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
static inline void wifi_connect(){
      while(WiFiMulti.run() != WL_CONNECTED) {
        delay(100);    
    }
}


#if SSL_SERVER
static void openssl_server_init(void);
#endif

void wifiSetup(){
      uint8_t mac_addr[8] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };
    //esp_base_mac_addr_set(mac_addr);
      const char * ssid ="Livebox-66CD";
    WiFiMulti.addAP(ssid, "*****");
    Serial.printf("Connecting to : %s\n",ssid);
    WiFi.onEvent(WiFiEvent);
  wifi_connect();

        if (!MDNS.begin("smartpotier")) {
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




bool endsWith(const char * p,const char *endc){
  if(p && endc){
    const char* c = p;
    const char* endcc = endc;
    while(*c!='\0'&& *endcc!='\0'){
      if(*c==*endcc){
        endcc++;
      }
      else{
        endcc=endc;
      }
      c++;
    }
    if(*endcc=='\0' && *c=='\0'){
      return true;
    }
  }
  return false;
}

void writeHttp( WiFiClient & client,const char * fpath,String mimeType = "text/html"){
  // the content of the HTTP response follows the header:
//            client->print(http_page);
// HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
// and a content-type so the client knows what's coming, then a blank line:
     client.println("HTTP/1.1 200 OK");
     client.print("Content-type:");
     client.println(mimeType);
     bool gzipped = endsWith(fpath,".gz");
     if(gzipped){
      Serial.println("serving gz");
      client.println("Content-Encoding:gzip");
     }
     client.println();
            
    File file = SPIFFS.open(fpath);
    if(!file || file.isDirectory()){
        Serial.println("Failed to open file for reading");
        return;
    }

    Serial.print("Read from file: ");
    const int bS = 2*1024;
    unsigned char blockC[bS];
    int n = -1;
    while(file.available() && n!=0){
      n = file.read(blockC,bS);
      //Serial.write(&blockC[0],n);
      client.write(&blockC[0],n);
    }
    // The HTTP response ends with another blank line:
     client.println();
     client.flush();
    Serial.println("finish reading from file: ");
    file.close();
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

void broadcastMsg(const char *msg){
  webSocket.broadcastTXT(msg);
  
}
void broadcastData(char cmd,float f){
  #if SEND_BIN
  uint8_t* i = reinterpret_cast<uint8_t*>(&f);
  uint8_t d[] ={cmd,i[3],i[2],i[1],i[0]};
 webSocket.broadcastBIN(d,5); 
 #else
 webSocket.broadcastTXT(cmd+":"+String(temp));
 #endif
}

void sendData(const uint8_t num,const char cmd,const float f){
  #if SEND_BIN
 const uint8_t* i = reinterpret_cast<const uint8_t*>(&f);
  uint8_t d[] ={cmd,i[3],i[2],i[1],i[0]};
 webSocket.sendBIN(num,d,5); 
 #else
 webSocket.sendTXT(num,cmd+":"+String(temp));
 #endif
}

void sendArray(const uint8_t num,const char cmd,const std::vector<TempPoint>& itv,const int from){
        int start = 0;
        if(from>=0 && itv.size()>0){
          
          auto it = std::upper_bound(itv.begin(),itv.end(),from,[]( const int  v,const TempPoint & it){return v<it.t;});
          if(it!=itv.end()){
            start = it - itv.begin();
          }
          else{
            //nothing to send
            return;
          }
         }
          Serial.println("start : "+String(start));
        #if SEND_BIN
        int toSend = itv.size() - start;
        const int maxBS=std::min(toSend,500);
        uint8_t d[maxBS*8+1] ;
        d[0]=cmd;
        uint8_t cd [8];
        #else
        const int maxBS = 100;
        #endif

         for(int i = start ; i < itv.size() ; i+=maxBS){
         
            int endi =i+maxBS;
            if(endi>itv.size()){
              endi-=endi-itv.size();
             }

           #if SEND_BIN
            size_t dlength = 8*(endi-i) + 1;
            for(int j = i;  j < endi ; j++){
               itv[j].fillData(cd);
                for(int k = 0 ; k < 8 ; k++){
                   d[(j-i)*8 + k+1]=cd[k];
                 }
             }
  
            webSocket.sendBIN(num,d,dlength); 
            #else
             String toSend(itv[i].toString());
             for(int j = i+1;  j < endi ; j++){
                    toSend+=",";
                    toSend+=itv[j].toString();
             }
            Serial.println(toSend);
            webSocket.sendTXT(num,cmd+":"+toSend);
 #endif
                  
       }
       Serial.println("sent : "+String(toSend));

}

void notifyError(uint8_t num,char * err){
              static String errorBase ("do not understand ");
            String error =errorBase+ (char*)err;
            Serial.println(error);
             webSocket.sendTXT(num, error);
}



#if SSL_SERVER
File certFile;
File pkFile;


#include "openssl/ssl.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#define OPENSSL_EXAMPLE_LOCAL_TCP_PORT     443
#define OPENSSL_EXAMPLE_TASK_NAME        "openssl_example"
#define OPENSSL_EXAMPLE_TASK_STACK_WORDS 10240
#define OPENSSL_EXAMPLE_TASK_PRIORITY    8
#define OPENSSL_EXAMPLE_RECV_BUF_LEN       1024

#define HTML_HEADER "HTTP/1.1 200 OK\r\n" \
                     "Content-Type: text/html\r\n" \
                     "\r\n"
                     /*"Content-Encoding : gzip\r\n" \
                     "\r\n"*/
                     
#define OPENSSL_EXAMPLE_SERVER_ACK "HTTP/1.1 200 OK\r\n" \
                                "Content-Type: text/html\r\n" \
                                "Content-Length: 98\r\n\r\n" \
                                "<html>\r\n" \
                                "<head>\r\n" \
                                "<title>OpenSSL example</title></head><body>\r\n" \
                                "OpenSSL server example!\r\n" \
                                "</body>\r\n" \
                                "</html>\r\n" \
                                "\r\n"

const static char *TAG = "Openssl_example";
#define LOG_LOCAL_LEVEL  ESP_LOG_VERBOSE
#undef ESP_LOGI
#define ESP_LOGI( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_INFO)    { esp_log_write(ESP_LOG_INFO,    tag, LOG_FORMAT(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
//#define ESP_LOGI(t,x) Serial.println(x);

static void openssl_example_task(void *p)
{

    int ret;

    SSL_CTX *ctx;
    SSL *ssl;

    int sockfd, new_sockfd;
    socklen_t addr_len;
    struct sockaddr_in sock_addr;

    char recv_buf[OPENSSL_EXAMPLE_RECV_BUF_LEN];

    if(!certFile || !pkFile){
      Serial.println("cert files not opened");
      vTaskDelete(NULL);
      return;
    }
     
     const unsigned int cert_pem_bytes = certFile.size();
    unsigned char cert_pem[cert_pem_bytes];
     certFile.read(cert_pem,cert_pem_bytes);
     const unsigned int pk_pem_bytes = pkFile.size();
     unsigned char pk_pem[pk_pem_bytes];
    pkFile.read(pk_pem,pk_pem_bytes);

    ESP_LOGI(TAG, "SSL server context create ......");
    /* For security reasons, it is best if you can use
       TLSv1_2_server_method() here instead of TLS_server_method().
       However some old browsers may not support TLS v1.2.
    */
    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) {
        ESP_LOGI(TAG, "failed");
        goto failed1;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set own certification......");
    ret = SSL_CTX_use_certificate_ASN1(ctx,cert_pem_bytes,cert_pem); //cacert_pem_bytes, cacert_pem_start);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server context set private key......");
    ret = SSL_CTX_use_PrivateKey_ASN1(0, ctx,pk_pem,pk_pem_bytes);// prvtkey_pem_start, prvtkey_pem_bytes);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server create socket ......");
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ESP_LOGI(TAG, "failed");
        goto failed2;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket bind ......");
    memset(&sock_addr, 0, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = 0;
    sock_addr.sin_port = htons(OPENSSL_EXAMPLE_LOCAL_TCP_PORT);
    ret = bind(sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket listen ......");
    ret = listen(sockfd, 32);
    if (ret) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

reconnect:
    ESP_LOGI(TAG, "SSL server create ......");
    ssl = SSL_new(ctx);
    if (!ssl) {
        ESP_LOGI(TAG, "failed");
        goto failed3;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server socket accept client ......");
    new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addr_len);
    if (new_sockfd < 0) {
        ESP_LOGI(TAG, "failed" );
        goto failed4;
    }
    ESP_LOGI(TAG, "OK");

    SSL_set_fd(ssl, new_sockfd);

    ESP_LOGI(TAG, "SSL server accept client ......");
    ret = SSL_accept(ssl);
    if (!ret) {
        ESP_LOGI(TAG, "failed");
        goto failed5;
    }
    ESP_LOGI(TAG, "OK");

    ESP_LOGI(TAG, "SSL server read message ......");
    do {
        memset(recv_buf, 0, OPENSSL_EXAMPLE_RECV_BUF_LEN);
        ret = SSL_read(ssl, recv_buf, OPENSSL_EXAMPLE_RECV_BUF_LEN - 1);
        if (ret <= 0) {
            break;
        }
        ESP_LOGI(TAG, "SSL read: %s", recv_buf);
        if (strstr(recv_buf, "GET ") &&
            strstr(recv_buf, " HTTP/1.1")) {
            ESP_LOGI(TAG, "SSL get matched message")
            ESP_LOGI(TAG, "SSL write message");
            int numWritten = 0;
            ret = SSL_write(ssl, HTML_HEADER, sizeof(HTML_HEADER));
            numWritten+=sizeof(HTML_HEADER);
            if(ret==0){Serial.println("error");}
            File file = SPIFFS.open(SERVED_HTML_PATH);
            const int bS = 1024;
            unsigned char blockC[bS];
            int n = -1;
            while(file.available() && n!=0){
              n = file.read(blockC,bS);
              numWritten+=n;
             ret = SSL_write(ssl,blockC,n);
             if(ret==0){Serial.println("error");}
            }
            numWritten+=2;
            ret = SSL_write(ssl,"\r\n",2);
            ret = SSL_write(ssl,"\r\n",2);
            if(ret==0){Serial.println("error");}
            if (ret > 0) {
              ESP_LOGI(TAG, "OK : %i",numWritten)
            } else {
                ESP_LOGI(TAG, "error")
            }
            file.close();
            break;
        }
    } while (1);
    
    SSL_shutdown(ssl);
failed5:
    close(new_sockfd);
    new_sockfd = -1;
failed4:
    SSL_free(ssl);
    ssl = NULL;
    goto reconnect;
failed3:
    close(sockfd);
    sockfd = -1;
failed2:
    SSL_CTX_free(ctx);
    ctx = NULL;
failed1:
    vTaskDelete(NULL);
    return ;
} 


 void openssl_server_init(void)
{
  esp_log_level_set(TAG, ESP_LOG_INFO); 
     certFile = SPIFFS.open("/cacert.pem");
    pkFile = SPIFFS.open("/prvtkey.pem");
    if(!certFile || !pkFile){
      Serial.println("fatal error");
      return;
    }
    else{
      Serial.println("cert files found");
    }


      if(sizeof(uint8_t)!=sizeof(unsigned char))
  {
    Serial.println("fatal size error");
    Serial.println();
  Serial.println(sizeof(uint8_t));
  Serial.println();
  Serial.println(sizeof(unsigned char));
  Serial.println();
  Serial.println();
    return;
    }
ESP_LOGI(TAG, "INIT OPENSSL");
    Serial.println("INIT OPENSSL");
    int ret;
    xTaskHandle openssl_handle;

    ret = xTaskCreate(openssl_example_task,
                      OPENSSL_EXAMPLE_TASK_NAME,
                      OPENSSL_EXAMPLE_TASK_STACK_WORDS,
                      NULL,
                      OPENSSL_EXAMPLE_TASK_PRIORITY,
                      &openssl_handle); 

    if (ret != pdPASS)  {
       Serial.printf("create task %s failed\n", OPENSSL_EXAMPLE_TASK_NAME);
    }
}
#endif
