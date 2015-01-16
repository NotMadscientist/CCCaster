#include "ReplayManager.h"
#include "Exceptions.h"
#include "Logger.h"
#include "Messages.h"
#include "ProcessManager.h"

#include <iostream>
#include <fstream>

using namespace std;


bool ReplayManager::load ( const string& replayFile, bool real )
{
    ifstream fin ( replayFile.c_str() );
    bool good = fin.good();

    if ( good )
    {
        uint32_t gameMode;
        string netplayState;
        uint32_t index, frame;
        string tag;

        while ( fin >> gameMode >> netplayState >> index >> frame >> tag )
        {
            string str;
            stringstream ss;

            getline ( fin, str );
            ss << trimmed ( str );

            if ( index >= modes.size() )
            {
                modes.resize ( index + 1 );
                modes[index] = 0;
            }

            if ( !modes[index] )
                modes[index] = gameMode;

            ASSERT ( modes[index] == gameMode );

            if ( index >= states.size() )
                states.resize ( index + 1 );

            if ( states[index].empty() )
                states[index] = "NetplayState::" + netplayState;

            ASSERT ( states[index] == "NetplayState::" + netplayState );

            if ( tag == "Inputs" || ( real && tag == "Reinputs" ) )
            {
                if ( index >= inputs.size() )
                    inputs.resize ( index + 1 );

                ASSERT ( index + 1 == inputs.size() );

                if ( frame >= inputs[index].size() )
                    inputs[index].resize ( frame + 1 );

                Inputs i;
                i.indexedFrame.parts.index = index;
                i.indexedFrame.parts.frame = frame;
                ss >> hex >> i.p1 >> i.p2;

                inputs[index][frame] = i;
            }
            else if ( tag == "RngState" )
            {
                if ( index >= rngStates.size() )
                    rngStates.resize ( index + 1 );

                ASSERT ( rngStates[index].get() == 0 );

                char data [ sizeof ( uint32_t ) * 3 + CC_RNGSTATE3_SIZE ];

                for ( char& c : data )
                {
                    uint32_t v;
                    ss >> hex >> v;
                    c = v;
                }

                RngState *rngState = new RngState ( 0 );

                memcpy ( &rngState->rngState0, &data[0], sizeof ( uint32_t ) );
                memcpy ( &rngState->rngState1, &data[4], sizeof ( uint32_t ) );
                memcpy ( &rngState->rngState2, &data[8], sizeof ( uint32_t ) );
                copy ( &data[12], &data[12 + CC_RNGSTATE3_SIZE], rngState->rngState3.begin() );

                rngStates[index].reset ( rngState );
            }
            else if ( tag == "Rollback" )
            {
                if ( real )
                    continue;

                if ( index >= rollbacks.size() )
                    rollbacks.resize ( index + 1 );

                ASSERT ( index + 1 == rollbacks.size() );

                if ( frame >= rollbacks[index].size() )
                    rollbacks[index].resize ( frame + 1, MaxIndexedFrame );

                IndexedFrame i;
                ss >> i.parts.index >> i.parts.frame;

                rollbacks[index][frame] = i;
            }
            else if ( tag == "Reinputs" )
            {
                if ( real )
                    continue;

                if ( rollbacks.size() > reinputs.size() )
                    reinputs.resize ( rollbacks.size() );

                ASSERT ( rollbacks.size() == reinputs.size() );

                if ( rollbacks.back().size() > reinputs.back().size() )
                    reinputs.back().resize ( rollbacks.back().size() );

                ASSERT ( rollbacks.back().size() == reinputs.back().size() );

                Inputs i;
                i.indexedFrame.parts.index = index;
                i.indexedFrame.parts.frame = frame;
                ss >> hex >> i.p1 >> i.p2;

                reinputs.back().back().push_back ( i );
            }
            else if ( tag == "P1" )
            {
                if ( gameMode != CC_GAME_MODE_IN_GAME )
                    continue;

                // TODO parse initial game state
            }
            else if ( tag == "P2" )
            {
                if ( gameMode != CC_GAME_MODE_IN_GAME )
                    continue;

                // TODO parse initial game state
            }
            else
            {
                THROW_EXCEPTION ( "Unhandled tag: '%s'", "Invalid replay file!", tag );
            }
        }

        LOG ( "Processed up to [%u:%u]", inputs.size() - 1, inputs.back().size() - 1 );
    }

    fin.close();
    return good;
}

uint32_t ReplayManager::getGameMode ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= modes.size() )
        return 0;

    return modes[indexedFrame.parts.index];
}

const string& ReplayManager::getStateStr ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= states.size() )
    {
        static const string empty;
        return empty;
    }

    return states[indexedFrame.parts.index];
}

const ReplayManager::Inputs& ReplayManager::getInputs ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= inputs.size()
            || indexedFrame.parts.frame >= inputs[indexedFrame.parts.index].size() )
    {
        static const uint16_t confirm = COMBINE_INPUT ( 0, CC_BUTTON_CONFIRM );
        static const Inputs a = { MaxIndexedFrame, 0, 0 };
        static const Inputs b = { MaxIndexedFrame, confirm, confirm };

        if ( indexedFrame.parts.index < modes.size() && modes[indexedFrame.parts.index] == CC_GAME_MODE_LOADING )
            return ( indexedFrame.parts.frame % 2 ? a : b );

        return a;
    }

    return inputs[indexedFrame.parts.index][indexedFrame.parts.frame];
}

IndexedFrame ReplayManager::getRollbackTarget ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= rollbacks.size()
            || indexedFrame.parts.frame >= rollbacks[indexedFrame.parts.index].size() )
    {
        return MaxIndexedFrame;
    }

    return rollbacks[indexedFrame.parts.index][indexedFrame.parts.frame];
}

const vector<ReplayManager::Inputs>& ReplayManager::getReinputs ( IndexedFrame indexedFrame )
{
    ASSERT ( indexedFrame.parts.index < reinputs.size() );
    ASSERT ( indexedFrame.parts.frame < reinputs[indexedFrame.parts.index].size() );

    return reinputs[indexedFrame.parts.index][indexedFrame.parts.frame];
}

MsgPtr ReplayManager::getRngState ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= rngStates.size() )
        return 0;

    return rngStates[indexedFrame.parts.index];
}

uint32_t ReplayManager::getLastIndex() const
{
    if ( inputs.empty() )
        return 0;

    return inputs.size() - 1;
}

uint32_t ReplayManager::getLastFrame() const
{
    if ( inputs.empty() )
        return 0;

    if ( inputs.back().empty() )
        return 0;

    return inputs.back().size() - 1;
}
