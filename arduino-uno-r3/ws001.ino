//Include: sensore di temperatura e umidità DHT20, sensore di fulmini MA5532_AE, sensore di pressione BE280 e web server
#include <Wire.h>
#include <Adafruit_BME280.h>
#include "DFRobot_AS3935_I2C.h"
#include <Ethernet.h>

volatile int8_t AS3935IsrTrig = 0;

#define SEALEVELPRESSURE_HPA (1013.25)
#if defined(ESP32) || defined(ESP8266)
#define IRQ_PIN       0
#else
#define IRQ_PIN       2
#endif

// Antenna tuning capcitance (must be integer multiple of 8, 8 - 120 pf)
#define AS3935_CAPACITANCE   0

// Indoor/outdoor mode selection
#define AS3935_INDOORS       0
#define AS3935_OUTDOORS      1
#define AS3935_MODE          AS3935_INDOORS

// Enable/disable disturber detection
#define AS3935_DIST_DIS      0
#define AS3935_DIST_EN       1
#define AS3935_DIST          AS3935_DIST_EN

// I2C address
#define AS3935_I2C_ADDR       AS3935_ADD3

Adafruit_BME280 bme;   // oggetto BME280

bool isBME = false;

// Configurazione della rete Ethernet
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; // MAC address (unico)
IPAddress server(172, 16, 27, 1); // IP del server web (modifica in base al tuo network)
EthernetClient client;

IPAddress ip(172, 16, 27, 11);
IPAddress subnet(255, 255, 255, 0);

int temp_read = 25; // Esempio di valore della temperatura acquisito dal sensore
long timestamp = 0; // Usa il timestamp se disponibile, altrimenti è 0 (null)

bool connectWithRetry(IPAddress server, int port, int retries, int waitMs) {
  for (int i = 0; i < retries; i++) {
    if (client.connect(server, port)) {
      return true; // connessione riuscita
    }
    delay(waitMs); // aspetta e riprova
  }
  return false; // dopo i tentativi falliti
}

void AS3935_ISR();

DFRobot_AS3935_I2C  lightning0((uint8_t)IRQ_PIN, (uint8_t)AS3935_I2C_ADDR);

void setupsenddata() {
 //Inizializzazione Ethernet
 Ethernet.begin(mac, ip);

 Serial.println("Qui");
 // Connetti al server
 if (connectWithRetry(server, 4999, 5, 2000)) {
 // Formatta il messaggio HTTP POST
 String url = "/update_data";
 String postData = "valore=" + String(temp_read) + "&timestamp=" + String(timestamp);

 // Invia la richiesta GET o POST
 client.println("POST " + url + " HTTP/1.1");
 client.println("Host: " + String(server));
 client.println("Content-Type: application/x-www-form-urlencoded");
 client.print("Content-Length: ");
 client.println(postData.length());
 client.println();
 client.println(postData);

 Serial.println("Dati inviati al server");

 } else {
 Serial.println("Connessione al server fallita");
 }

 delay(5000); // Ritardo di 5 secondi prima di un altro invio
}

void setupPressure() {
  while (!Serial);

  Serial.println("Ricerca sensore BME280 / BMP280...");

  // Prova BME280 con indirizzo 0x76
  if (bme.begin(0x76)) {
    Serial.println("✔ BME280 trovato (0x76)");
    isBME = true;
  } 
  else if (bme.begin(0x77)) {
    Serial.println("✔ BME280 trovato (0x77)");
    isBME = true;
  } 
}

void setupLighting()
{
  Serial.println("DFRobot AS3935 lightning sensor begin!");

  while (lightning0.begin() != 0){
    Serial.print(".");
  }
  lightning0.defInit();
  lightning0.manualCal(AS3935_CAPACITANCE, AS3935_MODE, AS3935_DIST);

  lightning0.printAllRegs();
  #if defined(ESP32) || defined(ESP8266)
    attachInterrupt(digitalPinToInterrupt(IRQ_PIN),AS3935_ISR,RISING);
  #else
    attachInterrupt(/*Interrupt No*/0,AS3935_ISR,RISING);
  #endif

  // Configure sensor
  // Enable interrupt (connect IRQ pin IRQ_PIN: 2, default)

//  Connect the IRQ and GND pin to the oscilloscope.
//  uncomment the following sentences to fine tune the antenna for better performance.
//  This will dispaly the antenna's resonance frequency/16 on IRQ pin (The resonance frequency will be divided by 16 on this pin)
//  Tuning AS3935_CAPACITANCE to make the frequency within 500/16 kHz ± 3.5%
//  lightning0.setLcoFdiv(0);
//  lightning0.setIRQOutputSource(3);

}

void setup() {
  Serial.begin(9600);     // Avvia la seriale (se non già avviata)
  setupPressure();
  setupLighting();
  setupsenddata();
}

void loopsenddata() {
  // Non è necessario ri-inizializzare Ethernet ogni ciclo
  // Ethernet.begin(mac, ip);  // ❌ rimosso

  Serial.println("loopweb");

  float t, p, a;

  // Leggo i valori dal sensore di pressione
  if (isBME) {
    t = bme.readTemperature();               // temperatura °C
    p = bme.readPressure() / 100.0F;         // pressione hPa
    a = bme.readAltitude(SEALEVELPRESSURE_HPA); // altitudine m
  }

  else {
    t = 0; p = 0; a = 0;                     // fallback
  }

  // Connetti al server
  if (connectWithRetry(server, 4999, 5, 2000)) {
    // Formatta il messaggio HTTP POST
    String url = "/update_data";

    String postData = "valore=" + String(temp_read) +
                      "&timestamp=" + String(timestamp) +
                      "&temperature=" + String(t, 2) +
                      "&pressure=" + String(p, 2) +
                      "&altitude=" + String(a, 2);

    // Invia la richiesta POST
    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(postData.length());
    client.println();
    client.println(postData);

    Serial.println("Dati inviati al server:");
    Serial.println(postData);
  }
  else {
    Serial.println("Connessione al server fallita");
  }

  delay(2000); // Ritardo di 2 secondi prima del prossimo invio
}



void loopPressure() {
  if (isBME) {
    Serial.print("Temperatura = ");
    Serial.print(bme.readTemperature());
    Serial.println(" °C");

    Serial.print("Pressione = ");
    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Umidità = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");

    Serial.println();
  } 
  delay(2000);
}

void loopLighting()
{
  // It does nothing until an interrupt is detected on the IRQ pin.
  while (AS3935IsrTrig == 0) {delay(1);}
  delay(5);

  // Reset interrupt flag
  AS3935IsrTrig = 0;

  // Get interrupt source
  uint8_t intSrc = lightning0.getInterruptSrc();
  if (intSrc == 1){
    // Get rid of non-distance data
    uint8_t lightningDistKm = lightning0.getLightningDistKm();
    Serial.println("Lightning occurs!");
    Serial.print("Distance: ");
    Serial.print(lightningDistKm);
    Serial.println(" km");

    // Get lightning energy intensity
    uint32_t lightningEnergyVal = lightning0.getStrikeEnergyRaw();
    Serial.print("Intensity: ");
    Serial.print(lightningEnergyVal);
    Serial.println("");
  }else if (intSrc == 2){
    Serial.println("Disturber discovered!");
  }else if (intSrc == 3){
    Serial.println("Noise level too high!");
  }
}

void loop() {
  loopPressure();
  loopLighting();
  loopsenddata();
}

//IRQ handler for AS3935 interrupts
void AS3935_ISR()
{
  AS3935IsrTrig = 1;
}

