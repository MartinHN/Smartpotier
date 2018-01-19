# Smartpotier
 monitor temperature from clay hoven
 
WARNING
this stays fundamentaly a week end project!
although its using a lot of functionnality of esp32, (I use some of its parts as reference design for my other implementation)


# basic functionality
ESP32 reads analog voltage given by a temp sensor.

then its serve a minimal webpage displaying : 
* a graph of the current temp recording
* (optionally) a reference curve given by the user
* current value of temperature and average slope on 3 different time interval
* Web audio alarms when temperature gets out of given thresholds (absolute value and slope)


# techno used

ESP
* esp integrated file system
* esp nstp to (allow consistents timestamps of recordings even if ESP gets out of power)
* esp FTP server (to modify the html served)
* esp Websockets (fullduplex communication protocol )

WebPage

* basic interface/ menus (start/stop / thresholds...)
* sleep proof alarm in webaudio 
* export curve as csv
* a cumbersome websocket auto reconnect




