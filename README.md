# ESPHome SC05

**Versione: v1.0.0 Release Candidate**

ESPHome 2025.12.x external component for the SC05 family of UART gas sensors.

The driver is designed around the shared SC05 UART frame format and currently publishes the **SC05-NH3 0–100 ppm** concentration. The UART parser is intentionally independent from gas-specific publishing so future SC05-CO, SC05-NO2, SC05-H2S, and SC05-O2 support can be added in the gas dispatcher without rewriting frame synchronization or recovery logic.

## Features

- Native ESPHome `external_components` integration.
- No deprecated `custom_component` usage.
- UART TTL, 9600 baud, 8 data bits, 1 stop bit, no parity.
- Continuous non-blocking parser; no `delay()` calls.
- Synchronization on `0xFF` start byte.
- Automatic resynchronization after lost or partial frames.
- Communication timeout detection.
- Diagnostic entities for CRC errors, lost frames, invalid frames, timeout counter, last frame age, and full scale.
- Checksum code isolated behind an explicit checksum mode for future datasheet-confirmed updates.

## Supported sensors

| Sensor | Gas ID | Range | Status |
| --- | --- | --- | --- |
| SC05-NH3 | `0x17` | 0–100 ppm | Implemented |
| SC05-CO | TBD | TBD | Planned |
| SC05-NO2 | TBD | TBD | Planned |
| SC05-H2S | TBD | TBD | Planned |
| SC05-O2 | TBD | TBD | Planned |

## Collegamenti ESP32

### SC05 UART

| SC05 pin | ESP32 pin | Notes |
| --- | --- | --- |
| VIN | 5V | Use the sensor supply required by your exact SC05 module. |
| GND | GND | Common ground is required. |
| TX | GPIO16 | SC05 TX goes to ESP32 UART RX. |
| RX | GPIO17 | SC05 RX goes to ESP32 UART TX. |

### SCD41 I²C

| SCD41 pin | ESP32 pin | Notes |
| --- | --- | --- |
| VIN | 3V3 or 5V | Use the breakout-board supply range. |
| GND | GND | Common ground is required. |
| SDA | GPIO21 | ESP32 default I²C SDA in the example. |
| SCL | GPIO22 | ESP32 default I²C SCL in the example. |

> The SC05 UART interface is TTL serial. Verify the logic level of your exact module before connecting it directly to an ESP32.

## Configurazione UART

The SC05-NH3 sends one frame per second over UART. The component uses ESPHome's UART hub through `uart_id`; `logger:` can remain enabled because the example uses GPIO16/GPIO17, not the default USB serial logger pins.

```yaml
uart:
  id: uart_sc05
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600
```

## Configurazione I²C

The SCD41 is not part of this component, but the complete example includes a standard ESPHome `scd4x` sensor on the same ESP32 for environmental monitoring.

```yaml
i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true
```

## Esempio completo ESPHome

```yaml
esphome:
  name: sensore-aria-cap-4
  friendly_name: Sensore Aria Capannone 4

esp32:
  board: esp32dev
  framework:
    type: arduino

logger:

api:

ota:
  platform: esphome

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

external_components:
  - source:
      type: git
      url: https://github.com/Diablo-81/esphome-sc05
      ref: main
    components:
      - sc05

i2c:
  sda: GPIO21
  scl: GPIO22
  scan: true

uart:
  id: uart_sc05
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 9600

sensor:
  - platform: sc05

    id: sc05_cap4

    uart_id: uart_sc05

    communication_timeout: 3s

    nh3:
      name: "Capannone-4 NH3"

    full_scale:
      name: "Capannone-4 Full Scale"

    online:
      name: "Capannone-4 Online"

    status:
      name: "Capannone-4 Stato"

    crc_errors:
      name: "Capannone-4 CRC Errors"

    lost_frames:
      name: "Capannone-4 Lost Frames"

    invalid_frame_counter:
      name: "Capannone-4 Invalid Frames"

    timeout_counter:
      name: "Capannone-4 Timeout Counter"

    last_frame_age:
      name: "Capannone-4 Last Frame Age"

  - platform: scd4x
    co2:
      name: "Capannone-4 CO2"
    temperature:
      name: "Capannone-4 Temperatura"
    humidity:
      name: "Capannone-4 Umidità"
```

## Diagnostics

| Entity | Meaning | Typical action |
| --- | --- | --- |
| CRC Errors | Frames with a valid length but a checksum that does not match the configured checksum algorithm. | Check UART noise, grounding, cable length, and confirm the checksum against captured sensor frames. |
| Lost Frames | Parser-level recovery events, such as a partial frame timing out or a new `0xFF` start byte appearing while another frame is being assembled. | Check serial wiring quality and electrical noise. |
| Invalid Frames | Checksum-valid NH3 frames whose semantic fields do not match the supported SC05-NH3 profile: unit `0x04` and decimal byte `0x00`. | Verify the exact SC05 model, gas variant, and datasheet. |
| Timeout Counter | Number of times no fully valid, supported frame arrived before `communication_timeout`. | Check sensor power, UART pins, baud rate, and sensor availability. |
| Last Frame Age | Milliseconds since the last fully valid and supported frame was accepted. | Use for live debugging and alert thresholds. |
| Full Scale | Full-scale value reported by the sensor frame bytes 6 and 7. | Verify the installed module range, currently expected to be 100 ppm for SC05-NH3 0–100 ppm. |

A frame only marks the sensor as online after checksum, unit, decimal, and gas support checks have all passed. Unsupported gases keep the UART parser synchronized and publish status `Unsupported gas` without incrementing `lost_frames`.

## Protocol notes

The SC05-NH3 module transmits one 9-byte frame per second:

| Byte | Meaning |
| --- | --- |
| 0 | Start byte, `0xFF` |
| 1 | Gas ID, `0x17` for NH3 |
| 2 | Unit, `0x04` |
| 3 | Decimal byte, expected `0x00` for this NH3 implementation |
| 4 | Concentration high byte |
| 5 | Concentration low byte |
| 6 | Full-scale high byte |
| 7 | Full-scale low byte |
| 8 | Checksum |

NH3 concentration is calculated as:

```text
ppm = (byte4 * 256 + byte5) / 100.0
```

The checksum is implemented in `SC05Component::calculate_checksum_()` as the low byte of the sum of bytes 1 through 7. The method is intentionally isolated so the algorithm can be changed in one place if a specific datasheet revision documents a different checksum.

**Nota:** Checksum da verificare con frame acquisiti dal sensore reale.

## Troubleshooting

### `Online` remains off

- Confirm SC05 VIN and GND.
- Confirm SC05 TX is connected to GPIO16 and SC05 RX to GPIO17.
- Confirm `uart_id` matches the UART block.
- Increase logger level to `VERBOSE` to see complete UART frame dumps from the component.

### CRC Errors increase

- Check cable length, shielding, grounding, and electrical noise.
- Capture raw UART bytes and compare the checksum with the implementation before using the sensor for production alarms.

### Invalid Frames increase

- Confirm the installed module is SC05-NH3 and that it reports unit `0x04` and decimal byte `0x00`.
- If the gas is not NH3, add support in the dispatcher instead of changing the parser.

### Lost Frames increase

- Lost frames indicate parser recovery events, not valid-but-unsupported gas frames.
- Check for noisy UART wiring or partial frames caused by unstable power.

### SCD41 not detected

- Confirm SDA/SCL wiring on GPIO21/GPIO22.
- Confirm the SCD41 breakout has appropriate pull-ups or enables onboard pull-ups.
- Keep I²C wiring short in noisy environments.

## Development roadmap

- Add gas metadata and dispatcher cases for additional SC05 family members.
- Add per-gas unit/range validation as datasheets are collected.
- Add hardware-in-the-loop test notes with captured UART frames.

## License

MIT. See [LICENSE](LICENSE).
