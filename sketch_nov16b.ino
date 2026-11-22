#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define SS_PIN 5
#define RST_PIN 22
#define LED_VERDE 25
#define LED_ROJO  26
#define BUZZER    27

const char* WIFI_SSID = "Mega_2.4G_E857";
const char* WIFI_PASS = "Ts9QP9Es";
const char* URL_IDENTIFICAR = "http://192.168.1.59/rfid/identificar.php";
const char* URL_EVENTO      = "http://192.168.1.59/rfid/evento.php";
const char* CLAVE_NODO = "poner clave nodo";

const unsigned long VENTANA_MS    = 30000;  
const unsigned long REPETICION_MS = 5000;   

MFRC522 rfid(SS_PIN, RST_PIN);
String ultimoTrabajador = "";
unsigned long ultimoTrabajadorMs = 0;
String ultimoUid = "";
unsigned long ultimoUidMs = 0;

String leerUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void conectarWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado. IP del ESP32: " + WiFi.localIP().toString());
}

String consultarTipo(String uid) {
  HTTPClient http;
  http.begin(URL_IDENTIFICAR);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int codigo = http.POST("uid=" + uid + "&clave=" + CLAVE_NODO);
  String r = http.getString();
  http.end();

  if (codigo != 200) return "error";
  if (r.indexOf("\"tipo\":\"trabajador\"") >= 0)  return "trabajador";
  if (r.indexOf("\"tipo\":\"herramienta\"") >= 0) return "herramienta";
  return "desconocido";
}

void enviarEvento(String trabajador, String herramienta) {
  HTTPClient http;
  http.begin(URL_EVENTO);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int codigo = http.POST("trabajador_uid=" + trabajador + "&herramienta_uid=" + herramienta + "&clave=" + CLAVE_NODO);
  String respuesta = http.getString();
  senal(codigo == 200);
  Serial.print("HTTP ");
  Serial.print(codigo);
  Serial.print(" -> ");
  Serial.println(respuesta);
  http.end();
}

void senal(bool ok) {
  digitalWrite(ok ? LED_VERDE : LED_ROJO, HIGH);
  digitalWrite(BUZZER, HIGH);
  delay(ok ? 150 : 800);
  digitalWrite(BUZZER, LOW);
  delay(300);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, LOW);
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(LED_VERDE, LOW);
  digitalWrite(LED_ROJO, LOW);
  digitalWrite(BUZZER, LOW);
  conectarWiFi();
  Serial.println("Listo. Acerca tarjeta de trabajador y luego la herramienta.");
}
void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  String uid = leerUID();
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  if (uid == ultimoUid && millis() - ultimoUidMs < REPETICION_MS) return;
  ultimoUid = uid;
  ultimoUidMs = millis();

  String tipo = consultarTipo(uid);

  if (tipo == "trabajador") {
    ultimoTrabajador = uid;
    ultimoTrabajadorMs = millis();
    Serial.println("Trabajador: " + uid + ". Ahora acerca la herramienta.");
    senal(true);
  } else if (tipo == "herramienta") {
    if (ultimoTrabajador != "" && millis() - ultimoTrabajadorMs < VENTANA_MS) {
      Serial.println("Herramienta: " + uid);
      enviarEvento(ultimoTrabajador, uid);
      ultimoTrabajador = "";
    } else {
      Serial.println("Primero acerca la tarjeta del trabajador.");
      senal(false);
    }
  } else {
    Serial.println("UID no registrado: " + uid + " (" + tipo + ")");
    senal(false);
  }
  delay(1500);
}