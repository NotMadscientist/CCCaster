#include "Messages.h"
#include "NetplayManager.h"


void InitialGameState::save ( cereal::BinaryOutputArchive& ar ) const
{
    ar ( indexedFrame.value, stage, netplayState, isTraining, character, moon, color );

    if ( netplayState == NetplayState::CharaSelect )
        ar ( charaSelector, charaSelectMode, isRandomColor );
}

void InitialGameState::load ( cereal::BinaryInputArchive& ar )
{
    ar ( indexedFrame.value, stage, netplayState, isTraining, character, moon, color );

    if ( netplayState == NetplayState::CharaSelect )
    {
        ar ( charaSelector, charaSelectMode, isRandomColor );
    }
    else
    {
        charaSelector[0] = charaToSelector ( character[0] );
        charaSelector[1] = charaToSelector ( character[1] );
    }
}
