# Stress-Monitor
Multi-sensor stress monitoring system using ESP32, BLE, and Python-based stress score analysis.

## Project Overview
This project demonstrates a wearable physiological stress monitoring system that integrates multiple biosensors to estimate user stress levels. The device collects heart rate, heart rate variability (HRV), blood oxygen saturation (SpO₂), skin temperature, and galvanic skin response (GSR) data using an ESP32 microcontroller.

Sensor data is transmitted via Bluetooth Low Energy (BLE) to a Python-based web application, where a literature-informed weighted stress scoring algorithm evaluates physiological stress indicators and displays a real-time stress score through an interactive dashboard.

This project was independently developed to explore wearable biosignal monitoring, sensor fusion, and real-time digital health analytics.

## Hardware and Firmware Implementation
The system was prototyped using a breadboard-based hardware platform integrating multiple physiological sensors with an ESP32 microcontroller.

The ESP32 firmware was developed using Arduino IDE and performs:
- Multi-sensor physiological data acquisition
- Discrete buffered sampling for consistent dataset generation
- Heart rate and HRV calculation using beat detection and RMSSD processing
- Physiological validation to filter unrealistic readings
- OLED display feedback to show sampling progress and measurement completion
- BLE data transmission to the web application

## Web Application & Stress Analysis
A Python-based dashboard was developed using Streamlit to visualize and analyze physiological sensor data.

The web application performs:
- BLE device scanning and connection using the Bleak library
- Real-time sensor data retrieval and parsing
- Weighted stress scoring using physiological stress indicators
- Interactive dashboard visualization of stress levels and sensor metrics

## Stress Scoring Method
Stress levels are estimated using a weighted scoring algorithm based on physiological stress markers including:
- Elevated heart rate
- Reduced heart rate variability (RMSSD)
- Skin temperature variation
- Increased galvanic skin response

These metrics are combined to generate a normalized stress score ranging from 0–100.
