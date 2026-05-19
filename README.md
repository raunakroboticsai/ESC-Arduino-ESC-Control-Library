# ⚡ ESC — Arduino ESC Control Library

> **Precision PWM control for Electronic Speed Controllers using hardware Timer1 interrupts.**  
> Lightweight. Accurate. Built for real embedded systems.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Supported Platforms](#supported-platforms)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [API Reference](#api-reference)
- [How It Works](#how-it-works)
- [Circuit Diagram](#circuit-diagram)
- [Examples](#examples)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## Overview

The **ESC Library** gives you hardware-accurate PWM generation for brushless motor Electronic Speed Controllers on AVR-based Arduino boards. Unlike `analogWrite()` or `Servo.h`, this library uses **Timer1 hardware interrupts** to produce a rock-solid 50 Hz PWM signal — the international standard for ESC control — with no CPU-blocking delays.

It is designed for robotics, drones, RC vehicles, and any embedded project where motor speed needs to be controlled precisely and reliably.

---

## Features

- ✅ **Hardware timer-driven** — no `delay()`, no blocking code
- ✅ **50 Hz PWM output** — standard ESC protocol (1000–2000 µs pulse width)
- ✅ **Auto-clamped input** — out-of-range values default to minimum throttle safely
- ✅ **Multi-clock support** — works at 8 MHz, 16 MHz, and 20 MHz
- ✅ **Direct port manipulation** — faster than `digitalWrite()`, lower overhead
- ✅ **Minimal footprint** — ideal for resource-constrained AVR MCUs

---

## Hardware Requirements

| Component | Details |
|-----------|---------|
| Microcontroller | AVR-based Arduino (Uno, Nano, Mega, etc.) |
| ESC | Any standard hobby ESC (PWM 50 Hz, 1000–2000 µs) |
| Power Supply | LiPo battery (7.4V / 11.1V typical) |
| UBEC | Required if powering Arduino from the same battery |
| Motor | Brushless DC motor matched to your ESC rating |

---

## Supported Platforms

| Board | Clock Speed | Status |
|-------|-------------|--------|
| Arduino Uno | 16 MHz | ✅ Supported |
| Arduino Nano | 16 MHz | ✅ Supported |
| Arduino Mega | 16 MHz | ✅ Supported |
| Arduino Pro Mini (5V) | 16 MHz | ✅ Supported |
| Arduino Pro Mini (3.3V) | 8 MHz | ✅ Supported |
| Arduino running at 20 MHz | 20 MHz | ✅ Supported |

> **Note:** This library uses **Timer1** exclusively. Any other library that also uses Timer1 (e.g., `Servo.h`, `tone()`) will conflict. Do not use them simultaneously.

---

## Installation

### Option 1 — Manual Install (Recommended)

1. Click the green **Code** button on this repository → **Download ZIP**
2. In the Arduino IDE: `Sketch → Include Library → Add .ZIP Library...`
3. Select the downloaded `.zip` file
4. Done — the library appears under `File → Examples → ESC`

### Option 2 — Clone via Git

```bash
cd ~/Arduino/libraries
git clone https://github.com/YOUR_USERNAME/ESC-Library.git
```

Restart the Arduino IDE after cloning.

---

## Quick Start

```cpp
#include "ESC.h"

// Attach ESC signal wire to digital pin 9
ESC myESC(9);

void setup() {
  myESC.init();       // Initialize Timer1 interrupt
  delay(3000);        // Allow ESC to complete its startup beep sequence
  myESC.setSpeed(1000); // Set to minimum throttle (required before arming)
  delay(2000);
}

void loop() {
  // Ramp throttle from 0% to 100% over 5 seconds
  for (int speed = 1000; speed <= 2000; speed += 10) {
    myESC.setSpeed(speed);
    delay(50);
  }

  // Hold at full throttle for 1 second
  delay(1000);

  // Ramp back down
  for (int speed = 2000; speed >= 1000; speed -= 10) {
    myESC.setSpeed(speed);
    delay(50);
  }

  delay(2000);
}
```

> ⚠️ **Important:** Most ESCs require an **arming sequence** — holding minimum throttle (`1000 µs`) for 1–3 seconds while powering on. Always call `setSpeed(1000)` in `setup()` and wait before running the motor.

---

## API Reference

### `ESC(uint8_t pin)`

**Constructor.** Creates an ESC instance and configures the specified pin as a digital output.

```cpp
ESC myESC(9); // Attach to pin 9
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `pin` | `uint8_t` | Arduino digital pin number connected to the ESC signal wire |

---

### `void init()`

**Initializes Timer1.** Must be called once in `setup()` before using `setSpeed()`.

```cpp
myESC.init();
```

**What it does internally:**
- Resets Timer1 control registers
- Sets a **1:8 prescaler** (0.5 µs resolution at 16 MHz)
- Configures **CTC (Clear Timer on Compare) mode**
- Enables the **Timer1 Compare Match A interrupt**

> ⚠️ This function calls `cli()` and `sei()` to safely configure the timer. Do not call it inside an ISR.

---

### `void setSpeed(uint16_t microseconds)`

**Sets the ESC throttle** by updating the PWM pulse width.

```cpp
myESC.setSpeed(1500); // 50% throttle
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `microseconds` | `uint16_t` | Pulse width in microseconds (1000–2000) |

| Value | Throttle | Meaning |
|-------|----------|---------|
| `1000` | 0% | Minimum / Stop |
| `1500` | 50% | Mid throttle |
| `2000` | 100% | Maximum throttle |

Values outside the `1000–2000 µs` range are **automatically clamped** to minimum throttle (`1000 µs`) to prevent unsafe behavior.

This function temporarily disables the Timer1 interrupt while updating the shared `ESC_HighTime` variable, then re-enables it — ensuring **atomic, glitch-free updates**.

---

## How It Works

### The PWM Signal

ESCs communicate via a **50 Hz PWM signal** — one pulse every 20 milliseconds. The pulse width encodes the throttle command:

```
|‾‾‾‾‾|___________________|  ← 1000 µs high, 19000 µs low  =  0% throttle
|‾‾‾‾‾‾‾‾‾‾|______________|  ← 1500 µs high, 18500 µs low  = 50% throttle
|‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾|_________|  ← 2000 µs high, 18000 µs low  = 100% throttle
```

### Timer1 Interrupt Logic

Rather than using `delayMicroseconds()`, this library uses **Timer1 in CTC mode** with a 1:8 prescaler. At 16 MHz, each timer tick = 0.5 µs. The interrupt fires frequently, accumulating elapsed time in a counter (`ESCTime`).

On each interrupt:
1. If the pin is **HIGH** and `ESCTime >= highTimeCopy` → pull the pin **LOW** (end of pulse)
2. If the pin is **LOW** and `ESCTime >= 40000` (= 20 ms at 16 MHz) → pull the pin **HIGH**, reset the counter (start of next pulse)

**Direct port manipulation** (`*pin_register |= bitMask`) is used instead of `digitalWrite()` for speed, since this runs inside an ISR.

### Clock Scaling

The library automatically adjusts tick counts to match the CPU clock:

| Clock | Prescaler Tick | `REFRESH_50HZ` | `MIN_VAL_COPY` |
|-------|---------------|----------------|----------------|
| 8 MHz | 1.0 µs | 20000 | 1000 |
| 16 MHz | 0.5 µs | 40000 | 2000 |
| 20 MHz | 0.4 µs | 50000 | 2500 |

---

## Circuit Diagram

```
                          ┌──────────────────────────┐
  ┌─────────────┐         │        Arduino Uno        │
  │  LiPo       │         │                           │
  │  Battery    │──(+)──► │ Vin (via UBEC 5V output)  │
  │  11.1V      │──(-)──► │ GND                       │
  └──────┬──────┘         │                           │
         │                │ Digital Pin 9 ────────────┼──► ESC Signal Wire (White/Yellow)
         │                └──────────────────────────-┘
         │
         ▼
   ┌──────────┐
   │   UBEC   │  (5V regulated output → Arduino Vin)
   │ 11.1V→5V │
   └──────────┘
         │
         └──────────────────────────────────────────► ESC Power Wires (Red/Black)
                                                       └── ESC → Brushless Motor (3 wires)
```

### Wiring Summary

| Wire | From | To |
|------|------|----|
| ESC Signal | Arduino Pin 9 | ESC Signal input (thin wire, usually white or yellow) |
| ESC Ground | Arduino GND | ESC Ground (common ground is essential) |
| Battery (+) | LiPo (+) | UBEC input (+) and ESC main power |
| Battery (−) | LiPo (−) | UBEC input (−), ESC ground, Arduino GND |
| UBEC Output | 5V regulated | Arduino Vin or 5V pin |

> ⚠️ **Critical:** Always share a **common ground** between Arduino and ESC. Without it, the signal reference is floating and the ESC will not respond correctly.

> ⚠️ **Do NOT** power the Arduino via the barrel jack and simultaneously connect to an ESC powered by a separate source without a shared ground.

---

## Examples

### Arming Sequence + Sweep

```cpp
#include "ESC.h"

ESC myESC(9);

void setup() {
  Serial.begin(9600);
  myESC.init();

  Serial.println("Arming ESC...");
  myESC.setSpeed(1000); // Send minimum throttle to arm
  delay(3000);
  Serial.println("Armed! Starting motor sweep.");
}

void loop() {
  Serial.println("Ramping up...");
  for (int spd = 1000; spd <= 2000; spd += 5) {
    myESC.setSpeed(spd);
    delay(20);
  }

  delay(500);

  Serial.println("Ramping down...");
  for (int spd = 2000; spd >= 1000; spd -= 5) {
    myESC.setSpeed(spd);
    delay(20);
  }

  delay(2000);
}
```

### Serial Control

Control speed in real time from the Serial Monitor:

```cpp
#include "ESC.h"

ESC myESC(9);

void setup() {
  Serial.begin(9600);
  myESC.init();
  myESC.setSpeed(1000);
  delay(3000);
  Serial.println("Enter speed (1000-2000):");
}

void loop() {
  if (Serial.available()) {
    int speed = Serial.parseInt();
    if (speed >= 1000 && speed <= 2000) {
      myESC.setSpeed(speed);
      Serial.print("Speed set to: ");
      Serial.println(speed);
    } else {
      Serial.println("Invalid! Enter a value between 1000 and 2000.");
    }
  }
}
```

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Motor doesn't spin | ESC not armed | Call `setSpeed(1000)` and wait 2–3 seconds in `setup()` |
| Erratic behavior | Ground not shared | Connect Arduino GND to ESC ground |
| Beeping on startup | Low voltage cutoff / not armed | Check battery voltage; complete arming sequence |
| No response at all | Wrong pin or loose connection | Verify signal wire is on correct pin; check connections |
| Conflicts with Servo.h | Both use Timer1 | Remove `#include <Servo.h>` — they cannot coexist |
| Speed jumps unexpectedly | Interrupt conflict | Ensure no other Timer1-using library is included |

---

## Contributing

Contributions, bug reports, and feature suggestions are welcome!

1. **Fork** this repository
2. Create a new branch: `git checkout -b feature/your-feature-name`
3. Make your changes and **commit**: `git commit -m "Add: your feature description"`
4. **Push**: `git push origin feature/your-feature-name`
5. Open a **Pull Request** with a clear description

Please follow existing code style and add comments where appropriate.

---

## License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

<div align="center">

Made with ❤️ by **Raunak Choudhary**

*If this library helped your project, consider giving it a ⭐ on GitHub!*

</div>
