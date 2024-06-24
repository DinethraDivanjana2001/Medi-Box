#include<PubSubClient.h>
#include<WiFi.h>
#include <DHTesp.h>
#include <NTPClient.h>  // Library for NTP client to sync time
#include <WiFiUdp.h>
#include <ESP32Servo.h>

#define Buzzer 5
const int DHTPIN = 12;  // Pin for DHT sensor
#define LeftLdrInput 32   // Pin for left LDR
#define RightLdrInput 33  // Pin for right LDR
#define servoM 18         // Pin for servo motor

// Parameters related to light intensity
float Temp = 0.75;       
const float RR = 50;     // Resistance of the LDR
float offsetAngle;       // Offset angle for servo motor
float Max = 0;           // Maximum intensity value

int angle = 0;           // Variable to store angle of servo motor
float ControllingFactor; // Controlling factor of servo motor
float D;                 // Weighting factor for LDR intensity

// To initialize MQTT (PubSub library) need WiFi client
WiFiClient espClient;
PubSubClient mqttClient(espClient);
DHTesp dhtSensor;
Servo servo;

WiFiUDP ntpUDP;            // Instance of WiFiUDP for NTP
NTPClient timeClient(ntpUDP);  // NTP client for time synchronization

char tempAr[6];  // Character array to store temperature data for MQTT publishing

void setup() {
  Serial.begin(115200);
  setupWifi();
  setupMqtt();
  pinMode(Buzzer, OUTPUT);
  digitalWrite(Buzzer, LOW);

  pinMode(LeftLdrInput, INPUT);
  pinMode(RightLdrInput, INPUT);

  servo.attach(servoM, 500, 2400);  // Attaches the servo motor to the pin and specifies the pulse width range for angle control.
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);  // Setup DHT sensor
  timeClient.begin();  // Begin NTP client
  timeClient.setTimeOffset(5.5 * 3600);  // Setting 5.5 hours offset
}

void loop() {
  if (!mqttClient.connected()) {
    connectToBroker();
  }
  mqttClient.loop();  // Handle MQTT incoming messages

  updateTemperature();

  mqttClient.publish("MEDI-TEMP", tempAr);
  checkIntensity();
  delay(1000);
}

void setupWifi() {
  WiFi.begin("Wokwi-GUEST", "");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setupMqtt() {
  mqttClient.setServer("test.mosquitto.org", 1883);  // Setting MQTT server and port
  mqttClient.setCallback(receiveCallback);  // Set callback function for MQTT messages
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect("ESP32-2346829324")) {
      Serial.println("connected");
      mqttClient.subscribe("Buzzer-on-off");  // Subscribe to buzzer control topic
      mqttClient.subscribe("Minimum angle");
      mqttClient.subscribe("Controlling factor");
    } else {
      Serial.println("failed");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

void updateTemperature() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String(data.temperature, 2).toCharArray(tempAr, 6);  // Convert temperature data to char array for MQTT
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  // Print received message topic
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]");

  // Buffer to hold payload as characters
  char payloadCharAr[length];
  
  // Iterate over payload bytes, printing and storing them as characters
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]); // Print payload byte as character
    payloadCharAr[i] = (char)payload[i]; // Store payload byte as character
  }
  Serial.println(); 

  
  if (strcmp(topic, "Buzzer-on-off") == 0) {
    Buzz(payloadCharAr[0] == '1'); // Call Buzz function with true/false based on payload
  } else if (strcmp(topic, "Minimum angle") == 0) {
    // interpret payload as float and assign to offsetAngle
    offsetAngle = atof(payloadCharAr);  // Convert payload to float
    Serial.print(offsetAngle); // Print converted offsetAngle
    Serial.println(offsetAngle);
  } else if (strcmp(topic, "Controlling factor") == 0) {
    // If topic is "Controlling factor", interpret payload as float and assign to ControllingFactor
    ControllingFactor = atof(payloadCharAr);  // Convert payload to float
    // Serial.print(offsetAngle); // Print converted ControllingFactor
    // Serial.println(ControllingFactor);
  }
}


void Buzz(bool on) {
  if (on) {
    tone(Buzzer, 125);  // Activate buzzer
  } else {
    noTone(Buzzer);     // Deactivate buzzer
  }
}

unsigned long getTime() {
  timeClient.update();
  return timeClient.getEpochTime();  // Get current time in seconds
}

void checkIntensity() {
  
  float LeftIntensity1 = LdrInputToRead(analogRead(LeftLdrInput));
  float RightIntensity2 = LdrInputToRead(analogRead(RightLdrInput));
  Serial.print("left");
  Serial.println(LeftIntensity1);
  Serial.print("right");
  Serial.println(RightIntensity2);

  Max = max(LeftIntensity1, RightIntensity2);
  char Char_arr[10];  // Buffer for intensity value
  String(Max).toCharArray(Char_arr, 6);

  // Publish maximum intensity and which LDR it came from
  if (Max == LeftIntensity1) {
    D = 1.5;
    mqttClient.publish("Right_or_Left", "Left LDR");
    mqttClient.publish("MaxIntensity", Char_arr);
  } else {
    D = 0.5;
    mqttClient.publish("Right_or_Left", "Right LDR");
    mqttClient.publish("MaxIntensity", Char_arr);
  }
  Onservo();
  delay(1000);
}

// Function to convert an analog reading from a LDR to a normalized lux value
float LdrInputToRead(int value) {
  float resistance = 2000 * (value / 1024. * 5) / (1 - (value / 1024. * 5) / 5);//resistance based on the analog reading value
  float Max = pow(RR * 1e3 * pow(10, Temp) / 322.58, (1 / Temp));// maximum lux value
  return pow(RR * 1e3 * pow(10, Temp) / resistance, (1 / Temp)) / Max;// esistance to lux value and normalize
}

void Onservo() {
  angle = min((int)(offsetAngle * D + (180 - offsetAngle) * Max * ControllingFactor), 180);  // Calculate angle of servo motor
  servo.write(angle);
}
