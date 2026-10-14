# 🛰️ LoRa-Based CanSat for Real-Time Atmospheric Weather Monitoring and Machine Learning-Based Weather Prediction

A CanSat-based atmospheric monitoring and machine learning research project developed as part of my undergraduate thesis. The system integrates embedded sensing, LoRa wireless communication, a ground station, real-time telemetry logging, and machine learning-based weather prediction.

## 📌 Project Overview

**Thesis Title:** *Design and Development of LoRa-Based CanSat for Real-Time Atmospheric Weather Monitoring and Machine-Based Weather Prediction*

The project aims to develop a compact CanSat platform capable of collecting atmospheric and environmental data, transmitting telemetry wirelessly to a ground station, and integrating machine learning models for weather prediction.

The system combines an ESP32-based flight computer with environmental sensors, an inertial measurement unit (IMU), a GPS receiver, and an SX1278 LoRa transceiver. At the ground station, an ESP32-S3 receives telemetry and forwards the data to a computer for logging, visualization, and integration with machine learning models.

The project explores how low-power wireless telemetry and data-driven prediction can be combined in a compact atmospheric monitoring platform.

## 🎯 Objectives

- Design and develop an ESP32-based CanSat flight computer.
- Collect atmospheric and environmental data using onboard sensors.
- Establish wireless telemetry using LoRa communication.
- Develop a ground station for receiving and monitoring telemetry.
- Log flight and sensor data for analysis and visualization.
- Integrate machine learning models for weather prediction.
- Explore the comparison of model predictions with real-world sensor observations.

## ⚙️ System Architecture

```text
        CAN SAT FLIGHT UNIT
                |
     +----------+-----------+
     |          |           |
   BMP280     DHT11       MPU6500
     |          |           |
     +----------+-----------+
                |
         ESP32 Flight Computer
                |
          SX1278 LoRa
          433 MHz Link
                |
          SX1278 LoRa
                |
          ESP32-S3 Ground Station
                |
            USB Serial
                |
          Python Ground Software
                |
      +---------+----------+
      |         |          |
   Telemetry   Live      ML-Based
     Logs    Dashboard   Prediction
      |                    |
      +---------+----------+
                |
       Performance Analysis
```

*Note: GPS and battery-voltage monitoring are also integrated into the flight telemetry system. The diagram illustrates the primary data flow.*

## 🔩 Hardware Components

| Component                       | Purpose                                                  |
| ------------------------------- | -------------------------------------------------------- |
| ESP32 DevKit                    | Flight computer and sensor data acquisition              |
| ESP32-S3                        | Ground-station controller                                |
| BMP280                          | Atmospheric pressure and temperature measurement         |
| DHT11                           | Temperature and relative humidity measurement            |
| MPU6500-family IMU              | Acceleration and angular velocity measurement            |
| NEO-6M GPS                      | Position, altitude, satellite count, and fix information |
| SX1278 LoRa modules             | Wireless telemetry at 433 MHz                            |
| Battery voltage sensing circuit | Monitoring the flight system's battery voltage           |

## 💻 Software and Technologies

- **Embedded programming:** C/C++ with Arduino framework
- **Microcontrollers:** ESP32 and ESP32-S3
- **Wireless communication:** LoRa, SX1278
- **Sensor interfaces:** I²C, UART, SPI, ADC
- **Ground-station software:** Python
- **Data logging:** CSV and Excel (`openpyxl`)
- **Data visualization:** Matplotlib
- **Machine Learning:** LSTM and Random Forest
- **Data processing:** Pandas and NumPy

## 📡 Real-Time Telemetry

The flight computer collects sensor readings and transmits telemetry packets to the ground station through LoRa.

The telemetry packet includes:

- Packet ID and elapsed time
- GPS latitude, longitude, altitude, satellite count, and fix status
- BMP280 temperature, atmospheric pressure, and calculated altitude
- DHT11 temperature and relative humidity
- IMU acceleration and gyroscope measurements
- IMU temperature
- Battery voltage
- Received Signal Strength Indicator (RSSI), added at the ground station

The ground-station software receives the data over USB serial and can store the records for further analysis.

## 🤖 Machine Learning Integration

The project integrates machine learning with the atmospheric monitoring workflow using historical weather data.

The planned workflow consists of:

1. **Historical data processing:** Prepare historical weather records for model input.
2. **Weather prediction:** Use an LSTM model for temperature and humidity prediction and a Random Forest model for weather-condition classification, subject to the trained models and their supported outputs.
3. **Real-time monitoring:** Collect environmental measurements through the CanSat sensors.
4. **Prediction comparison:** Compare model outputs with appropriate sensor observations where the prediction target, time, and measurement conditions are compatible.
5. **Performance evaluation:** Calculate evaluation metrics such as Mean Absolute Error (MAE), Root Mean Squared Error (RMSE), and maximum absolute error when valid paired predictions and observations are available.

**Research consideration:** Historical daily weather predictions and short-duration CanSat measurements may represent different time scales and measurement conditions. Meaningful validation therefore requires aligning the prediction target, observation period, and sensor measurements. The project does not assume that collecting telemetry alone establishes prediction accuracy.

## 📊 Data Logging and Analysis

The Python ground-station application is designed to support:

- Receiving and parsing telemetry packets
- Recording sensor observations
- Exporting telemetry to CSV or Excel
- Visualizing environmental readings and telemetry signal strength
- Comparing eligible machine learning predictions with observations
- Summarizing prediction errors using standard evaluation metrics

Actual results and performance metrics should be reported after the corresponding experiments and validation have been completed.

## 🚀 Getting Started

### 1. Clone the repository

```bash
git clone https://github.com/YOUR-USERNAME/LoRa-Based-CanSat-Weather-Monitoring.git
cd LoRa-Based-CanSat-Weather-Monitoring
```

Replace `YOUR-USERNAME` with your GitHub username.

### 2. Install Python dependencies

```bash
pip install pyserial pandas numpy openpyxl matplotlib scikit-learn
```

If the LSTM model uses TensorFlow/Keras, install a compatible TensorFlow version for your Python environment.

### 3. Configure the ground station

Update the serial port and baud rate in the Python configuration:

```python
SERIAL_PORT = "COM4"
BAUD = 115200
```

Change `COM4` to the port assigned to your ground-station ESP32-S3.

### 4. Connect the hardware

Connect the flight computer sensors and LoRa module according to the wiring configuration documented in the source code. Configure the ground-station LoRa pins to match the ESP32-S3 board and wiring.

Ensure both LoRa modules use compatible frequency, spreading factor, bandwidth, coding rate, and CRC settings.

### 5. Run the ground-station application

```bash
python cansat_ml_ground_station.py
```

The exact command may differ depending on the Python script included in the repository.

### 6. Configure the machine learning models

Place the required trained model files and preprocessing artifacts in the configured locations. Ensure the feature names, feature order, scaling procedures, and input sequence length match the original model training pipeline.

**Important:** The ground-station application requires the appropriate model artifacts and historical weather dataset for ML prediction. Telemetry collection may be tested independently of the ML component.

## 📁 Suggested Repository Structure

```text
LoRa-Based-CanSat-Weather-Monitoring/
├── firmware/
│   ├── flight_computer/
│   └── ground_station/
├── software/
│   └── cansat_ml_ground_station.py
├── machine_learning/
│   ├── thesis_canSAT.ipynb
│   └── README.md
├── datasets/
│   └── README.md
├── docs/
│   ├── system_architecture.png
│   └── hardware_setup.png
├── requirements.txt
├── .gitignore
└── README.md
```

This is a suggested structure; adjust it to match the files actually included in the repository.

## 🔬 Research Significance

This project explores the integration of low-power wireless communication, embedded environmental sensing, and machine learning in a compact atmospheric monitoring platform. It provides a practical foundation for studying real-time telemetry acquisition, sensor data analysis, and the challenges of validating data-driven weather predictions against field observations.

## 👨‍💻 Author

**Md. Mursalin Hossain Misat**

BSc in IoT and Robotics Engineering
University of Frontier Technology, Bangladesh

- **LinkedIn:** [https://www.linkedin.com/in/mursalin-hossain-612337250](https://www.linkedin.com/in/mursalin-hossain-612337250)
- **Google Scholar:** [https://scholar.google.com/citations?user=cKDnq9kAAAAJ&hl=en](https://scholar.google.com/citations?user=cKDnq9kAAAAJ\&hl=en)

## 🎓 Thesis Supervision

**Supervisor:** Md. Ashiquessalehin

Department of IoT and Robotics Engineering
University of Frontier Technology, Bangladesh

## 🙏 Acknowledgements

I sincerely thank my thesis supervisor for her guidance, constructive feedback, and support throughout the research, development, testing, and thesis defense.

## 📄 License

This repository does not specify a license yet. If you want others to reuse your code, select an appropriate open-source license and add its license file. Otherwise, leave the repository without a license until you decide how the project should be shared.

---

*Developed as an undergraduate thesis project exploring CanSat technology, LoRa-based telemetry, atmospheric monitoring, and machine learning-based weather prediction.*
