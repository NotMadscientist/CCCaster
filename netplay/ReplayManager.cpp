#include "ReplayManager.hpp"
#include "Exceptions.hpp"
#include "Logger.hpp"
#include "Messages.hpp"

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

            if ( index >= _modes.size() )
            {
                _modes.resize ( index + 1 );
                _modes[index] = 0;
            }

            if ( ! _modes[index] )
                _modes[index] = gameMode;

            ASSERT ( _modes[index] == gameMode );

            if ( index >= _states.size() )
                _states.resize ( index + 1 );

            if ( _states[index].empty() )
                _states[index] = "NetplayState::" + netplayState;

            if ( gameMode == CC_GAME_MODE_LOADING )
            {
                if ( _initialStates.empty() )
                    _initialStates.push_back ( MsgPtr ( new InitialGameState ( { 0, index } ) ) );

                ASSERT ( _initialStates.back().get() != 0 );

                if ( _initialStates.back()->getAs<InitialGameState>().indexedFrame.parts.index != index )
                    _initialStates.push_back ( MsgPtr ( new InitialGameState ( { 0, index } ) ) );
            }

            ASSERT ( _states[index] == "NetplayState::" + netplayState );

            if ( tag == "Inputs" || ( real && tag == "Reinputs" ) )
            {
                if ( index >= _inputs.size() )
                    _inputs.resize ( index + 1 );

                ASSERT ( index + 1 == _inputs.size() );

                if ( frame >= _inputs[index].size() )
                    _inputs[index].resize ( frame + 1 );

                Inputs i;
                i.indexedFrame.parts.index = index;
                i.indexedFrame.parts.frame = frame;
                ss >> hex >> i.p1 >> i.p2;

                _inputs[index][frame] = i;
            }
            else if ( tag == "RngState" )
            {
                if ( index >= _rngStates.size() )
                    _rngStates.resize ( index + 1 );

                ASSERT ( _rngStates[index].get() == 0 );

                RngState *rngState = 0;

                if ( ss.str().size() == 707 ) // Old RngState hex dump size
                {
                    rngState = new RngState ( 0 );

                    char data [ sizeof ( uint32_t ) * 3 + 4 + CC_RNG_STATE3_SIZE ];

                    for ( char& c : data )
                    {
                        uint32_t v;
                        ss >> hex >> v;
                        c = v;
                    }

                    memcpy ( &rngState->rngState0, &data[0], sizeof ( uint32_t ) );
                    memcpy ( &rngState->rngState1, &data[4], sizeof ( uint32_t ) );
                    memcpy ( &rngState->rngState2, &data[8], sizeof ( uint32_t ) );
                    copy ( &data[16], &data[16 + CC_RNG_STATE3_SIZE], rngState->rngState3.begin() );
                }
                else if ( ss.str().size() == 695 ) // New RngState hex dump size
                {
                    rngState = new RngState ( 0 );

                    char data [ sizeof ( uint32_t ) * 3 + CC_RNG_STATE3_SIZE ];

                    for ( char& c : data )
                    {
                        uint32_t v;
                        ss >> hex >> v;
                        c = v;
                    }

                    memcpy ( &rngState->rngState0, &data[0], sizeof ( uint32_t ) );
                    memcpy ( &rngState->rngState1, &data[4], sizeof ( uint32_t ) );
                    memcpy ( &rngState->rngState2, &data[8], sizeof ( uint32_t ) );
                    copy ( &data[12], &data[12 + CC_RNG_STATE3_SIZE], rngState->rngState3.begin() );
                }
                else
                {
                    THROW_EXCEPTION ( "Unknown RngState size: %u", "Invalid replay file!", ss.str().size() );
                }

                _rngStates[index].reset ( rngState );
            }
            else if ( tag == "Rollback" )
            {
                if ( real )
                    continue;

                if ( index >= _rollbacks.size() )
                    _rollbacks.resize ( index + 1 );

                ASSERT ( index + 1 == _rollbacks.size() );

                if ( frame >= _rollbacks[index].size() )
                    _rollbacks[index].resize ( frame + 1, MaxIndexedFrame );

                IndexedFrame i;
                ss >> i.parts.index >> i.parts.frame;

                _rollbacks[index][frame] = i;
            }
            else if ( tag == "Reinputs" )
            {
                if ( real )
                    continue;

                if ( _rollbacks.size() > _reinputs.size() )
                    _reinputs.resize ( _rollbacks.size() );

                ASSERT ( _rollbacks.size() == _reinputs.size() );

                if ( _rollbacks.back().size() > _reinputs.back().size() )
                    _reinputs.back().resize ( _rollbacks.back().size() );

                ASSERT ( _rollbacks.back().size() == _reinputs.back().size() );

                Inputs i;
                i.indexedFrame.parts.index = index;
                i.indexedFrame.parts.frame = frame;
                ss >> hex >> i.p1 >> i.p2;

                _reinputs.back().back().push_back ( i );
            }
            else if ( tag == "P1" )
            {
                if ( gameMode != CC_GAME_MODE_IN_GAME )
                    continue;

                ASSERT ( _initialStates.back().get() != 0 );

                uint32_t chara, moon, color;
                ss >> chara >> moon >> color;

                _initialStates.back()->getAs<InitialGameState>().chara[0] = chara;
                _initialStates.back()->getAs<InitialGameState>().moon[0] = moon;
                _initialStates.back()->getAs<InitialGameState>().color[0] = color;
            }
            else if ( tag == "P2" )
            {
                if ( gameMode != CC_GAME_MODE_IN_GAME )
                    continue;

                ASSERT ( _initialStates.back().get() != 0 );

                uint32_t chara, moon, color;
                ss >> chara >> moon >> color;

                _initialStates.back()->getAs<InitialGameState>().chara[1] = chara;
                _initialStates.back()->getAs<InitialGameState>().moon[1] = moon;
                _initialStates.back()->getAs<InitialGameState>().color[1] = color;
            }
            else
            {
                THROW_EXCEPTION ( "Unhandled tag: '%s'", "Invalid replay file!", tag );
            }
        }

        LOG ( "Processed up to [%u:%u]", _inputs.size() - 1, _inputs.back().size() - 1 );
    }

    fin.close();
    return good;
}

uint32_t ReplayManager::getGameMode ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= _modes.size() )
        return 0;

    return _modes[indexedFrame.parts.index];
}

const string& ReplayManager::getStateStr ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= _states.size() )
    {
        static const string empty;
        return empty;
    }

    return _states[indexedFrame.parts.index];
}

const ReplayManager::Inputs& ReplayManager::getInputs ( IndexedFrame indexedFrame )
{
    static const Inputs confirm = { MaxIndexedFrame, CC_BUTTON_CONFIRM << 4, CC_BUTTON_CONFIRM << 4 };
    static const Inputs down = { MaxIndexedFrame, 2, 2 };
    static const Inputs empty = { MaxIndexedFrame, 0, 0 };

    if ( _modes[indexedFrame.parts.index] == CC_GAME_MODE_LOADING )
        return ( ( indexedFrame.parts.frame % 2 ) ? empty : confirm );

    if ( _modes[indexedFrame.parts.index] == CC_GAME_MODE_RETRY )
    {
        if ( _modes[indexedFrame.parts.index + 1] == CC_GAME_MODE_LOADING )
            return ( ( indexedFrame.parts.frame % 2 ) ? empty : confirm );

        if ( indexedFrame.parts.frame == 30 )
            return confirm;

        if ( indexedFrame.parts.frame == 120 )
            return down;

        if ( indexedFrame.parts.frame == 125 )
            return confirm;

        return empty;
    }

    if ( indexedFrame.parts.index >= _inputs.size()
            || indexedFrame.parts.frame >= _inputs[indexedFrame.parts.index].size() )
    {
        return empty;
    }

    return _inputs[indexedFrame.parts.index][indexedFrame.parts.frame];
}

IndexedFrame ReplayManager::getRollbackTarget ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= _rollbacks.size()
            || indexedFrame.parts.frame >= _rollbacks[indexedFrame.parts.index].size() )
    {
        return MaxIndexedFrame;
    }

    return _rollbacks[indexedFrame.parts.index][indexedFrame.parts.frame];
}

const vector<ReplayManager::Inputs>& ReplayManager::getReinputs ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= _reinputs.size()
            || indexedFrame.parts.frame >= _reinputs[indexedFrame.parts.index].size() )
    {
        static const vector<ReplayManager::Inputs> empty;
        return empty;
    }

    return _reinputs[indexedFrame.parts.index][indexedFrame.parts.frame];
}

MsgPtr ReplayManager::getRngState ( IndexedFrame indexedFrame )
{
    if ( indexedFrame.parts.index >= _rngStates.size() )
        return 0;

    return _rngStates[indexedFrame.parts.index];
}

uint32_t ReplayManager::getLastIndex() const
{
    if ( _inputs.empty() )
        return 0;

    return _inputs.size() - 1;
}

uint32_t ReplayManager::getLastFrame() const
{
    if ( _inputs.empty() )
        return 0;

    if ( _inputs.back().empty() )
        return 0;

    return _inputs.back().size() - 1;
}

MsgPtr ReplayManager::getInitialStateBefore ( uint32_t index ) const
{
    for ( int i = _initialStates.size() - 1; i >= 0; --i )
    {
        ASSERT ( _initialStates[i].get() != 0 );

        if ( _initialStates[i]->getAs<InitialGameState>().indexedFrame.parts.index < index )
            return _initialStates[i];
    }

    return 0;
}
