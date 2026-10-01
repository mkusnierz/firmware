#pragma once
#include "XboxChatpadKeyboard.h"

class XboxChatpadKeyboardImpl : public XboxChatpadKeyboard
{
public:
    XboxChatpadKeyboardImpl();
    void init();
};

extern XboxChatpadKeyboardImpl *xboxChatpadKeyboardImpl;