#include <WiFi.h>
#include <esp_wifi.h>
#include <driver/twai.h>

#define TX_PIN GPIO_NUM_5
#define RX_PIN GPIO_NUM_4

const char* ssid = "Esp32_S3";
const char* password = "12345678";
const int port = 5555;

IPAddress local_IP(192, 168, 1, 2);
WiFiUDP udp;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  WiFi.config(local_IP);

  // Configure AP as FTM Responder
  wifi_config_t ap_config;
  esp_wifi_get_config(WIFI_IF_AP, &ap_config);
  ap_config.ap.max_connection = 1;
  ap_config.ap.ftm_responder = 1;
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);

  Serial.println("AP started: " + String(ssid));
  udp.begin(port);
  Twai_Config();
}

void loop() {
  readNotification();
}

// Listens for UDP notifications from the FTM Initiator
void readNotification() {
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char incomingPacket[255];
    int len = udp.read(incomingPacket, 255);
    if (len > 0) {
      incomingPacket[len] = 0;
      Serial.println(incomingPacket);
    }
    if (String(incomingPacket) == "MEMBER IS OUT OF RANGE") {
      Serial.println("OUT OF RANGE NOTIFICATION RECEIVED");
      // Handle notification — e.g. trigger speed limiter or alert
    }
  }
}

// Sets up TWAI (CAN bus) driver
void Twai_Config() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("TWAI driver installed");
  } else {
    Serial.println("Failed to install TWAI driver");
    return;
  }

  if (twai_start() == ESP_OK) {
    Serial.println("TWAI driver started");
  } else {
    Serial.println("Failed to start TWAI driver");
  }
}

// Reads an incoming CAN message
void ReadCan() {
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.print("Received CAN message with ID: ");
    Serial.println(message.identifier, HEX);
    Serial.print("Data: ");
    for (int i = 0; i < message.data_length_code; i++) {
      Serial.print(message.data[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  } else {
    Serial.println("Failed to read CAN message");
  }
}
