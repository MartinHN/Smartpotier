"use strict"
  var secToMin = 60; // change when using minute based analysis
    var rec_interval = 30; // interval to set on esp for rec
    var WebSocket_connection = null;
    var isOpen = false;
    var lastWsActivityDate = new Date();
    var recStartDate = "None";
    var recState=-1;
    var shouldAskForRecStart = true;
    var lastCloseDate = new Date();
    var lastAskedRecI = 0;
var dps = []; // dataPoints
var dpsdeltas = [];
var modAvgT = 60;
var refCurv = [];
var audio = null;
var alarmWatcher = null;

var hasLocalStorage = false;

var alarmRangeSlope = [0,0];
var alarmRangeAbs = [0,0];
var noSleep = null;
var avgMod=0;
var curTemp= 0;
function xToString(e){
  var s = (e.value!==undefined)?e.value : e
  // var m = s/60
  // var h = m/60
  // return  Math.floor(h)+":"+Math.floor(m%60)+":"+Math.floor(s%60);  
  if(s.getDate()>1){
    var td = new Date(s);
    td.setDate(td.getDate()-1);
  return CanvasJS.formatDate(td,"D'j' + HH:mm:ss");  
  }
  return CanvasJS.formatDate(s,"HH:mm:ss");
  // return s.toISOString().substr(11, 8);
}

function xToSeconds(x){
  return  x.getTime()/1000.0 ;
}
function secondsToD(s){
  var m = Math.floor(s/60)
  var h = Math.floor(m/60)
  var d = Math.floor(h/24)

  return {s:s%60,m:m%60,h:h%24,d:d}
}

function secondsToX(s){
  // UTC offset
  
  var dl = new Date(Date.UTC(1970,0,1,0,0,0))
  var d0 = dl.getTimezoneOffset()*60;
  //var uOff = parseInt(r[1])
  return new Date((s +d0)*1000  )   ; // forUTC -1
}

if (!"WebSocket" in window){
  alert("WebSocket NOT supported by your Browser!");
  $('#WebSocket_State').text("WebSocket NOT supported by your Browser!");
}
var chart=null;
var contexteAudio =null;
window.onload = function(){
  try{
    hasLocalStorage  ='localStorage' in window && window.localStorage !== null;
  }
  catch(error){

  }
  if(!hasLocalStorage){
    var err =  ["local storage not supported","values (reference curve, alarm thresholds...) won't be saved"]
    console.error(err)
    $( "#log_div" ).append( "<font color=\"red\">"+err.join("<br>")+ "</font><br>" );
  }
  initNotify(true);

  noSleep = new NoSleep();

  // crée un contexteaudio
  contexteAudio = new (window.AudioContext || window.webkitAudioContext)();
  audio = new function(){


// create Oscillator node
this.oscillator = contexteAudio.createOscillator();
this.gainNode = contexteAudio.createGain();
this.lfoGain = contexteAudio.createGain();
this.lfo = contexteAudio.createOscillator();
this.lfo.frequency.setTargetAtTime(1,contexteAudio.currentTime,0.015);


this.lfo.type = 'square';
this.lfo.connect(this.lfoGain.gain);

this.oscillator.connect(this.lfoGain);
//this.lfo.connect(this.gainNode.gain);
this.lfoGain.connect(this.gainNode)
this.gainNode.connect(contexteAudio.destination);
this.oscillator.type = 'triangle';
this.oscillator.frequency.setTargetAtTime( 440,contexteAudio.currentTime,0.015); // valeur en hertz
this.openGain = .85;
this.gainNode.gain.setTargetAtTime(0,contexteAudio.currentTime,0.00015);;//this.openGain;
this.oscillator.start();
this.lfo.start();



// oscillator.start();
this.play2 = function(state){
  var v = 0;
  if(state===null || state===true ){v=this.openGain}
    this.gainNode.gain.setTargetAtTime(v,contexteAudio.currentTime,0.15);

}
this.play = function(){
  this.gainNode.gain.setTargetAtTime(this.openGain,contexteAudio.currentTime,0.15);
  
  try {
    this.oscillator.start();
    this.lfo.start();
  }
  catch(error) {
  // console.log('necessary but weird error on ipad')
}


}
this.pause =function(){
  this.gainNode.gain.setTargetAtTime(0,contexteAudio.currentTime,0.15);

}
this.setVolume = function(v){
  this.gainNode.gain.setTargetAtTime(v,contexteAudio.currentTime,0.15);
}

}
// }
$("#enableAlarm").change(enableAlarm);
connect();
var lastTouchTime = 0;
chart = new CanvasJS.Chart("chartContainer", {
  zoomEnabled: true,
  animationEnabled: false,
  backgroundColor: "transparent",
  title :{text: ""},
  toolTip: {shared: true,backgroundColor: "#DDDDDD"},
  axisX:{

    labelFormatter: xToString,  
    // valueFormatString:"HH:mm:ss",
    labelFontColor:"black"
  },

  legend:{
    cursor: "pointer",
    fontColor:"white",
    itemclick: function (e) {
      // avoid double click on touch screens
      var now = new Date();

      if(now-lastTouchTime>800){
        lastTouchTime = now
        if (typeof (e.dataSeries.visible) === "undefined" || e.dataSeries.visible) {
          e.dataSeries.visible = false;
        } else {
          e.dataSeries.visible = true;
        }
      }

      e.chart.render();
    }

  },
  axisY: {
    includeZero: false,
    suffix : "C",
    labelFontColor:"black",
    valueFormatString:"####"
  },
  axisY2: {
    includeZero: false,
    suffix : "C/h",
    labelFontColor:"black",
  },
  data: [
  {
    type: "splineArea",
    name: "Temperature",
    showInLegend: true,
    xValueType: "dateTime",
    xValueFormatString:"HH:mm:ss",
    dataPoints: dps,
    color:"#5D31FF"
  },
  {
    type: "spline",
    name: "Slope",
    showInLegend: true,
    visible:false,
    axisYType: "secondary",
    xValueType: "dateTime",
    xValueFormatString:"HH:mm:ss",
    dataPoints: dpsdeltas,
    color:"#9BBB58"
  },
  {
    type: "line",
    name: "Ref",
    showInLegend: true,
    xValueType: "dateTime",
    xValueFormatString:"HH:mm:ss",
    dataPoints: refCurv,
    color:"#FFDDB8",//"#C0504E",

  }
  ]
});
setDarkTheme(false);
chart.render();
if(hasLocalStorage){
  var it = window.localStorage.getItem("modAvgT");
  if(it){modAvgT = parseFloat(it);}
  $("#refCurv").val(window.localStorage.getItem("refCurv"));
  var ar = window.localStorage.getItem("alarmRange")
  if(ar){ar = ar.split(',');}
  else{ar = [0,0]}
    $("#alarmRangeMin").val( parseFloat(ar[0]))
  $("#alarmRangeMax").val( parseFloat(ar[1]))

  ar = window.localStorage.getItem("alarmAbsRange")
  if(ar){ar = ar.split(',');}
  else{ar = [0,0]}
    $("#alarmAbsRangeMin").val( parseFloat(ar[0]))
  $("#alarmAbsRangeMax").val( parseFloat(ar[1]))
}
updateRefCurv()
var connCheckInterval = 1000;
var connTimeOut = 3*connCheckInterval;
var errTimeOut = 10000;


window.setInterval(function(){
  var d = new Date();
  if(WebSocket_connection && (WebSocket_connection.readyState==WebSocket_connection.OPEN)){
    WebSocket_connection.send("p:");
    var lastKnownT = -1;
    if(dps.length){
      var lastE = dps[dps.length-1].x;
      lastKnownT=Math.floor(xToSeconds(lastE) );
    }
    var deltaT =d -lastWsActivityDate ;
    if(recState!=0 && d-lastAskedRecI >=rec_interval*1000/4.0 ){
      lastAskedRecI = d;
      var msg = "i:"+lastKnownT;
      if(lastKnownT>=0  || lastKnownT==-1){
              // console.log(msg);
              WebSocket_connection.send(msg);
            }
            else{
              console.error("Error lastKnownt: "+msg);
            }
          }
        }
        
        var deltaT =d -lastWsActivityDate ;
        var timeOut = (deltaT>connTimeOut);
        if(timeOut || !isOpen){
          if(timeOut){_setState(false);}
          var doClose =timeOut&& (d-lastCloseDate> errTimeOut) ;
          if (doClose){
            lastCloseDate = d;
            connect(true);
          }
          connect(false);
        }
        // Save data to sessionStorage
        //sessionStorage.setItem('dps',JSON.stringify( dps));
      },connCheckInterval);
};
window.onbeforeunload = function() {
  // disable onclose handler first
  // WebSocket_connection.onclose = function () {};
  WebSocket_connection.close();
  WebSocket_connection=null;
};

function computeAvg(time){
  var scale = 1.0/(60*60);
  var avg = 0;
  if(dps.length>1){
    var meanX =0;
    var meanY=0;
    var count = 0;
    var i = 0;
    for (i = dps.length-1 ; i>=0;i-- ){
      if(xToSeconds(dps[i].x)- xToSeconds(dps[dps.length-1].x)< (-time)){break;}
      meanX+=xToSeconds(dps[i].x);
      meanY+=dps[i].y;
      count++;
    }

    meanX/=(count);meanY/=(count);


    var sum1 = 0;
    var sum2 = 0;
    var j = 0;
    var d = 0;
    for(j = 0 ; j < count ; j+=1){
      i = dps.length-1 - j;
      d = (xToSeconds(dps[i].x)-meanX)*scale;
      sum1+=d*(dps[i].y-meanY);
      sum2+=d*d;
    }
    
    avg = sum1/sum2;

  }
  return avg;

}

function connect(doClose){
  if(WebSocket_connection && !doClose && (WebSocket_connection.readyState == WebSocket_connection.CONNECTING)){
    console.log("connecting");
    $('#WebSocket_State').text("Connecting");
    // return;
  }
  else if(WebSocket_connection && doClose && (WebSocket_connection.readyState != WebSocket_connection.CLOSING)){
    console.log("closing");
    $('#WebSocket_State').text("Closing");
    WebSocket_connection.close();
    shouldAskForRecStart = true;
  }
  else if(WebSocket_connection==null){
    try{
      console.log("start new connection");
      shouldAskForRecStart = true;
        WebSocket_connection = new WebSocket("ws://smartpotier.local:81");//$('#websocket_address').val());
        $('#WebSocket_State').text("Starting Connection");
      }
      catch (exception){
        $('#WebSocket_State').text("smartpotier not found");
        console.error(exception);
        WebSocket_connection = null;

      }
    }
    else if(doClose){
      WebSocket_connection = null;
    }
    else if(WebSocket_connection!=null){
      if(!isOpen&&WebSocket_connection.readyState==WebSocket_connection.OPEN ){
        _setState(true);
        console.error("weird state error");
      }
      else{
        console.log("ws already connected "+WebSocket_connection.readyState);
      }
    }
    if(WebSocket_connection){

      WebSocket_connection.onopen = function(event){
        lastWsActivityDate = new Date();
        lastAskedRecI = 0;
        console.log("ws opened");
        _setState(true);
        WebSocket_connection.send("r:")
        WebSocket_connection.send("q:"+rec_interval)

      };

      WebSocket_connection.onmessage = function (evt) {
        lastWsActivityDate = new Date() ;
        if(!isOpen){_setState(true);console.error("ws : recieving msg, but unmatching state");}
        
        //console.log(evt.data);
        if (typeof evt.data === 'string' || evt.data instanceof String){
          if( evt.data.startsWith("p:")){
            var startDate = evt.data.substr(2).trim();
            if(startDate!=recStartDate){
              dps.length = 0;
              dpsdeltas.length=0;
              recStartDate=startDate;
            }
          }

         //  else if( evt.data.startsWith("v:")){
         //   curTemp = Math.floor(parseFloat(evt.data.substr(2).trim())+0.5);
         //   $( "#output_div" ).html( curTemp);//+ " &#8451;" );
         // }
         else if( evt.data.startsWith("r:")){
          recState = parseInt(evt.data.substr(2).trim());
          lastAskedRecI=0;
          $("#rec_button").css("background",recState==1?"red":"green")
          $("#rec_button").text(recState==1?"Stop Rec":"Start Rec")
          console.log("recieved recstate : "+recState);
          if (recState)WebSocket_connection.send("p:");
        }
        

        else if(evt.data.startsWith("err:")){
          console.error(evt);
          $( "#log_div" ).append( evt.data.substr(4).trim()+ "<br>" );
          $('#WebSocket_State').text(evt.data);
        }
        else if(evt.data.startsWith("inf:")){
          console.log(evt);
          $( "#log_div" ).append( evt.data.substr(4).trim()+ "<br>" );
          
        }
        else{
          console.log(evt);
        }
      }
      else{
        var fileReader = new FileReader();

        fileReader.onload = function(progressEvent) {
          var Arr = new Uint8Array(this.result);
          var cmd = String.fromCharCode(Arr[0]);
          var dataview = new DataView(this.result);
          if(cmd=='v'){
            var val = dataview.getFloat32(1,false);
            curTemp = Math.floor(val+0.5);
            $( "#output_div" ).html( curTemp);//+ " &#8451;" );
            var totalTime = "NA"
            if(dps.length>0){
              totalTime = xToString(dps[dps.length-1].x)
            }
            $("#total_time").html(totalTime)
          }
          else if(cmd=='l'){
            var avg5 = 0.0;
            var avg30 = 0.0;
            var i = 0;
            var logRcv = []
            var hasNewInfo = false;
            for(i = 1 ; i < dataview.byteLength ; i+=8){
              var x = dataview.getUint32(i,false); // in seconds
              var xval = secondsToX(x);
              var yval= dataview.getFloat32(i+4,false)
              logRcv.push([x,yval])
              var maxSeconds =  3*24*60*60; 
              if( x < maxSeconds && (dps.length==0 || (dps[dps.length-1].x<xval))){
                dps.push({x:xval,y: yval});
                avg5 = computeAvg(5*secToMin);
                avg30 = computeAvg(30*secToMin);
                dpsdeltas.push({x:xval,y: x<30*secToMin?0:avg30 });
                hasNewInfo = true;
                
              }
              else{
                //console.error("non valid entry ",xval,xToSeconds(dps[dps.length-1].x));
              }

            }
          // if(logRcv.length){console.log(logRcv);}

          chart.render();
          var prec = 1.0;
          if(hasNewInfo){
            $( "#avg5_div" ).text(Math.round(avg5*prec)/prec);
            $( "#avg30_div" ).text(Math.round(avg30*prec)/prec);
          }
          avgMod = computeAvg(modAvgT*secToMin);
          $( "#avgmod_div" ).text(Math.round(avgMod*prec)/prec);
          $("#mod_units").html("&#8451;/h ("+modAvgT+(secToMin==1?"s":"m")+")");
          $("#5_units").html("&#8451;/h (5"+(secToMin==1?"s":"m")+")");
          $("#30_units").html("&#8451;/h (30"+(secToMin==1?"s":"m")+")");

        }
        else{
          console.error('unknown cmd : '+cmd,Arr);
        }


      };

      fileReader.readAsArrayBuffer(evt.data);




          // it's something else
        }
      };

      WebSocket_connection.onerror = function( error)
      {
        lastAskedRecI = 0;
        console.error(error);
        _setState(false);
        $('#WebSocket_State').text("Error,...  connection closed");
        WebSocket_connection.close();
      };
      
      WebSocket_connection.onclose = function()
      { 
        lastAskedRecI = 0;
        lastCloseDate = new Date();
        console.log("ws closed");
        _setState(false);
        $('#WebSocket_State').text("Disconnected");
        WebSocket_connection = null;
      };
    }
  }

  function _setState(s){
    //if(isOpen!=s){var debug = 1;}
    isOpen = s;
    $('#led').css("background",s===true?"green":"red");
    if(s){
      $('#WebSocket_Form').html('<input type="text" id="websocket_message" value="" onkeydown = \"if (event.keyCode == 13)javascript:WebSocketSend()\"/>');
      $('#WebSocket_State').text("Connected");
    }
    else{
      $('#WebSocket_Form').html('');
      $('#WebSocket_State').text("Disconnected");
    }
  }

  function WebSocketSend(){
    var valToSend = $('#websocket_message').val();
    WebSocket_connection.send(valToSend);
  }

  function download_graph(){

    var csvContent = "data:text/csv;charset=utf-8,";
    if(dps){
      dps.forEach(function(p){
        var diffMs = p.x-dps[0].x;
        var diffMn=  diffMs/60000.0;
        var row = diffMn+","+p.y;
      // add carriage return
      csvContent += row + "\r\n"; 
    }); 

      var encodedUri = encodeURI(csvContent);
      var link = document.createElement("a");
      link.setAttribute("href", encodedUri);
      link.setAttribute("download", "my_data.csv");
    // Required for FF

    document.body.appendChild(link); 
    link.click();
    document.body.removeChild(link); 
  }


}
function toggleRec(){
  lastAskedRecI=0;
  if (recState==0 || window.confirm("t'es sur de sur??") === true){
    WebSocket_connection.send("r:"+(recState==1?"0":"1"))
  }

}

function update3rdSlope(){
  console.log($("#3rdSlope").val());
  var test=parseFloat($("#3rdSlope").val());
  if(test>0){
    modAvgT = test;
    if(hasLocalStorage){window.localStorage.setItem("modAvgT",modAvgT);}
    $("#mod_units").html("&#8451;/h ("+modAvgT+(secToMin==1?"s":"m")+")");
  }
}
function updateRefCurv(){
  var tmp=$("#refCurv").val().split(',');
  if(tmp.length){
    refCurv.length=0;
    for (var i = 0 ;i < tmp.length ;i++){
      var x = 0;
      
      var y = -1
      var split = tmp[i].split(':')
      if( split.length>1){
        x = parseFloat(split[0])*60*60
        y = parseFloat(split[1])
      }
      else{
        y = parseFloat(tmp[i]);
        x = i *60*60;
      }
      refCurv.push({x:secondsToX(x),y:y})
    }
    chart.render();
    console.log(refCurv)

    if(hasLocalStorage){window.localStorage.setItem("refCurv",$("#refCurv").val());}

  }
}

function updateAlarm(){
  audio.setVolume( 0);
  document.getElementById("output_div").style.backgroundColor = null;
  document.getElementById("mod_units").style.backgroundColor = null;
  alarmRangeSlope = [parseFloat($("#alarmRangeMin").val()),parseFloat($("#alarmRangeMax").val())]
  alarmRangeAbs=[parseFloat($("#alarmAbsRangeMin").val()),parseFloat($("#alarmAbsRangeMax").val())]
  if(hasLocalStorage){
    window.localStorage.setItem("alarmRange",alarmRangeSlope);
    window.localStorage.setItem("alarmAbsRange",alarmRangeAbs);
  }


  
}

function notifyWatch(){
  var wasAlarm = false;
  return setInterval(function(){
    var isOutOfSlope =  (alarmRangeSlope[0]!=alarmRangeSlope[1] && (avgMod<alarmRangeSlope[0] || avgMod>alarmRangeSlope[1]));
    var isOutOfAbs = (alarmRangeAbs[0]!=0 && curTemp<alarmRangeAbs[0]) || (alarmRangeAbs[1]!=0 && curTemp>alarmRangeAbs[1]);
    var newAlarmState = isOutOfAbs || isOutOfSlope
    document.getElementById("output_div").style.backgroundColor = null;
    document.getElementById("mod_units").style.backgroundColor = null;
    if(wasAlarm!=newAlarmState){
      if(newAlarmState){audio.setVolume( 1);}
      else{audio.setVolume(0);}
    }
    if(newAlarmState){

                // $("#output_div").
                if(isOutOfAbs)  document.getElementById("output_div").style.backgroundColor = "red";
                if(isOutOfSlope)  document.getElementById("mod_units").style.backgroundColor = "red";

                if(("Notification" in window) &&
                  (window.Notification.permission === "granted") && 
                  !wasAlarm){
                  var alarmMsg = "alarm :"
                if(isOutOfSlope) alarmMsg+=' out of slope '
                  if(isOutOfAbs) alarmMsg+=' out of abs '

                    var notification = new Notification('Notification title', {
                      icon: 'https://image.flaticon.com/icons/png/128/649/649393.png',
                      body: alarmMsg,
                      renotify:true,
                      vibrate:[200, 100, 200],
                      tag:"alarm",
                    });
                }
              }
              wasAlarm=newAlarmState;
            },1000);
}


function initNotify(firstCall) {

  // Let's check if the browser supports notifications
  // if ("Notification" in window) {
  //   if (firstCall || window.Notification.permission !== "denied") {
  //     window.Notification.requestPermission(function (permission) {
  //     });
  //   }
  // }
  // else{
  //     if(firstCall){console.error("This browser does not support desktop notification");}
  // }

  // At last, if the user has denied notifications, and you 
  // want to be respectful there is no need to bother them any more.
}
function enableAlarm() {

  // needed here because enableAlarm called on user input
  audio.play();
  audio.setVolume(0);
  initNotify(false);
  if(this.checked ){
    updateAlarm();
    noSleep.enable();
    if(alarmWatcher){
      clearInterval(alarmWatcher);
    }
    alarmWatcher = notifyWatch();
    console.log('alarm set : ',alarmRangeSlope);
    
  }
  else{
    document.getElementById("output_div").style.backgroundColor = null;
    document.getElementById("mod_units").style.backgroundColor = null;
    noSleep.disable();
    if(alarmWatcher){
      clearInterval(alarmWatcher);
    }
    audio.setVolume(0);
    // audio.pause();
    console.log('alarm unset')
  // document.removeEventListener('touchstart', enableNoSleep, false);
}


}

function reset(){
  if (window.confirm("t'es sur de sur??") === true){
    WebSocket_connection.send("reset");
  }
  else{

  }
}

function setDarkTheme(isDark){

  var fontColor = isDark?"#FFFDFB":"black";
  $('body').css("color",fontColor);
  $('body').css("background-color", isDark?"#222222":"white");
  chart.toolTip.backgroundColor = isDark? "#DDDDDD":"white";
  chart.axisX.labelFontColor = fontColor;
  chart.axisY.labelFontColor = fontColor;
  chart.axisY2.labelFontColor = fontColor;
  chart.options.legend.fontColor = fontColor;


}