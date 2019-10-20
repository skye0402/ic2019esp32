*****************************************************
* Decawave DWM1001 to MQTT via ESP32
*****************************************************

#include <WiFi.h>
#include <PubSubClient.h>
#include <HardwareSerial.h>

// Connection settings for the WiFi
const char* ssid = "Your WiFi's SSID";
const char* password = "Your WiFiPassword goes here";

// Create the client for WiFi
WiFiClient espClient;

// Connection settings to the MQTT Broker
const char* mqtt_server = "MQTT URL or IP Address";
// Create the MQTT Client
PubSubClient client(espClient);

// Variables for output of position
const int bufferSize = 128;
char inputBuffer[bufferSize];
int bufferPointer = 0;
char inByte;

// Connection settings for the serial
HardwareSerial SerialRTLS(2);
#define RXD2 16
#define TXD2 17

void setup_wifi() {
  // Connect to WiFi network as we need an internet connection
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup_gateway() {
  char inByte;
  
  // Open the serial connection to the decaWave Gateway
  Serial.println("");
  Serial.println("Connecting to DecaWave Gateway");

  // send command to start streaming positions
  SerialRTLS.print("\r\r");
  delay(2000);
  if (SerialRTLS.available() >= 1) {    
    while (SerialRTLS.available()) {  
      inByte = char(SerialRTLS.read());
      Serial.print(inByte);
    }
  }
  Serial.println();
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Command arrived [");
  Serial.print(topic);
  Serial.print("] ");

  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    SerialRTLS.write(payload[i]); // added
  }
  //SerialRTLS.println();
  Serial.println(); 
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("rtls/ESP32Status", "Connected to MQTT Server.");
      // ... and resubscribe
      client.subscribe("rtls/Command");
      client.publish("rtls/Info", "Send commands by topic rtls/Command.");
    } 
    else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output

  Serial.begin(115200);
  SerialRTLS.begin(115200); // Open serial for RTLS connection

  randomSeed(analogRead(0));
  
  setup_wifi();
  setup_gateway();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  // The MQTT Client call
  client.loop();

  // Send the tag positions to the serial output
  if (SerialRTLS.available() >= 1) {
    // POS,0,D113,-0.01,-0.58,-0.04,80,x01
    while (SerialRTLS.available()) {
      inByte = char(SerialRTLS.read());
      if (inByte == '\n') {
        // 'newline' character
        inputBuffer[bufferPointer++] = '\0';
        client.publish("rtls/StreamPos", inputBuffer);
        Serial.println(inputBuffer);
        bufferPointer = 0;
      }
      else {
        // not a 'newline' character
        if ( bufferPointer < bufferSize - 1 )  // Leave room for a null terminator
          inputBuffer[bufferPointer++] = inByte;
      }
    }    
  }
}
