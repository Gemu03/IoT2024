#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <OneWire.h>
#include <PubSubClient.h>  // Biblioteca MQTT

// Pines del sistema
#define BUZZER_PIN 19
#define DHT_PIN 4
#define DHT_TYPE DHT11
#define DS18B20_PIN 25
#define RELAY_PIN 18          
#define SDA_PIN 21            
#define SCL_PIN 22           

// Configuración de WiFi y MQTT
const char* ssid = "S24";
const char* password = "23456789";
const char* mqtt_server = "RaspberryPi_IP";

// Inicialización de sensores y pantalla LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// Inicialización del servidor web y MQTT
WiFiClient espClient;
PubSubClient client(espClient);
WebServer server(80);

// Variables
float axillary_temp_min = 36.5;
float axillary_temp_max = 37.5;
float humidity_min = 70;
float humidity_max = 100;

float temperaturaExterior = 0.0;
float temperaturaAxilar = 0.0;
float humedad = 0.0;
bool alarmaManualApagada = false;
bool activarAlarma = false;

SemaphoreHandle_t xMutex;

// Función para conectarse al WiFi
void connectToWiFi() {
  Serial.println("====================================");
  Serial.println("Conectando a la red WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  Serial.println("====================================");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("\n====================================");
  Serial.println("WiFi conectado exitosamente.");
  Serial.print("Dirección IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("====================================");
}

// Función para conectarse al MQTT
void reconnectMQTT() {
  while (!client.connected()) {
    if (client.connect("ESP32Client")) {
      Serial.println("Conectado al broker MQTT");
    } else {
      delay(5000);
      Serial.println("Intentando conectar al broker MQTT...");
    }
  }
}

// Publicación de datos a MQTT
void publishMQTT() {
  char msg[100];
  snprintf(msg, 100, "T_Ext=%.2f, T_Axil=%.2f, Hum=%.2f", temperaturaExterior, temperaturaAxilar, humedad);
  client.publish("incubadora/data", msg);
}

// Tarea 1: Medición de sensores
void sensorTask(void *pvParameters) {
  for (;;) {
    // Medir sensores
    float tempExt = dht.readTemperature();
    sensors.requestTemperatures();
    float tempAxi = sensors.getTempCByIndex(0);
    float hum = dht.readHumidity();

    // Verificar si las lecturas son válidas
    if (isnan(tempExt) || isnan(tempAxi) || isnan(hum)) {
      Serial.println("Error en la lectura de sensores");
      continue;
    }

    // Proteger con Mutex
    xSemaphoreTake(xMutex, portMAX_DELAY);
    temperaturaExterior = tempExt;
    temperaturaAxilar = tempAxi;
    humedad = hum;
    xSemaphoreGive(xMutex);

    // Control de Alarma
    bool alarma = false;
    if (tempAxi < axillary_temp_min || hum < humidity_min || hum > humidity_max) {
      alarma = true;
      digitalWrite(RELAY_PIN, LOW);   // Encender relé
      digitalWrite(BUZZER_PIN, HIGH); // Encender alarma
    } else {
      digitalWrite(RELAY_PIN, HIGH);  // Apagar relé
      digitalWrite(BUZZER_PIN, LOW);  // Apagar alarma
    }

    xSemaphoreTake(xMutex, portMAX_DELAY);
    activarAlarma = alarma;
    xSemaphoreGive(xMutex);

    delay(1000);  // Espera de 1 segundo
  }
}

// Tarea 2: Comunicación MQTT
void mqttTask(void *pvParameters) {
  for (;;) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    xSemaphoreTake(xMutex, portMAX_DELAY);
    publishMQTT();  // Publicar los datos de temperatura y humedad
    xSemaphoreGive(xMutex);

    delay(2000);  // Publicar datos cada 2 segundos
  }
}

// Tarea 3: Servidor web embebido
void serverTask(void *pvParameters) {
  setupWebServer();

  for (;;) {
    server.handleClient();
    delay(10);
  }
}

// Configuración del servidor web
void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/sensor", handleSensor);
  server.on("/apagarAlarma", handleApagarAlarma);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("Servidor HTTP iniciado.");
}

// Página web
void handleRoot() {
  String message = "<html><head>";
  message += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  message += "<style>";
  message += "body { font-family: Arial, sans-serif; margin: 0; padding: 0; background-color: #f4f4f4; }";
  message += ".container { max-width: 600px; margin: 50px auto; padding: 20px; background-color: #fff; box-shadow: 0 0 10px rgba(0, 0, 0, 0.1); border-radius: 10px; }";
  message += "h1 { color: #333; text-align: center; margin-bottom: 20px; }";
  message += "p { font-size: 1.2em; color: #666; text-align: center; }";
  message += ".data { font-size: 1.5em; font-weight: bold; color: #333; }";
  message += ".high { color: red; }";  // Clase para cuando hay un problema con la temperatura o humedad
  message += ".low { color: blue; }";  // Clase para cuando la humedad es baja
  message += ".normal { color: green; }";  // Clase para valores normales
  message += "button { display: block; width: 100%; padding: 10px; margin-top: 20px; font-size: 1.2em; color: #fff; background-color: #007BFF; border: none; border-radius: 5px; cursor: pointer; }";
  message += "button:hover { background-color: #0056b3; }";
  message += ".alarm-on { color: red; font-weight: bold; }";  // Clase para cuando la alarma está activa
  message += ".alarm-off { color: green; font-weight: bold; }";  // Clase para cuando la alarma está apagada
  message += "</style>";
  message += "</head><body>";
  message += "<div class='container'>";
  message += "<h1>Monitor de Incubadora</h1>";
  message += "<p>Temperatura Exterior: <span id='temperature' class='data'>Cargando...</span> &deg;C</p>";
  message += "<p>Humedad: <span id='humidity' class='data'>Cargando...</span> %</p>";
  message += "<p>Temperatura Axilar: <span id='tempAxilar' class='data'>Cargando...</span> &deg;C</p>";
  message += "<p>Alarma: <span id='alarmStatus' class='alarm-off'>Apagada</span></p>";
  message += "<button type='button' onclick='apagarAlarma()'>Apagar Alarma</button>";

  message += "<script>";
  message += "function actualizarDatos() {";
  message += "fetch('/sensor').then(response => response.text()).then(data => {";
  message += "var values = data.split(',');";
  message += "var temp = parseFloat(values[0]);";  // Temperatura exterior
  message += "var hum = parseFloat(values[1]);";   // Humedad
  message += "var tempAxilar = parseFloat(values[2]);";  // Temperatura axilar
  message += "var alarma = values[3] === '1';";  // Estado de la alarma";

  // Actualizar datos
  message += "document.getElementById('temperature').innerHTML = temp;";
  message += "document.getElementById('humidity').innerHTML = hum;";
  message += "document.getElementById('tempAxilar').innerHTML = tempAxilar;";

  // Actualizar el estado de la alarma
  message += "if (alarma) {";
  message += "document.getElementById('alarmStatus').innerHTML = 'Encendida';";
  message += "document.getElementById('alarmStatus').className = 'alarm-on';";
  message += "} else {";
  message += "document.getElementById('alarmStatus').innerHTML = 'Apagada';";
  message += "document.getElementById('alarmStatus').className = 'alarm-off';";
  message += "}";

  message += "});";
  message += "}";
  message += "setInterval(actualizarDatos, 1000);";

  // Función apagar alarma
  message += "function apagarAlarma() {";
  message += "fetch('/apagarAlarma').then(response => {";
  message += "if (response.ok) {";
  message += "alert('La alarma ha sido apagada manualmente.');";
  message += "} else {";
  message += "alert('Error al apagar la alarma.');";
  message += "}";
  message += "});";
  message += "}";
  message += "</script>";

  message += "</div>";
  message += "</body></html>";

  server.send(200, "text/html", message);
}

// Función para manejar las solicitudes de datos del sensor
void handleSensor() {
  xSemaphoreTake(xMutex, portMAX_DELAY);
  float tExt = temperaturaExterior;
  float tAxi = temperaturaAxilar;
  float hum = humedad;
  bool alarmaEstado = activarAlarma && !alarmaManualApagada;
  xSemaphoreGive(xMutex);

  String alarmaStr = alarmaEstado ? "1" : "0";
  server.send(200, "text/plain", String(tExt) + "," + String(hum) + "," + String(tAxi) + "," + alarmaStr);
}

// Apagar la alarma manualmente
void handleApagarAlarma() {
  xSemaphoreTake(xMutex, portMAX_DELAY);
  alarmaManualApagada = true;
  digitalWrite(BUZZER_PIN, LOW);
  xSemaphoreGive(xMutex);
  server.send(200, "text/plain", "Alarma apagada");
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);

  dht.begin();
  sensors.begin();
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  Serial.begin(115200);

  xMutex = xSemaphoreCreateMutex();
  
  connectToWiFi();
  client.setServer(mqtt_server, 1883);

  // Crear los tres hilos
  xTaskCreatePinnedToCore(
    sensorTask,        // Hilo para medir sensores
    "Sensor Task",     
    10000,             
    NULL,              
    1,                 
    NULL,              
    1                  
  );

  xTaskCreatePinnedToCore(
    mqttTask,          // Hilo para manejar MQTT
    "MQTT Task",       
    10000,             
    NULL,              
    1,                 
    NULL,              
    0                  
  );

  xTaskCreatePinnedToCore(
    serverTask,        // Hilo para manejar el servidor web
    "Server Task",     
    10000,             
    NULL,              
    1,                 
    NULL,              
    1                  
  );
}

void loop() {
  delay(1000);  // Tiempo en el bucle principal para no sobrecargar
}