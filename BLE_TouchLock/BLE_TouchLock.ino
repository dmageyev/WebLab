/*
 * Сенсорний замок з Bluetooth (ESP32 + BLE + серво)
 *
 * Бібліотеки: ESP32Servo (Менеджер бібліотек), BLE та Preferences (входять до ядра ESP32).
 */
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <Preferences.h>
#include <ESP32Servo.h>

// ---------- Налаштування ----------
const int TOUCH_PIN        = 4;      // T0 (GPIO4) - сенсорна кнопка
const int TOUCH_THRESHOLD  = 40;     // touchRead() < порогу => дотик (ESP32 classic)
const int SERVO_PIN        = 18;
const int LED_PIN          = 2;      // вбудований світлодіод
const int SERVO_OPEN_ANGLE   = 90;
const int SERVO_CLOSED_ANGLE = 0;
const uint32_t HOLD_CLOSE_MS = 2000; // утримання для закриття
const uint32_t SCAN_SECONDS  = 3;    // тривалість сканування
const int MAX_DEVICES        = 5;    // макс. кількість пристроїв у пам'яті
const char* BLE_NAME         = "TouchLock";

Preferences prefs;
Servo lockServo;
String knownDevices[MAX_DEVICES];
int knownCount = 0;

// Адреси пристроїв, що підключилися у режимі реєстрації
String pendingDevices[MAX_DEVICES];
volatile int pendingCount = 0;

// ---------- Допоміжні функції ----------
bool isTouched() { return touchRead(TOUCH_PIN) < TOUCH_THRESHOLD; }

void blink(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(onMs);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) delay(offMs);
  }
}

void loadDevices() {
  prefs.begin("lock", true);
  knownCount = min((int)prefs.getUInt("count", 0), MAX_DEVICES);
  for (int i = 0; i < knownCount; i++) {
    knownDevices[i] = prefs.getString(("d" + String(i)).c_str(), "");
  }
  prefs.end();
}

void saveDevices(String* list, int count) {
  prefs.begin("lock", false);
  prefs.putUInt("count", count);
  for (int i = 0; i < count; i++) {
    prefs.putString(("d" + String(i)).c_str(), list[i]);
  }
  prefs.end();
}

bool isKnown(const String& addr) {
  for (int i = 0; i < knownCount; i++)
    if (knownDevices[i].equalsIgnoreCase(addr)) return true;
  return false;
}

// ---------- Режим реєстрації ----------
class EnrollCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server, esp_ble_gatts_cb_param_t* param) {
    char buf[18];
    const uint8_t* a = param->connect.remote_bda;
    snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", a[0], a[1], a[2], a[3], a[4], a[5]);
    String addr(buf);
    bool exists = false;
    for (int i = 0; i < pendingCount; i++) if (pendingDevices[i] == addr) exists = true;
    if (!exists && pendingCount < MAX_DEVICES) {
      pendingDevices[pendingCount] = addr;
      pendingCount = pendingCount + 1;
    }
    BLEDevice::startAdvertising();  // дозволити підключення наступних пристроїв
  }
  void onDisconnect(BLEServer* server) { BLEDevice::startAdvertising(); }
};

void enrollMode() {
  BLEDevice::init(BLE_NAME);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new EnrollCallbacks());
  BLEDevice::getAdvertising()->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("Пам'ять порожня. Підключіть пристрої до BLE \"" + String(BLE_NAME) + "\" і торкніться кнопки.");

  // Чекаємо дотик (і хоча б один підключений пристрій)
  while (true) {
    if (isTouched() && pendingCount > 0) break;
    delay(50);
  }
  BLEDevice::getAdvertising()->stop();
  saveDevices(pendingDevices, pendingCount);
  blink(3, 250, 250);
  Serial.printf("Збережено пристроїв: %d\n", pendingCount);
  delay(500);
  ESP.restart();  // перезапуск у робочий режим
}

// ---------- Робочий режим ----------
bool scanForKnownDevice() {
  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  BLEScanResults* results = scan->start(SCAN_SECONDS, false);
  bool found = false;
  for (int i = 0; i < results->getCount(); i++) {
    String addr = String(results->getDevice(i).getAddress().toString().c_str());
    if (isKnown(addr)) { found = true; break; }
  }
  scan->clearResults();
  return found;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  lockServo.attach(SERVO_PIN);
  lockServo.write(SERVO_CLOSED_ANGLE);

  loadDevices();
  if (knownCount == 0) enrollMode();

  BLEDevice::init("");
  Serial.printf("Пристроїв у пам'яті: %d\n", knownCount);
}

void loop() {
  if (!isTouched()) { delay(20); return; }

  // Визначаємо тривалість дотику
  uint32_t start = millis();
  while (isTouched() && millis() - start < HOLD_CLOSE_MS) delay(10);
  bool closeRequested = (millis() - start >= HOLD_CLOSE_MS);

  bool ok = scanForKnownDevice();
  if (closeRequested) {
    if (ok) {
      blink(2, 250, 250);
      lockServo.write(SERVO_CLOSED_ANGLE);
    } else {
      blink(1, 250, 0);
    }
  } else {
    if (ok) {
      digitalWrite(LED_PIN, HIGH);
      lockServo.write(SERVO_OPEN_ANGLE);
      delay(1000);
      digitalWrite(LED_PIN, LOW);
    } else {
      blink(1, 250, 0);
    }
  }
  while (isTouched()) delay(20);  // чекаємо відпускання
  delay(200);
}
