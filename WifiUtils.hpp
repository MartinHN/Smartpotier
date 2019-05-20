void debug(const String& d){
  if(numClients>0){webSocket.broadcastTXT("err:"+String(d));}
  Serial.println(d);
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

/////////////////
//
////////////////

static inline void wifi_connect(){
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(100);    
  }
}



//////////////
// WS
//////////////


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

/////////////////
// HTTP
///////////



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
  Serial.println(file.name());
  const int bS = 2*1024;
  unsigned char blockC[bS];
  int n = -1;
  int total = 0;
  while(file.available() && n!=0){
    n = file.read(blockC,bS);
    total+=n;
      //Serial.write(&blockC[0],n);
    client.write(&blockC[0],n);
  }
    // The HTTP response ends with another blank line:
  client.println();
  client.flush();
  Serial.print("finish reading from file  : ");
  Serial.println(total);
  file.close();
}
















// #if SSL_SERVER
// File certFile;
// File pkFile;


// #include "openssl/ssl.h"
// #include "lwip/sockets.h"
// #include "lwip/netdb.h"
// #define OPENSSL_EXAMPLE_LOCAL_TCP_PORT     443
// #define OPENSSL_EXAMPLE_TASK_NAME        "openssl_example"
// #define OPENSSL_EXAMPLE_TASK_STACK_WORDS 10240
// #define OPENSSL_EXAMPLE_TASK_PRIORITY    8
// #define OPENSSL_EXAMPLE_RECV_BUF_LEN       1024

// #define HTML_HEADER "HTTP/1.1 200 OK\r\n" \
// "Content-Type: text/html\r\n" \
// "\r\n"
//                      /*"Content-Encoding : gzip\r\n" \
//                      "\r\n"*/

// #define OPENSSL_EXAMPLE_SERVER_ACK "HTTP/1.1 200 OK\r\n" \
// "Content-Type: text/html\r\n" \
// "Content-Length: 98\r\n\r\n" \
// "<html>\r\n" \
// "<head>\r\n" \
// "<title>OpenSSL example</title></head><body>\r\n" \
// "OpenSSL server example!\r\n" \
// "</body>\r\n" \
// "</html>\r\n" \
// "\r\n"

// const static char *TAG = "Openssl_example";
// #define LOG_LOCAL_LEVEL  ESP_LOG_VERBOSE
// #undef ESP_LOGI
// #define ESP_LOGI( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_INFO)    { esp_log_write(ESP_LOG_INFO,    tag, LOG_FORMAT(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
// //#define ESP_LOGI(t,x) Serial.println(x);

// static void openssl_example_task(void *p)
// {

//   int ret;

//   SSL_CTX *ctx;
//   SSL *ssl;

//   int sockfd, new_sockfd;
//   socklen_t addr_len;
//   struct sockaddr_in sock_addr;

//   char recv_buf[OPENSSL_EXAMPLE_RECV_BUF_LEN];

//   if(!certFile || !pkFile){
//     Serial.println("cert files not opened");
//     vTaskDelete(NULL);
//     return;
//   }

//   const unsigned int cert_pem_bytes = certFile.size();
//   unsigned char cert_pem[cert_pem_bytes];
//   certFile.read(cert_pem,cert_pem_bytes);
//   const unsigned int pk_pem_bytes = pkFile.size();
//   unsigned char pk_pem[pk_pem_bytes];
//   pkFile.read(pk_pem,pk_pem_bytes);

//   ESP_LOGI(TAG, "SSL server context create ......");
//     /* For security reasons, it is best if you can use
//        TLSv1_2_server_method() here instead of TLS_server_method().
//        However some old browsers may not support TLS v1.2.
//     */
//   ctx = SSL_CTX_new(TLS_server_method());
//   if (!ctx) {
//     ESP_LOGI(TAG, "failed");
//     goto failed1;
//   }
//   ESP_LOGI(TAG, "OK");

//   ESP_LOGI(TAG, "SSL server context set own certification......");
//     ret = SSL_CTX_use_certificate_ASN1(ctx,cert_pem_bytes,cert_pem); //cacert_pem_bytes, cacert_pem_start);
//     if (!ret) {
//       ESP_LOGI(TAG, "failed");
//       goto failed2;
//     }
//     ESP_LOGI(TAG, "OK");

//     ESP_LOGI(TAG, "SSL server context set private key......");
//     ret = SSL_CTX_use_PrivateKey_ASN1(0, ctx,pk_pem,pk_pem_bytes);// prvtkey_pem_start, prvtkey_pem_bytes);
//     if (!ret) {
//       ESP_LOGI(TAG, "failed");
//       goto failed2;
//     }
//     ESP_LOGI(TAG, "OK");

//     ESP_LOGI(TAG, "SSL server create socket ......");
//     sockfd = socket(AF_INET, SOCK_STREAM, 0);
//     if (sockfd < 0) {
//       ESP_LOGI(TAG, "failed");
//       goto failed2;
//     }
//     ESP_LOGI(TAG, "OK");

//     ESP_LOGI(TAG, "SSL server socket bind ......");
//     memset(&sock_addr, 0, sizeof(sock_addr));
//     sock_addr.sin_family = AF_INET;
//     sock_addr.sin_addr.s_addr = 0;
//     sock_addr.sin_port = htons(OPENSSL_EXAMPLE_LOCAL_TCP_PORT);
//     ret = bind(sockfd, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
//     if (ret) {
//       ESP_LOGI(TAG, "failed");
//       goto failed3;
//     }
//     ESP_LOGI(TAG, "OK");

//     ESP_LOGI(TAG, "SSL server socket listen ......");
//     ret = listen(sockfd, 32);
//     if (ret) {
//       ESP_LOGI(TAG, "failed");
//       goto failed3;
//     }
//     ESP_LOGI(TAG, "OK");

//     reconnect:
//     ESP_LOGI(TAG, "SSL server create ......");
//     ssl = SSL_new(ctx);
//     if (!ssl) {
//       ESP_LOGI(TAG, "failed");
//       goto failed3;
//     }
//     ESP_LOGI(TAG, "OK");

//     ESP_LOGI(TAG, "SSL server socket accept client ......");
//     new_sockfd = accept(sockfd, (struct sockaddr *)&sock_addr, &addr_len);
//     if (new_sockfd < 0) {
//       ESP_LOGI(TAG, "failed" );
//       goto failed4;
//     }
//     ESP_LOGI(TAG, "OK");

//     SSL_set_fd(ssl, new_sockfd);

//     ESP_LOGI(TAG, "SSL server accept client ......");
//     ret = SSL_accept(ssl);
//     if (!ret) {
//       ESP_LOGI(TAG, "failed");
//       goto failed5;
//     }
//     ESP_LOGI(TAG, "OK");

//     ESP_LOGI(TAG, "SSL server read message ......");
//     do {
//       memset(recv_buf, 0, OPENSSL_EXAMPLE_RECV_BUF_LEN);
//       ret = SSL_read(ssl, recv_buf, OPENSSL_EXAMPLE_RECV_BUF_LEN - 1);
//       if (ret <= 0) {
//         break;
//       }
//       ESP_LOGI(TAG, "SSL read: %s", recv_buf);
//       if (strstr(recv_buf, "GET ") &&
//         strstr(recv_buf, " HTTP/1.1")) {
//         ESP_LOGI(TAG, "SSL get matched message")
//       ESP_LOGI(TAG, "SSL write message");
//       int numWritten = 0;
//       ret = SSL_write(ssl, HTML_HEADER, sizeof(HTML_HEADER));
//       numWritten+=sizeof(HTML_HEADER);
//       if(ret==0){Serial.println("error");}
//       File file = SPIFFS.open(SERVED_HTML_PATH);
//       const int bS = 1024;
//       unsigned char blockC[bS];
//       int n = -1;
//       while(file.available() && n!=0){
//         n = file.read(blockC,bS);
//         numWritten+=n;
//         ret = SSL_write(ssl,blockC,n);
//         if(ret==0){Serial.println("error");}
//       }
//       numWritten+=2;
//       ret = SSL_write(ssl,"\r\n",2);
//       ret = SSL_write(ssl,"\r\n",2);
//       if(ret==0){Serial.println("error");}
//       if (ret > 0) {
//         ESP_LOGI(TAG, "OK : %i",numWritten)
//       } else {
//         ESP_LOGI(TAG, "error")
//       }
//       file.close();
//       break;
//     }
//   } while (1);

//   SSL_shutdown(ssl);
//   failed5:
//   close(new_sockfd);
//   new_sockfd = -1;
//   failed4:
//   SSL_free(ssl);
//   ssl = NULL;
//   goto reconnect;
//   failed3:
//   close(sockfd);
//   sockfd = -1;
//   failed2:
//   SSL_CTX_free(ctx);
//   ctx = NULL;
//   failed1:
//   vTaskDelete(NULL);
//   return ;
// } 


// void openssl_server_init(void)
// {
//   esp_log_level_set(TAG, ESP_LOG_INFO); 
//   certFile = SPIFFS.open("/cacert.pem");
//   pkFile = SPIFFS.open("/prvtkey.pem");
//   if(!certFile || !pkFile){
//     Serial.println("fatal error");
//     return;
//   }
//   else{
//     Serial.println("cert files found");
//   }


//   if(sizeof(uint8_t)!=sizeof(unsigned char))
//   {
//     Serial.println("fatal size error");
//     Serial.println();
//     Serial.println(sizeof(uint8_t));
//     Serial.println();
//     Serial.println(sizeof(unsigned char));
//     Serial.println();
//     Serial.println();
//     return;
//   }
//   ESP_LOGI(TAG, "INIT OPENSSL");
//   Serial.println("INIT OPENSSL");
//   int ret;
//   xTaskHandle openssl_handle;

//   ret = xTaskCreate(openssl_example_task,
//     OPENSSL_EXAMPLE_TASK_NAME,
//     OPENSSL_EXAMPLE_TASK_STACK_WORDS,
//     NULL,
//     OPENSSL_EXAMPLE_TASK_PRIORITY,
//     &openssl_handle); 

//   if (ret != pdPASS)  {
//    Serial.printf("create task %s failed\n", OPENSSL_EXAMPLE_TASK_NAME);
//  }
// }
// #endif
