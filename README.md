# FTM-Based Safety System for Parent-Child Cycling

A real-time distance estimation system for e-bikes using the **Fine Time Measurement (FTM)** protocol on ESP32-S3 microcontrollers. Developed as a Bachelor's thesis at Halmstad University (2025).

> **Authors:** Jimmy Ly & William Ondrejov

---

## Overview

This project addresses the safety challenge parents face when cycling with children on electrically assisted bicycles (e-bikes). By measuring the distance between two ESP32-S3 microcontrollers using Wi-Fi FTM (IEEE 802.11-2016), the system can detect when a child's bike drifts too far from the parent's — and automatically trigger a speed limiter via the CAN bus.

Key results from testing:
- **96.31% accuracy** in static outdoor conditions (with Kalman filter + bias correction)
- **±0.43 m MAD** — outperforms GPS and RSSI-based methods at short range
- Boundary-crossing detection accuracy of ~85% at 10 m in dynamic conditions

---

## How It Works

```
[Child's Bike — FTM Initiator]  <──WiFi FTM──>  [Parent's Bike — FTM Responder]
        │                                                    │
   Kalman filter                                      Receives UDP
   + bias correction                                  notification
        │
   Distance > threshold?
        │
       YES → Send UDP alert + CAN bus message to disable e-bike motor
```

1. The **Responder** (parent's bike) broadcasts a Wi-Fi access point and listens for notifications.
2. The **Initiator** (child's bike) connects to the AP and continuously measures distance using FTM.
3. If the measured distance exceeds the configured boundary, the Initiator sends a UDP notification and a CAN bus message to limit the e-bike's speed.

---

## Hardware Requirements

| Component | Details |
|---|---|
| Microcontroller | 2× ESP32-S3 |
| CAN transceiver | Compatible with TWAI (e.g. SN65HVD230) |
| Power supply | 3.3V (via bike or battery) |
| Mounting | Recommended antenna height: **≥ 1.75 m** (helmet level) |

---

## Software & Dependencies

- [Arduino IDE](https://www.arduino.cc/en/software)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32) by Espressif Systems
- [SimpleKalmanFilter](https://github.com/denyssene/SimpleKalmanFilter) by Denys Sene

Install the ESP32 board package in Arduino IDE:
```
File → Preferences → Additional boards manager URLs:
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

---

## Project Structure

```
ftm-cycling-safety/
├── src/
│   ├── responder/
│   │   └── responder.ino       # Parent's bike — FTM Responder + UDP listener
│   └── initiator/
│       └── initiator.ino       # Child's bike — FTM Initiator + Kalman filter
├── docs/
│   └── thesis.pdf              # Full bachelor's thesis (optional)
└── README.md
```

---

## Configuration

Both files share the same Wi-Fi credentials — update these before flashing:

```cpp
const char* ssid     = "Esp32_S3";
const char* password = "12345678";
```

In `initiator.ino`, the key tunable parameters are:

```cpp
int samplesize           = 100;    // Number of samples before stopping (sampling mode)
const unsigned long interval = 1000; // ms between FTM sessions
SimpleKalmanFilter kalmanFilter(2, 2, 0.5); // e_mea, e_est, q
const float desiredRange = 0;      // Boundary threshold in metres
const float offset       = 5.5;    // Bias correction offset in metres
```

To switch from **sampling mode** to **continuous operation**, comment out the sampling `while` loop in `loop()` and uncomment the continuous block:

```cpp
// Continuous mode
StartFTM();
delay(interval);
```

---

## Flashing

1. Open `responder.ino` in Arduino IDE, select your ESP32-S3 board, and flash it to the **parent's** device.
2. Open `initiator.ino` and flash it to the **child's** device.
3. Power both devices — the Initiator will connect to the Responder's AP automatically.
4. Open the Serial Monitor (115200 baud) to observe live distance readings.

---

## Results Summary

| Condition | Accuracy |
|---|---|
| Static outdoor (unfiltered) | ~69.9% |
| Static outdoor (Kalman + bias correction) | **~96.3%** |
| Dynamic (cycling, 10 m boundary) | ~84.9% |
| Dynamic (cycling, 20 m boundary) | ~79.9% |

For full methodology, test setups, and result analysis — see the [thesis](docs/thesis.pdf).

---

## Limitations

- Performance degrades significantly in **dynamic (cycling) conditions** due to packet loss and multipath effects.
- Bias correction is calibrated for **5–20 metre range** only; longer ranges need additional tuning.
- Antenna placement strongly affects accuracy — mounting below **0.53 m** (60% of Fresnel zone radius at 25 m) significantly reduces performance.

---

## Future Work

- Investigate **Extended Kalman Filter (EKF)** or **Unscented Kalman Filter (UKF)** for better dynamic performance.
- Add **token-based authentication** to replace hardcoded credentials.
- Design a **shock-absorbing enclosure** for the ESP32 to reduce vibration noise during cycling.
- Extend operational range beyond 20 m with additional bias profiling.

---

## License

This project is released for academic and educational purposes. See `LICENSE` for details.
