#include "XboxChatpadKeyboardImpl.h"
#include "InputBroker.h"
#include "configuration.h"

XboxChatpadKeyboardImpl *xboxChatpadKeyboardImpl = nullptr;

XboxChatpadKeyboardImpl::XboxChatpadKeyboardImpl()
    : XboxChatpadKeyboard("xboxChatpad",
#if defined(XBOX_CHATPAD_UART_NUM)
                          XBOX_CHATPAD_UART_NUM
#else
                          1
#endif
#if defined(XBOX_CHATPAD_RX_PIN)
                          ,
                          XBOX_CHATPAD_RX_PIN
#else
                          ,
                          1
#endif
#if defined(XBOX_CHATPAD_TX_PIN)
                          ,
                          XBOX_CHATPAD_TX_PIN
#else
                          ,
                          2
#endif
                          )
{
}

void XboxChatpadKeyboardImpl::init()
{
#if !MESHTASTIC_EXCLUDE_INPUTBROKER
    XboxChatpadKeyboard::init();
    inputBroker->registerSource(this);
#endif
}