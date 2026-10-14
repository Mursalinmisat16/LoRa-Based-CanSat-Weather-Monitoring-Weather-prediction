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

// ---------------- I2C ----------------
#define I2C_SDA 21
#define I2C_SCL 22

// ---------------- DHT11 ----------------
#define DHT_PIN 4
#define DHT_TYPE DHT11

// ---------------- GPS ----------------
#define GPS_RX 16
#define GPS_TX 17

// ---------------- LoRa SX1278 ----------------
#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  26

#define LORA_FREQUENCY 433E6

// ---------------- Battery ----------------
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
//                 SENSOR STATUS FLAGS
// =====================================================

bool bmpOK = false;
bool loraOK = false;
bool mpuOK = false;


// =====================================================
//                 MPU CALIBRATION
// =====================================================

// Gyroscope bias
float gyroBiasX = 0.0;
float gyroBiasY = 0.0;
float gyroBiasZ = 0.0;


// =====================================================
//                 TIMING
// =====================================================

unsigned long lastTelemetry = 0;
unsigned long lastSerial = 0;

const unsigned long TELEMETRY_INTERVAL = 1000;


// =====================================================
//                 I2C REGISTER FUNCTIONS
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
  uint8_t lowByte  = Wire.read();

  return (int16_t)((highByte << 8) | lowByte);
}


// =====================================================
//                 INITIALIZE MPU6500
// =====================================================

bool initializeMPU()
{
  Serial.println();
  Serial.println("===== MPU6500 INITIALIZATION =====");

  uint8_t whoAmI = readRegister(WHO_AM_I);

  Serial.print("WHO_AM_I = 0x");
  Serial.println(whoAmI, HEX);

  if (whoAmI != 0x70)
  {
    Serial.println("[ERROR] MPU6500-family device not detected.");
    return false;
  }

  // Wake sensor
  writeRegister(PWR_MGMT_1, 0x01);

  delay(100);

  // Enable all axes
  writeRegister(PWR_MGMT_2, 0x00);

  // Normal DLPF
  writeRegister(CONFIG_REG, 0x00);

  // Gyro ±250 deg/s
  writeRegister(GYRO_CONFIG, 0x00);

  // Accelerometer ±2g
  writeRegister(ACCEL_CONFIG, 0x00);

  delay(100);

  Serial.println("MPU6500-family IMU detected.");
  Serial.println("Gyro range : +/-250 deg/s");
  Serial.println("Accel range: +/-2 g");

  return true;
}


// =====================================================
//                 GYRO CALIBRATION
// =====================================================

void calibrateGyroscope()
{
  Serial.println();
  Serial.println("===== GYROSCOPE CALIBRATION =====");
  Serial.println("Keep the CanSat completely STILL.");

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

  Serial.println("Calibration complete.");

  Serial.print("Gyro bias X: ");
  Serial.println(gyroBiasX, 3);

  Serial.print("Gyro bias Y: ");
  Serial.println(gyroBiasY, 3);

  Serial.print("Gyro bias Z: ");
  Serial.println(gyroBiasZ, 3);
}


// =====================================================
//                 BMP280 INITIALIZATION
// =====================================================

bool initializeBMP()
{
  Serial.println();
  Serial.println("===== BMP280 INITIALIZATION =====");

  // Try 0x76
  if (bmp.begin(0x76))
  {
    Serial.println("BMP280 found at 0x76");
    return true;
  }

  // Try 0x77
  if (bmp.begin(0x77))
  {
    Serial.println("BMP280 found at 0x77");
    return true;
  }

  Serial.println("[ERROR] BMP280 not detected.");

  return false;
}


// =====================================================
//                 LORA INITIALIZATION
// =====================================================

bool initializeLoRa()
{
  Serial.println();
  Serial.println("===== LORA INITIALIZATION =====");

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
    Serial.println("[ERROR] LoRa initialization failed.");
    return false;
  }

  // Same settings as your working LoRa test
  LoRa.setTxPower(17);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();

  Serial.println("LoRa initialized successfully.");
  Serial.println("Frequency: 433 MHz");

  return true;
}


// =====================================================
//                 GPS INITIALIZATION
// =====================================================

void initializeGPS()
{
  Serial.println();
  Serial.println("===== GPS INITIALIZATION =====");

  GPS.begin(
    9600,
    SERIAL_8N1,
    GPS_RX,
    GPS_TX
  );

  Serial.println("GPS UART started.");
  Serial.println("GPS RX = GPIO16");
  Serial.println("GPS TX = GPIO17");
}


// =====================================================
//                 READ GPS
// =====================================================

void updateGPS()
{
  while (GPS.available())
  {
    char c = GPS.read();
    gps.encode(c);
  }
}


// =====================================================
//                 BATTERY READING
// =====================================================

float readBatteryVoltage()
{
  int raw = analogRead(BATTERY_PIN);

  float adcVoltage =
      (raw / 4095.0) * 3.3;

  /*
   * IMPORTANT:
   *
   * This assumes a 1:1 voltage divider:
   *
   * Battery ---- R1 ----+---- R2 ---- GND
   *                     |
   *                   GPIO34
   *
   * where R1 = R2.
   *
   * If your voltage divider uses different resistor
   * values, change BATTERY_DIVIDER_RATIO.
   */

  const float BATTERY_DIVIDER_RATIO = 2.0;

  float batteryVoltage =
      adcVoltage * BATTERY_DIVIDER_RATIO;

  return batteryVoltage;
}


// =====================================================
//                 SEND LORA TELEMETRY
// =====================================================

void sendTelemetry()
{
  // ---------------------------------------------------
  // BMP280
  // ---------------------------------------------------

  float bmpTemp = NAN;
  float pressure = NAN;
  float altitude = NAN;

  if (bmpOK)
  {
    bmpTemp = bmp.readTemperature();

    pressure = bmp.readPressure() / 100.0;

    altitude = bmp.readAltitude(1013.25);
  }


  // ---------------------------------------------------
  // DHT11
  // ---------------------------------------------------

  float humidity = dht.readHumidity();
  float dhtTemp = dht.readTemperature();


  // ---------------------------------------------------
  // MPU6500
  // ---------------------------------------------------

  float gyroX = 0;
  float gyroY = 0;
  float gyroZ = 0;

  float accelX = 0;
  float accelY = 0;
  float accelZ = 0;

  float imuTemp = 0;

  if (mpuOK)
  {
    int16_t rawAX = read16(ACCEL_XOUT_H);
    int16_t rawAY = read16(ACCEL_XOUT_H + 2);
    int16_t rawAZ = read16(ACCEL_XOUT_H + 4);

    int16_t rawTemp = read16(TEMP_OUT_H);

    int16_t rawGX = read16(GYRO_XOUT_H);
    int16_t rawGY = read16(GYRO_XOUT_H + 2);
    int16_t rawGZ = read16(GYRO_XOUT_H + 4);

    // Accelerometer ±2g
    accelX = rawAX / 16384.0;
    accelY = rawAY / 16384.0;
    accelZ = rawAZ / 16384.0;

    // Gyroscope ±250 deg/s
    gyroX = (rawGX / 131.0) - gyroBiasX;
    gyroY = (rawGY / 131.0) - gyroBiasY;
    gyroZ = (rawGZ / 131.0) - gyroBiasZ;

    // MPU6500 temperature
    imuTemp = (rawTemp / 333.87) + 21.0;
  }


  // ---------------------------------------------------
  // GPS
  // ---------------------------------------------------

  double latitude = 0.0;
  double longitude = 0.0;

  float gpsAltitude = 0;
  float gpsSpeed = 0;
  float gpsCourse = 0;

  int satellites = 0;

  bool gpsFix = false;

  if (gps.location.isValid())
  {
    gpsFix = true;

    latitude = gps.location.lat();
    longitude = gps.location.lng();
  }

  if (gps.altitude.isValid())
    gpsAltitude = gps.altitude.meters();

  if (gps.speed.isValid())
    gpsSpeed = gps.speed.kmph();

  if (gps.course.isValid())
    gpsCourse = gps.course.deg();

  if (gps.satellites.isValid())
    satellites = gps.satellites.value();


  // ---------------------------------------------------
  // Battery
  // ---------------------------------------------------

  float batteryVoltage = readBatteryVoltage();


  // ---------------------------------------------------
  // Build telemetry packet
  // ---------------------------------------------------

  String packet = "";

  packet += String(millis());
  packet += ",";

  // GPS
  packet += String(latitude, 6);
  packet += ",";
  packet += String(longitude, 6);
  packet += ",";
  packet += String(gpsAltitude, 2);
  packet += ",";
  packet += String(satellites);
  packet += ",";
  packet += String(gpsFix ? 1 : 0);
  packet += ",";

  // BMP280
  packet += String(bmpTemp, 2);
  packet += ",";
  packet += String(pressure, 2);
  packet += ",";
  packet += String(altitude, 2);
  packet += ",";

  // DHT11
  packet += String(dhtTemp, 2);
  packet += ",";
  packet += String(humidity, 2);
  packet += ",";

  // MPU6500 acceleration
  packet += String(accelX, 3);
  packet += ",";
  packet += String(accelY, 3);
  packet += ",";
  packet += String(accelZ, 3);
  packet += ",";

  // MPU6500 gyro
  packet += String(gyroX, 2);
  packet += ",";
  packet += String(gyroY, 2);
  packet += ",";
  packet += String(gyroZ, 2);
  packet += ",";

  // MPU temperature
  packet += String(imuTemp, 2);
  packet += ",";

  // Battery
  packet += String(batteryVoltage, 2);


  // ---------------------------------------------------
  // Send LoRa
  // ---------------------------------------------------

  if (loraOK)
  {
    LoRa.beginPacket();
    LoRa.print(packet);
    LoRa.endPacket();
  }


  // ---------------------------------------------------
  // Serial output
  // ---------------------------------------------------

  Serial.println();
  Serial.println("========== CANSAT TELEMETRY ==========");

  Serial.print("GPS Fix      : ");
  Serial.println(gpsFix ? "YES" : "NO");

  Serial.print("Latitude     : ");
  Serial.println(latitude, 6);

  Serial.print("Longitude    : ");
  Serial.println(longitude, 6);

  Serial.print("GPS Altitude : ");
  Serial.print(gpsAltitude, 2);
  Serial.println(" m");

  Serial.print("Satellites   : ");
  Serial.println(satellites);

  Serial.print("BMP Temp     : ");
  Serial.print(bmpTemp, 2);
  Serial.println(" C");

  Serial.print("Pressure     : ");
  Serial.print(pressure, 2);
  Serial.println(" hPa");

  Serial.print("BMP Altitude : ");
  Serial.print(altitude, 2);
  Serial.println(" m");

  Serial.print("DHT Temp     : ");
  Serial.print(dhtTemp, 2);
  Serial.println(" C");

  Serial.print("Humidity     : ");
  Serial.print(humidity, 2);
  Serial.println(" %");

  Serial.println();

  Serial.print("Accel X      : ");
  Serial.print(accelX, 3);
  Serial.println(" g");

  Serial.print("Accel Y      : ");
  Serial.print(accelY, 3);
  Serial.println(" g");

  Serial.print("Accel Z      : ");
  Serial.print(accelZ, 3);
  Serial.println(" g");

  Serial.println();

  Serial.print("Gyro X       : ");
  Serial.print(gyroX, 2);
  Serial.println(" deg/s");

  Serial.print("Gyro Y       : ");
  Serial.print(gyroY, 2);
  Serial.println(" deg/s");

  Serial.print("Gyro Z       : ");
  Serial.print(gyroZ, 2);
  Serial.println(" deg/s");

  Serial.print("IMU Temp     : ");
  Serial.print(imuTemp, 2);
  Serial.println(" C");

  Serial.println();

  Serial.print("Battery      : ");
  Serial.print(batteryVoltage, 2);
  Serial.println(" V");

  Serial.println();

  Serial.print("LoRa Packet  : ");
  Serial.println(packet);

  Serial.println("=======================================");
}


// =====================================================
//                       SETUP
// =====================================================

void setup()
{
  Serial.begin(115200);

  delay(2000);

  Serial.println();
  Serial.println("########################################");
  Serial.println("#      CANSAT FLIGHT COMPUTER          #");
  Serial.println("#          ESP32 DEVKIT                 #");
  Serial.println("########################################");

  // ---------------------------------------------------
  // I2C
  // ---------------------------------------------------

  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );

  Serial.println();
  Serial.println("I2C started.");
  Serial.println("SDA = GPIO21");
  Serial.println("SCL = GPIO22");


  // ---------------------------------------------------
  // DHT11
  // ---------------------------------------------------

  dht.begin();

  Serial.println("DHT11 initialized.");


  // ---------------------------------------------------
  // BMP280
  // ---------------------------------------------------

  bmpOK = initializeBMP();


  // ---------------------------------------------------
  // MPU6500
  // ---------------------------------------------------

  mpuOK = initializeMPU();

  if (mpuOK)
  {
    calibrateGyroscope();
  }


  // ---------------------------------------------------
  // GPS
  // ---------------------------------------------------

  initializeGPS();


  // ---------------------------------------------------
  // LoRa
  // ---------------------------------------------------

  loraOK = initializeLoRa();


  // ---------------------------------------------------
  // Battery ADC
  // ---------------------------------------------------

  analogReadResolution(12);

  Serial.println();
  Serial.println("===== SYSTEM STATUS =====");

  Serial.print("BMP280 : ");
  Serial.println(bmpOK ? "OK" : "FAILED");

  Serial.print("MPU6500: ");
  Serial.println(mpuOK ? "OK" : "FAILED");

  Serial.print("GPS    : OK");

  Serial.println();

  Serial.print("LoRa   : ");
  Serial.println(loraOK ? "OK" : "FAILED");

  Serial.println("=========================");

  Serial.println();
  Serial.println("CANSAT FLIGHT COMPUTER READY");
  Serial.println();
}


// =====================================================
//                        LOOP
// =====================================================

void loop()
{
  // GPS must be read continuously
  updateGPS();


  // Send telemetry every 1 second
  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL)
  {
    lastTelemetry = millis();

    sendTelemetry();
  }
}