#pragma once

#include "InputBroker.h"
#include "concurrency/OSThread.h"
#include <HardwareSerial.h>

class XboxChatpadKeyboard : public Observable<const InputEvent *>, public concurrency::OSThread
{
public:
    explicit XboxChatpadKeyboard(const char *name, int uartNum, int rxPin, int txPin);
    virtual ~XboxChatpadKeyboard();
    void init();

protected:
    virtual int32_t runOnce() override;

private:
    HardwareSerial *serial;
    const char *_originName;
    int _uartNum;
    int _rxPin;
    int _txPin;
    uint8_t buffer[8];
    uint8_t lastKeys[2] = {0, 0};
    uint8_t commandKeys = 0;
    uint32_t lastKeepAlive = 0;
    bool initialized = false;

    // Protocol constants
    static constexpr uint32_t BAUD_RATE = 19200;
    static constexpr uint32_t KEEP_ALIVE_INTERVAL_MS = 1000;
    static constexpr uint8_t SYNC_BYTE_DATA = 0xB4;
    static constexpr uint8_t SYNC_BYTE_HEARTBEAT = 0xA5;

    void sendInitSequence();
    void sendKeepAlive();
    void processPacket();
    void evaluateModifiers(uint8_t modifiers);
    char chatpadToChar(uint8_t chatpadByte, uint8_t modifier);
    void notifyKeyPress(char key);
    void notifyKeyRelease(char key);
    void fixStream();
};

extern XboxChatpadKeyboard *xboxChatpadKeyboard;