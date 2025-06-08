#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <EEPROM.h>

// Дефиниране на константи в началото
// MQTT Last Will and Testament (LWT) настройки:  
// - MQTT_WILL_TOPIC: Топикът, в който се публикува статусът на устройството (ONLINE/OFFLINE).  
// - MQTT_WILL_ONLINE/OFFLINE: Съобщенията, изпращани при свързване/прекъсване на MQTT връзката.  
// - MQTT_WILL_RETAIN: Запазва последното съобщение в брокера, за да е винаги достъпно.  
#define RELAY_PIN D1                // Пин на релето
#define EEPROM_SIZE 128
#define MQTT_PORT 1883              //Порта на MQTT
#define MQTT_CONNECT_TIMEOUT 5000  // 5 секунди timeout за връзка
#undef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 60          // MQTT keepalive интервал
#define MQTT_RETRY_INTERVAL 10000  // 10 секунди между опитите за свързване
#define CLIENT_ID "ESP8266Relay"    // Име на релето
#define MQTT_TOPIC_CONTROL "home/relay/control"
#define MQTT_TOPIC_STATUS "home/relay/status"
#define MQTT_TOPIC_ONLINE "home/relay/online"
#define MQTT_WILL_TOPIC "home/relay/online"
#define MQTT_WILL_ONLINE "ONLINE"
#define MQTT_WILL_OFFLINE "OFFLINE"
#define MQTT_WILL_QOS 1
#define MQTT_WILL_RETAIN true
// Създаване на обекти за web сървър, WiFi и MQTT клиент
ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// Допълнителни променливи за състоянието
// Променливи за управление на състоянието на MQTT връзката и релето
unsigned long lastMqttConnectionAttempt = 0;
int mqttConnectionAttempts = 0;
const int MAX_MQTT_ATTEMPTS = 3;

bool relayState = true;
bool mqttConnected = false;

// Данни за mqtt сървъра 
char mqtt_server[40] = "192.168.1.171";
char mqtt_user[32] = "relay_user";
char mqtt_pass[32] = "12345678";

bool shouldSaveConfig = false;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000;

// Функция saveConfigCallback
void saveConfigCallback () {
  shouldSaveConfig = true;
}
// Функция callback
// Получава съобщения от MQTT топика и реагира на тях:
// "on" - включва релето
// "off" - изключва релето
// "restart" - рестартира устройството
// "status" - връща текущото състояние
void callback(char* topic, byte* payload, unsigned int length) {
    if (length == 0) return;
    
    if (String(topic) == MQTT_TOPIC_CONTROL) {
        bool newState = (payload[0] == 'O' && payload[1] == 'N');
        digitalWrite(RELAY_PIN, newState ? HIGH : LOW);
        relayState = newState;
        client.publish(MQTT_TOPIC_STATUS, relayState ? "ON" : "OFF");
    }
}

void handleRoot() {
// Уеб интерфейс за управление на релето:  
// - Използва JavaScript за асинхронни заявки (fetch) към REST API (/relay/on, /relay/off и др.).  
// - R"rawliteral" позволява лесно писане на многоредов HTML без екраниране.  
// - Статусът се обновява на всеки 5 секунди чрез refreshStatus().  
  String html = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0">
<title>Modem Relay Control</title>
<style>
  body { font-family: Arial; text-align: center; background: #f0f0f0; margin: 0; padding: 0; }
  .container { padding: 20px; }
  button { 
    cursor: pointer;
    font-size: 1.2em; 
    padding: 15px; 
    width: 80%; 
    max-width: 300px; 
    margin: 10px; 
    border-radius: 10px; 
    border: none; 
    color: white;
    transition: all 0.2s;
    box-shadow: 0 4px 0 rgba(0,0,0,0.2);
    position: relative;
  }
  button:active, .pressed {
    transform: translateY(4px);
    box-shadow: 0 1px 0 rgba(0,0,0,0.2);
  }
  .on { background-color: #28a745; }
  .off { background-color: #dc3545; }
  .restart { background-color: #094ab3; }
  .status { background-color: #6c757d; }
  .indicator { display: inline-block; width: 20px; height: 20px; border-radius: 50%; margin-left: 10px; vertical-align: middle; }
  .on-indicator { background-color: #28a745; }
  .off-indicator { background-color: #dc3545; }
</style>
<script>
  function sendCommand(cmd) {
    const btn = event.currentTarget;
    btn.classList.add("pressed");
    
    fetch("/relay/" + cmd)
      .then(response => response.text())
      .then(data => {
        updateStatus(data);
        setTimeout(() => btn.classList.remove("pressed"), 200);
      });
  }

  function refreshStatus() {
    fetch("/relay/status")
      .then(response => response.text())
      .then(data => updateStatus(data));
    fetch("/mqtt/status")
      .then(response => response.text())
      .then(data => updateMQTTStatus(data));
  }
  
  function updateStatus(statusText) {
    document.getElementById("state").innerText = statusText;
    document.getElementById("indicator").className = "indicator " +
      (statusText.includes("ON") ? "on-indicator" : "off-indicator");
  }
  
  function updateMQTTStatus(status) {
    document.getElementById("mqtt-status").innerText = status;
    document.getElementById("mqtt-indicator").className = "indicator " +
      (status.includes("CONNECTED") ? "on-indicator" : "off-indicator");
  }
  
  setInterval(refreshStatus, 5000);
</script>
</head>
<body>
<div class="container">
<h1>Modem Relay Control</h1>
<p>Current State: <strong id="state">)rawliteral";

  html += relayState ? "ON (power flowing)" : "OFF (power cut)";
  html += R"rawliteral(</strong><span id="indicator" class="indicator )rawliteral";
  html += relayState ? "on-indicator" : "off-indicator";
  html += R"rawliteral("></span></p>

<p>MQTT Status: <strong id="mqtt-status">)rawliteral";
  html += mqttConnected ? "CONNECTED" : "DISCONNECTED";
  html += R"rawliteral(</strong><span id="mqtt-indicator" class="indicator )rawliteral";
  html += mqttConnected ? "on-indicator" : "off-indicator";
  html += R"rawliteral("></span></p>

<p><strong>WiFi SSID:</strong> )rawliteral";
  html += WiFi.SSID();
  html += R"rawliteral(<br><strong>IP Address:</strong> )rawliteral";
  html += WiFi.localIP().toString();
  html += R"rawliteral(<br><strong>MQTT Server:</strong> )rawliteral";
  html += mqtt_server;
  html += R"rawliteral(<br><strong>User:</strong> )rawliteral";
  html += mqtt_user;

  html += R"rawliteral(</p>
<button class="on" onclick="sendCommand('on')" ontouchstart="this.classList.add('pressed')" ontouchend="this.classList.remove('pressed')">Turn ON</button><br>
<button class="off" onclick="sendCommand('off')" ontouchstart="this.classList.add('pressed')" ontouchend="this.classList.remove('pressed')">Turn OFF</button><br>
<button class="restart" onclick="if(confirm('Are you sure you want to restart?')) restartRelay();">Restart module</button><br>
<button class="status" onclick="if(confirm('Are you sure you want to reset all settings?')) location.href='/reset';">Reset Settings</button>
</div>
<script>
function restartRelay() {
  fetch('/restart').then(response => {
    if (response.ok) {
      alert('Relay successfully restarted!');
    } else {
      alert('Failed to restart relay.');
    }
  });
}
</script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}
// Функция handle On
void handleOn() {
  digitalWrite(RELAY_PIN, HIGH);
  relayState = true;
  client.publish(MQTT_TOPIC_STATUS, "ON"); // Използвайте константата вместо хардкоднат стринг
  server.send(200, "text/plain", "ON (power flowing)");
}

// Функция handle Off
void handleOff() {
  digitalWrite(RELAY_PIN, LOW);
  relayState = false;
  client.publish(MQTT_TOPIC_STATUS, "OFF"); // Използвайте константата вместо хардкоднат стринг
  server.send(200, "text/plain", "OFF (power cut)");
}

// Функция handle Status
// Връща състоянието на релето през уеб интерфейса
// Формат: "ON (power flowing)" или "OFF (power cut)"
void handleStatus() {
  String status = relayState ? "ON (power flowing)" : "OFF (power cut)";
  server.send(200, "text/plain", status);
}

// Функция handle MQTT Status
// Връща текущото състояние на MQTT връзката през уеб интерфейса:
// "CONNECTED" или "DISCONNECTED"
void handleMQTTStatus() {
  server.send(200, "text/plain", mqttConnected ? "CONNECTED" : "DISCONNECTED");
}

// Функция handle Reset
// Функция за пълно нулиране на настройките:  
// - Изтрива запазените данни в EEPROM.  
// - Изтрива WiFi настройките чрез WiFiManager.  
// - Рестартира устройството, за да влезе в режим на конфигурация.  
void handleReset() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  EEPROM.end();

// Функция WiFi Manager
WiFiManager wm;
  wm.resetSettings();

  server.send(200, "text/plain", "Settings erased. Restarting...");
  delay(1000);
  ESP.restart();
}

// Fункция за повторно свързване
// Опит за свързване с MQTT брокера:  
// - Ако не успее след MAX_MQTT_ATTEMPTS опита, устройството се рестартира.  
// - При успех се абонира за MQTT_TOPIC_CONTROL и публикува текущото състояние на релето.  
// - LWT съобщението (ONLINE) се изпраща, за да се сигнализира успешно свързване.  
// - Използва LWT съобщения, за да информира брокера при загуба или възстановяване на връзка
bool connectToMqtt() {
    if (strlen(mqtt_server) == 0 || strlen(mqtt_user) == 0) {
        Serial.println("Invalid MQTT configuration");
        return false;
    }
    Serial.println("Attempting MQTT connection...");
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("MQTT connection failed: No WiFi connection");
        return false;
    }

    // Подобрена LWT конфигурация
    bool connected = client.connect(
        CLIENT_ID,
        mqtt_user,
        mqtt_pass,
        MQTT_WILL_TOPIC,
        MQTT_WILL_QOS,
        MQTT_WILL_RETAIN,
        MQTT_WILL_OFFLINE,
        true
    );

    if (connected) {
        Serial.println("MQTT connected");
        mqttConnectionAttempts = 0;
        // При успех се абонира и изпраща текущ статус на релето и ONLINE съобщение
        client.subscribe(MQTT_TOPIC_CONTROL);
        client.publish(MQTT_TOPIC_STATUS, relayState ? "ON" : "OFF", true);
        // Публикуване на ONLINE статус при успешно свързване
        client.publish(MQTT_WILL_TOPIC, MQTT_WILL_ONLINE, MQTT_WILL_RETAIN);
        
        return true;
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.print(client.state());
        Serial.println(" trying again later");
        
        mqttConnectionAttempts++;
        if (mqttConnectionAttempts >= MAX_MQTT_ATTEMPTS) {
            Serial.println("Max MQTT connection attempts reached, restarting...");
            delay(1000);
            ESP.restart();
        }
        return false;
    }
}

// Функция за управление на връзката
// Управление на MQTT връзката:  
// - Проверява дали е време за нов опит за свързване (MQTT_RETRY_INTERVAL).  
// - Логва промените в състоянието (CONNECTED/DISCONNECTED).  
// - Ако връзката е изгубена, опитва повторно свързване след интервал.  
void manageMqttConnection() {
    static bool lastMqttState = false;
    
    if (!client.connected()) {
        mqttConnected = false;
        
        // Проверка дали е време за нов опит за свързване
        if (millis() - lastMqttConnectionAttempt > MQTT_RETRY_INTERVAL) {
            lastMqttConnectionAttempt = millis();
            if (connectToMqtt()) {
                mqttConnected = true;
            }
            yield();
        }
    } else {
        mqttConnected = true;
    }

    // Логване на промени в състоянието
    if (lastMqttState != mqttConnected) {
        Serial.print("MQTT state changed: ");
        Serial.println(mqttConnected ? "CONNECTED" : "DISCONNECTED");
        lastMqttState = mqttConnected;
    }
}

void setup() {
// Обработка на изключване
// Уеб ендпойнт за прекъсване на MQTT връзката ръчно
  server.on("/disconnect", []() {
    if (client.connected()) {
      // Публикуване на OFFLINE статус преди изключване
      client.publish(MQTT_WILL_TOPIC, MQTT_WILL_OFFLINE, MQTT_WILL_RETAIN);
      client.disconnect();
    }
    server.send(200, "text/plain", "Disconnecting from MQTT...");
  });
// Обработка на restart
// Уеб ендпойнт за рестартиране на устройството по заявка
  server.on("/restart", []() {
    server.send(200, "text/plain", "Relay restarting...");
    delay(1000);
    ESP.restart();
  });

  Serial.begin(115200);
  // Always set relay to ON state on startup
  // Инициализиране на серийния порт за дебъг
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(RELAY_PIN, OUTPUT);
  relayState = true;
  
  // Запис в EEPROM
  // Структура на EEPROM данните:  
// - mqtt_server (0-39 байта): IP/адрес на MQTT брокера.  
// - mqtt_user (40-79 байта): потребителско име за MQTT.  
// - mqtt_pass (80-111 байта): парола за MQTT.  
// При първо стартиране (0xFF) се използват стойности по подразбиране.  
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) != 0xFF) {
      EEPROM.get(0, mqtt_server);
      EEPROM.get(40, mqtt_user);
      EEPROM.get(80, mqtt_pass);
      // Добавете проверки за валидност
      mqtt_server[sizeof(mqtt_server)-1] = '\0';
      mqtt_user[sizeof(mqtt_user)-1] = '\0';
      mqtt_pass[sizeof(mqtt_pass)-1] = '\0';
  }
  // Избор на мрежа и въвеждане на данни за MQTT връзка
  // Конфигуриране на WiFi:  
// - Ако няма запазена мрежа, създава AP "Relay-Setup" за конфигурация.  
// - Потребителят може да зададе MQTT сървър, потребител и парола.  
// - При успешно свързване настройките се запазват в EEPROM.  
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT Username", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_pass("pass", "MQTT Password", mqtt_pass, 32);

  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);

// Свързване с WIFI
  if (!wm.autoConnect("Relay-Setup")) {
    Serial.println("Failed to connect.");
    ESP.restart();
  }

  // WiFi авто-reconnect (добре е да е след успешно свързване)
  WiFi.setAutoReconnect(true);    // Автоматично повторно свързване при прекъсване
  WiFi.persistent(true);          // Запазва WiFi настройките в паметта

  if (shouldSaveConfig) {
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());
// Структура на EEPROM данните:  
// - mqtt_server (0-39 байта): IP/адрес на MQTT брокера.  
// - mqtt_user (40-79 байта): потребителско име за MQTT.  
// - mqtt_pass (80-111 байта): парола за MQTT.  
// При първо стартиране (0xFF) се използват стойности по подразбиране.  
    EEPROM.put(0, mqtt_server);
    EEPROM.put(40, mqtt_user);
    EEPROM.put(80, mqtt_pass);
    EEPROM.commit();
  }

  client.setServer(mqtt_server, 1883);
  // Задаване на функция за обработка на входящи MQTT съобщения
  client.setCallback(callback);
  client.setSocketTimeout(5); // Timeout в секунди за MQTT операции
  client.setKeepAlive(MQTT_KEEPALIVE);
  client.setBufferSize(512);  // Увеличаване на буфера за по-големи съобщения

  server.on("/", handleRoot);
  server.on("/relay/on", handleOn);
  server.on("/relay/off", handleOff);
  server.on("/relay/status", handleStatus);
  server.on("/mqtt/status", handleMQTTStatus);
  server.on("/reset", handleReset);
  server.begin();
  // Ensure WiFi reconnects after restart
  // Ръчно стартиране на WiFi с вече запазени настройки (дублиране за надеждност)
  WiFi.begin();  // Use stored credentials
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    yield();
  }
  Serial.println("WiFi connected after restart!");
}

void loop() {
// Подобрен loop() с heartbeat
// Основен цикъл с периодични задачи:  
// - Heartbeat: На всеки HEARTBEAT_INTERVAL се изпраща ONLINE статус към MQTT.  
// - Диагностика: Публикува RSSI (сила на сигнала) и свободна памет (heap).  
// - Проверява WiFi връзката и опитва повторно свързване при прекъсване.  
    static unsigned long lastHeartbeat = 0;
    const unsigned long HEARTBEAT_INTERVAL = 30000; // 30 секунди
    
    server.handleClient();
    manageMqttConnection();
    
if (mqttConnected) {
    // Обработка на входящи MQTT съобщения
    client.loop();
    
    if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
        lastHeartbeat = millis();
        // Публикуване на heartbeat с правилните параметри
        client.publish(MQTT_WILL_TOPIC, MQTT_WILL_ONLINE, MQTT_WILL_RETAIN);
        yield();
        
        char payload[50];
        snprintf(payload, sizeof(payload), 
                "{\"state\":\"%s\",\"rssi\":%d,\"heap\":%d}", 
                relayState ? "ON" : "OFF", 
                WiFi.RSSI(), 
                ESP.getFreeHeap());
        client.publish("home/relay/diagnostics", payload, true);
    }
}
    // Допълнителна WiFi проверка
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Attempting to reconnect...");
        // Опит за автоматично възстановяване на WiFi връзката
        WiFi.reconnect();
        yield();
    }
}

/*
 * Обобщение:
 * - Свързва се с WiFi и MQTT брокер
 * - Управлява реле чрез MQTT команди
 * - Изпраща статус и използва LWT за известяване при отпадане
 */
