# Управление на реле с ESP8266 чрез MQTT и уеб интерфейс

Този проект реализира управление на интелигентно реле с помощта на ESP8266, интегрирайки MQTT и отзивчив уеб интерфейс за дистанционно управление. Устройството поддържа Last Will and Testament (LWT) за мониторинг на връзката, автоматично пресвързване и конфигурация чрез WiFiManager.

# Възможности

- **Управление на реле:** Контрол на реле чрез GPIO пин (D1).
- **Интеграция с MQTT:**
  - Публикуване/абониране за теми за дистанционно управление (home/relay/control, home/relay/status).
  - LWT съобщения (home/relay/online) за проследяване на свързаността на устройството.
  - Хартбит и диагностика (RSSI, heap памет) на всеки 30 секунди.

- **Уеб интерфейс:**
  - Отзивчив интерфейс с реално-времеви актуализации на статуса.
  - REST API endpoints (/relay/on, /relay/off, /relay/status).
  - Опции за нулиране на конфигурацията и рестартиране на устройството.
  - WiFiManager: Лесна настройка на WiFi и MQTT идентификационни данни чрез captive портал.
  - Постоянно съхранение: Запазва MQTT настройките в EEPROM.

## Хардуерни изисквания
  - ESP8266 (напр. NodeMCU, Wemos D1 Mini)
  - Реле модул (съвместим с 3.3V логика)
  - Захранване (5V за ESP8266, напрежение, специфично за релето)

## Конфигурация

1. **Настройка на WiFi и MQTT:**
  - При първо стартиране се свържете с AP `Relay-Setup`.
  - Конфигурирайте WiFi и MQTT настройки (сървър, потребител, парола) чрез уеб портала.
  - Настройките се запазват в EEPROM за следващи рестарти.

2. **MQTT теми:**
   - **Контрол:** `home/relay/control` (изпратете `ON/OFF/RESTART`).
   - **Статус:** `home/relay/status` (публикува `ON/OFF`).
   - **Онлайн статус:** `home/relay/online` (LWT съобщения: `ONLINE/OFFLINE`).

3. **Уеб endpoints:**
  - `/relay/on`: Включва релето.
  - `/relay/off`: Изключва релето.
  - `/restart`: Рестартира устройството.
  - `/reset`: Изтрива всички настройки и рестартира.

## Инсталация

1. **Настройка на Arduino IDE:**
   - Инсталирайте поддръжка за ESP8266 чрез Boards Manager.
   - Инсталирайте необходимите библиотеки:
     - `ESP8266WiFi`
     - `ESP8266WebServer`
     - `WiFiManager`
     - `PubSubClient`
     - `EEPROM`

2. **Качване на код:**
   - Компилирайте и качете `esp8266_relay_mqtt_ver3_3_comments_ENG.ino` на вашия ESP8266.

3. **Хардуерна връзка:**
   - Свържете релето към пин `D1 (GPIO5)`.
   - Осигурете подходящо захранване за ESP8266 и релето.

## Употреба
   - Уеб интерфейс: Достъпен чрез `http://<ESP_IP>` за управление на релето и преглед на статуса.
   - MQTT команди: Публикувайте към `home/relay/control` със съдържание `ON/OFF/RESTART`.
   - Автоматично пресвързване: Устройството автоматично се свързва отново с WiFi/MQTT при загуба на връзка.
   - Конфигурация за Home Assistant: В съответствие с приложения файл `Configuration.yaml`
   - 
## Отстраняване на проблеми
- **Проверете Serial Monitor:** Дебъг изход при `115200 baud`.
- **Нулиране на настройки:** Използвайте бутона "Reset Settings" в уеб интерфейса или задръжте boot бутона на ESP8266 по време на стартиране, за да активирате AP режим.
- **Проблеми с MQTT:** Проверете IP на сървъра, идентификационни данни и firewall настройки.

## EEPROM карта на паметта:

Адрес---Размер----Поле----------Описание
0----------40------mqtt_server---IP или хост на MQTT брокер
40---------40------mqtt_user-----Потребител за MQTT
80---------32------mqtt_pass-----Парола

## Диаграма:

За повече информация относно модулите и последователността от действия вижте Diagram mermaid.png

---
## Лиценз

Този проект е с отворен код под MIT лиценз. Можете свободно да го модифицирате и разпространявате.
