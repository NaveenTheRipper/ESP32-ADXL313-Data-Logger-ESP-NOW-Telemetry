# ESP32-ADXL313-Data-Logger-ESP-NOW-Telemetry
A compact ESP32 project that samples a SparkFun ADXL313 accelerometer, timestamps readings via NTP, logs to an SD card as CSV, and broadcasts X/Y/Z over ESP-NOW—managed by three FreeRTOS tasks for sampling, scheduling, and telemetry. Designed for low-power duty cycling (Wi-Fi off between sends) and robust field logging.
