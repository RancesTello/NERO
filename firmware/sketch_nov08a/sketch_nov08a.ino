/*
 * Conexión WiFi para Arduino Nano RP2040 Connect
 * Este código conecta el board a tu red WiFi
 */

#include <WiFiNINA.h>

// Reemplaza con tus credenciales WiFi
#include "secrets.h"   // credenciales (ver secrets.example.h)
const char* ssid = SECRET_WIFI_SSID;
const char* password = SECRET_WIFI_PASSWORD;

int status = WL_IDLE_STATUS;

void setup() {
  Serial.begin(9600);
  
  // Esperar a que se abra el monitor serial (opcional)
  while (!Serial) {
    delay(10);
  }
  
  // Verificar si el módulo WiFi está presente
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("ERROR: Módulo WiFi no encontrado!");
    while (true); // Detener ejecución
  }
  
  // Verificar versión del firmware
  String fv = WiFi.firmwareVersion();
  Serial.print("Versión firmware WiFi: ");
  Serial.println(fv);
  
  // Intentar conectar a la red WiFi
  Serial.print("Conectando a: ");
  Serial.println(ssid);
  
  while (status != WL_CONNECTED) {
    status = WiFi.begin(ssid, password);
    delay(5000); // Esperar 5 segundos antes de reintentar
  }
  
  // Conexión exitosa
  Serial.println("\n¡Conectado a WiFi!");
  mostrarInfoRed();
}

void loop() {
  // Verificar si la conexión sigue activa
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Conexión perdida. Reconectando...");
    WiFi.begin(ssid, password);
    delay(5000);
  } else {
    // Mostrar estado cada 10 segundos
    Serial.println("WiFi conectado - RSSI: " + String(WiFi.RSSI()) + " dBm");
    delay(10000);
  }
}

void mostrarInfoRed() {
  Serial.println("\n=== Información de Red ===");
  
  // Dirección IP
  IPAddress ip = WiFi.localIP();
  Serial.print("IP: ");
  Serial.println(ip);
  
  // Gateway
  IPAddress gateway = WiFi.gatewayIP();
  Serial.print("Gateway: ");
  Serial.println(gateway);
  
  // Máscara de subred
  IPAddress subnet = WiFi.subnetMask();
  Serial.print("Subnet: ");
  Serial.println(subnet);
  
  // Intensidad de señal
  long rssi = WiFi.RSSI();
  Serial.print("RSSI: ");
  Serial.print(rssi);
  Serial.println(" dBm");
  
  // MAC Address
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i > 0) Serial.print(":");
  }
  Serial.println("\n========================\n");
}
