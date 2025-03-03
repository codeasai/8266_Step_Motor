#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <Stepper.h>
#include <EEPROM.h>
#include "credentials.h"
#include "28BYJ48.h"

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

// สร้าง object สำหรับควบคุมมอเตอร์
StepperMotor motor(IN1, IN2, IN3, IN4);

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
    .clock-face {
      width: 300px;
      height: 300px;
      border: 10px solid #333;
      border-radius: 50%;
      margin: 20px auto;
      position: relative;
      background: #fff;
    }
    .hour-button {
      position: absolute;
      width: 40px;
      height: 40px;
      border-radius: 50%;
      background: #4CAF50;
      color: white;
      border: none;
      cursor: pointer;
      font-size: 18px;
      transform-origin: center;
    }
    .hour-button:hover {
      background: #45a049;
    }
    .hour-button.active {
      background: #ff9800;
    }
    .control-group {
      margin: 20px 0;
      padding: 15px;
      border: 1px solid #ddd;
      border-radius: 10px;
      background-color: #f9f9f9;
    }
    .slider {width: 100%; height: 50px;}
    .status {
      padding: 10px;
      margin-top: 20px;
      border-radius: 5px;
      font-weight: bold;
    }
    .status-stopped { background-color: #ffcccc; color: #f44336; }
    .status-running { background-color: #ccffcc; color: #4CAF50; }
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
    .button-stop {
      background-color: #f44336;
    }
    .direction-button {
        background-color: #2196F3;
        opacity: 0.6;
        padding: 10px 20px;
    }
    .direction-button.active {
        opacity: 1;
    }
    .speed-button {
        background-color: #2196F3;
        opacity: 0.6;
        padding: 10px 20px;
        margin: 0 5px;
    }
    .speed-button.active {
        opacity: 1;
    }
    .button-reset {
        background-color: #ff9800;  // สีส้ม
    }
  </style>
</head>
<body>
  <h2>ESP8266 Stepper Motor Control</h2>
  <h3>28BYJ-48 Clock Position Control</h3>
  
  <div class="clock-face" id="clockFace">
    <!-- ปุ่มจะถูกสร้างด้วย JavaScript -->
  </div>
  
  <div class="control-group">
    <p>Motor Speed:</p>
    <button class="button speed-button" id="lowSpeed" onclick="setSpeed('low')">Low Speed</button>
    <button class="button speed-button" id="highSpeed" onclick="setSpeed('high')">High Speed</button>
  </div>

  <div class="status" id="statusDiv">
    <p>Current Position: <span id="currentHour">12</span></p>
    <p>Target Position: <span id="targetHour">12</span></p>
  </div>

  <div class="control-group">
    <p>Rotation Direction:</p>
    <button class="button direction-button" id="autoButton" onclick="setDirection('auto')">Auto</button>
    <button class="button direction-button" id="cwButton" onclick="setDirection('cw')">Clockwise</button>
    <button class="button direction-button" id="ccwButton" onclick="setDirection('ccw')">Counter-Clockwise</button>
  </div>

  <div class="control-group">
    <button class="button" id="startButton" onclick="startMotor()">MOVE TO TARGET</button>
    <button class="button button-reset" id="resetButton" onclick="resetMotor()">RESET</button>
  </div>

  <script>
    // สร้างปุ่มบนหน้าปัดนาฬิกา
    const clockFace = document.getElementById('clockFace');
    for (let i = 1; i <= 12; i++) {
      const button = document.createElement('button');
      button.textContent = i;
      button.className = 'hour-button';
      
      // คำนวณตำแหน่งของปุ่ม
      const angle = (i - 3) * 30 * Math.PI / 180; // เริ่มที่ 3 นาฬิกา
      const radius = 120; // รัศมีของวงกลม
      const left = 150 + radius * Math.cos(angle);
      const top = 150 + radius * Math.sin(angle);
      
      button.style.left = left - 20 + 'px';  // ลบครึ่งความกว้างของปุ่ม
      button.style.top = top - 20 + 'px';   // ลบครึ่งความสูงของปุ่ม
      
      button.onclick = () => moveToHour(i);
      clockFace.appendChild(button);
    }

    let currentSpeed = 'low';  // เริ่มต้นที่ความเร็วต่ำ

    function setSpeed(speed) {
        currentSpeed = speed;
        
        // อัพเดทสถานะปุ่ม
        document.getElementById('lowSpeed').classList.toggle('active', speed === 'low');
        document.getElementById('highSpeed').classList.toggle('active', speed === 'high');

        // ส่งค่าความเร็วไปยังบอร์ด (low = 5 RPM, high = 12 RPM)
        const rpm = speed === 'low' ? 5 : 12;
        fetch('/speed?value=' + rpm)
            .then(response => response.text())
            .catch(error => console.error('Error:', error));
    }

    // เริ่มต้นที่ความเร็วต่ำ
    setSpeed('low');

    function moveToHour(hour) {
      // อัพเดท UI ทันทีเมื่อเลือกตำแหน่งเป้าหมาย
      document.getElementById('targetHour').textContent = hour;
      
      fetch('/moveToHour?hour=' + hour)
        .then(response => response.text())
        .catch(error => console.error('Error:', error));
    }

    function startMotor() {
      const currentHour = parseInt(document.getElementById('currentHour').textContent);
      const targetHour = parseInt(document.getElementById('targetHour').textContent);
      
      if (currentHour === targetHour) {
        alert('Current position is same as target position!');
        return;
      }
      
      // เพิ่มการแสดงสถานะกำลังเคลื่อนที่
      const statusDiv = document.getElementById('statusDiv');
      statusDiv.className = 'status status-running';
      
      fetch('/start')
        .then(response => response.text())
        .then(() => {
          // อัพเดทสถานะหลังจากเคลื่อนที่เสร็จ
          setTimeout(() => {
            fetch('/status')
              .then(response => response.json())
              .then(data => {
                document.getElementById('currentHour').textContent = data.currentHour;
                statusDiv.className = 'status status-stopped';
              });
          }, 1000);
        })
        .catch(error => console.error('Error:', error));
    }

    function resetMotor() {
        if (confirm('Are you sure you want to reset? Motor will move to position 12.')) {
            // อัพเดท UI ทันที
            document.getElementById('targetHour').textContent = '12';
            const statusDiv = document.getElementById('statusDiv');
            statusDiv.className = 'status status-running';
            
            fetch('/reset')
                .then(response => response.text())
                .then(() => {
                    // อัพเดทสถานะหลังจากเคลื่อนที่เสร็จ
                    setTimeout(() => {
                        fetch('/status')
                            .then(response => response.json())
                            .then(data => {
                                document.getElementById('currentHour').textContent = data.currentHour;
                                statusDiv.className = 'status status-stopped';
                            });
                    }, 1000);
                })
                .catch(error => console.error('Error:', error));
        }
    }

    // อัพเดทสถานะทุก 1 วินาที
    setInterval(() => {
      fetch('/status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('currentHour').textContent = data.currentHour;
          document.getElementById('targetHour').textContent = data.targetHour;
          
          const statusDiv = document.getElementById('statusDiv');
          statusDiv.className = data.running ? 'status status-running' : 'status status-stopped';
          
          // อัพเดทปุ่มที่เป็นตำแหน่งปัจจุบัน
          document.querySelectorAll('.hour-button').forEach(button => {
            button.classList.toggle('active', 
              parseInt(button.textContent) === data.currentHour);
          });
        })
        .catch(error => console.error('Error:', error));
    }, 1000);

    let currentDirection = 'auto';

    function setDirection(direction) {
        currentDirection = direction;
        
        // อัพเดทสถานะปุ่ม
        document.getElementById('autoButton').classList.toggle('active', direction === 'auto');
        document.getElementById('cwButton').classList.toggle('active', direction === 'cw');
        document.getElementById('ccwButton').classList.toggle('active', direction === 'ccw');

        // ส่งคำสั่งไปยังบอร์ด
        fetch('/direction?mode=' + direction)
            .then(response => response.text())
            .catch(error => console.error('Error:', error));
    }

    // เริ่มต้นให้ Auto เป็นค่าเริ่มต้น
    setDirection('auto');
  </script>
</body>
</html>
)rawliteral";

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
  motor.setSpeed(10);

  // เริ่มต้น EEPROM
  EEPROM.begin(512);
  
  // เริ่มต้นมอเตอร์และ home ไปที่ตำแหน่ง 0 องศา
  motor.setup();

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
  server.on("/moveToHour", handleMoveToHour);
  server.on("/status", handleStatus);
  server.on("/start", handleStart);
  server.on("/reset", handleReset);
  server.on("/direction", handleDirection);
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not found");
  });

  // เริ่มต้น server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
    server.handleClient();
    motor.update();
}

// ส่งหน้าเว็บหลัก
void handleRoot() {
  server.send(200, "text/html", index_html);
}

// จัดการคำขอปรับความเร็ว
void handleSpeed() {
    if (server.hasArg("value")) {
        int speed = server.arg("value").toInt();
        // จำกัดความเร็วให้เป็นแค่ 5 หรือ 12 RPM เท่านั้น
        if (speed != 5 && speed != 12) {
            speed = 5;  // ถ้าค่าไม่ถูกต้อง ให้ใช้ความเร็วต่ำ
        }
        motor.setSpeed(speed);
        Serial.print("Motor speed set to: ");
        Serial.print(speed);
        Serial.println(" RPM");
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad request");
    }
}

// เพิ่ม handler สำหรับการเคลื่อนที่ไปยังตำแหน่งที่ต้องการ
void handleMoveToHour() {
    if (server.hasArg("hour")) {
        int hour = server.arg("hour").toInt();
        motor.moveToHour(hour);
        Serial.print("Moving to hour position: ");
        Serial.println(hour);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad request");
    }
}

// อัพเดท handleStatus
void handleStatus() {
    String json = "{";
    json += "\"speed\":" + String(motor.getSpeed()) + ",";
    json += "\"currentHour\":" + String(motor.getCurrentHour()) + ",";
    json += "\"targetHour\":" + String(motor.getTargetHour()) + ",";
    json += "\"running\":" + String(motor.getRunningStatus());
    json += "}";
    server.send(200, "application/json", json);
}

// เพิ่ม handler สำหรับ start/reset
void handleStart() {
    motor.moveToTarget();  // เปลี่ยนจาก start() เป็น moveToTarget()
    Serial.println("Moving to target position...");
    server.send(200, "text/plain", "Moving");
}

// เพิ่ม handler สำหรับ reset
void handleReset() {
    motor.resetToHome();
    Serial.println("Resetting motor to 0 degrees (position 12)...");
    server.send(200, "text/plain", "Resetting");
}

// เพิ่ม handler สำหรับการตั้งค่าทิศทาง
void handleDirection() {
    if (server.hasArg("mode")) {
        String mode = server.arg("mode");
        if (mode == "auto") {
            motor.setRotationDirection(false, false);
            Serial.println("Set direction: Auto");
        } else if (mode == "cw") {
            motor.setRotationDirection(true, false);
            Serial.println("Set direction: Clockwise");
        } else if (mode == "ccw") {
            motor.setRotationDirection(false, true);
            Serial.println("Set direction: Counter-Clockwise");
        }
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Bad request");
    }
}
