#include <Bluepad32.h>
#include <ESP32Servo.h>

const int SERVO_PIN = 13;
Servo myServo;

GamepadPtr myGamepads[BP32_MAX_GAMEPADS];

void onConnectedGamepad(GamepadPtr gp) {
    Serial.println("\n--- Gamepad connected ---");
    GamepadProperties properties = gp->getProperties();
    Serial.printf("Model Name : %s\n", gp->getModelName().c_str());
    Serial.printf("Vendor ID  : 0x%04x\n", properties.vendor_id);
    Serial.printf("Product ID : 0x%04x\n", properties.product_id);

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myGamepads[i] == nullptr) {
            myGamepads[i] = gp;
            break;
        }
    }
}

void onDisconnectedGamepad(GamepadPtr gp) {
    Serial.println("\n--- Gamepad Disabled ---");
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myGamepads[i] == gp) {
            myGamepads[i] = nullptr;
            myServo.write(90);
            break;
        }
    }
}

void setup() {
    Serial.begin(9600);
    delay(500);

    // Serial.printf("Bluepad32 Firmware: %s\n", BP32.firmwareVersion());
    
    myServo.setPeriodHertz(50);
    myServo.attach(SERVO_PIN, 500, 2400);
    myServo.write(90); // Стоп для 360° сервопривода

    BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);
}

void loop() {
    BP32.update();

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        GamepadPtr myGamepad = myGamepads[i];

        if (myGamepad && myGamepad->isConnected()) {
            // Зчитування всіх доступних даних від геймпада
            uint16_t buttons = myGamepad->buttons();
            uint8_t dpad = myGamepad->dpad();
            uint8_t misc = myGamepad->miscButtons();

            int16_t axisX = myGamepad->axisX();
            int16_t axisY = myGamepad->axisY();
            int16_t axisRX = myGamepad->axisRX();
            int16_t axisRY = myGamepad->axisRY();
            
            // Задаємо поріг чутливості (відсікаємо дребезг до 15)
            int threshold = 15;

            // Аналогові триггери LT и RT
            int32_t brake = myGamepad->brake();       // LT
            int32_t throttle = myGamepad->throttle(); // RT

            // Перевіряємо загальну активність
            bool isPressed = (buttons != 0) || (dpad != 0) || (misc != 0) || 
                             (brake > 0) || (throttle > 0) || 
                             (abs(axisX) > 30) || (abs(axisY) > 30) || 
                             (abs(axisRX) > 30) || (abs(axisRY) > 30);

            if (isPressed) {
                // Виводимо лише те, що реально натиснуто на даній ітерації
                Serial.print("Aktiv: ");
                
                // 1. Звичачйні цифрові кнопки
                if (buttons != 0) {
                    Serial.printf("[BtnHex: 0x%04X] ", buttons);
                }
                
                // 2. Крестовина (D-pad)
                if (dpad != 0) {
                    Serial.printf("[Dpad: 0x%02X] ", dpad);
                }

                // 3. Аналогові триггеры LT (brake) та RT (throttle) із даними про силу нажжиму
                if (brake > 0) {
                    Serial.printf("[LT/Brake - Code/Power: %d] ", brake);
                }
                if (throttle > 0) {
                    Serial.printf("[RT/Throttle - Code/Power: %d] ", throttle);
                }

                // 4. Стіки ("джойстики") по осях X, Y, RX, RY
                if (abs(axisX) > 30 || abs(axisY) > 30 || abs(axisRX) > 30 || abs(axisRY) > 30) {
                    Serial.printf("[Sticks -> X:%d Y:%d | RX:%d RY:%d]", axisX, axisY, axisRX, axisRY);
                }

                // 5. Системні кнопки
                if (misc != 0) {
                    Serial.printf("[Misc: 0x%02X] ", misc);
                }
                
                Serial.println();

                // Збільшуємо поріг відсічки для триггерів (наприклад, до 15), 
                // для того щоб під час відпускання кнопка раніше переходила в нуль
                int triggerDeadzone = 15;

                // Логіка управління сервоприводом
                if (throttle > triggerDeadzone) {
                    // Плавний маппінг від 90 до 180 - вправо по часовій
                    int servoSpeed = map(throttle, triggerDeadzone, 1023, 90, 180);
                    myServo.write(servoSpeed);
                } 
                else if (brake > triggerDeadzone) {
                    // Плавний маппінг от 90 до 0 - вліво проти часової
                    int servoSpeed = map(brake, triggerDeadzone, 1023, 90, 0);
                    myServo.write(servoSpeed);
                } 
                else {
                    // Коли не натиснуто ані LT, ані RT - повна зупинка
                    // Коли не натиснуто ані LT, ані RT - даємо подвійний імпульс стопу для надійності
                    myServo.write(90);
                    vTaskDelay(pdMS_TO_TICKS(10));
                    myServo.write(90);
                }
            } else {
                // У спокої, коли жодна кнопка не натиснута — повна зупинка
                myServo.write(90);
            }
        }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
}