#include <WiFi.h>

const char* ssid = "Dmytro";
const char* password = "12345678";

WiFiServer server(80);

void setup() {
  Serial.begin(115200);
  Serial.println("\nSetting up Access Point...");
  
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
    Serial.println("У нас гости");
    String currLine = "";
    String request = ""; // Оголошуємо змінну для збереження HTTP-запиту

    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        
        if (c == '\n') {
          if (currLine.length() == 0) {
            // Відправляємо HTTP-заголовок
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html; charset=utf-8");
            client.println("Connection: close");
            client.println();
            
            // Генеруємо HTML-сторінку з кнопками та стилями
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<style>html { font-family: Arial; text-align: center; }");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer; border-radius: 5px; }");
            client.println(".button2 { background-color: #555555; }</style></head>");
            client.println("<body><h1>ESP32 Веб-сервер</h1>");
            
            // Тут ви можете додавати кнопки на сторінку, наприклад:
            // client.println("<p><a href=\"/H\"><button class=\"button\">Увімкнути</button></a></p>");
            
            client.println("</body></html>");
            break;
          } else {
            // Зберігаємо перший рядок HTTP-запиту (наприклад, "GET /H HTTP/1.1")
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
    
    // Тут можна обробляти отриманий запит, який зберігся у змінній request
    // Наприклад: if (request.indexOf("GET /H") >= 0) { // увімкнути LED }

    client.stop();
    Serial.println("Гости ушли");
  }
}
