#ifndef BYJ48_H
#define BYJ48_H

#include <Stepper.h>
#include <EEPROM.h>

class StepperMotor {
private:
    // จำนวนสเต็ปต่อรอบของ 28BYJ-48 กับ ULN2003
    static const int STEPS_PER_REVOLUTION = 2048; // 32 สเต็ปต่อรอบ x 64:1 เกียร์ทด
    static const int STEPS_PER_HOUR = STEPS_PER_REVOLUTION / 12; // จำนวนสเต็ปต่อ 1 ชั่วโมง
    static const int STEPS_PER_MOVE = 1;  // ลดจำนวนสเต็ปต่อครั้งลงเหลือ 1
    static const unsigned long STEP_DELAY = 2;  // เพิ่มดีเลย์ระหว่างสเต็ป (มิลลิวินาที)
    
    Stepper stepper;
    int motorSpeed;        // ความเร็วมอเตอร์ปัจจุบัน (RPM)
    int currentHour;      // ตำแหน่งปัจจุบัน (1-12)
    int targetHour;       // ตำแหน่งที่ต้องการไป (1-12)
    int motorRunning;     // สถานะการทำงาน (0 = หยุด, 1 = กำลังทำงาน)
    unsigned long previousMillis;
    bool continuousMode;  // โหมดหมุนต่อเนื่อง
    bool forceClockwise;  // บังคับให้หมุนตามเข็มนาฬิกา
    bool forceCounterClockwise;  // บังคับให้หมุนทวนเข็มนาฬิกา
    static const int LOW_SPEED = 5;   // ความเร็วต่ำ 5 RPM
    static const int HIGH_SPEED = 12; // ความเร็วสูง 12 RPM
    static const int HOME_POSITION = 12;  // ตำแหน่ง 0 องศา (12 นาฬิกา)
    int isHomed;  // สถานะการ home (0 = ยังไม่ home, 1 = home แล้ว)

    void savePosition() {
        if (EEPROM.read(0) != currentHour) {
            EEPROM.write(0, currentHour);
            EEPROM.commit();
            Serial.print("Saved position to EEPROM: ");
            Serial.println(currentHour);
        }
    }

    // เพิ่มฟังก์ชันแปลงตำแหน่งนาฬิกาเป็นองศา
    int hourToDegrees(int hour) {
        // 12 นาฬิกา = 0 องศา, 3 นาฬิกา = 90 องศา, 6 นาฬิกา = 180 องศา, 9 นาฬิกา = 270 องศา
        return ((hour % 12) * 30 + 270) % 360;  // +270 เพื่อให้ 12 นาฬิกาเป็น 0 องศา
    }

public:
    StepperMotor(int in1, int in2, int in3, int in4) 
        : stepper(STEPS_PER_REVOLUTION, in1, in3, in2, in4),
          motorSpeed(LOW_SPEED),  // เริ่มต้นที่ความเร็วต่ำ
          currentHour(HOME_POSITION),
          targetHour(HOME_POSITION),
          motorRunning(0),
          previousMillis(0),
          continuousMode(false),
          forceClockwise(false),
          forceCounterClockwise(false),
          isHomed(0) {
        stepper.setSpeed(motorSpeed);
    }

    void setSpeed(int speed) {
        // จำกัดความเร็วให้เป็นแค่ LOW_SPEED หรือ HIGH_SPEED เท่านั้น
        motorSpeed = (speed == HIGH_SPEED) ? HIGH_SPEED : LOW_SPEED;
        stepper.setSpeed(motorSpeed);
    }

    void setRotationDirection(bool clockwise, bool counterClockwise) {
        forceClockwise = clockwise;
        forceCounterClockwise = counterClockwise;
    }

    void moveToHour(int hour) {
        if (hour < 1 || hour > 12) return;
        targetHour = hour;
        // ไม่เริ่มหมุนทันที รอให้กดปุ่ม start
    }

    void moveToTarget() {
        if (currentHour != targetHour) {
            motorRunning = 1;
            continuousMode = false;
            
            int clockwise_steps = (targetHour - currentHour + 12) % 12;
            int counter_clockwise_steps = (currentHour - targetHour + 12) % 12;
            
            int totalSteps;
            int direction;
            
            // แสดงค่าองศาเริ่มต้นและเป้าหมาย
            Serial.print("Moving from ");
            Serial.print(hourToDegrees(currentHour));
            Serial.print("° (Hour ");
            Serial.print(currentHour);
            Serial.print(") to ");
            Serial.print(hourToDegrees(targetHour));
            Serial.print("° (Hour ");
            Serial.print(targetHour);
            Serial.println(")");
            
            if (forceClockwise || (!forceCounterClockwise && clockwise_steps <= counter_clockwise_steps)) {
                totalSteps = clockwise_steps * STEPS_PER_HOUR;
                direction = 1;
                Serial.println("Direction: Clockwise");
            } else {
                totalSteps = counter_clockwise_steps * STEPS_PER_HOUR;
                direction = -1;
                Serial.println("Direction: Counter-Clockwise");
            }
            
            // เคลื่อนที่ทีละสเต็ปพร้อมดีเลย์
            for (int steps = 0; steps < totalSteps; steps++) {
                stepper.step(direction);
                delay(STEP_DELAY);
                yield();
                
                // อัพเดทและแสดงตำแหน่งทุกชั่วโมง
                if ((steps + 1) % STEPS_PER_HOUR == 0) {
                    if (direction == 1) {
                        currentHour = (currentHour % 12) + 1;
                    } else {
                        currentHour = ((currentHour - 2 + 12) % 12) + 1;
                    }
                    Serial.print("Current position: ");
                    Serial.print(hourToDegrees(currentHour));
                    Serial.print("° (Hour ");
                    Serial.print(currentHour);
                    Serial.println(")");
                }
            }
            
            currentHour = targetHour;
            motorRunning = 0;
            savePosition();
            
            // แสดงสถานะเมื่อเสร็จสิ้น
            Serial.print("Movement complete. Final position: ");
            Serial.print(hourToDegrees(currentHour));
            Serial.print("° (Hour ");
            Serial.print(currentHour);
            Serial.println(")");
        }
    }

    void start() {
        if (currentHour != targetHour) {  // เริ่มหมุนเฉพาะเมื่อตำแหน่งปัจจุบันไม่ใช่เป้าหมาย
            motorRunning = 1;
            continuousMode = false;  // ปิดโหมดหมุนต่อเนื่อง
            previousMillis = millis();
        }
    }

    void stop() {
        motorRunning = 0;
        continuousMode = false;
    }

    bool isRunning() {
        return motorRunning == 1;
    }

    void update() {
        if (motorRunning) {
            if (!continuousMode && currentHour == targetHour) {
                motorRunning = 0;
                savePosition();
                return;
            }

            unsigned long currentMillis = millis();
            // เพิ่มดีเลย์ในการอัพเดท
            if (currentMillis - previousMillis >= STEP_DELAY) {
                previousMillis = currentMillis;
                
                if (continuousMode) {
                    stepper.step(1);
                    delay(STEP_DELAY);
                    currentHour = (currentHour % 12) + 1;
                    savePosition();
                }
                yield();
            }
        }
    }

    void setCurrentPosition(int hour) {
        if (hour >= 1 && hour <= 12) {
            currentHour = hour;
            targetHour = hour;
            isHomed = (hour == HOME_POSITION) ? 1 : 0;  // ตั้งค่าสถานะ home
        }
    }

    // สำหรับส่งค่าสถานะกลับไป
    int getSpeed() { return motorSpeed; }
    int getCurrentHour() { return currentHour; }
    int getTargetHour() { return targetHour; }
    int getRunningStatus() { return motorRunning; }

    void resetToHome() {
        Serial.println("Resetting to home position (0°)...");
        
        targetHour = HOME_POSITION;
        int startHour = currentHour;
        
        Serial.print("Current position before reset: ");
        Serial.print(hourToDegrees(startHour));
        Serial.print("° (Hour ");
        Serial.print(startHour);
        Serial.println(")");
        
        forceClockwise = true;
        forceCounterClockwise = false;
        
        moveToTarget();
        
        if (currentHour == HOME_POSITION) {
            isHomed = 1;
            EEPROM.write(0, HOME_POSITION);
            EEPROM.commit();
            Serial.println("Home position (0°) reached and saved to EEPROM");
        }
        
        forceClockwise = false;
        forceCounterClockwise = false;
    }

    void setup() {
        // เรียกใช้เมื่อเริ่มต้นระบบ
        resetToHome();
    }

    // เพิ่มฟังก์ชันเช็คสถานะ home
    bool isHomeDone() {
        return isHomed == 1;
    }
};

#endif 