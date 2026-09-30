#include "CHA10KeyboardImpl.h"
#include "InputBroker.h"
#include "configuration.h"

CHA10KeyboardImpl *cha10KeyboardImpl = nullptr;

CHA10KeyboardImpl::CHA10KeyboardImpl() : CHA10Keyboard("CHA10")
{
}

void CHA10KeyboardImpl::init()
{
#ifdef INPUTBROKER_CHA10_TYPE
#if INPUTBROKER_CHA10_TYPE == 1

#ifdef CHA10_UART_RX
    setPins(CHA10_UART_RX, CHA10_UART_TX, CHA10_UART_NUM);
#endif

#ifdef CHA10_BAUD_RATE
    setBaudRate(CHA10_BAUD_RATE);
#endif

    inputBroker->registerSource(this);

    LOG_DEBUG("CHA10Keyboard initialized");

#else
    disable();
    return;
#endif // INPUTBROKER_CHA10_TYPE == 1
#else
    disable();
    return;
#endif // INPUTBROKER_CHA10_TYPE
}