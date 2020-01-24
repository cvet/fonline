#pragma once

#include "Common.h"

class MessageBox : public StaticClass
{
public:
    static void ShowErrorMessage(const string& message, const string& traceback);
};
