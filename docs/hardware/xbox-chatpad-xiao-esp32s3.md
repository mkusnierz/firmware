# Xbox 360 Chatpad Support for Xiao ESP32-S3

This document describes how to connect and use an Xbox 360 Chatpad with the Seeed Xiao ESP32-S3 running Meshtastic firmware.

## Hardware Requirements

- Seeed Xiao ESP32-S3 (with or without expansion board)
- Xbox 360 Chatpad (model 1413 or compatible)
- Jumper wires
- Optional: Seeeduino Xiao Expansion Board for easier connections

## Wiring

The Xbox 360 Chatpad uses a proprietary connector. You'll need to either:
1. Solder wires directly to the chatpad PCB pads
2. Use a breakout board
3. Modify an Xbox 360 controller cable

### Chatpad Pinout (from connector side)
```
Pin 1: VCC (3.3V)  ───► Xiao 3V3
Pin 2: GND         ───► Xiao GND
Pin 3: DATA_TX     ───► Xiao GPIO1 (RX / UART1)
Pin 4: DATA_RX     ───► Xiao GPIO2 (TX / UART1)  [Optional - for init/keep-alive]
Pins 5-8: Not used for basic operation
```

### Xiao ESP32-S3 Connections

**Using Expansion Board:**
- GPIO1 and GPIO2 are available on the expansion header
- Connect chatpad DATA_TX to GPIO1 (RX)
- Connect chatpad DATA_RX to GPIO2 (TX)
- Connect VCC to 3V3, GND to GND

**Using Bare Xiao Module:**
- Solder to castellated pins GPIO1 and GPIO2
- Or use the expansion board pins

## Firmware Configuration

### Enable in platformio.ini
Uncomment the following lines in `variants/esp32s3/seeed_xiao_s3/platformio.ini`:

```ini
build_flags = 
  ${esp32s3_base.build_flags}
  -D SEEED_XIAO_S3
  -I variants/esp32s3/seeed_xiao_s3
  -DBOARD_HAS_PSRAM 
  -DARDUINO_USB_MODE=0
  -D XBOX_CHATPAD_ENABLED
  -D XBOX_CHATPAD_UART_NUM=1
  -D XBOX_CHATPAD_RX_PIN=1
  -D XBOX_CHATPAD_TX_PIN=2
```

### Build and Flash
```bash
pio run -e seeed-xiao-s3 -t clean
pio run -e seeed-xiao-s3
pio run -e seeed-xiao-s3 -t upload --upload-port /dev/ttyUSB0
```

## Usage

Once flashed and connected, the chatpad works as a text input device for Meshtastic:

### Text Input
- Type messages in channel chat
- Enter canned messages
- Set node names, channel names
- Any text input field in the Meshtastic UI

### Key Mappings
| Chatpad Key | Meshtastic Action |
|-------------|-------------------|
| A-Z, 0-9 | Text input |
| Space | Space |
| Enter | Select/Confirm |
| Backspace | Delete/Back |
| Tab | Tab (cycle fields) |
| Escape | Cancel/Back |
| Arrow Keys | Navigation (Up/Down/Left/Right) |
| Shift + Letter | Uppercase |
| Orange (Left Ctrl) + Key | Symbols (!@#$%^&*()~{}[] etc.) |
| Green (Right Ctrl) + Key | More symbols ($=+\_| etc.) |
| Messenger (Orange+Shift) | Caps Lock toggle |

### Modifier Keys
- **Shift** (⇧): Uppercase letters
- **Orange** (Left Ctrl): First symbol layer
- **Green** (Right Ctrl): Second symbol layer
- **Orange + Shift**: Caps Lock

## Troubleshooting

### Chatpad Not Detected
1. Check wiring - especially TX/RX not swapped
2. Verify 3.3V power (chatpad draws ~10-20mA)
3. Check serial monitor for "Xbox Chatpad initialized on UART" message
4. Try swapping TX/RX pins

### Garbled Input
1. Ensure baud rate is 19200 (hardcoded in firmware)
2. Check for loose connections
3. Verify UART pins match variant.h configuration

### Chatpad Goes to Sleep
- Firmware sends keep-alive every 1 second
- If chatpad sleeps, press any key to wake
- Deep sleep on Xiao will power down UART - chatpad will need re-init on wake

### Pin Conflicts
- UART1 (GPIO1/2) is used by default
- Ensure no other peripheral uses these pins
- GPS uses UART2 (GPIO43/44) - no conflict
- LoRa uses SPI - no conflict

## Technical Details

### Protocol
- UART: 19200 baud, 8N1
- Packet: 8 bytes
- Sync: 0xB4 (data) or 0xA5 (heartbeat)
- Byte 3: Modifiers (Shift=bit0, LeftCtrl=bit1, RightCtrl=bit2, Messenger=bit3)
- Bytes 4-5: Key codes (2 simultaneous keys)

### Firmware Architecture
- `XboxChatpadKeyboard` class in `src/input/XboxChatpadKeyboard.cpp`
- Integrates with `InputBroker` event system
- Runs as FreeRTOS task polling at 50ms interval
- Sends `InputEvent` to observers for key presses/releases

## Customization

### Change UART Pins
Modify in `variants/esp32s3/seeed_xiao_s3/variant.h`:
```cpp
#define XBOX_CHATPAD_UART_NUM 1  // UART1
#define XBOX_CHATPAD_RX_PIN 1    // GPIO1
#define XBOX_CHATPAD_TX_PIN 2    // GPIO2
```

### Use Different UART
```cpp
#define XBOX_CHATPAD_UART_NUM 2  // UART2 (if available)
#define XBOX_CHATPAD_RX_PIN 16   // Custom RX
#define XBOX_CHATPAD_TX_PIN 17   // Custom TX
```

## Known Limitations

1. **No USB HID** - Chatpad is not a standard USB HID device, requires UART
2. **Proprietary connector** - Requires soldering or breakout
3. **Single chatpad** - Only one chatpad supported per device
4. **No LED control** - Chatpad LEDs not currently controllable
5. **Deep sleep** - Chatpad loses connection during deep sleep, re-inits on wake

## References

- [frequem/Chatpad Arduino Library](https://github.com/frequem/Chatpad) - Protocol reference
- [Xbox 360 Chatpad Pinout](https://www.evan.kale.com/2019/05/xbox360-chat-pad-to-usb.html)
- [Meshtastic InputBroker Architecture](https://github.com/meshtastic/firmware/tree/master/src/input)

## License

This implementation follows the Meshtastic firmware license (GPL v3).