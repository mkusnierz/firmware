#pragma once

#include "CHA10Keyboard.h"

class CHA10KeyboardImpl : public CHA10Keyboard
{
public:
    CHA10KeyboardImpl();
    void init();
};

extern CHA10KeyboardImpl *cha10KeyboardImpl;