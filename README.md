# M5GO Petbot

Friendly pet face firmware for the **M5GO IoT Kit v2.7**, built with **ESP-IDF 6.1** and FreeRTOS.

## What it does

| Event | Behavior |
| --- | --- |
| Idle | Black robot face: large round eyes, a small smile, blink and glance |
| Turn the M5GO | Onboard accelerometer keeps the face upright |
| PIR sees a person | Big cyan grin, **Hello!**. No sound. Side LEDs turn **green for 1 minute**, then off until the next detection |
| Under 20 °C | Cold face, shiver, **Brrr** |
| Humidity ≥ 70 % | Droopy face, **Muggy** |
| Button A (left) | Toggle **clock + temperature/humidity** ↔ pet face. PIR is ignored on the clock |
| Button B (middle) | Open the **cube timer**. Press again to go back to the pet face. PIR is ignored |
| Turn it in timer mode | Each side is a time: screen up **1**, buttons down **5**, sides **10** and **30**, buttons up **15**, screen down **60** minutes. Turning only changes the time while the timer is stopped |
| Button C (right) | Start or pause the cube timer. A chime plays when it reaches zero |

More features can plug into the same FreeRTOS tasks / `app_state` later.

## Hardware wiring

| Unit | Port | Notes |
| --- | --- | --- |
| **ENV-III** | Port **A** (I2C) | SHT30 @ `0x44` — temp & humidity |
| **PIR** | Port **B** | Digital out on the white wire, **GPIO36** |
| Button A | Built-in | **GPIO39** (left) |
| Button B | Built-in | **GPIO38** (middle) |
| Button C | Built-in | **GPIO37** (right) |

Core: M5GO / M5Stack Core v2.7 — ESP32, 320×240 IPS (ILI9342C), 16 MB flash.

## Requirements

- [ESP-IDF v6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/get-started/index.html) (or compatible ≥ 5.4)
- USB cable + CH9102 / CP210x driver for the M5GO

## Build & flash

```bash
# activate ESP-IDF 6.1, then:
cd m5go_petbot
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

On first configure, Component Manager pulls `espressif/m5stack_core` automatically.

If colors look wrong on an older TN panel, disable invert in `menuconfig`:

`Component config` → `Board Support Package` → uncheck **LCD invert color**.

## Architecture (FreeRTOS)

```
app_main
 ├── ui_tick (esp_timer)     face mood, blink, rotation
 ├── pir task                GPIO36 → hello face + green LEDs (no beep)
 ├── env task                SHT30 every 2 s → cold / humid mood
 ├── imu task                MPU6886 → upright face, cube-timer side
 └── buttons task            A clock, B timer, C start/stop
```

Shared state lives in `app_state` (mutex-protected). UI only reads dirty flags — keeps the face smooth.

## Project layout

```
main/
  app_main.c          boot + button task
  app_state.c         screen / env state machine
  ui/ui.c             pet face, clock, cube timer
  sensors/pir.c       Unit PIR
  sensors/env_sht30.c Unit ENV-III (SHT30, new I2C master API)
  audio/beep.c        short speaker chirp
```

## Next ideas

- Wi-Fi + SNTP for a real wall clock
- Mood / hunger based on ENV + time
- RGB LED bar reactions on the M5GO base
- Angle unit for “petting”
