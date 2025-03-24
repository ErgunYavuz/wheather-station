Overview

This project is a complete IoT weather station solution that collects temperature and humidity data from a DHT22 sensor using custom made driver hooked to an ESP32 microcontroller, transmits the data via MQTT to a backend service, stores it in a PostgreSQL database, and provides a REST API to access the collected data.

Components
1. ESP32 Firmware (espressif/main)

    Reads temperature and humidity data from DHT22 sensor

    Connects to WiFi network

    Publishes sensor data to MQTT broker every 2 seconds

    Files:

        dht.c/dht.h: DHT22 sensor driver

        main.c: Main application logic

2. Backend Service (Weather_station/backend)

    Subscribes to MQTT topics

    Stores received sensor data in PostgreSQL

    REST API endpoints:

        /data/latest: Get latest sensor reading

        /data/history: Get last 100 sensor readings

3. Database (Weather_station/db)

    PostgreSQL database

    Stores temperature, humidity, and timestamp
