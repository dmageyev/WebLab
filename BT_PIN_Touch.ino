#include "BluetoothSerial.h"
#include <ESP32Servo.h>
#include <Preferences.h>
#include "esp_spp_api.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled!
#endif

BluetoothSerial SerialBT;
Servo myServo;
Preferences preferences;

// Налаштування пінів
const int servoPin = 13;
const int touchPin = 4; // GPIO 4 (Touch0)

// Налаштування безпеки та сенсора
const String CORRECT_PIN = "1234";
const int TOUCH_THRESHOLD = 40; // Чутливість дотику (чим менше значення при торканні, тим сильніший контакт)

String currentClientMAC = "";   // MAC-адреса поточного підключеного пристрою
String authorizedMAC = "";      // Збережена MAC-адреса власника
bool isClientConnected = false;

// Функція зворотного виклику для відстеження підключення та отримання MAC-адреси
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    isClientConnected = true;
    char macStr[18];
    // Форматуємо байти MAC-адреси у звичний рядок XX:XX:XX:XX:XX:XX
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            param->srv_open.rem_bda[0], param->srv_open.rem_bda[1],
            param->srv_open.rem_bda[2], param->srv_open.rem_bda[3],
            param->srv_open.rem_bda[4], param->srv_open.rem_bda[5]);
    currentClientMAC = String(macStr);
    
    Serial.print("Підключився клієнт з MAC: ");
    Serial.println(currentClientMAC);
  } 
  else if (event == ESP_SPP_CLOSE_EVT) {
    isClientConnected = false;
    currentClientMAC = "";
    Serial.println("Клієнт відключився.");
  }
}

void setup() {
  Serial.begin(115200);

  // Ініціалізація сервоприводу
  myServo.attach(servoPin);
  myServo.write(0); // Закрито

  // Завантажуємо збережений MAC із пам'яті ESP32
  preferences.begin("lock-auth", false);
  authorizedMAC = preferences.getString("saved_mac", "");
  if (authorizedMAC != "") {
    Serial.print("Завантажено довірений MAC з пам'яті: ");
    Serial.println(authorizedMAC);
  } else {
    Serial.println("Довірений MAC ще не задано. Введіть ПІН через Bluetooth.");
  }

  // Реєструємо обробник подій Bluetooth та запускаємо його
  SerialBT.register_callback(btCallback);
  SerialBT.begin("ESP32_Touch_Lock");
  Serial.println("Система готова до роботи.");
}

void loop() {
  // 1. Обробка команд через Bluetooth (реєстрація MAC за ПІН-кодом)
  if (SerialBT.available()) {
    String receivedData = SerialBT.readStringUntil('\n');
    receivedData.trim();

    if (receivedData == CORRECT_PIN) {
      if (currentClientMAC != "") {
        authorizedMAC = currentClientMAC;
        // Зберігаємо MAC в енергонезалежну пам'ять
        preferences.putString("saved_mac", authorizedMAC);
        
        Serial.print("Успіх! Новий довірений MAC збережено: ");
        Serial.println(authorizedMAC);
        SerialBT.println("PIN OK! Your device MAC (" + authorizedMAC + ") is now authorized. Touch sensor to unlock.");
      }
    } else {
      Serial.println("Введено невірний ПІН.");
      SerialBT.println("Access Denied: Wrong PIN.");
    }
  }

  // 2. Перевірка дотику до сенсорного піна
  int touchValue = touchRead(touchPin);
  
  // Якщо зафіксовано дотик (значення падає нижче порогу)
  if (touchValue < TOUCH_THRESHOLD) {
    // Невелика затримка для захисту від випадкових завад (антибрязкіт)
    delay(50);
    if (touchRead(touchPin) < TOUCH_THRESHOLD) {
      Serial.print("Зафіксовано дотик! Поточний MAC: ");
      Serial.println(currentClientMAC);

      // Перевіряємо: чи підключений телефон, чи є збережений MAC і чи збігаються вони
      if (isClientConnected && authorizedMAC != "" && currentClientMAC == authorizedMAC) {
        Serial.println("Доступ дозволено! Відкриваю замок...");
        SerialBT.println("Touch detected: Unlocking...");

        myServo.write(90); // Відкрити
        delay(5000);       // Чекати 5 секунд
        myServo.write(0);  // Закрити

        Serial.println("Замок закрито.");
        SerialBT.println("Locked.");
      } else {
        Serial.println("Відмова! Пристрій не підключено або невідомий MAC.");
        if (isClientConnected) {
          SerialBT.println("Access Denied: Unauthorized device. Send PIN first.");
        }
      }
      
      // Чекаємо, поки палець приберуть із сенсора, щоб не спрацьовувало по колу
      while (touchRead(touchPin) < TOUCH_THRESHOLD) {
        delay(50);
      }
    }
  }

  delay(20);
}
