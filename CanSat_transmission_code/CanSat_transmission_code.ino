#include <Wire.h>
#include <SPI.h>
#include <LoRa.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <TinyGPSPlus.h>

// =====================================================
//                    PIN DEFINITIONS
//                 REGULAR ESP32 DEVKIT
// =====================================================

// I2C
#define I2C_SDA 21
#define I2C_SCL 22

// DHT11
#define DHT_PIN 4
#define DHT_TYPE DHT11

// GPS
#define GPS_RX 16
#define GPS_TX 17

// SX1278
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26

#define LORA_FREQUENCY 433E6

// Battery
#define BATTERY_PIN 34


// =====================================================
//                    OBJECTS
// =====================================================

Adafruit_BMP280 bmp;
DHT dht(DHT_PIN, DHT_TYPE);

TinyGPSPlus gps;
HardwareSerial GPS(2);


// =====================================================
//                    MPU6500
// =====================================================

#define MPU_ADDR 0x68

#define WHO_AM_I      0x75
#define PWR_MGMT_1    0x6B
#define PWR_MGMT_2    0x6C
#define CONFIG_REG    0x1A
#define GYRO_CONFIG   0x1B
#define ACCEL_CONFIG  0x1C

#define ACCEL_XOUT_H  0x3B
#define TEMP_OUT_H    0x41
#define GYRO_XOUT_H   0x43


// =====================================================
//                 STATUS FLAGS
// =====================================================

bool bmpOK = false;
bool loraOK = false;
bool mpuOK = false;


// =====================================================
//                 GYRO CALIBRATION
// =====================================================

float gyroBiasX = 0;
float gyroBiasY = 0;
float gyroBiasZ = 0;


// =====================================================
//                    PACKET ID
// =====================================================

unsigned long packetID = 0;


// =====================================================
//                    TIMING
// =====================================================

unsigned long lastTelemetry = 0;

const unsigned long TELEMETRY_INTERVAL = 1000;


// =====================================================
//                 MPU FUNCTIONS
// =====================================================

uint8_t readRegister(uint8_t reg)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0)
    return 0;

  Wire.requestFrom(MPU_ADDR, (uint8_t)1);

  if (Wire.available())
    return Wire.read();

  return 0;
}


void writeRegister(uint8_t reg, uint8_t value)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}


int16_t read16(uint8_t reg)
{
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);

  if (Wire.endTransmission(false) != 0)
    return 0;

  Wire.requestFrom(MPU_ADDR, (uint8_t)2);

  if (Wire.available() < 2)
    return 0;

  uint8_t highByte = Wire.read();
  uint8_t lowByte = Wire.read();

  return (int16_t)((highByte << 8) | lowByte);
}


// =====================================================
//                 INITIALIZE MPU6500
// =====================================================

bool initializeMPU()
{
  Serial.println("===== MPU6500 =====");

  uint8_t whoAmI = readRegister(WHO_AM_I);

  Serial.print("WHO_AM_I = 0x");
  Serial.println(whoAmI, HEX);

  if (whoAmI != 0x70)
  {
    Serial.println("MPU6500-family IMU NOT detected.");
    return false;
  }

  writeRegister(PWR_MGMT_1, 0x01);
  delay(100);

  writeRegister(PWR_MGMT_2, 0x00);

  // DLPF
  writeRegister(CONFIG_REG, 0x00);

  // ±250 deg/s
  writeRegister(GYRO_CONFIG, 0x00);

  // ±2g
  writeRegister(ACCEL_CONFIG, 0x00);

  delay(100);

  Serial.println("MPU6500-family IMU OK");

  return true;
}


// =====================================================
//                 GYRO CALIBRATION
// =====================================================

void calibrateGyroscope()
{
  Serial.println();
  Serial.println("===== GYRO CALIBRATION =====");
  Serial.println("KEEP CANSAT COMPLETELY STILL");
  delay(2000);

  const int samples = 300;

  float sumX = 0;
  float sumY = 0;
  float sumZ = 0;

  for (int i = 0; i < samples; i++)
  {
    int16_t rawX = read16(GYRO_XOUT_H);
    int16_t rawY = read16(GYRO_XOUT_H + 2);
    int16_t rawZ = read16(GYRO_XOUT_H + 4);

    sumX += rawX / 131.0;
    sumY += rawY / 131.0;
    sumZ += rawZ / 131.0;

    delay(10);
  }

  gyroBiasX = sumX / samples;
  gyroBiasY = sumY / samples;
  gyroBiasZ = sumZ / samples;

  Serial.println("Gyro calibration complete.");

  Serial.print("Bias X: ");
  Serial.println(gyroBiasX, 3);

  Serial.print("Bias Y: ");
  Serial.println(gyroBiasY, 3);

  Serial.print("Bias Z: ");
  Serial.println(gyroBiasZ, 3);
}


// =====================================================
//                 BMP280
// =====================================================

bool initializeBMP()
{
  Serial.println("===== BMP280 =====");

  if (bmp.begin(0x76))
  {
    Serial.println("BMP280 found at 0x76");
    return true;
  }

  if (bmp.begin(0x77))
  {
    Serial.println("BMP280 found at 0x77");
    return true;
  }

  Serial.println("BMP280 NOT detected.");

  return false;
}


// =====================================================
//                 LORA
// =====================================================

bool initializeLoRa()
{
  Serial.println("===== LORA =====");

  SPI.begin(
    LORA_SCK,
    LORA_MISO,
    LORA_MOSI,
    LORA_SS
  );

  LoRa.setSPI(SPI);

  LoRa.setPins(
    LORA_SS,
    LORA_RST,
    LORA_DIO0
  );

  if (!LoRa.begin(LORA_FREQUENCY))
  {
    Serial.println("LoRa initialization FAILED.");
    return false;
  }

  // MUST MATCH GROUND STATION
  LoRa.setTxPower(17);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();

  Serial.println("LoRa initialized.");
  Serial.println("Frequency: 433 MHz");
  Serial.println("SF: 7");

  return true;
}


// =====================================================
//                    GPS
// =====================================================

void initializeGPS()
{
  GPS.begin(
    9600,
    SERIAL_8N1,
    GPS_RX,
    GPS_TX
  );

  Serial.println("GPS initialized.");
}


void updateGPS()
{
  while (GPS.available())
  {
    char c = GPS.read();
    gps.encode(c);
  }
}


// =====================================================
//                 BATTERY
// =====================================================

float readBatteryVoltage()
{
  int raw = analogRead(BATTERY_PIN);

  float adcVoltage =
    (raw / 4095.0) * 3.3;

  // Change this according to your resistor divider
  const float BATTERY_DIVIDER_RATIO = 2.0;

  return adcVoltage * BATTERY_DIVIDER_RATIO;
}


// =====================================================
//                 SEND TELEMETRY
// =====================================================

void sendTelemetry()
{
  // ---------------- BMP280 ----------------

  float bmpTemp = NAN;
  float pressure = NAN;
  float bmpAltitude = NAN;

  if (bmpOK)
  {
    bmpTemp = bmp.readTemperature();

    pressure =
      bmp.readPressure() / 100.0;

    bmpAltitude =
      bmp.readAltitude(1013.25);
  }


  // ---------------- DHT11 ----------------

  float dhtTemp = dht.readTemperature();
  float dhtHum = dht.readHumidity();


  // ---------------- MPU6500 ----------------

  float ax = 0;
  float ay = 0;
  float az = 0;

  float gx = 0;
  float gy = 0;
  float gz = 0;

  float imuTemp = 0;

  if (mpuOK)
  {
    int16_t rawAX =
      read16(ACCEL_XOUT_H);

    int16_t rawAY =
      read16(ACCEL_XOUT_H + 2);

    int16_t rawAZ =
      read16(ACCEL_XOUT_H + 4);

    int16_t rawTemp =
      read16(TEMP_OUT_H);

    int16_t rawGX =
      read16(GYRO_XOUT_H);

    int16_t rawGY =
      read16(GYRO_XOUT_H + 2);

    int16_t rawGZ =
      read16(GYRO_XOUT_H + 4);

    // ±2g
    ax = rawAX / 16384.0;
    ay = rawAY / 16384.0;
    az = rawAZ / 16384.0;

    // ±250 deg/s
    gx = (rawGX / 131.0) - gyroBiasX;
    gy = (rawGY / 131.0) - gyroBiasY;
    gz = (rawGZ / 131.0) - gyroBiasZ;

    imuTemp =
      (rawTemp / 333.87) + 21.0;
  }


  // ---------------- GPS ----------------

  double lat = 0;
  double lon = 0;

  float gpsAlt = 0;

  int sats = 0;

  int gpsFix = 0;

  if (gps.location.isValid())
  {
    gpsFix = 1;

    lat = gps.location.lat();
    lon = gps.location.lng();
  }

  if (gps.altitude.isValid())
    gpsAlt = gps.altitude.meters();

  if (gps.satellites.isValid())
    sats = gps.satellites.value();


  // ---------------- Battery ----------------

  float battV =
    readBatteryVoltage();


  // =================================================
  // BUILD PACKET
  // =================================================

  String packet = "";

  packet += String(packetID);
  packet += ",";

  packet += String(millis());
  packet += ",";

  packet += String(lat, 6);
  packet += ",";

  packet += String(lon, 6);
  packet += ",";

  packet += String(gpsAlt, 2);
  packet += ",";

  packet += String(sats);
  packet += ",";

  packet += String(gpsFix);
  packet += ",";

  packet += String(bmpTemp, 2);
  packet += ",";

  packet += String(pressure, 2);
  packet += ",";

  packet += String(bmpAltitude, 2);
  packet += ",";

  packet += String(dhtTemp, 2);
  packet += ",";

  packet += String(dhtHum, 2);
  packet += ",";

  packet += String(ax, 3);
  packet += ",";

  packet += String(ay, 3);
  packet += ",";

  packet += String(az, 3);
  packet += ",";

  packet += String(gx, 2);
  packet += ",";

  packet += String(gy, 2);
  packet += ",";

  packet += String(gz, 2);
  packet += ",";

  packet += String(imuTemp, 2);
  packet += ",";

  packet += String(battV, 2);


  // =================================================
  // SEND
  // =================================================

  if (loraOK)
  {
    LoRa.beginPacket();

    LoRa.print(packet);

    LoRa.endPacket();
  }


  // =================================================
  // SERIAL MONITOR
  // =================================================

  Serial.println();
  Serial.println("========== TRANSMITTING ==========");

  Serial.print("Packet ID: ");
  Serial.println(packetID);

  Serial.println(packet);

  Serial.println("==================================");


  packetID++;
}


// =====================================================
//                       SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(2000);

  Serial.println();
  Serial.println("####################################");
  Serial.println("#       CANSAT FLIGHT COMPUTER     #");
  Serial.println("#          ESP32 DEVKIT            #");
  Serial.println("####################################");


  // I2C
  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );


  // DHT
  dht.begin();


  // BMP280
  bmpOK = initializeBMP();


  // MPU6500
  mpuOK = initializeMPU();

  if (mpuOK)
  {
    calibrateGyroscope();
  }


  // GPS
  initializeGPS();


  // LoRa
  loraOK = initializeLoRa();


  // ADC
  analogReadResolution(12);


  // Status
  Serial.println();
  Serial.println("========== SYSTEM STATUS ==========");

  Serial.print("BMP280 : ");
  Serial.println(bmpOK ? "OK" : "FAILED");

  Serial.print("MPU6500: ");
  Serial.println(mpuOK ? "OK" : "FAILED");

  Serial.println("GPS    : OK");

  Serial.print("LoRa   : ");
  Serial.println(loraOK ? "OK" : "FAILED");

  Serial.println("===================================");

  Serial.println();
  Serial.println("CANSAT READY");
}


// =====================================================
//                        LOOP
// =====================================================

void loop()
{
  // GPS needs continuous processing
  updateGPS();


  // Telemetry every 1 second
  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL)
  {
    lastTelemetry = millis();

    sendTelemetry();
  }
}