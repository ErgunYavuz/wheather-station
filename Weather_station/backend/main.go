package main

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"

	mqtt "github.com/eclipse/paho.mqtt.golang"
	_ "github.com/lib/pq"
)

// SensorData represents the incoming sensor data
type SensorData struct {
	Temperature float64 `json:"temperature"`
	Humidity    float64 `json:"humidity"`
	Timestamp   string  `json:"timestamp"`
}

// Database connection
var db *sql.DB

// MQTT message handler
var messageHandler mqtt.MessageHandler = func(client mqtt.Client, msg mqtt.Message) {
	log.Println("Received MQTT message:", string(msg.Payload()))

	var data SensorData
	err := json.Unmarshal(msg.Payload(), &data)
	if err != nil {
		log.Println("Error parsing JSON:", err)
		return
	}

	// Store data in PostgreSQL
	_, err = db.Exec("INSERT INTO sensor_data (temperature, humidity) VALUES ($1, $2)", data.Temperature, data.Humidity)
	if err != nil {
		log.Println("Failed to insert into database:", err)
	} else {
		log.Println("Sensor data inserted successfully")
	}
}

// Connect to database
func connectDB() {
	dbURL := fmt.Sprintf("host=%s user=%s password=%s dbname=%s sslmode=disable",
		os.Getenv("DB_HOST"), os.Getenv("DB_USER"), os.Getenv("DB_PASSWORD"), os.Getenv("DB_NAME"))

	var err error
	db, err = sql.Open("postgres", dbURL)
	if err != nil {
		log.Fatal("Error connecting to database:", err)
	}

	err = db.Ping()
	if err != nil {
		log.Fatal("Database is unreachable:", err)
	}

	log.Println("Connected to PostgreSQL database")
}

// Start MQTT client
func startMQTT() {
	opts := mqtt.NewClientOptions().AddBroker("tcp://mqtt-broker:1883")
	client := mqtt.NewClient(opts)

	if token := client.Connect(); token.Wait() && token.Error() != nil {
		log.Fatal("MQTT connection error:", token.Error())
	}

	log.Println("Connected to MQTT broker")
	client.Subscribe("sensor/data", 0, messageHandler)
}

// Get latest sensor data from database
func getLatestSensorData(w http.ResponseWriter, r *http.Request) {
	row := db.QueryRow("SELECT temperature, humidity, timestamp FROM sensor_data ORDER BY timestamp DESC LIMIT 1")

	var data SensorData
	err := row.Scan(&data.Temperature, &data.Humidity, &data.Timestamp)
	if err != nil {
		http.Error(w, "Failed to fetch latest data", http.StatusInternalServerError)
		return
	}

	json.NewEncoder(w).Encode(data)
}

// Get sensor history from database
func getSensorHistory(w http.ResponseWriter, r *http.Request) {
	rows, err := db.Query("SELECT temperature, humidity, timestamp FROM sensor_data ORDER BY timestamp DESC LIMIT 100")
	if err != nil {
		http.Error(w, "Failed to fetch history", http.StatusInternalServerError)
		return
	}
	defer rows.Close()

	var history []SensorData
	for rows.Next() {
		var data SensorData
		rows.Scan(&data.Temperature, &data.Humidity, &data.Timestamp)
		history = append(history, data)
	}

	json.NewEncoder(w).Encode(history)
}

// Setup API server
func startServer() {
	http.HandleFunc("/data/latest", getLatestSensorData)
	http.HandleFunc("/data/history", getSensorHistory)

	log.Println("Server started on port 5000")
	log.Fatal(http.ListenAndServe(":5000", nil))
}

func main() {
	connectDB()
	startMQTT()
	startServer()
}
