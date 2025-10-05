# Wireless data logger with ESP-NOW-Telemetry

A compact ESP32 project that samples a SparkFun ADXL313 accelerometer, timestamps readings via NTP, logs to an SD card as CSV, and broadcasts X/Y/Z over ESP-NOW—managed by three FreeRTOS tasks for sampling, scheduling, and telemetry. Designed for low-power duty cycling (Wi-Fi off between sends) and robust field logging.


-- Objective

Develop a low-power, multi-tasking system that continuously records 3-axis acceleration data with timestamps to an SD card and transmits telemetry packets wirelessly via ESP-NOW.

-- Key Features

Real-time accelerometer sampling (X, Y, Z)

NTP-synchronized timekeeping with timezone offsets

Daily CSV log creation (Date,X,Y,Z)

Wireless telemetry broadcast {id, x, y, z} over ESP-NOW

FreeRTOS task management for parallel operation

Automatic duty cycling (Wi-Fi off when idle)

Configurable suspend/resume schedule and self-restart

--Technical Highlights

ADXL313 configured for ±2g range, 50 Hz bandwidth, full resolution

SdFat library for efficient CSV logging

ESP-NOW peer-to-peer communication without router dependency

FreeRTOS tasks pinned to cores for deterministic scheduling

Low-power design: disables Wi-Fi between sends, supports autosleep

--Outputs

Daily CSV logs on SD card

Real-time telemetry packets to paired ESP32
