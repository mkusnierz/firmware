#include "XboxChatpadKeyboard.h"
#include "configuration.h"
#include <Throttle.h>

// Modifier bit definitions (from Chatpad library protocol)
#define MODIFIER_SHIFT        0b0001
#define MODIFIER_LEFTCTRL     0b0010
#define MODIFIER_RIGHTCTRL    0b0100
#define MODIFIER_MESSENGER    0b1000

// Command key definitions
#define COMMANDKEY_CAPS_LOCK  0b0010
#define COMMANDKEY_SHIFT      0b0001
#define COMMANDKEY_CTRL       0b0100

// Key codes (from Arduino Keyboard.h)
#define KEY_UP_ARROW        0xDA
#define KEY_DOWN_ARROW      0xD9
#define KEY_LEFT_ARROW      0xD8
#define KEY_RIGHT_ARROW     0xD7
#define KEY_BACKSPACE       0xB2
#define KEY_RETURN          0xB0
#define KEY_TAB             0xB3
#define KEY_LEFT_CTRL       0x80
#define KEY_LEFT_SHIFT      0x81
#define KEY_LEFT_ALT        0x82
#define KEY_CAPS_LOCK       0xC1

XboxChatpadKeyboard *xboxChatpadKeyboard = nullptr;

XboxChatpadKeyboard::XboxChatpadKeyboard(const char *name, int uartNum, int rxPin, int txPin)
    : concurrency::OSThread(name)
{
    this->_originName = name;
    _uartNum = uartNum;
    _rxPin = rxPin;
    _txPin = txPin;
    serial = new HardwareSerial(uartNum);
}

XboxChatpadKeyboard::~XboxChatpadKeyboard()
{
    if (serial) {
        serial->end();
        delete serial;
        serial = nullptr;
    }
}

void XboxChatpadKeyboard::init()
{
    if (initialized) {
        return;
    }

    // Configure UART pins
    if (_rxPin >= 0 && _txPin >= 0) {
        serial->begin(BAUD_RATE, SERIAL_8N1, _rxPin, _txPin);
    } else {
        serial->begin(BAUD_RATE);
    }

    sendInitSequence();
    lastKeepAlive = millis();
    initialized = true;
    LOG_DEBUG("Xbox Chatpad initialized on UART%u (RX=%d, TX=%d)", _uartNum, _rxPin, _txPin);
}

void XboxChatpadKeyboard::sendInitSequence()
{
    // Init sequence from Chatpad library: 87 02 8C 1F CC
    serial->write(0x87);
    serial->write(0x02);
    serial->write(0x8C);
    serial->write(0x1F);
    serial->write(0xCC);
    serial->flush();
    delay(100); // Wait for chatpad to process init
}

void XboxChatpadKeyboard::sendKeepAlive()
{
    // Keep-alive sequence: 87 02 8C 1B D0
    serial->write(0x87);
    serial->write(0x02);
    serial->write(0x8C);
    serial->write(0x1B);
    serial->write(0xD0);
    serial->flush();
}

int32_t XboxChatpadKeyboard::runOnce()
{
    if (!initialized) {
        init();
    }

    // Send keep-alive every second
    if (millis() - lastKeepAlive >= KEEP_ALIVE_INTERVAL_MS) {
        sendKeepAlive();
        lastKeepAlive = millis();
    }

    // Need at least 8 bytes for a complete packet
    if (serial->available() >= 8) {
        serial->readBytes(buffer, 8);

        // Validate sync byte
        if (buffer[0] != SYNC_BYTE_DATA && buffer[0] != SYNC_BYTE_HEARTBEAT) {
            fixStream();
            return 50;
        }

        if (buffer[0] == SYNC_BYTE_DATA) {
            processPacket();
        }
        // Heartbeat response (0xA5) - just acknowledge, no key data
    }

    return 50; // Poll every 50ms like other keyboards
}

void XboxChatpadKeyboard::fixStream()
{
    // Re-sync stream by looking for sync byte in the buffer we already read
    for (int i = 1; i < 8; i++) {
        if (buffer[i] == SYNC_BYTE_DATA || buffer[i] == SYNC_BYTE_HEARTBEAT) {
            // Found sync byte at position i, shift buffer to start there
            // We need to read (8-i) more bytes to complete the packet
            int remaining = 8 - i;
            // Shift existing bytes to beginning
            for (int j = 0; j < i; j++) {
                buffer[j] = buffer[i + j];
            }
            // Read remaining bytes
            while (serial->available() < remaining) {
                delay(1);
            }
            serial->readBytes(&buffer[i], remaining);
            break;
        }
    }
}

void XboxChatpadKeyboard::processPacket()
{
    // Byte 3 contains modifiers
    evaluateModifiers(buffer[3]);

    // Byte 4: first key
    char k = chatpadToChar(buffer[4], buffer[3]);

    if (k != 0 && lastKeys[0] == 0) {
        // Key 1 pressed
        notifyKeyPress(k);
        lastKeys[0] = k;
        return;
    }
    if (lastKeys[0] != 0 && k == 0) {
        // Key 1 released
        notifyKeyRelease(lastKeys[0]);
        lastKeys[0] = 0;
        return;
    }
    if (k != 0 && k == lastKeys[1]) {
        // Key 2 switched to key 1
        notifyKeyRelease(lastKeys[0]);
        lastKeys[0] = lastKeys[1];
        lastKeys[1] = 0;
        return;
    }
    if (k != 0 && lastKeys[0] != 0 && k != lastKeys[0]) {
        // Key 1 changed character (e.g., caps lock)
        notifyKeyRelease(lastKeys[0]);
        notifyKeyPress(k);
        lastKeys[0] = k;
        return;
    }

    // Byte 5: second key
    k = chatpadToChar(buffer[5], buffer[3]);

    if (k != 0 && lastKeys[1] == 0) {
        // Key 2 pressed
        notifyKeyPress(k);
        lastKeys[1] = k;
        return;
    }
    if (lastKeys[1] != 0 && k == 0) {
        // Key 2 released
        notifyKeyRelease(lastKeys[1]);
        lastKeys[1] = 0;
    }
}

void XboxChatpadKeyboard::evaluateModifiers(uint8_t modifiers)
{
    // Modifier bit mappings from Chatpad library:
    // MODIFIER_SHIFT = 0b0001
    // MODIFIER_LEFTCTRL = 0b0010
    // MODIFIER_RIGHTCTRL = 0b0100
    // MODIFIER_MESSENGER = 0b1000 (orange key)

    // Command key mappings:
    // COMMANDKEY_CAPS_LOCK = 0b0010 (Shift + RightCtrl)
    // COMMANDKEY_SHIFT = 0b0001 (Shift)
    // COMMANDKEY_CTRL = 0b0100 (LeftCtrl)

    static const uint8_t modMasks[3] = {
        MODIFIER_SHIFT | MODIFIER_RIGHTCTRL,  // Caps Lock
        MODIFIER_SHIFT,                        // Shift
        MODIFIER_LEFTCTRL                      // Ctrl
    };
    static const uint8_t comMasks[3] = {
        0b0010,  // COMMANDKEY_CAPS_LOCK
        0b0001,  // COMMANDKEY_SHIFT
        0b0100   // COMMANDKEY_CTRL
    };
    static const char comKeys[3] = {
        0xC1,  // KEY_CAPS_LOCK
        0x81,  // KEY_LEFT_SHIFT
        0x80   // KEY_LEFT_CTRL
    };

    for (int i = 0; i < 3; i++) {
        bool modActive = (modifiers & modMasks[i]) == modMasks[i];
        bool comActive = commandKeys & comMasks[i];

        if (modActive && !comActive) {
            commandKeys |= comMasks[i];
            notifyKeyPress(comKeys[i]);
        } else if (!modActive && comActive) {
            commandKeys &= ~comMasks[i];
            notifyKeyRelease(comKeys[i]);
        }
    }
}

// Key code mappings from Chatpad library
char XboxChatpadKeyboard::chatpadToChar(uint8_t chatpadByte, uint8_t modifier)
{
    char c = 0;

    switch (chatpadByte) {
    case 0x65: return '0';
    case 0x17: return '1';
    case 0x16: return '2';
    case 0x15: return '3';
    case 0x14: return '4';
    case 0x13: return '5';
    case 0x12: return '6';
    case 0x11: return '7';
    case 0x67: return '8';
    case 0x66: return '9';
    case 0x37: c = 'a'; break;
    case 0x42: c = 'b'; break;
    case 0x44: c = 'c'; break;
    case 0x35: c = 'd'; break;
    case 0x25: c = 'e'; break;
    case 0x34: c = 'f'; break;
    case 0x33: c = 'g'; break;
    case 0x32: c = 'h'; break;
    case 0x76: c = 'i'; break;
    case 0x31: c = 'j'; break;
    case 0x77: c = 'k'; break;
    case 0x72: c = 'l'; break;
    case 0x52: c = 'm'; break;
    case 0x41: c = 'n'; break;
    case 0x75: c = 'o'; break;
    case 0x64: c = 'p'; break;
    case 0x27: c = 'q'; break;
    case 0x24: c = 'r'; break;
    case 0x36: c = 's'; break;
    case 0x23: c = 't'; break;
    case 0x21: c = 'u'; break;
    case 0x43: c = 'v'; break;
    case 0x26: c = 'w'; break;
    case 0x45: c = 'x'; break;
    case 0x22: c = 'y'; break;
    case 0x46: c = 'z'; break;
    case 0x54: return ' ';
    case 0x62: c = ','; break;
    case 0x53: c = '.'; break;
    case 0x71: return 0x08;  // KEY_BACKSPACE
    case 0x51: return 0xD7;  // KEY_RIGHT_ARROW
    case 0x55: return 0xD8;  // KEY_LEFT_ARROW
    case 0x63: return 0x0D;  // KEY_RETURN (Enter)
    default: return 0;
    }

    // Apply Shift modifier
    if (modifier & MODIFIER_SHIFT) {
        if (c >= 'a' && c <= 'z') {
            return c - 32; // Uppercase
        }
    }

    // Apply LeftCtrl modifier (orange key combinations)
    if (modifier & MODIFIER_LEFTCTRL) {
        switch (c) {
        case 'q': return '!';
        case 'w': return '@';
        case 'r': return '#';
        case 't': return '%';
        case 'y': return '^';
        case 'u': return '&';
        case 'i': return '*';
        case 'o': return '(';
        case 'p': return ')';
        case 'a': return '~';
        case 'd': return '{';
        case 'f': return '}';
        case 'h': return '/';
        case 'j': return '\'';
        case 'k': return '[';
        case 'l': return ']';
        case ',': return ':';
        case 'z': return '`';
        case 'v': return '-';
        case 'b': return '|';
        case 'n': return '<';
        case 'm': return '>';
        case '.': return '?';
        case KEY_RIGHT_ARROW: return KEY_DOWN_ARROW; // Right arrow -> Down arrow
        case KEY_LEFT_ARROW: return KEY_UP_ARROW;    // Left arrow -> Up arrow
        }
    }

    // Apply RightCtrl modifier (green key combinations)
    if (modifier & MODIFIER_RIGHTCTRL) {
        switch (c) {
        case 'r': return '$';
        case 'p': return '=';
        case 'h': return '\\';
        case 'j': return '"';
        case ',': return ';';
        case 'v': return '_';
        case 'b': return '+';
        case KEY_RIGHT_ARROW: return KEY_DOWN_ARROW; // Right arrow -> Down arrow
        case KEY_LEFT_ARROW: return KEY_UP_ARROW;    // Left arrow -> Up arrow
        }
    }

    return c;
}

void XboxChatpadKeyboard::notifyKeyPress(char key)
{
    InputEvent e = {};
    e.source = this->_originName;

    // Map special keys to Meshtastic input events
    switch (key) {
    case KEY_RETURN: // Enter
        e.inputEvent = INPUT_BROKER_SELECT;
        e.kbchar = 0;
        break;
    case KEY_BACKSPACE: // Backspace
        e.inputEvent = INPUT_BROKER_BACK;
        e.kbchar = 0x08;
        break;
    case KEY_TAB: // Tab
        e.inputEvent = INPUT_BROKER_ANYKEY;
        e.kbchar = INPUT_BROKER_MSG_TAB;
        break;
    case 0x1B: // Escape
        e.inputEvent = INPUT_BROKER_CANCEL;
        e.kbchar = 0;
        break;
    case KEY_UP_ARROW: // Up arrow
        e.inputEvent = INPUT_BROKER_UP;
        e.kbchar = 0;
        break;
    case KEY_DOWN_ARROW: // Down arrow
        e.inputEvent = INPUT_BROKER_DOWN;
        e.kbchar = 0;
        break;
    case KEY_LEFT_ARROW: // Left arrow
        e.inputEvent = INPUT_BROKER_LEFT;
        e.kbchar = 0;
        break;
    case KEY_RIGHT_ARROW: // Right arrow
        e.inputEvent = INPUT_BROKER_RIGHT;
        e.kbchar = 0;
        break;
    case KEY_CAPS_LOCK: // Caps Lock
    case KEY_LEFT_SHIFT: // Left Shift
    case KEY_LEFT_CTRL: // Left Ctrl
        // Modifier keys - track but don't send as text
        return;
    default:
        // Regular character input
        if (key >= 32 && key <= 126) {
            e.inputEvent = INPUT_BROKER_ANYKEY;
            e.kbchar = key;
        } else {
            return; // Ignore non-printable
        }
        break;
    }

    if (e.inputEvent != INPUT_BROKER_NONE) {
        this->notifyObservers(&e);
    }
}

void XboxChatpadKeyboard::notifyKeyRelease(char key)
{
    // For most keys, we only send press events
    // Modifier keys (shift, ctrl) are tracked internally
    // Other keys are momentary - no release event needed for text input

    // Send release events for navigation keys for proper repeat handling
    InputEvent e = {};
    e.source = this->_originName;
    e.inputEvent = INPUT_BROKER_NONE;
    e.kbchar = 0;

    switch (key) {
    case KEY_UP_ARROW:
        e.inputEvent = INPUT_BROKER_UP;
        break;
    case KEY_DOWN_ARROW:
        e.inputEvent = INPUT_BROKER_DOWN;
        break;
    case KEY_LEFT_ARROW:
        e.inputEvent = INPUT_BROKER_LEFT;
        break;
    case KEY_RIGHT_ARROW:
        e.inputEvent = INPUT_BROKER_RIGHT;
        break;
    default:
        return;
    }

    if (e.inputEvent != INPUT_BROKER_NONE) {
        this->notifyObservers(&e);
    }
}