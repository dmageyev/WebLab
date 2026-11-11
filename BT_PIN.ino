#include "BluetoothSerial.h"
#include <ESP32Servo.h>

BluetoothSerial SerialBT;
Servo myServo;

// --- НАЛАШТУВАННЯ ПІНІВ ТА ПАРАМЕТРІВ ---
const int servoPin = 13;
const int touchPin = 4;        // Використовуємо T0 (GPIO4) як тач-сенсор
const int TOUCH_THRESHOLD = 30; // Поріг спрацьовування тач (налаштуйте під свій датчик)

const String CORRECT_PIN = "1234"; // Залишається можливість відкрити по ПІН-коду

// --- НАЛАШТУВАННЯ ЗАТВОРУ (ТАЙМЕР ЗАМІСТЬ DELAY) ---
unsigned long lockOpenedTime = 0;
const unsigned long lockDelay = 5000; // Час відчиненого замка (5 секунд)
bool isUnlocked = false;

String receivedData = "";

// --- АВТОРЫЗАЦІЯ ЗА MAC-АДРЕСОЮ ---
// ⚠️ ЗАМІНІТЬ ЦІ БАЙТИ НА MAC-АДРЕСУ ВАШОГО ТЕЛЕФОНУ, ЯКУ ВИ ПОБАЧИЛИ В КОНСОЛІ
const uint8_t ALLOWED_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; 

uint8_t currentClientMac[6] = {0}; // Сюди записується адреса поточного підключеного пристрою
bool isAuthorizedClient = false;   // Прапорець: чи є підключений клієнт «своїм»

// --- ФУНКЦІЯ ЗВОРОТНОГО ВИКЛИКУ (CALLBACK) ДЛЯ BLUETOOTH ---
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
    // Подія: Клієнт успішно підключився до нашого ESP32
    if (event == ESP_SPP_SRV_OPEN_EVT) {
        Serial.println("\n[BT] Клієнт приєднався!");
        Serial.print("[BT] MAC-адреса клієнта: ");
        
        // Зберігаємо MAC-адресу підключеного пристрою
        for (int i = 0; i < 6; i++) {
            currentClientMac[i] = param->srv_open.rem_bda[i];
            
            // Виведення адреси в консоль
            if (currentClientMac[i] < 16) Serial.print("0");
            Serial.print(currentClientMac[i], HEX);
            if (i < 5) Serial.print(":");
        }
        Serial.println();

        // ПЕРЕВІРКА: чи збігається адреса підключеного пристрою з дозволеною?
        isAuthorizedClient = true; // Припускаємо, що збігається, і перевіряємо байт за байтом
        for (int i = 0; i < 6; i++) {
            if (currentClientMac[i] != ALLOWED_MAC[i]) {
                isAuthorizedClient = false; // Знайшли розбіжність — це чужий пристрій
                break;
            }
        }

        if (isAuthorizedClient) {
            Serial.println("[БЕЗПЕКА] ДОСТУП ДО ТАЧ-СЕНСОРУ НАДАНО (Свій телефон)");
        } else {
            Serial.println("[БЕЗПЕКА] УВАГА! Підключено неавторизований пристрій. Тач-сенсор заблоковано.");
        }
        Serial.println("-------------------------------------");
    }
    
    // Подія: Клієнт відключився
    if (event == ESP_SPP_CLOSE_EVT) {
        Serial.println("\n[BT] Клієнт відключився.");
        isAuthorizedClient = false; // Скидаємо авторизацію
        memset(currentClientMac, 0, 6); // Очищаємо збережену адресу
        Serial.println("-------------------------------------");
    }
}

void openLock(String reason) {
    Serial.println("Замок ВІДКРИТО (" + reason + ")");
    if (SerialBT.hasClient()) {
        SerialBT.println("Access Granted: Unlocked");
    }
    myServo.write(90);
    lockOpenedTime = millis();
    isUnlocked = true;
}

void closeLock() {
    Serial.println("Закриваю замок...");
    if (SerialBT.hasClient()) {
        SerialBT.println("Locked");
    }
    myServo.write(0);
    isUnlocked = false;
}

void setup() {
    Serial.begin(115200);
    myServo.attach(servoPin);
    myServo.write(0); // Закритий стан за замовчуванням

    // Реєструємо callback-функцію ПЕРЕД запуском Bluetooth
    SerialBT.register_callback(btCallback);

    SerialBT.begin("ESP32_Classic_Lock");
    Serial.println("Bluetooth замок готовий. З'єднайтеся з телефоном!");
}

void loop() {
    // 1. ПЕРЕВІРКА ТАЧ-СЕНСОРУ (Працює тільки якщо підключено АВТОРИЗОВАНИЙ пристрій)
    if (SerialBT.hasClient() && isAuthorizedClient) { 
        int touchValue = touchRead(touchPin);
        
        if (touchValue < TOUCH_THRESHOLD && !isUnlocked) {
            Serial.print("Тач активовано (значення: ");
            Serial.print(touchValue);
            Serial.println("). Власник поруч.");
            
            openLock("Дотик від авторизованого MAC");
        }
    }

    // 2. ПЕРЕВІРКА ДАНИХ З BLUETOOTH (Резервний ПІН-код — працює для будь-яких підключень)
    if (SerialBT.available()) {
        receivedData = SerialBT.readStringUntil('\n');
        receivedData.trim();
        Serial.print("Отримано ПІН: ");
        Serial.println(receivedData);

        if (receivedData == CORRECT_PIN) {
            if (!isUnlocked) openLock("Вірний ПІН-код");
        } else {
            Serial.println("Невірний ПІН!");
            SerialBT.println("Access Denied: Wrong PIN");
        }
    }

    // 3. АВТОМАТИЧНЕ ЗАКРИТТЯ ЗА ТАЙМЕРОМ
    if (isUnlocked && (millis() - lockOpenedTime >= lockDelay)) {
        closeLock();
    }

    delay(10);
}
