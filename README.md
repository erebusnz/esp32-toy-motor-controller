# ESP32 Toy Motor Controller

A compact dual DC motor controller board designed around an **ESP32-S3 (18-pin layout)** compatible with common compact modules such as the **TENSTAR ESP32-S3 18PIN board**, and the **TB6612FNG** dual H-bridge motor driver.

This project targets small toy hacking with reliable bidirectional DC motor control, PWM speed control, and simple GPIO-controlled switching of accessories.

---

## Overview

The board integrates:
- An **ESP32-S3 using an 18-pin module layout** (aligned with TENSTAR-style boards from aliexpress, avoid clones and use TENSTAR store for purchase)
- A **TB6612FNG** dual H-bridge motor driver
- Two GPIO-controlled low-side switches using NPN transistors
- On-board decoupling and bulk capacitance for motor stability

---

## Key Features

- Dual DC motor control (independent speed and direction)
- ESP32-S3 controller with Wi-Fi / BLE capability
- Efficient TB6612FNG motor driver (lower loss than legacy H-bridges)
- Two low-side switched outputs using NPN transistors

---

## Hardware Architecture

### Microcontroller

- **ESP32-S3 (18-pin layout)**
- Compatible with compact modules such as the TENSTAR ESP32-S3 18PIN board
- GPIOs directly control motor driver and switch circuitry

### Motor Driver

- **TB6612FNG Dual H-Bridge**
- Motor connections:
  - Motor A: AO1 / AO2
  - Motor B: BO1 / BO2
- Control signals:
  - AIN1, AIN2, PWMA
  - BIN1, BIN2, PWMB
- Motor supply via VM (VCC)

### Power

- **VCC input connector**
  - Supplies motor driver VM
  - Bulk capacitor: 470 µF for transient suppression
- **3.3 V logic rail**
  - Local decoupling: 100 nF + 10 µF
- Common ground between logic and motor domains

### Switch Outputs (Low-Side Switching)

- Two GPIO-controlled switches implemented using **NPN transistors**
- Low-side switching topology
- Suitable for driving small loads referenced to VCC

---

## Connectors

| Connector | Function |
|---------|---------|
| CN1 | Motor 1 output |
| CN2 | Motor 2 output |
| CN3 | VCC power input |
| CN4 | Switch 2 |
| CN5 | Switch 1 |

---

## ESP32-S3 (18-Pin) GPIO Mapping

This GPIO assignment is fixed and intended to align with common 18-pin ESP32-S3 modules.

### Motor Driver (TB6612FNG)

| ESP32-S3 GPIO | TB6612FNG Signal | Function |
|---------------|------------------|----------|
| GP8  | PWMA | Motor A speed |
| GP10 | AIN1 | Motor A direction |
| GP9  | AIN2 | Motor A direction |
| GP13 | PWMB | Motor B speed |
| GP11 | BIN1 | Motor B direction |
| GP12 | BIN2 | Motor B direction |

### Switch Control

| ESP32-S3 GPIO | Function |
|---------------|----------|
| GP1 | Switch 1 control |
| GP7 | Switch 2 control |

- Switches use **low-side NPN switching**: the ESP32 GPIO must be driven **LOW to turn the switch ON** and **HIGH to turn it OFF**.

---

## Arduino Software

TBD / coming soon!

---

## Power & Electrical Considerations

- Design is intended for **small DC motors and low-current switched loads**
- No onboard reverse-polarity protection
- No current sensing
- External motor suppression may be required for noisy loads

---

## Notes & Limitations

- Not suitable for high-current motors
- No hardware fault protection
- ESP32 module pinout must match the 18-pin layout described above
