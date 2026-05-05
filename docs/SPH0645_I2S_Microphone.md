# SPH0645 I2S MEMS Microphone

## Overview

The **SPH0645LM4H** is a digital MEMS microphone manufactured by Knowles. It outputs a pulse-density modulated (PDM) signal internally, but the chip integrates an on-chip I2S decoder that converts it to standard I2S digital output — no external ADC or codec needed.

For Privacy Shield, this is the microphone used in each masking node to capture ambient room noise for Voice Activity Detection (VAD) and adaptive masking.

---

## Key Specifications

| Parameter | Value | Notes |
|---|---|---|
| **Manufacturer** | Knowles | Industry leader in MEMS audio |
| **Interface** | **I2S** (24-bit) | Digital output, no ADC needed |
| **Sensitivity** | -26 dBFS (94 dB SPL @ 1 kHz) | ±1 dB typical |
| **Signal-to-Noise Ratio** | ~65 dB | Good for speech capture |
| **Frequency Response** | 100 Hz — ~16 kHz | Covers full speech spectrum |
| **Supply Voltage** | 1.62 V — 3.6 V | Compatible with ESP32-S3 3.3V |
| **Current Consumption** | ~600 μA (active) | Very low power |
| **Acoustic Overload Point** | 120 dB SPL | Can handle loud sounds |
| **Package** | 3.5 mm × 2.65 mm MEMS package | Tiny surface-mount |
| **Breakout Board** | Adafruit I2S MEMS Mic (SPH0645) | Most common hobbyist format |

---

## I2S Interface

The SPH0645 uses a **3-wire I2S interface** (plus power):

| Pin | Name | Function | Connect To |
|---|---|---|---|
| 1 | **GND** | Ground | ESP32 GND |
| 2 | **DOUT** | I2S Data Output (serial audio) | ESP32 DIN (GPIO 4) |
| 3 | **BCLK** | Bit Clock (I2S serial clock) | ESP32 BCLK (GPIO 5) |
| 4 | **WS / LRCLK** | Word Select (frame sync) | ESP32 WS (GPIO 6) |
| 5 | **3V** | Power supply (1.62–3.6V) | ESP32 3.3V |
| 6 | **SEL** | Channel select (L/R) | GND = Left, 3.3V = Right |

> **Note on naming:** ESP-IDF v5.x uses **WS** (Word Select) instead of the older **LRCLK** (Left/Right Clock) naming. They refer to the same signal.

### I2S Data Format

- **Format:** Standard I2S (Philips format), **24-bit** data, MSB first
- **Bit depth in ESP32:** Sample as `I2S_DATA_BIT_WIDTH_32BIT` — the 24-bit data is left-aligned into 32-bit words
- **Sample rate:** 16 kHz (configured in `i2s_std_clk_config_t`)
- **Channel:** Mono (SEL tied to GND, `I2S_SLOT_MODE_MONO`)

---

## Implementation: ESP-IDF v5.x Standard API

The code uses ESP-IDF's **new I2S standard mode driver** (`driver/i2s_std.h`), which is the modern API introduced in ESP-IDF v5.x. This replaces the legacy `driver/i2s.h` API.

### Header — `components/audio_hal/include/i2s_mic.h`

```c
#pragma once

#include "driver/i2s_std.h"

// The rx_chan handle is shared with main.c via extern
extern i2s_chan_handle_t rx_chan;

void mic_init(void);
```

Key design decisions:
- `rx_chan` is declared `extern` so `main.c` can use it directly with `i2s_channel_read()` — no wrapper functions needed for the prototype
- `mic_init()` handles all I2S configuration in one call

### Driver — `components/audio_hal/i2s_mic.c`

```c
#include "driver/i2s_std.h"
#include "esp_err.h"

#define I2S_BCLK        GPIO_NUM_5
#define I2S_WS          GPIO_NUM_6
#define I2S_DIN         GPIO_NUM_4

i2s_chan_handle_t rx_chan; 

void mic_init(void) {
    // 1. Allocate a new RX channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_chan));

    // 2. Configure standard I2S mode (Philips format)
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK,
            .ws   = I2S_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_chan, &std_cfg));

    // 3. Enable the channel — starts receiving immediately
    ESP_ERROR_CHECK(i2s_channel_enable(rx_chan));
}
```

### What each configuration does

| Config | Value | Why |
|---|---|---|
| `I2S_NUM_AUTO` | Auto-select | Lets ESP-IDF pick the I2S controller, avoids hardcoding |
| `I2S_ROLE_MASTER` | Master | ESP32 generates BCLK and WS clocks |
| `16 kHz` | Sample rate | Optimal for speech — Nyquist covers 8 kHz voice bandwidth |
| `32-bit` | Slot width | SPH0645 sends 24-bit data; 32-bit mode aligns it cleanly |
| `MONO` | Single channel | One mic per node, SEL tied to GND (left channel) |
| `mclk = I2S_GPIO_UNUSED` | No MCLK | SPH0645 doesn't require a master clock pin |

### Reading Audio — `main/main.c`

```c
#include "i2s_mic.h"

#define SAMPLE_BUFFER_SIZE 256

void app_main(void) {
    mic_init();

    int32_t sample_buffer[SAMPLE_BUFFER_SIZE];
    size_t bytes_read;

    while (1) {
        esp_err_t err = i2s_channel_read(
            rx_chan, sample_buffer, sizeof(sample_buffer),
            &bytes_read, portMAX_DELAY
        );

        if (err == ESP_OK) {
            int samples_read = bytes_read / sizeof(int32_t);
            // Process samples...
        }
    }
}
```

The read is **blocking with `portMAX_DELAY`** — the task sleeps until DMA fills the buffer. This is simple and efficient for a prototype. The double-buffered DMA (configured via `I2S_CHANNEL_DEFAULT_CONFIG`) ensures no data is lost while processing.

### CMakeLists.txt — `components/audio_hal/CMakeLists.txt`

```cmake
idf_component_register(SRCS "i2s_mic.c" "max_amp.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_i2s)
```

Key: `REQUIRES esp_driver_i2s` links the I2S standard driver. This is the v5.x name — the legacy `REQUIRES driver` is deprecated.

---

## How It Works (Signal Chain)

```
  Sound waves (air pressure)
      │
      ▼
  ┌──────────────────────┐
  │   MEMS Diaphragm     │  Tiny silicon membrane
  │   (capacitive)       │  Vibrates → changes capacitance
  └──────────┬───────────┘
             │ (analog voltage)
             ▼
  ┌──────────────────────┐
  │   PDM Modulator      │  1-bit Sigma-Delta modulation
  │   (on-chip ASIC)     │  ~2-4 MHz bitstream
  └──────────┬───────────┘
             │ (1-bit PDM)
             ▼
  ┌──────────────────────┐
  │   I2S Decoder        │  Decimates PDM → 24-bit PCM
  │   (on-chip)          │  Outputs standard I2S frames
  └──────────┬───────────┘
             │ (24-bit I2S @ 16 kHz)
             ▼
  ┌──────────────────────┐
  │   ESP32 I2S (DMA)    │  DMA transfers samples to RAM
  │   I2S_NUM_AUTO       │  Ping-pong buffers
  └──────────┬───────────┘
             │ (32-bit samples in buffer[])
             ▼
  ┌──────────────────────┐
  │   main.c loop        │  Reads via i2s_channel_read()
  │   (energy detection) │  Computes |sample| energy, prints VU bar
  └──────────────────────┘
```

---

## Wiring Diagram (Privacy Shield Node)

```
                    ┌───────────────┐
                    │   ESP32-S3    │
                    │               │
                    │  GPIO 5 ──────┼─────► Mic BCLK
                    │               │      Amp BCLK (shared)
                    │  GPIO 6 ──────┼─────► Mic WS
                    │               │      Amp WS (shared)
                    │  GPIO 4 ◄────┼────── Mic DOUT
                    │  GPIO 7 ────►┼────── Amp DIN (separate)
                    │               │
                    │  3.3V  ──────┼─────► Mic VCC
                    │               │      Amp VCC
                    │  GND   ──────┼─────► Mic GND
                    │               │      Amp GND
                    └───────────────┘
```

BCLK (bit clock) and WS (word select) are **shared** between the microphone and the MAX98357A amplifier — they run on the same clock domain. The data lines are **separate**: mic DOUT → ESP32 DIN, ESP32 DOUT → amp DIN.

---

## Common Pitfalls & Tips

### 1. API Version: Use the Standard Driver, Not Legacy

ESP-IDF v5.x has two I2S APIs:

| API | Header | Functions | Status |
|---|---|---|---|
| **Standard (new)** | `driver/i2s_std.h` | `i2s_new_channel`, `i2s_channel_init_std_mode` | ✅ Use this |
| Legacy (old) | `driver/i2s.h` | `i2s_driver_install`, `i2s_set_pin` | ❌ Deprecated |

The new API uses channel handles (`i2s_chan_handle_t`) instead of numeric I2S port numbers. It also requires `REQUIRES esp_driver_i2s` in CMakeLists.txt (not `REQUIRES driver`).

### 2. GPIO Pin Conflicts

The ESP32-S3 has specific pins that work with I2S. The current config uses:
- **BCLK = GPIO 5** ✓
- **WS = GPIO 6** ✓
- **DIN = GPIO 4** ✓

If you change pins, verify they support I2S function in the ESP32-S3 datasheet.

### 3. 24-bit Data in 32-bit Containers

The SPH0645 sends 24-bit data. ESP-IDF's 32-bit mode left-aligns it into each 32-bit word. The lower 8 bits are always 0. If you need clean 24-bit values:

```c
int32_t sample = sample_buffer[i] >> 8;
```

### 4. Power Supply Decoupling

MEMS microphones are sensitive to PSRR (Power Supply Rejection Ratio). Add a **10–100 µF capacitor** near the mic's VCC pin. The Adafruit breakout board includes this, but if you use bare SPH0645 chips, you need one.

### 5. Mechanical Isolation

The DAEX25 transducer vibrates the mounting surface. That vibration can travel through the enclosure and reach the microphone, creating a feedback loop. Mitigations:
- Mount the mic on foam or rubber grommets
- Use separate chambers in the 3D-printed enclosure (mic chamber isolated from transducer chamber)
- The AEC pipeline (Sprint 3) handles residual acoustic feedback

### 6. SEL Pin Configuration

Tie SEL to **GND** for LEFT channel operation. Configure ESP32 for `I2S_SLOT_MODE_MONO` — it will read only the left slot. The right slot is ignored.

### 7. Dual-Mic Setup

Two SPH0645 mics can share the same BCLK/WS bus — tie one SEL to GND (left channel), the other to 3.3V (right channel). Configure the ESP32 for `I2S_SLOT_MODE_STEREO` and each channel gives you a separate mic stream.

---

## Pin-compatible Alternatives

| Microphone | Interface | Notes |
|---|---|---|
| **SPH0645LM4H** | I2S | The one we use. Great for speech. |
| **INMP441** | I2S | Very similar sensitivity. Drop-in compatible. |
| **ICS-43434** | I2S | Higher SNR (~70 dB). Same pinout. |
| **PMOD I2S2** | I2S | Based on ICS-43432, breadboard-friendly. |

All share the same 6-pin I2S pinout — same PCB footprint works for any of them.

---

## References

- Knowles SPH0645LM4H Datasheet: search "SPH0645LM4H-B datasheet"
- Adafruit I2S MEMS Microphone Breakout Guide: learn.adafruit.com
- ESP-IDF I2S Standard Driver docs: docs.espressif.com (search "I2S standard mode")
- ESP32-S3 Technical Reference Manual: Chapter on I2S Controller
