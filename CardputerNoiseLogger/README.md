# Cardputer Noise Logger

Arduino sketch for M5Stack Cardputer to log noise levels with timestamps to SD card.

## Features

1. **Start Screen** - Custom branding with SD card status
2. **Time Setting Menu** - Set current date/time using buttons
3. **Noise Logging** - Real-time decibel measurement with threshold detection
4. **SD Card Logging** - CSV format with timestamps

## Hardware Requirements

- M5Stack Cardputer
- Micro SD card (formatted as FAT32)

## Required Libraries

Install via Arduino Library Manager:
- **M5Unified** (by M5Stack)

## Pin Configuration (Cardputer)

| Function | GPIO |
|----------|------|
| I2S BCLK | 33 |
| I2S WS   | 34 |
| I2S DIN  | 35 |
| SD CS    | 4  |

## Controls

### Start Screen
- **Any button** → Enter Time Menu

### Time Menu
- **UP/DOWN** → Select field (Year/Month/Day/Hour/Minute/Second)
- **LEFT/RIGHT** → Adjust value
- **BTN_A** → Confirm time
- **BTN_B** → Start Logging (requires SD card)

### Logging Screen
- **BTN_A** → Stop logging, return to Time Menu
- **BTN_B** → Adjust noise threshold (30-100 dB)
- **LEFT/RIGHT** (in threshold adjust) → Change threshold

## Output Format

CSV file on SD card: `noise_YYYYMMDD.csv`

```
Timestamp,dB
2024-01-15 14:30:45,52.3
2024-01-15 14:30:46,55.1
```

## Installation

1. Open `CardputerNoiseLogger.ino` in Arduino IDE
2. Select board: **M5Stack-Cardputer** (or ESP32S3 Dev Module)
3. Install M5Unified library
4. Upload to device
5. Insert SD card before powering on

## Calibration

The microphone reading includes a +94 dB offset (standard reference). Adjust `NOISE_THRESHOLD_DB` in code if needed:
```cpp
#define NOISE_THRESHOLD_DB 50.0
```

## Troubleshooting

- **SD Card not detected**: Ensure FAT32 format, try different card
- **No microphone data**: Check I2S wiring, Cardputer has built-in mic on GPIO 35
- **Time not saved**: Cardputer has no RTC battery, time resets on power loss