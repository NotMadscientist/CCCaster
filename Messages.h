#pragma once

#include "Protocol.h"


struct ExitGame : public SerializableMessage
{
    inline ExitGame() {}
    MsgType getMsgType() const override;
};
