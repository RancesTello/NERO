#include <WiFi.h>
#include <HTTPClient.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define DEVICE "ESP32"

#define BAUDRATE 57600

#include "secrets.h"   // credenciales (ver secrets.example.h)
#define INFLUXDB_BUCKET "UNIMATION_A"
#define TZ_INFO "UTC-5"

const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;

const char* serverName = "https://script.google.com/macros/s/AKfycbxG69-CNQVj0WDJjMyI0L6QouHQr013nAv6QTncTuxshGSKDvkaMJW0YG3U8pzhHgGS/exec";

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);
//WiFiClient client;
//ThingsBoard tb(client);

Point sensor("sensor_data");

void setup() {
  Serial.begin(BAUDRATE);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");
  if (client.validateConnection()) {
    Serial.print("Conectado a InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("Error conexión InfluxDB: ");
    Serial.println(client.getLastErrorMessage());
  }

  sensor.addTag("device", DEVICE);
  sensor.addTag("location", "oficina");
  sensor.addTag("SSID", WiFi.SSID());
  Serial.print("•❲🛰❳︶︶︶︶︶︶︶︶︶︶︶❲🛰❳•\n │ㅤㅤ──╯꒰☁︎ ꒱ ʟɪʟʏ ꒰☁︎ ꒱╰──\n │✄. . . . . . . .  . . . . . . . . . . . . . . . ↻° :   ﹢\n │ㅤ↬xㅤ✰;; _Conexión Exitosa_  ♡ʾʾ . . \n │✄. . . . . . . . . . . . . . . . . . .ⵓ‧˖˚࿔₊˚\n ☁︎ ♡ ʟɪʟʏ ☁︎ ♡ \n •❲🛰❳︶︶︶︶︶︶︶︶︶︶︶❲🛰❳•");

}

void enviarDatos(uint32_t att, uint32_t med, uint8_t focusRatio) {

  Serial.println("BASE DE DATOS V1: " + String(att));
  Serial.println("BASE DE DATOS V2: " + String(med));
  Serial.println("BASE DE DATOS V3: " + String(focusRatio));

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String jsonData = "{\"attention\":\"" + String(att) + "\", \"meditation\":\"" + String(med) + "\", \"poorQuality\":\"" + String(focusRatio) + "\"}";

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(httpResponseCode);
      Serial.println(response);
    } else {
      Serial.print("Wrong request POST: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  }
}

void loop() {

  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    int startA = data.indexOf("&") + 1;
    int endA = data.indexOf("?");
    int startM = endA + 1;
    int endM = data.indexOf("#");
    int startQ = endM + 1;
    int endQ = data.indexOf(";");

    if (startA > 0 && endA > startA && endM > startM && endQ > startQ) {
      int attention = data.substring(startA, endA).toInt();
      int meditation = data.substring(startM, endM).toInt();
      int focusRatio = data.substring(startQ, endQ).toInt();

      sensor.clearFields();

      sensor.addField("rssi", WiFi.RSSI());
      sensor.addField("free_heap", ESP.getFreeHeap());
      sensor.addField("uptime", millis());

      sensor.addField("attention", attention);
      sensor.addField("meditation", meditation);
      sensor.addField("focusRatio", focusRatio);

      Serial.print("Enviando: ");
      Serial.println(sensor.toLineProtocol());

      if (!client.writePoint(sensor)) {
        Serial.print("Error enviando datos: ");
        Serial.println(client.getLastErrorMessage());
      } else {
        Serial.println("✅ Datos enviados correctamente");
      }

      enviarDatos(attention, meditation, focusRatio);
    }
  }
}
