# ESPHome SC05

ESPHome 2025.x external component for the SC05 family of UART gas sensors.

The driver is designed around the shared SC05 UART frame format and currently publishes the **SC05-NH3 0–100 ppm** concentration. The parser is intentionally separated from the gas-specific publisher so additional SC05 variants such as CO, NO2, H2S, and O2 can be added without rewriting UART synchronization and recovery logic.

## Features

- Native ESPHome `external_components` integration.
- No deprecated `custom_component` usage.
- UART TTL, 9600 baud, 8 data bits, 1 stop bit, no parity.
- Continuous non-blocking parser; no `delay()` calls.
- Synchronization on `0xFF` start byte.
- Automatic resynchronization after lost or partial frames.
- Communication timeout detection.
- Isolated checksum implementation for easy adjustment if a future SC05 datasheet variant uses a different formula.
- Diagnostic entities for CRC errors, lost frames, and communication timeouts.

## Supported sensors

| Sensor | Gas ID | Range | Status |
| --- | --- | --- | --- |
| SC05-NH3 | `0x17` | 0–100 ppm | Implemented |
| SC05-CO | TBD | TBD | Planned |
| SC05-NO2 | TBD | TBD | Planned |
| SC05-H2S | TBD | TBD | Planned |
| SC05-O2 | TBD | TBD | Planned |

## Wiring

### ESP32

| SC05 pin | ESP32 pin | Notes |
| --- | --- | --- |
| VIN | 5V | Use the sensor supply required by your module. |
| GND | GND | Common ground is required. |
| TX | GPIO16 | ESP32 UART RX. |
| RX | GPIO17 | ESP32 UART TX. |

> The SC05 UART interface is TTL serial. Verify the logic level of your exact module before connecting it directly to an ESP32.

## ESPHome YAML example

```yaml
esphome:
  name: capannone-4-aria
  friendly_name: Capannone 4 Aria

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
      type: local
      path: components

uart:
  id: uart_sc05
  rx_pin: GPIO16
  tx_pin: GPIO17
  baud_rate: 9600
  data_bits: 8
  stop_bits: 1
  parity: NONE

sensor:
  - platform: sc05
    uart_id: uart_sc05
    communication_timeout: 3s

    nh3:
      name: "Capannone-4 NH3"

    crc_errors:
      name: "Capannone-4 SC05 CRC Errors"

    lost_frames:
      name: "Capannone-4 SC05 Lost Frames"

    timeout_counter:
      name: "Capannone-4 SC05 Timeout Counter"

    online:
      name: "Capannone-4 SC05 Online"

    status:
      name: "Capannone-4 SC05 Stato"
```

## Protocol notes

The SC05-NH3 module transmits one 9-byte frame per second:

| Byte | Meaning |
| --- | --- |
| 0 | Start byte, `0xFF` |
| 1 | Gas ID, `0x17` for NH3 |
| 2 | Unit, `0x04` |
| 3 | Decimal byte |
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

## Development roadmap

- Add gas metadata for additional SC05 family members.
- Add per-gas unit/range validation as datasheets are collected.
- Add hardware-in-the-loop test notes with captured UART frames.

## License

MIT. See [LICENSE](LICENSE).
