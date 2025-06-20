#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <EEPROM.h>

// Define constants at the beginning
// MQTT Last Will and Testament (LWT) settings:
// - MQTT_WILL_TOPIC: Topic for publishing device status (ONLINE/OFFLINE).
// - MQTT_WILL_ONLINE/OFFLINE: Messages sent when MQTT connection is established/lost.
// - MQTT_WILL_RETAIN: Retains the last message in the broker for always available status.
#define RELAY_PIN D1                // Relay pin
#define EEPROM_SIZE 128
#define MQTT_PORT 1883              // MQTT port
#define MQTT_CONNECT_TIMEOUT 5000  // 5 seconds connection timeout
#undef MQTT_KEEPALIVE
#define MQTT_KEEPALIVE 60          // MQTT keepalive interval
#define MQTT_RETRY_INTERVAL 10000  // 10 seconds between connection attempts
#define CLIENT_ID "ESP8266Relay"    // Relay name
#define MQTT_TOPIC_CONTROL "home/relay/control"
#define MQTT_TOPIC_STATUS "home/relay/status"
#define MQTT_TOPIC_ONLINE "home/relay/online"
#define MQTT_WILL_TOPIC "home/relay/online"
#define MQTT_WILL_ONLINE "ONLINE"
#define MQTT_WILL_OFFLINE "OFFLINE"
#define MQTT_WILL_QOS 1
#define MQTT_WILL_RETAIN true
#define WIFI_RETRY_DELAY 3000      // 3 seconds delay between attempts
#define MAX_WIFI_RETRIES 5         // Maximum number of reconnection attempts
#define WIFI_CONNECTION_TIMEOUT 30000 // 30 seconds connection timeout


// Create objects for web server, WiFi and MQTT client
ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);

// Additional state variables
// Variables for managing MQTT connection state and relay
unsigned long lastMqttConnectionAttempt = 0;
int mqttConnectionAttempts = 0;
const int MAX_MQTT_ATTEMPTS = 3;

bool relayState = true;
bool mqttConnected = false;

// MQTT server data 
char mqtt_server[40] = "192.168.1.171";
char mqtt_user[32] = "relay_user";
char mqtt_pass[32] = "12345678";

bool shouldSaveConfig = false;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatInterval = 30000;

// Function saveConfigCallback
void saveConfigCallback () {
  shouldSaveConfig = true;
}

// Callback function
// Receives messages from MQTT topic and reacts to them:
// "on" - turns relay on
// "off" - turns relay off
// "restart" - restarts the device
// "status" - returns current state
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
// Web interface for relay control:
// - Uses JavaScript for asynchronous requests (fetch) to REST API (/relay/on, /relay/off etc.).
// - R"rawliteral" allows easy multiline HTML writing without escaping.
// - Status updates every 5 seconds via refreshStatus().
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

// Function handle On
void handleOn() {
  digitalWrite(RELAY_PIN, HIGH);
  relayState = true;
  client.publish(MQTT_TOPIC_STATUS, "ON"); // Use constant instead of hardcoded string
  server.send(200, "text/plain", "ON (power flowing)");
}

// Function handle Off
void handleOff() {
  digitalWrite(RELAY_PIN, LOW);
  relayState = false;
  client.publish(MQTT_TOPIC_STATUS, "OFF"); // Use constant instead of hardcoded string
  server.send(200, "text/plain", "OFF (power cut)");
}

// Function handle Status
// Returns relay state via web interface
// Format: "ON (power flowing)" or "OFF (power cut)"
void handleStatus() {
  String status = relayState ? "ON (power flowing)" : "OFF (power cut)";
  server.send(200, "text/plain", status);
}

// Function handle MQTT Status
// Returns current MQTT connection state via web interface:
// "CONNECTED" or "DISCONNECTED"
void handleMQTTStatus() {
  server.send(200, "text/plain", mqttConnected ? "CONNECTED" : "DISCONNECTED");
}

// Function handle Reset
// Function for complete settings reset:
// - Erases saved data in EEPROM.
// - Erases WiFi settings via WiFiManager.
// - Restarts device to enter configuration mode.
void handleReset() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  EEPROM.end();

  WiFiManager wm;
  wm.resetSettings();

  server.send(200, "text/plain", "Settings erased. Restarting...");
  delay(1000);
  ESP.restart();
}

// Reconnect function
// Attempt to connect to MQTT broker:
// - If fails after MAX_MQTT_ATTEMPTS attempts, device restarts.
// - On success, subscribes to MQTT_TOPIC_CONTROL and publishes current relay state.
// - LWT message (ONLINE) is sent to signal successful connection.
// - Uses LWT messages to inform broker about connection loss/recovery
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

    // Improved LWT configuration
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
        // On success, subscribe and send current relay status and ONLINE message
        client.subscribe(MQTT_TOPIC_CONTROL);
        client.publish(MQTT_TOPIC_STATUS, relayState ? "ON" : "OFF", true);
        // Publish ONLINE status on successful connection
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

// Connection management function
// MQTT connection management:
// - Checks if it's time for new connection attempt (MQTT_RETRY_INTERVAL).
// - Logs state changes (CONNECTED/DISCONNECTED).
// - If connection is lost, attempts reconnection after interval.
void manageMqttConnection() {
    static bool lastMqttState = false;
    
    if (!client.connected()) {
        mqttConnected = false;
        
        // Check if it's time for new connection attempt
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

    // Log state changes
    if (lastMqttState != mqttConnected) {
        Serial.print("MQTT state changed: ");
        Serial.println(mqttConnected ? "CONNECTED" : "DISCONNECTED");
        lastMqttState = mqttConnected;
    }
}

// Add this new feature to launch WiFi configuration portal
void startWiFiConfigPortal() {
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);

  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT Username", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_pass("pass", "MQTT Password", mqtt_pass, 32);

  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);

  if (!wm.startConfigPortal("Relay-Setup")) {
    Serial.println("Failed to connect to WiFi and config portal timed out. Restarting...");
    delay(1000);
    ESP.restart();
  }

  // If we have successfully configured WiFi, we save the settings
  if (shouldSaveConfig) {
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());

    EEPROM.put(0, mqtt_server);
    EEPROM.put(40, mqtt_user);
    EEPROM.put(80, mqtt_pass);
    EEPROM.commit();
  }
}


void setup() {
// Disconnect handling
// Web endpoint for manual MQTT disconnection
  server.on("/disconnect", []() {
    if (client.connected()) {
      // Publish OFFLINE status before disconnecting
      client.publish(MQTT_WILL_TOPIC, MQTT_WILL_OFFLINE, MQTT_WILL_RETAIN);
      client.disconnect();
    }
    server.send(200, "text/plain", "Disconnecting from MQTT...");
  });

// Restart handling
// Web endpoint for device restart on request
  server.on("/restart", []() {
    server.send(200, "text/plain", "Relay restarting...");
    delay(1000);
    ESP.restart();
  });

  Serial.begin(115200);
  // Always set relay to ON state on startup
  // Initialize serial port for debugging
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(RELAY_PIN, OUTPUT);
  relayState = true;
  
  // EEPROM structure:
// - mqtt_server (0-39 bytes): MQTT broker IP/address.
// - mqtt_user (40-79 bytes): MQTT username.
// - mqtt_pass (80-111 bytes): MQTT password.
// On first boot (0xFF), default values are used.
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(0) != 0xFF) {
      EEPROM.get(0, mqtt_server);
      EEPROM.get(40, mqtt_user);
      EEPROM.get(80, mqtt_pass);
      mqtt_server[sizeof(mqtt_server)-1] = '\0';
      mqtt_user[sizeof(mqtt_user)-1] = '\0';
      mqtt_pass[sizeof(mqtt_pass)-1] = '\0';
  }

  // Declare WiFiManager parameters here (they were missing)
  WiFiManagerParameter custom_mqtt_server("server", "MQTT Server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT Username", mqtt_user, 32);
  WiFiManagerParameter custom_mqtt_pass("pass", "MQTT Password", mqtt_pass, 32);

  // New WiFi connection logic
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);

  unsigned long wifiConnectionStart = millis();
  int wifiRetryCount = 0;
  bool wifiConnected = false;

  // WiFi connection attempts
  while (wifiRetryCount < MAX_WIFI_RETRIES && !wifiConnected) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.printf("Attempting WiFi connection (try %d of %d)...\n", wifiRetryCount + 1, MAX_WIFI_RETRIES);
      
      // Fixed if condition - missing closing parenthesis
      if (strlen(WiFi.SSID().c_str()) > 0) {
        WiFi.begin();
      } else {
        // No saved credentials, start WiFiManager
        break;
      }

      unsigned long attemptStart = millis(); // Fixed: declared before use
      while (millis() - attemptStart < WIFI_RETRY_DELAY && WiFi.status() != WL_CONNECTED) {
        delay(100);
        yield();
      }

      if (WiFi.status() != WL_CONNECTED) {
        wifiRetryCount++;
        if (wifiRetryCount < MAX_WIFI_RETRIES) {
          delay(WIFI_RETRY_DELAY);
        }
      } else {
        wifiConnected = true;
      }
    } else {
      wifiConnected = true;
    }
  }

  // If we failed to connect after all attempts
  if (!wifiConnected) {
    if (millis() - wifiConnectionStart >= WIFI_CONNECTION_TIMEOUT || strlen(WiFi.SSID().c_str()) == 0) {
      Serial.println("Failed to connect to WiFi. Starting configuration portal...");
      startWiFiConfigPortal(custom_mqtt_server, custom_mqtt_user, custom_mqtt_pass);
    } else {
      Serial.println("WiFi connection attempts exhausted but within timeout. Continuing...");
    }
  }

  //  WiFiManager setup
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);

  if (shouldSaveConfig) {
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_user, custom_mqtt_user.getValue());
    strcpy(mqtt_pass, custom_mqtt_pass.getValue());

    EEPROM.put(0, mqtt_server);
    EEPROM.put(40, mqtt_user);
    EEPROM.put(80, mqtt_pass);
    EEPROM.commit();
  }

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.setSocketTimeout(5);
  client.setKeepAlive(MQTT_KEEPALIVE);
  client.setBufferSize(512);

  server.on("/", handleRoot);
  server.on("/relay/on", handleOn);
  server.on("/relay/off", handleOff);
  server.on("/relay/status", handleStatus);
  server.on("/mqtt/status", handleMQTTStatus);
  server.on("/reset", handleReset);
  server.begin();
}

//  startWiFiConfigPortal function to accept parameters
void startWiFiConfigPortal(WiFiManagerParameter& custom_mqtt_server, 
                          WiFiManagerParameter& custom_mqtt_user, 
                          WiFiManagerParameter& custom_mqtt_pass) {
  WiFiManager wm;
  wm.setSaveConfigCallback(saveConfigCallback);
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_user);
  wm.addParameter(&custom_mqtt_pass);

  if (!wm.startConfigPortal("Relay-Setup")) {
    Serial.println("Failed to connect to WiFi and config portal timed out. Restarting...");
    delay(1000);
    ESP.restart();
  }
}

void loop() {
// Improved loop() with heartbeat
// Main cycle with periodic tasks:
// - Heartbeat: Every HEARTBEAT_INTERVAL sends ONLINE status to MQTT.
// - Diagnostics: Publishes RSSI (signal strength) and free memory (heap).
// - Checks WiFi connection and attempts reconnection if lost.
  static unsigned long lastHeartbeat = 0;
  static unsigned long lastWiFiCheck = 0;
  const unsigned long HEARTBEAT_INTERVAL = 30000;
  const unsigned long WIFI_CHECK_INTERVAL = 10000;

  server.handleClient();
  manageMqttConnection();
  
  if (mqttConnected) {
    client.loop();
    
    if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
      lastHeartbeat = millis();
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
  
  //  WiFi connection verification
  if (millis() - lastWiFiCheck > WIFI_CHECK_INTERVAL) {
    lastWiFiCheck = millis();
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected! Attempting to reconnect...");
      WiFi.reconnect();
      
      // Check if the connection is restored after 3 seconds
      delay(3000);
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi reconnection failed. Will retry later.");
      }
    }
    yield();
  }
}

/*
 * Summary:
 * - Connects to WiFi and MQTT broker
 * - Controls relay via MQTT commands
 * - Sends status and uses LWT to notify on disconnection
 */