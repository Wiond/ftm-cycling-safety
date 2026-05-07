#include <WiFi.h>
#include <esp_wifi.h>
#include "driver/twai.h"
#include <SimpleKalmanFilter.h>
#include <WiFiUdp.h>

#define standardFrame false
#define extendedFrame true

#define RX_PIN    GPIO_NUM_4
#define TX_PIN    GPIO_NUM_5
#define LED_PIN   GPIO_NUM_6

const char* ssid     = "Esp32_S3";
const char* password = "12345678";
const int   receiverPort = 5555;
const char* ip       = "192.168.1.2";

// ── Tunable parameters ────────────────────────────────────────────────────────
int samplesize = 100;                      // Number of FTM samples to collect
const unsigned long interval = 1000;       // Delay (ms) between FTM sessions
SimpleKalmanFilter kalmanFilter(2, 2, 0.5);// e_mea, e_est, q
const float desiredRange = 0;              // Boundary distance in metres
const float offset = 5.5;                  // System bias correction (metres)
// ─────────────────────────────────────────────────────────────────────────────

unsigned long startTime;
unsigned long endTime;
float filteredVal;
int loopcount = 0;

WiFiUDP udp;

const uint8_t disableMotorID = 0xAA;
uint8_t canMsg[] = {0x02, 0x3E, 0x80, 0xAA, 0xAD, 0xAB, 0xAA, 0xAD};

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(2000);
    Serial.println("Connecting to AP...");
  }
  Serial.println("Connected to AP");

  // Register FTM event handler
  esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_FTM_REPORT,
                              &Ftm_report_handler, NULL);
  Twai_Setup();
}

void loop() {
  // ── Sampling mode: collect N samples then stop ───────────────────────────
  while (loopcount < samplesize) {
    StartFTM();
    delay(interval);
    loopcount++;
  }
  Serial.println("Sampling complete (" + String(samplesize) + " samples).");
  delay(INT_MAX);

  // ── Continuous mode: uncomment below and comment out the block above ──────
  // StartFTM();
  // delay(interval);
}

// Initiates a single FTM ranging session
void StartFTM() {
  wifi_ftm_initiator_cfg_t ftm_cfg = {
    .channel     = 1,
    .frm_count   = 32,   // Higher = more accurate but slower
    .burst_period = 0,   // 0 = ASAP mode
  };

  esp_err_t err = esp_wifi_ftm_initiate_session(&ftm_cfg);
  if (err != ESP_OK) {
    Serial.println("FTM session initiation failed");
  }
}

// Handles FTM report events (called asynchronously)
void Ftm_report_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data) {
  wifi_event_ftm_report_t* report = (wifi_event_ftm_report_t*)event_data;

  if (report->status == 0) {
    // Apply Kalman filter and bias correction
    filteredVal   = kalmanFilter.updateEstimate((float)report->dist_est) / 100.0f;
    float adjusted = filteredVal - offset;

    Serial.printf("Raw distance:      %.2f m\n", (float)report->dist_est / 100.0f);
    Serial.printf("RTT:               %.5f ns\n", (float)report->rtt_raw);
    Serial.printf("Filtered distance: %.2f m\n", adjusted);

    if (adjusted > desiredRange + 1.0f) {  // +1 m threshold
      Serial.printf("⚠  Boundary crossing detected at: %.2f m\n", adjusted);
      SendOutofRangeNotification();
      WriteCan(disableMotorID, canMsg, standardFrame);
      LedOn();
    } else {
      LedOff();
    }

    // Free FTM report memory to prevent heap exhaustion
    if (report->ftm_report_data != NULL) {
      free(report->ftm_report_data);
    }

  } else {
    Serial.printf("FTM measurement failed — status: %d\n", report->status);
  }
}

// Sends UDP notification to the responder device
void SendOutofRangeNotification() {
  String msg = "MEMBER IS OUT OF RANGE";
  udp.beginPacket(ip, receiverPort);
  udp.print(msg);
  Serial.println("UDP notification sent to " + String(ip));
  if (udp.endPacket() == 0) {
    Serial.println("Failed to send UDP packet");
  }
}

// Configures and starts the TWAI (CAN bus) driver
void Twai_Setup() {
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

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

// Sends a CAN message over the TWAI bus
void WriteCan(uint32_t id, uint8_t data[], bool extended) {
  twai_message_t message;
  message.identifier       = id;
  message.extd             = extended ? 1 : 0;
  message.data_length_code = 8;
  for (int i = 0; i < 8; i++) message.data[i] = data[i];

  if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.println("CAN message sent (ID: 0x" + String(id, HEX) + ")");
  } else {
    Serial.println("Failed to send CAN message");
  }
}

void LedOn()  { pinMode(LED_PIN, HIGH); }
void LedOff() { pinMode(LED_PIN, LOW);  }
