#include "CHA10Keyboard.h"
#include "configuration.h"
#include <Arduino.h>
#include <Throttle.h>

const char *CHA10Keyboard::charmap[12] = {
    " -?!,.:;\"'<=>()1",  // 0
    "abc2",               // 1
    "def3",               // 2
    "ghi4",               // 3
    "jkl5",               // 4
    "mno6",               // 5
    "pqrs7",              // 6
    "tuv8",               // 7
    "wxyz9",              // 8
    "+&@/%$£0",           // 9
    "*",                  // * (key 10)
    "#"                   // # (key 11)
};

CHA10Keyboard::CHA10Keyboard(const char *name) : concurrency::OSThread(name)
{
}

CHA10Keyboard::~CHA10Keyboard()
{
    if (uart) {
        uart->end();
        uart = nullptr;
    }
}

void CHA10Keyboard::setPins(int rxPin, int txPin, int uartNum)
{
    _rxPin = rxPin;
    _txPin = txPin;
    _uartNum = uartNum;
}

void CHA10Keyboard::setBaudRate(uint32_t baud)
{
    _baud = baud;
}

void CHA10Keyboard::sendOK()
{
    if (uart) {
        uart->print("OK\r\n");
    }
}

uint8_t CHA10Keyboard::getMaxTaps(char key)
{
    int idx = -1;
    if (key >= '0' && key <= '9') {
        idx = key - '0';
    } else if (key == '*') {
        idx = 10;
    } else if (key == '#') {
        idx = 11;
    }
    if (idx >= 0 && idx < 12) {
        return strlen(charmap[idx]);
    }
    return 1;
}

char CHA10Keyboard::decodeMultiTap(char key, uint8_t count)
{
    int idx = -1;
    if (key >= '0' && key <= '9') {
        idx = key - '0';
    } else if (key == '*') {
        idx = 10;
    } else if (key == '#') {
        idx = 11;
    }
    if (idx >= 0 && idx < 12) {
        const char *map = charmap[idx];
        uint8_t len = strlen(map);
        if (count < len) {
            return map[count];
        }
        return map[len - 1];
    }
    return 0;
}

void CHA10Keyboard::emitKey(char c)
{
    InputEvent e = {};
    e.inputEvent = INPUT_BROKER_ANYKEY;
    e.kbchar = c;
    e.source = this->_originName;
    this->notifyObservers(&e);
}

void CHA10Keyboard::emitNav(input_broker_event evt)
{
    InputEvent e = {};
    e.inputEvent = evt;
    e.kbchar = 0;
    e.source = this->_originName;
    this->notifyObservers(&e);
}

void CHA10Keyboard::processHandshake(const char *line)
{
    LOG_DEBUG("CHA10 HS: %s", line);

    switch (hsState) {
        case HS_AT:
            if (strcmp(line, "AT") == 0) {
                sendOK();
                hsState = HS_CGMM;
            }
            break;
        case HS_CGMM:
            if (strcmp(line, "AT+CGMM") == 0) {
                sendOK();
                hsState = HS_CGMR;
            }
            break;
        case HS_CGMR:
            if (strcmp(line, "AT+CGMR") == 0) {
                sendOK();
                hsState = HS_EDME;
            }
            break;
        case HS_EDME:
            if (strcmp(line, "AT*EDME?") == 0) {
                sendOK();
                hsState = HS_ESVM_Q;
            }
            break;
        case HS_ESVM_Q:
            if (strcmp(line, "AT*ESVM?") == 0) {
                sendOK();
                hsState = HS_ESVM_0;
            }
            break;
        case HS_ESVM_0:
            if (strcmp(line, "AT*ESVM=0") == 0) {
                sendOK();
                hsState = HS_CKPD;
            }
            break;
        case HS_CKPD:
            if (strncmp(line, "AT+CKPD", 7) == 0) {
                sendOK();
                hsState = HS_ESVM_1;
            }
            break;
        case HS_ESVM_1:
            if (strcmp(line, "AT*ESVM=1") == 0) {
                sendOK();
                hsState = HS_READY;
                LOG_DEBUG("CHA10 handshake complete");
            }
            break;
        default:
            break;
    }
}

void CHA10Keyboard::processKeypress(const char *value)
{
    // Handle repeat prefix (1c or 0c)
    bool isRepeat = (strncmp(value, "1c", 2) == 0 || strncmp(value, "0c", 2) == 0);
    if (isRepeat) {
        value += 2;
    }

    // Handle long press suffix (,20)
    bool longPress = false;
    size_t len = strlen(value);
    if (len >= 3 && strcmp(value + len - 3, ",20") == 0) {
        longPress = true;
        len -= 3;  // Exclude the ",20" suffix from processing
    }

    // Single repeated char = multi-tap
    char key = value[0];
    bool allSame = true;
    for (size_t i = 1; i < len; i++) {
        if (value[i] != key) {
            allSame = false;
            break;
        }
    }

    if (allSame) {
        uint8_t count = len;
        uint32_t now = millis();

        if (key == lastKey && Throttle::isWithinTimespanMs(lastPressTime, 500)) {
            pressCount = (pressCount + 1) % getMaxTaps(key);
        } else {
            pressCount = 0;
        }

        lastKey = key;
        lastPressTime = now;

        // Handle special keys
        if (key == 's') {  // YES key
            if (longPress) {
                emitNav(INPUT_BROKER_SELECT);  // Enter
            } else {
                emitNav(INPUT_BROKER_USER_PRESS);  // Yes
            }
            alphaMode = false;  // Returns to numeric mode
        } else if (key == 'e') {  // NO key
            if (longPress) {
                emitNav(INPUT_BROKER_CANCEL);  // Escape
            } else {
                emitNav(INPUT_BROKER_BACK);  // No/Back
            }
            alphaMode = false;  // Returns to numeric mode
        } else if (key == '<') {  // Left arrow
            if (longPress) {
                emitNav(INPUT_BROKER_LEFT);  // Home (long press)
            } else {
                emitNav(INPUT_BROKER_LEFT);
            }
        } else if (key == '>') {  // Right arrow
            if (longPress) {
                emitNav(INPUT_BROKER_RIGHT);  // End (long press)
            } else {
                emitNav(INPUT_BROKER_RIGHT);
            }
        } else if (key == 'c') {  // Clear/Backspace
            if (longPress) {
                emitKey(0x08);  // Ctrl+Backspace (send backspace)
            } else {
                emitKey(0x08);  // Backspace
            }
        } else if (key == '*') {
            if (longPress) {
                emitKey('8');
            } else if (alphaMode) {
                emitKey('*');
            } else {
                emitKey('*');
            }
        } else if (key == '#') {
            if (longPress) {
                emitKey('3');
            } else if (alphaMode) {
                emitKey('#');
            } else {
                emitKey('#');
            }
        } else {
            // Normal multi-tap character
            char decoded = decodeMultiTap(key, pressCount);
            if (decoded) {
                // Check if we should be in alpha mode
                if (!alphaMode && (key >= '2' && key <= '9')) {
                    // In numeric mode, digits just emit as-is
                    emitKey(decoded);
                } else {
                    emitKey(decoded);
                }
            }
        }
    } else {
        // Handle SMS/eMail key (switches to alpha mode)
        // The protocol doesn't explicitly send a key for mode switch,
        // but the Python code detects it via the AT command sequence.
        // We'll track alpha mode based on key patterns.
        LOG_DEBUG("CHA10: non-uniform keypress: %s", value);
    }
}

int32_t CHA10Keyboard::runOnce()
{
    // Initialize UART on first run
    if (!uart) {
        switch (_uartNum) {
            case 0:
                uart = &Serial;
                break;
            case 1:
                uart = &Serial1;
                break;
            case 2:
                uart = &Serial2;
                break;
            default:
                uart = &Serial;
                break;
        }

        if (_rxPin >= 0 && _txPin >= 0) {
            uart->begin(_baud, SERIAL_8N1, _rxPin, _txPin);
        } else {
            uart->begin(_baud);
        }

        LOG_DEBUG("CHA10 UART initialized on UART%d (RX=%d, TX=%d) at %d baud",
                  _uartNum, _rxPin, _txPin, _baud);

        // Reset handshake state
        hsState = HS_AT;
        alphaMode = false;
        lastKey = 0xFF;
        pressCount = 0;
        lastPressTime = 0;

        return 100;  // Check again soon
    }

    // Process incoming data
    while (uart->available()) {
        String line = uart->readStringUntil('\r');
        line.trim();

        if (line.length() == 0) {
            continue;
        }

        LOG_DEBUG("CHA10 RX: %s", line.c_str());

        if (hsState < HS_READY) {
            processHandshake(line.c_str());
        } else if (line.startsWith("AT+CKPD=")) {
            // Extract value between quotes
            int q1 = line.indexOf('"');
            int q2 = line.lastIndexOf('"');
            if (q1 >= 0 && q2 > q1) {
                String val = line.substring(q1 + 1, q2);
                processKeypress(val.c_str());
            }
            sendOK();
        } else if (line.startsWith("AT")) {
            // ACK any other AT command during ready state
            sendOK();
        }
    }

    // Check for multi-tap timeout (500ms)
    if (lastKey != 0xFF && !Throttle::isWithinTimespanMs(lastPressTime, 500)) {
        // Timeout - reset multi-tap state
        lastKey = 0xFF;
        pressCount = 0;
    }

    return 50;  // Poll every 50ms
}