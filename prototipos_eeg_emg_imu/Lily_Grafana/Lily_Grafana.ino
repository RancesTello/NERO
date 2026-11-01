#include <WiFi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

#define DEVICE "ESP32"

#include "secrets.h"   // credenciales (ver secrets.example.h)
const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;

#define INFLUXDB_BUCKET "UNIMATION_A"
#define TZ_INFO "UTC-5"

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

Point sensor("sensor_data");

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" ¡Conectado!");
  
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
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado");
    return;
  }

  sensor.clearFields();

  sensor.addField("rssi", WiFi.RSSI());
  sensor.addField("free_heap", ESP.getFreeHeap());
  sensor.addField("uptime", millis());
  
  // 🌡️ Ejemplo: Sensor de temperatura (reemplaza con tu sensor real)
  float temperature = 25.4; // readTemperatureSensor();
  sensor.addField("temperature", temperature);
  sensor.addField("temperature", temperature);

  Serial.print("Enviando: ");
  Serial.println(sensor.toLineProtocol());
  
  if (!client.writePoint(sensor)) {
    Serial.print("Error enviando datos: ");
    Serial.println(client.getLastErrorMessage());
  } else {
    Serial.println("✅ Datos enviados correctamente");
  }
}
