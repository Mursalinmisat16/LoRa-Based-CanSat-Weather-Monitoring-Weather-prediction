/*
  =========================================================
          CANSAT GROUND STATION RECEIVER
  =========================================================

  Board:
    ESP32-S3 + SX1278 LoRa 433 MHz

  Job:
    Receive telemetry packets from CanSAT flight computer
    and print them as CSV over USB Serial.

  =========================================================
  ESP32-S3 <----> SX1278
  =========================================================

  VCC  -> 3.3V
  GND  -> GND

  SCK  -> GPIO18
  MISO -> GPIO19
  MOSI -> GPIO20

  NSS  -> GPIO5
  RST  -> GPIO14
  DIO0 -> GPIO46

  =========================================================
*/

#include <SPI.h>
#include <LoRa.h>

// =========================================================
// ESP32-S3 LoRa PIN CONFIGURATION
// =========================================================

#define LORA_SCK    18
#define LORA_MISO   19
#define LORA_MOSI   20

#define LORA_SS     5
#define LORA_RST    14
#define LORA_DIO0   46

#define LORA_FREQ   433E6


// =========================================================
// SETUP
// =========================================================

void setup() {

  Serial.begin(115200);

  // Give USB Serial time to start
  delay(2000);

  Serial.println();
  Serial.println("======================================");
  Serial.println("        CANSAT GROUND STATION");
  Serial.println("======================================");

  Serial.println();
  Serial.println("Board: ESP32-S3");
  Serial.println("LoRa: SX1278");
  Serial.println("Frequency: 433 MHz");

  // =====================================================
  // INITIALIZE SPI
  // =====================================================

  Serial.println();
  Serial.println("Initializing SPI...");

  SPI.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_SS
  );

  Serial.println("SPI initialized successfully.");

  // Tell LoRa library to use this SPI bus
  LoRa.setSPI(SPI);

  // Set SX1278 control pins
  LoRa.setPins(
    LORA_SS,
    LORA_RST,
    LORA_DIO0
  );

  // =====================================================
  // INITIALIZE LoRa
  // =====================================================

  Serial.println();
  Serial.println("Initializing LoRa...");

  int result = LoRa.begin(LORA_FREQ);

  if (result != 1) {

    Serial.println();
    Serial.println("======================================");
    Serial.println("[FATAL] LoRa initialization FAILED");
    Serial.println("======================================");

    Serial.println();
    Serial.println("Check SX1278 wiring:");
    Serial.println();
    Serial.println("VCC  -> 3.3V");
    Serial.println("GND  -> GND");
    Serial.println("SCK  -> GPIO18");
    Serial.println("MISO -> GPIO19");
    Serial.println("MOSI -> GPIO20");
    Serial.println("NSS  -> GPIO5");
    Serial.println("RST  -> GPIO14");
    Serial.println("DIO0 -> GPIO46");

    Serial.println();
    Serial.println("Also check that the SX1278 is powered.");
    Serial.println();

    while (true) {
      delay(1000);
    }
  }

  // =====================================================
  // LoRa CONFIGURATION
  // =====================================================

  Serial.println();
  Serial.println("======================================");
  Serial.println("     LoRa INITIALIZATION SUCCESS");
  Serial.println("======================================");

  Serial.println("SX1278 detected!");
  Serial.println();

  // MUST MATCH FLIGHT COMPUTER
  LoRa.setTxPower(17);

  // IMPORTANT:
  // Flight computer uses SF7
  LoRa.setSpreadingFactor(7);

  // Bandwidth
  LoRa.setSignalBandwidth(125E3);

  // Coding rate
  LoRa.setCodingRate4(5);

  // CRC
  LoRa.enableCrc();

  Serial.println("LoRa configuration:");
  Serial.println("--------------------");
  Serial.println("Frequency : 433 MHz");
  Serial.println("TX Power  : 17 dBm");
  Serial.println("SF        : 7");
  Serial.println("Bandwidth : 125 kHz");
  Serial.println("Coding    : 4/5");
  Serial.println("CRC       : Enabled");

  Serial.println();
  Serial.println("======================================");
  Serial.println("       GROUND STATION READY");
  Serial.println("======================================");

  Serial.println();
  Serial.println("Waiting for CanSAT telemetry...");
  Serial.println();

  // =====================================================
  // CSV HEADER
  // =====================================================

  Serial.println(
    "packetID,ms,lat,lon,gpsAlt,sats,gpsFix,"
    "bmpTemp,bmpPress,bmpAlt,"
    "dhtTemp,dhtHum,"
    "ax,ay,az,"
    "gx,gy,gz,"
    "imuTemp,battV,rssi"
  );
}


// =========================================================
// LOOP
// =========================================================

void loop() {

  // Check whether a LoRa packet has arrived
  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    // -----------------------------------------------------
    // Read packet
    // -----------------------------------------------------

    String receivedData = "";

    while (LoRa.available()) {
      receivedData += (char)LoRa.read();
    }

    // -----------------------------------------------------
    // Get RSSI
    // -----------------------------------------------------

    int rssi = LoRa.packetRssi();

    // -----------------------------------------------------
    // Print telemetry as CSV
    // -----------------------------------------------------

    Serial.print(receivedData);
    Serial.print(",");
    Serial.println(rssi);

  }
}