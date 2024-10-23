// Watchdog設定
const WATCHDOG_INTERVAL = 100; // 100msごとにwatchdog信号を送信
let timestamp_ms = 0;

var countup = setInterval(function(){
  timestamp_ms += WATCHDOG_INTERVAL;
  var xhr = new XMLHttpRequest();
  xhr.open("PUT", `./watchdog?timestamp_ms=${JSON.stringify(timestamp_ms)}`);
  xhr.send();
} ,WATCHDOG_INTERVAL);