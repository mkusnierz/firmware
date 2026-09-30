#pragma once

#include "InputBroker.h"
#include "concurrency/OSThread.h"
#include <HardwareSerial.h>

class CHA10Keyboard : public Observable<const InputEvent *>, public concurrency::OSThread
{
public:
    explicit CHA10Keyboard(const char *name = "CHA10");
    ~CHA10Keyboard();

    void setPins(int rxPin, int txPin, int uartNum = 1);
    void setBaudRate(uint32_t baud = 9600);

protected:
    virtual int32_t runOnce() override;

private:
    HardwareSerial *uart = nullptr;
    int _rxPin = -1;
    int _txPin = -1;
    int _uartNum = 1;
    uint32_t _baud = 9600;

    enum HandshakeState {
        HS_AT,
        HS_CGMM,
        HS_CGMR,
        HS_EDME,
        HS_ESVM_Q,
        HS_ESVM_0,
        HS_CKPD,
        HS_ESVM_1,
        HS_READY
    } hsState = HS_AT;

    uint8_t lastKey = 0xFF;
    uint8_t pressCount = 0;
    uint32_t lastPressTime = 0;
    bool alphaMode = false;

    void sendOK();
    void processHandshake(const char *line);
    void processKeypress(const char *ckpdValue);
    void emitKey(char c);
    void emitNav(input_broker_event evt);
    char decodeMultiTap(char key, uint8_t count);
    uint8_t getMaxTaps(char key);

public:  // For unit testing
    static const char *charmap[12];
};