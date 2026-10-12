#include <WiFi.h>
// === ЗМІНА: Додано бібліотеку та конфігурацію DHT11 ===
#include <DHT.h>

#define DHTPIN 4        // Пін, до якого підключено DHT11 #include
#define DHTTYPE DHT11   // Тип датчика
DHT dht(DHTPIN, DHTTYPE);
// === КІНЕЦЬ ЗМІН ===

const char* ssid = "Dmytro";
const char* password = "12345678";

WiFiServer server(80);

// Змінна для збереження поточного стану світлодіода (LOW або HIGH)
int ledState = LOW; 

void setup() {
  Serial.begin(115200);
  
  // === ЗМІНА: Запуск датчика DHT11 ===
  dht.begin();
  // === КІНЕЦЬ ЗМІН ===
  
  // Налаштування 2 піна як виходу та його вимкнення за замовчуванням
  pinMode(2, OUTPUT);
  digitalWrite(2, ledState);
  
  Serial.println("\n[CYBERNET] Setting up Access Point...");
  WiFi.softAP(ssid, password);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP Wi-Fi Name (SSID): ");
  Serial.println(ssid);
  Serial.print("Web Server active at IP Address: ");
  Serial.println(IP);
  
  server.begin();
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.println("У нас гості [CONNECTION ESTABLISHED]");
    String currLine = "";
    String request = ""; // Змінна для збереження першого рядка запиту (де міститься URL)
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        
        if (c == '\n') {
          if (currLine.length() == 0) {
            
            // Перевіряємо, який URL прийшов від користувача
            if (request.indexOf("GET /H") >= 0) {
              ledState = HIGH;
              digitalWrite(2, ledState); // Вмикаємо LED
            } else if (request.indexOf("GET /L") >= 0) {
              ledState = LOW;
              digitalWrite(2, ledState); // Вимикаємо LED
            }

            // === ЗМІНА: Зчитування температури та вологості з DHT11 ===
            float h = dht.readHumidity();
            float t = dht.readTemperature();
            // === КІНЕЦЬ ЗМІН ===

            // Відправляємо HTTP-заголовок
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html; charset=utf-8");
            client.println("Connection: close");
            client.println();
            
            // Генеруємо HTML-сторінку з кнопками та стилями
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            
            // === ЗМІНА: Оновлені стилі CSS у стилі Кіберпанк ===
            client.println("<title>ESP32 // CYBER_DECK</title>");
            client.println("<style>");
            client.println("body { background-color: #0b0b0e; color: #00ffcc; font-family: 'Courier New', Courier, monospace; text-align: center; margin: 0; padding: 20px; }");
            client.println("h1 { color: #ff0055; text-shadow: 0 0 10px #ff0055; font-size: 24px; letter-spacing: 2px; }");
            client.println(".card { background: #12131c; border: 1px solid #00ffcc; box-shadow: 0 0 15px rgba(0,255,204,0.2); max-width: 400px; margin: 20px auto; padding: 20px; border-radius: 4px; }");
            client.println(".sensor-data { font-size: 20px; color: #ffae00; text-shadow: 0 0 5px #ffae00; margin: 15px 0; }");
            client.println(".button { display: inline-block; background-color: transparent; border: 2px solid #00ffcc; color: #00ffcc; padding: 12px 30px; text-decoration: none; font-size: 18px; font-weight: bold; margin: 10px 0; cursor: pointer; text-transform: uppercase; letter-spacing: 1px; box-shadow: 0 0 10px rgba(0,255,204,0.4); transition: 0.2s; }");
            client.println(".button:hover { background-color: #00ffcc; color: #0b0b0e; box-shadow: 0 0 20px #00ffcc; }");
            client.println(".button-off { border-color: #ff0055; color: #ff0055; box-shadow: 0 0 10px rgba(255,0,85,0.4); }");
            client.println(".button-off:hover { background-color: #ff0055; color: #0b0b0e; box-shadow: 0 0 20px #ff0055; }");
            client.println(".status { font-size: 16px; color: #ffffff; margin-bottom: 15px; }");
            client.println("</style></head>");
            // === КІНЕЦЬ ЗМІН ===
            
            // === ЗМІНА: Оновлений кіберпанк інтерфейс та виведення датчика DHT11 ===
            client.println("<body>");
            client.println("<h1>// ESP32 SYSTEM: ДМИТРО</h1>");
            
            // Картка телеметрії DHT11
            client.println("<div class=\"card\">");
            client.println("<h3>[ ТЕЛЕМЕТРІЯ СЕРЕДОВИЩА ]</h3>");
            if (isnan(h) || isnan(t)) {
                client.println("<p class=\"sensor-data\" style=\"color: #ff0055;\">ПОМИЛКА ЗЧИТУВАННЯ DHT11</p>");
            } else {
                client.print("<p class=\"sensor-data\">ТЕМПЕРАТУРА: ");
                client.print(t);
                client.println(" &deg;C</p>");
                client.print("<p class=\"sensor-data\">ВОЛОГІСТЬ: ");
                client.print(h);
                client.println(" %</p>");
            }
            client.println("</div>");
            
            // Картка керування
            client.println("<div class=\"card\">");
            client.println("<h3>[ КЕРУВАННЯ ВУЗЛОМ ]</h3>");
            client.print("<p class=\"status\">LED (Пін 2): <strong style=\"color: ");
            client.print(ledState == HIGH ? "#00ffcc" : "#ff0055");
            client.print(";\">");
            client.print(ledState == HIGH ? "АКТИВОВАНО" : "ВИМКНЕНО");
            client.println("</strong></p>");
            
            // Кнопки
            if (ledState == LOW) {
              client.println("<p><a href=\"/H\"><button class=\"button\">УВІМКНУТИ</button></a></p>");
            } else {
              client.println("<p><a href=\"/L\"><button class=\"button button-off\">ВИМКНУТИ</button></a></p>");
            }
            client.println("</div>");
            
            client.println("</body></html>");
            // === КІНЕЦЬ ЗМІН ===

            break;
          } else {
            // Якщо це перший рядок запиту (наприклад, GET /H HTTP/1.1), зберігаємо його
            if (request == "") {
              request = currLine;
            }
            currLine = "";
          }
        } else if (c != '\r') {
          currLine += c;
        }
      }
    }
    client.stop();
    Serial.println("Гості пішли [DISCONNECTED]");
  }
}
