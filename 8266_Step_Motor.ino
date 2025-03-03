#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Stepper.h>
#include "credentials.h"  // เพิ่ม include สำหรับไฟล์ credentials

// กำหนดค่าสเต็ปปเปอร์มอเตอร์ 28BYJ-48
// จำนวนสเต็ปต่อรอบของ 28BYJ-48 กับ ULN2003
#define STEPS_PER_REVOLUTION 2048 // 32 สเต็ปต่อรอบ x 64:1 เกียร์ทด

// ในกรณีที่บอร์ดใช้เลข D1, D2, D3, D4 โดยตรง จะต้องกำหนดค่าเหล่านี้ให้ตรงกับ GPIO
// ถ้าบอร์ดของคุณเป็น NodeMCU หรือ Wemos D1 Mini จะมีค่าเหล่านี้กำหนดไว้แล้ว
#ifndef D1
#define D1 5  // GPIO5
#endif

#ifndef D2
#define D2 4  // GPIO4
#endif

#ifndef D3
#define D3 0  // GPIO0
#endif

#ifndef D4
#define D4 2  // GPIO2
#endif

// พอร์ตเชื่อมต่อกับไดรเวอร์ ULN2003 (ใช้ D1-D4)
#define IN1 D1
#define IN2 D2
#define IN3 D3
#define IN4 D4

// ความเร็วมอเตอร์ปัจจุบัน (RPM)
int motorSpeed = 10;
// ทิศทาง (1 = ตามเข็ม, -1 = ทวนเข็ม)
int motorDirection = 1;
// จำนวนสเต็ปที่จะหมุน
int stepsToMove = 2048;
// สถานะการทำงาน (0 = หยุด, 1 = กำลังทำงาน)
int motorRunning = 0;

// สร้าง Object สำหรับควบคุมสเต็ปเปอร์มอเตอร์
Stepper stepper(STEPS_PER_REVOLUTION, IN1, IN3, IN2, IN4);

// สร้าง WebServer ที่พอร์ต 80
ESP8266WebServer server(80);

// HTML สำหรับหน้าเว็บควบคุม
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP8266 Stepper Motor Control</title>
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 2.4rem;}
    p {font-size: 1.6rem;}
    body {max-width: 600px; margin:0px auto; padding: 20px;}
    .slider {width: 100%; height: 50px;}
    .output {font-size: 2rem; font-weight: bold; margin: 20px 0;}
    .button {
      background-color: #4CAF50;
      border: none;
      color: white;
      padding: 16px 40px;
      text-decoration: none;
      font-size: 18px;
      margin: 10px;
      cursor: pointer;
      border-radius: 6px;
    }
    .button-stop {background-color: #f44336;}
    .button-cw {background-color: #2196F3;}
    .button-ccw {background-color: #ff9800;}
    .control-group {
      margin: 20px 0;
      padding: 15px;
      border: 1px solid #ddd;
      border-radius: 10px;
      background-color: #f9f9f9;
    }
    .speed-display {
      font-size: 2.5rem;
      font-weight: bold;
      margin: 20px 0;
      color: #2196F3;
    }
    .direction-display {
      font-size: 1.5rem;
      margin: 10px 0;
      color: #ff9800;
    }
    .steps-input {
      width: 100px;
      padding: 10px;
      font-size: 16px;
      border-radius: 5px;
      border: 1px solid #ddd;
      text-align: center;
    }
    .status {
      padding: 10px;
      margin-top: 20px;
      border-radius: 5px;
      font-weight: bold;
    }
    .status-stopped {
      background-color: #ffcccc;
      color: #f44336;
    }
    .status-running {
      background-color: #ccffcc;
      color: #4CAF50;
    }
  </style>
</head>
<body>
  <h2>ESP8266 Stepper Motor Control</h2>
  <h3>28BYJ-48 with ULN2003</h3>
  
  <div class="speed-display">
    <span id="speedValue">10</span> RPM
  </div>
  
  <div class="direction-display">
    Direction: <span id="directionValue">Clockwise</span>
  </div>
  
  <div class="control-group">
    <p>Speed (RPM)</p>
    <input type="range" min="1" max="20" value="10" class="slider" id="speedSlider">
  </div>
  
  <div class="control-group">
    <p>Steps to move</p>
    <input type="number" min="1" max="4096" value="2048" class="steps-input" id="stepsInput">
    <p><small>2048 steps = 1 complete revolution</small></p>
  </div>
  
  <div class="control-group">
    <p>Direction</p>
    <button class="button button-cw" id="cwButton" onclick="setDirection(1)">Clockwise</button>
    <button class="button button-ccw" id="ccwButton" onclick="setDirection(-1)">Counter-Clockwise</button>
  </div>
  
  <div class="control-group">
    <p>Control</p>
    <button class="button" id="startButton" onclick="startMotor()">START</button>
    <button class="button button-stop" id="stopButton" onclick="stopMotor()">STOP</button>
  </div>
  
  <div id="statusDiv" class="status status-stopped">
    Motor Status: <span id="statusText">Stopped</span>
  </div>

  <script>
    var speedSlider = document.getElementById("speedSlider");
    var speedValue = document.getElementById("speedValue");
    var directionValue = document.getElementById("directionValue");
    var stepsInput = document.getElementById("stepsInput");
    var statusText = document.getElementById("statusText");
    var statusDiv = document.getElementById("statusDiv");
    var cwButton = document.getElementById("cwButton");
    var ccwButton = document.getElementById("ccwButton");
    
    // อัพเดทความเร็ว
    speedSlider.oninput = function() {
      speedValue.innerHTML = this.value;
      updateMotorSpeed(this.value);
    }
    
    // อัพเดทจำนวนสเต็ป
    stepsInput.onchange = function() {
      updateStepsToMove(this.value);
    }
    
    // อัพเดทความเร็วมอเตอร์
    function updateMotorSpeed(speed) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/speed?value=" + speed, true);
      xhr.send();
    }
    
    // อัพเดทจำนวนสเต็ปที่จะหมุน
    function updateStepsToMove(steps) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/steps?value=" + steps, true);
      xhr.send();
    }
    
    // ตั้งค่าทิศทาง
    function setDirection(direction) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/direction?value=" + direction, true);
      xhr.send();
      
      if (direction == 1) {
        directionValue.innerHTML = "Clockwise";
        cwButton.style.opacity = "1";
        ccwButton.style.opacity = "0.6";
      } else {
        directionValue.innerHTML = "Counter-Clockwise";
        ccwButton.style.opacity = "1";
        cwButton.style.opacity = "0.6";
      }
    }
    
    // เริ่มหมุนมอเตอร์
    function startMotor() {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/start", true);
      xhr.send();
      statusText.innerHTML = "Running";
      statusDiv.className = "status status-running";
    }
    
    // หยุดมอเตอร์
    function stopMotor() {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/stop", true);
      xhr.send();
      statusText.innerHTML = "Stopped";
      statusDiv.className = "status status-stopped";
    }
    
    // ดึงสถานะเริ่มต้น
    window.onload = function() {
      // ดึงค่าความเร็วปัจจุบัน
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          speedSlider.value = data.speed;
          speedValue.innerHTML = data.speed;
          stepsInput.value = data.steps;
          
          if (data.direction == 1) {
            directionValue.innerHTML = "Clockwise";
            cwButton.style.opacity = "1";
            ccwButton.style.opacity = "0.6";
          } else {
            directionValue.innerHTML = "Counter-Clockwise";
            ccwButton.style.opacity = "1";
            cwButton.style.opacity = "0.6";
          }
          
          if (data.running == 1) {
            statusText.innerHTML = "Running";
            statusDiv.className = "status status-running";
          } else {
            statusText.innerHTML = "Stopped";
            statusDiv.className = "status status-stopped";
          }
        })
        .catch(error => console.error('Error:', error));
    }
  </script>
</body>
</html>
)rawliteral";

// ตัวแปรสำหรับตรวจสอบเวลาในการหมุนมอเตอร์
unsigned long previousMillis = 0;
int stepsCompleted = 0;

void setup() {
  // เริ่มการสื่อสาร Serial
  Serial.begin(115200);
  Serial.println("ESP8266 Stepper Motor Control");
  
  // แสดงค่า pin ที่ใช้งาน
  Serial.print("Using pins: IN1=");
  Serial.print(IN1);
  Serial.print(", IN2=");
  Serial.print(IN2);
  Serial.print(", IN3=");
  Serial.print(IN3);
  Serial.print(", IN4=");
  Serial.println(IN4);

  // ตั้งค่าความเร็วเริ่มต้นของมอเตอร์
  stepper.setSpeed(motorSpeed);

  // เชื่อมต่อ WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  // แสดง local IP address
  Serial.print("Connected to WiFi! IP Address: ");
  Serial.println(WiFi.localIP());

  // กำหนด Route handlers
  server.on("/", handleRoot);
  server.on("/speed", handleSpeed);
  server.on("/steps", handleSteps);
  server.on("/direction", handleDirection);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);
  server.on("/status", handleStatus);
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not found");
  });

  // เริ่มต้น server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // จัดการคำขอจาก WebServer
  server.handleClient();
  
  // ตรวจสอบว่าต้องหมุนมอเตอร์หรือไม่
  if (motorRunning) {
    unsigned long currentMillis = millis();
    
    // คำนวณระยะเวลาระหว่างการหมุนแต่ละสเต็ป
    unsigned long stepInterval = (60L * 1000L) / (motorSpeed * STEPS_PER_REVOLUTION);
    
    if (currentMillis - previousMillis >= stepInterval) {
      previousMillis = currentMillis;
      
      // หมุนมอเตอร์หนึ่งสเต็ป
      stepper.step(motorDirection);
      stepsCompleted++;
      
      // เมื่อหมุนครบตามจำนวนแล้ว ให้หยุด
      if (stepsCompleted >= stepsToMove) {
        stepsCompleted = 0;
        motorRunning = 0;
        Serial.println("Motion completed.");
      }
      
      // ให้ตัว CPU ทำงานอื่นๆ ได้บ้าง
      yield();
    }
  }
}

// ส่งหน้าเว็บหลัก
void handleRoot() {
  server.send(200, "text/html", index_html);
}

// จัดการคำขอปรับความเร็ว
void handleSpeed() {
  if (server.hasArg("value")) {
    motorSpeed = server.arg("value").toInt();
    stepper.setSpeed(motorSpeed);
    Serial.print("Motor speed set to: ");
    Serial.println(motorSpeed);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad request");
  }
}

// จัดการคำขอปรับจำนวนสเต็ป
void handleSteps() {
  if (server.hasArg("value")) {
    stepsToMove = server.arg("value").toInt();
    Serial.print("Steps to move set to: ");
    Serial.println(stepsToMove);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad request");
  }
}

// จัดการคำขอปรับทิศทาง
void handleDirection() {
  if (server.hasArg("value")) {
    motorDirection = server.arg("value").toInt();
    Serial.print("Motor direction set to: ");
    Serial.println(motorDirection == 1 ? "Clockwise" : "Counter-Clockwise");
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad request");
  }
}

// จัดการคำขอเริ่มทำงาน
void handleStart() {
  if (!motorRunning) {
    motorRunning = 1;
    stepsCompleted = 0;
    previousMillis = millis();
    Serial.println("Starting motor rotation...");
  }
  server.send(200, "text/plain", "Started");
}

// จัดการคำขอหยุดทำงาน
void handleStop() {
  motorRunning = 0;
  stepsCompleted = 0;
  Serial.println("Stopping motor...");
  server.send(200, "text/plain", "Stopped");
}

// จัดการคำขอดึงสถานะ
void handleStatus() {
  String json = "{";
  json += "\"speed\":" + String(motorSpeed) + ",";
  json += "\"direction\":" + String(motorDirection) + ",";
  json += "\"steps\":" + String(stepsToMove) + ",";
  json += "\"running\":" + String(motorRunning);
  json += "}";
  server.send(200, "application/json", json);
}
