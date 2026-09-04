# Hardware Pinout

| GPIO | Device | Signal |
|---:|---|---|
| 15 | DHT22 | DATA |
| 21 | SSD1306 | SDA |
| 22 | SSD1306 | SCL |
| 25 | Push button | UP |
| 26 | Push button | DOWN |
| 27 | Push button | SELECT |
| 34 | Analog source | Load input |
| 4 | LED | Alarm |
| 5 | Buzzer | Alarm |

Buttons are configured with `INPUT_PULLUP`; connect the other side of each button to GND.

OLED I²C address: `0x3C`.

ADC resolution: 12-bit, giving a nominal range of 0–4095.
