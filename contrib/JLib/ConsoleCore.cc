// Title: Jeff Benson's Console Library
// File: ConsoleCore.cpp
// Author: Jeff Benson
// Date: 6/26/03
// Last Updated: 8/2/2011
// Contact: pepsibot@hotmail.com
//
// Copyright (C) 2011
// This file is part of JLib.
//
// JLib is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// JLib is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with JLib.  If not, see <http://www.gnu.org/licenses/>.
#include "ConsoleCore.h"
#include "CodeFinder.h"

#include <cctype>
#include <cstdio>
#include <pthread.h>

using namespace std;

#ifdef JLIB_MUTEXED
static pthread_mutex_t mutex;
#endif

void ConsoleCore::UpdateWindowSize()
{
    // Get the actual console window size
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    MAXSCREENX = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    MAXSCREENY = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    // Increase the buffer size if needed
    if (m_screenBuffer.size() < (size_t)MAXSCREENX*MAXSCREENY)
         m_screenBuffer.resize(MAXSCREENX*MAXSCREENY);
}

ConsoleCore* ConsoleCore::m_theOnlyInstance = new ConsoleCore();

ConsoleCore* ConsoleCore::GetInstance()
{
    return m_theOnlyInstance;
}

ConsoleCore::ConsoleCore()
{
    UpdateWindowSize();
#ifdef JLIB_MUTEXED
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mutex, &attr);
#endif
    // The console begins by placing the cursor in the upper left
    // console corner.  The default fore and back ground are
    // white and black respectivley.
    m_cursorPosition.X = 0;
    m_cursorPosition.Y = 0;
    m_consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    m_currentFormat.Color(ConsoleFormat::SYSTEM);

    GetConsoleScreenBufferInfo(m_consoleHandle,&m_csbi);

    // // Get the screen handle and screen buffer info
    // ClearScreen();
    // SaveScreen();
    // // Set the saved screen to blank.
}

ConsoleCore::~ConsoleCore()
{
    ClearScreen();
    CloseHandle(m_consoleHandle);
    delete m_theOnlyInstance;
#ifdef JLIB_MUTEXED
    pthread_mutex_destroy(&mutex);
#endif
}

void ConsoleCore::ClearScreen()
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    UpdateWindowSize();
    LPWORD lpAttributes = new WORD[MAXSCREENX*MAXSCREENY];
    memset((void*)lpAttributes,0x00,sizeof(WORD)*MAXSCREENX*MAXSCREENY);
    DWORD whatWasWritten;
    COORD origin = {0,0};
    FillConsoleOutputCharacter( m_consoleHandle, ' ', m_csbi.dwSize.X * m_csbi.dwSize.Y, m_cursorPosition, &whatWasWritten );
    WriteConsoleOutputAttribute(m_consoleHandle, lpAttributes,
        MAXSCREENX*MAXSCREENY,origin,&whatWasWritten);
    delete [] lpAttributes;
    CursorPosition(&origin);
    // Place the cursor back on 0,0.
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
}

void ConsoleCore::SaveScreen()
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    UpdateWindowSize();
    COORD bufferSize = {MAXSCREENX,MAXSCREENY},
        bufferOrigin = {0,0};
    SMALL_RECT rectToRead = {0,0,MAXSCREENX,MAXSCREENY};
    if (m_screenBuffer.size() != (size_t)MAXSCREENX*MAXSCREENY)
         m_screenBuffer.resize(MAXSCREENX*MAXSCREENY);
    ReadConsoleOutput(m_consoleHandle,&m_screenBuffer[0],
        bufferSize,bufferOrigin,&rectToRead);
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
}

void ConsoleCore::SaveScreen(PCHAR_INFO buffer, COORD bufferSize, COORD saveOrigin)
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    COORD bufferOrigin = {0,0};
    SMALL_RECT rectToRead = {saveOrigin.X,saveOrigin.Y,
        saveOrigin.X + bufferSize.X, saveOrigin.Y + bufferSize.Y};
    ReadConsoleOutput(m_consoleHandle,buffer,
        bufferSize,bufferOrigin,&rectToRead);
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
}
void ConsoleCore::LoadScreen()
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    UpdateWindowSize();
    COORD bufferSize = {MAXSCREENX,MAXSCREENY},
        bufferOrigin = {0,0};
    SMALL_RECT rectToWrite = {0,0,MAXSCREENX,MAXSCREENY};
    WriteConsoleOutput(m_consoleHandle,&m_screenBuffer[0],
        bufferSize,bufferOrigin,&rectToWrite);
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
}

void ConsoleCore::LoadScreen(PCHAR_INFO buffer, COORD bufferSize, COORD loadOrigin)
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    COORD bufferOrigin = {0,0};
    SMALL_RECT rectToWrite = {loadOrigin.X,loadOrigin.Y,
        loadOrigin.X + bufferSize.X, loadOrigin.Y + bufferSize.Y};
    WriteConsoleOutput(m_consoleHandle,buffer,
        bufferSize,bufferOrigin,&rectToWrite);
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
}

// Mutator/Accessor combo for the default format
// Pass NULL to get the current value
// Pass a pointer to a CONSOLECHARFROMAT to change
// the color, and get the old color in return
ConsoleFormat ConsoleCore::Color(const ConsoleFormat* newFormat)
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    ConsoleFormat fmt;
    if(newFormat != NULL)
    {
        fmt = m_currentFormat;
        m_currentFormat = *newFormat;
    }
    else
        fmt = m_currentFormat;

#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
    return fmt;
}


// Mutator/Accessor combo for the cursor position
// Pass NULL to get the current value
// Pass a pointer to a COORD to change the position
// and get the old position back in return
COORD ConsoleCore::CursorPosition(PCOORD lpPosition)
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    UpdateWindowSize();
    COORD pos;
    if(lpPosition != NULL)
    {
        pos = m_cursorPosition;
        m_cursorPosition = *lpPosition;
        m_cursorPosition.X %= MAXSCREENX;
        m_cursorPosition.Y %= MAXSCREENY;
        SetConsoleCursorPosition(m_consoleHandle,m_cursorPosition);
    }
    else
        pos = m_cursorPosition;
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
    return pos;
}

COORD ConsoleCore::CursorPosition(SHORT x, SHORT y)
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    UpdateWindowSize();
    COORD old = m_cursorPosition;
    if(x != -1)
        m_cursorPosition.X = x;
    if(y != -1)
        m_cursorPosition.Y = y;
    m_cursorPosition.X %= MAXSCREENX;
    m_cursorPosition.Y %= MAXSCREENY;
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
    return old;
}

void ConsoleCore::Printn(int number, BOOL endLine, ConsoleFormat* color, SHORT x, SHORT y)
{
    char numberAsText[256];
    snprintf(numberAsText, sizeof(numberAsText), "%d", number);
    Prints(numberAsText,endLine,color,x,y);
}
void ConsoleCore::Printd(double number, int characterLength, BOOL endLine, ConsoleFormat* color, SHORT x, SHORT y)
{
    char numberAsText[256];
    snprintf(numberAsText, sizeof(numberAsText), "%f", number);
    Prints(numberAsText,endLine,color,x,y);
}

void ConsoleCore::Prints(const string& text, BOOL endLine, const ConsoleFormat* color, SHORT x, SHORT y)
{
    _Prints(text, endLine, color, x, y);
}

void ConsoleCore::_Prints(const string& text, BOOL endLine, const ConsoleFormat* color, SHORT x, SHORT y)
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    const UINT stringLength = text.length();
    if(stringLength != 0)
    {
        DWORD whatWasWritten = 0;
        CursorPosition(x,y);
        Color(color);
        vector<string> textVector;
        textVector.push_back(text);
        CodeFinder<> codes;
        codes = for_each(textVector.begin(), textVector.end(),codes);

        CodeFinder<>::PhraseLocationVector::iterator it = codes.m_phrasesAndLocations.begin()
            ,end = codes.m_phrasesAndLocations.end();
        for(;it != end; ++it)
        {
            CodeFinder<>::CodePhraseVector codesAndPairs = RemoveCodes(it);
            CodeFinder<>::CodePhraseVector::reverse_iterator rIt = codesAndPairs.rbegin()
                ,rEnd = codesAndPairs.rend();
            for(; rIt != rEnd; ++rIt)
            {
                ConsoleFormat colorCode;

            unsigned stringLength = rIt->second.length();

            if(rIt->first.length() != 0)
                    colorCode = ConsoleFormat(rIt->first.c_str()+1);
                else
                    colorCode = m_currentFormat;

            WORD* lpAttributes = new WORD[stringLength];
                memset((void*)lpAttributes,colorCode.Color(),sizeof(WORD)*stringLength);
                WriteConsoleOutputAttribute(m_consoleHandle, lpAttributes,
                    stringLength,m_cursorPosition,&whatWasWritten);

            delete [] lpAttributes;
                WriteConsoleOutputCharacter(m_consoleHandle,rIt->second.c_str(),stringLength,
                    m_cursorPosition,&whatWasWritten);

            AdvanceCursor((SHORT)whatWasWritten);
            }
        }

        if(endLine)
            EndLine();
    }
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
}

void ConsoleCore::EndLine()
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    m_cursorPosition.Y++;
    m_cursorPosition.X = 0;
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
}

void ConsoleCore::AdvanceCursor(SHORT length)
{
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    UpdateWindowSize();
    m_cursorPosition.Y %= MAXSCREENY;
    if((m_cursorPosition.X += length) >= MAXSCREENX)
    {
        m_cursorPosition.Y += m_cursorPosition.X / MAXSCREENX;
        m_cursorPosition.X %= MAXSCREENX;
    }
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
}

static string getClipboard()
{
    const char *buffer = "";
    if (OpenClipboard(NULL)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        buffer = (const char *)GlobalLock(hData);
        if (buffer == NULL)
            buffer = "";
        GlobalUnlock(hData);
        CloseClipboard();
    }
    return string(buffer);
}

bool ConsoleCore::ScanNumber(COORD origin, int& number,
        int width, bool allowNegative, bool hasDefault, int minDigit, int maxDigit)
{
    bool ret = true;
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    string buffer;
    int ch, pos = 0;
    COORD tempOrigin;
    SaveScreen();
    CursorPosition(&origin);
    if (hasDefault) {
        ConsoleFormat originalColor = ConsoleCore::GetInstance()->Color();
        ConsoleFormat selectedColor = ConsoleFormat::BLACK | ConsoleFormat::ONBRIGHTWHITE;
        char buf[width+1];
        snprintf(buf, width+1, "%d", number);
        buffer = buf;
        Prints(buffer, FALSE, &selectedColor);
        ConsoleCore::GetInstance()->Color(&originalColor);
        pos = buffer.size();
        tempOrigin = origin;
        tempOrigin.X = origin.X + pos;
        CursorPosition(&tempOrigin);
    } else {
        Prints(string(width, ' '));
        CursorPosition(&origin);
    }
    do
    {
        ch = _getch();
        switch (ch)
        {
            case RETURN_KEY:
                break;
            case ESCAPE_KEY:
                ret = false;
                break;
            case BACKSPACE_KEY:
                if (hasDefault) {
                    buffer.clear();
                    pos = 0;
                    hasDefault = false;
                } else if (!buffer.empty() && pos > 0) {
                    buffer.erase(--pos, 1);
                }
                break;
            case CONTROL_V_KEY:
                if (hasDefault) {
                    buffer.clear();
                    pos = 0;
                    hasDefault = false;
                }
                {
                    string clipboard = getClipboard();
                    buffer.insert(pos, clipboard);
                    pos += clipboard.size();
                }
                break;
            case KB_EXTENDED_KEY: // control key
                ch = _getch();
                if (ch == HOME_KEY) {
                    pos = 0;
                } else if (ch == END_KEY) {
                    pos = buffer.size();
                } else if (pos > 0 && ch == LEFT_KEY) {
                    --pos;
                } else if (pos < buffer.size() && ch == RIGHT_KEY) {
                    ++pos;
                } else if (hasDefault && ch == DELETE_KEY) {
                    buffer.clear();
                    pos = 0;
                } else if (!buffer.empty() && pos < buffer.size() && ch == DELETE_KEY) {
                    buffer.erase(pos, 1);
                }
                break;
            default: // digit or first '-'
                if ((ch == '-' && !allowNegative) || (isdigit(ch) && (ch - '0' < minDigit || ch - '0' > maxDigit)))
                    break;
                if (hasDefault && (isdigit(ch) || ch == '-')) {
                    buffer.clear();
                    pos = 0;
                    hasDefault = false;
                }
                if ((isdigit(ch) && buffer.size() < width) || (ch == '-' && pos == 0))
                    buffer.insert(pos++, 1, (char)ch);
                break;
        }
        hasDefault = false;
        ClearScreen();
        LoadScreen();
        CursorPosition(&origin);
        Prints(buffer, FALSE);
        tempOrigin = origin;
        tempOrigin.X = origin.X + pos;
        CursorPosition(&tempOrigin);
    }
    while(ch != RETURN_KEY && ch != ESCAPE_KEY);
    // clear on escape
    if (ch == ESCAPE_KEY)
    {
        CursorPosition(&origin);
        Prints(string(width, ' '));
        buffer.clear();
    }
    else
        number = atoi(buffer.c_str());
    tempOrigin.X = 0;
    tempOrigin.Y++;
    CursorPosition(&tempOrigin);
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
    return ret;
}

bool ConsoleCore::ScanDouble(COORD origin, double& number)
{
    bool ret = true;
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    const int MAXLENGTH = 24;
    bool decimalPassed = false;
    bool exponentPassed = false;
    bool signPassed = false;
    char ch;
    char buffer[MAXLENGTH];
    int x,
        charactersEntered = 0;
    COORD tempOrigin;
    for(x = 0; x < MAXLENGTH; x++)
        buffer[x] = ' ';
    buffer[x-1] = '\0';
    SaveScreen();
    CursorPosition(&origin);
    Prints(buffer);
    CursorPosition(&origin);
    do
    {
        ch = _getch();
        switch(ch)
        {
        case RETURN_KEY:
            number = atof(buffer);
            break;
        case ESCAPE_KEY:
            ret = false;
            break;
        case BACKSPACE_KEY:
            {
                if(charactersEntered > 0)
                {
                    charactersEntered--;
                    if(tolower(buffer[charactersEntered]) == 'e')
                    {
                        exponentPassed = false;
                    }
                    else if((buffer[charactersEntered] == '-') || (buffer[charactersEntered] == '+'))
                    {
                        signPassed = false;
                    }
                    else if(buffer[charactersEntered] == '.')
                        decimalPassed = false;
                    buffer[charactersEntered] = ' ';
                }
            } break;
        case 'e':
        case 'E':
            {
                if(!exponentPassed)
                {
                    if(charactersEntered < MAXLENGTH-3)
                    {
                        buffer[charactersEntered] = ch;
                        charactersEntered++;
                    }
                    exponentPassed = true;
                }
            } break;
        case '.':
            {
                if(!decimalPassed && !exponentPassed)
                {
                    if(charactersEntered < MAXLENGTH-1)
                    {
                        buffer[charactersEntered] = ch;
                        charactersEntered++;
                        decimalPassed = true;
                    }
                }
            } break;
        case '+':
        case '-':
            {
                if(exponentPassed && !signPassed)
                {
                    exponentPassed = true;
                    if(charactersEntered < MAXLENGTH-2)
                    {
                        buffer[charactersEntered] = ch;
                        charactersEntered++;
                        signPassed = true;
                    }
                }
            } break;
        case '0': // digit
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            {
                if(charactersEntered < MAXLENGTH-1)
                {
                    if(exponentPassed == signPassed)
                    {
                        buffer[charactersEntered] = ch;
                        charactersEntered++;
                    }
                }
            } break;
        }
        ClearScreen();
        LoadScreen();
        CursorPosition(&origin);
        Prints(buffer,FALSE);
        tempOrigin = origin;
        tempOrigin.X = origin.X + charactersEntered;
        CursorPosition(&tempOrigin);
    }
    while(ch != RETURN_KEY && ch != ESCAPE_KEY);
    // clear on escape
    if (ch == ESCAPE_KEY)
    {
        CursorPosition(&origin);
        Prints(string(MAXLENGTH, ' '));
    }
    tempOrigin.X = 0;
    tempOrigin.Y++;
    CursorPosition(&tempOrigin);
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
    return ret;
}

bool ConsoleCore::ScanString(COORD origin, string& buffer, UINT width)
{
    bool ret = true;
#ifdef JLIB_MUTEXED
    pthread_mutex_lock(&mutex);
#endif
    bool hasDefault = !buffer.empty();
    int ch, pos = 0, offset = 0;
    COORD tempOrigin;
    SaveScreen();
    CursorPosition(&origin);
    if (hasDefault) {
        pos = buffer.size();
        if (pos - offset > width)
            offset = pos - width;

        ConsoleFormat originalColor = ConsoleCore::GetInstance()->Color();
        ConsoleFormat selectedColor = ConsoleFormat::BLACK | ConsoleFormat::ONBRIGHTWHITE;
        Prints(buffer.substr(offset, width), FALSE, &selectedColor);
        ConsoleCore::GetInstance()->Color(&originalColor);

        tempOrigin = origin;
        tempOrigin.X = origin.X + pos - offset;
        CursorPosition(&tempOrigin);
    } else {
        Prints(string(width, ' '));
        CursorPosition(&origin);
    }
    do
    {
        ch = _getch();
        switch (ch)
        {
            case RETURN_KEY:
                break;
            case ESCAPE_KEY:
                ret = false;
                break;
            case BACKSPACE_KEY:
                if (hasDefault) {
                    buffer.clear();
                    pos = offset = 0;
                    hasDefault = false;
                } else if (!buffer.empty() && pos > 0) {
                    buffer.erase(--pos, 1);
                    if (offset > 0)
                        --offset;
                }
                break;
            case CONTROL_V_KEY:
                if (hasDefault) {
                    buffer.clear();
                    pos = offset = 0;
                    hasDefault = false;
                }
                {
                    string clipboard = getClipboard();
                    buffer.insert(pos, clipboard);
                    pos += clipboard.size();
                }
                break;
            case KB_EXTENDED_KEY: // control key
                ch = _getch();
                if (ch == HOME_KEY) {
                    pos = 0;
                } else if (ch == END_KEY) {
                    pos = buffer.size();
                } else if (pos > 0 && ch == LEFT_KEY) {
                    --pos;
                } else if (pos < buffer.size() && ch == RIGHT_KEY) {
                    ++pos;
                } else if (hasDefault && ch == DELETE_KEY) {
                    buffer.clear();
                    pos = offset = 0;
                } else if (!buffer.empty() && pos < buffer.size() && ch == DELETE_KEY) {
                    buffer.erase(pos, 1);
                    if (offset + width > buffer.size())
                        offset = buffer.size() - width;
                }
                break;
            default: // char or digit
                if (!iscntrl(ch))
                {
                    if (hasDefault) {
                        buffer.clear();
                        pos = offset = 0;
                        hasDefault = false;
                    }
                    buffer.insert(pos++, 1, (char)ch);
                }
                break;
        }
        hasDefault = false;
        // scroll the input width
        if (pos < offset)
            offset = pos;
        else if (pos - offset > width)
            offset = pos - width;
        else if (buffer.size() <= width)
            offset = 0;
        ClearScreen();
        LoadScreen();
        CursorPosition(&origin);
        Prints(buffer.substr(offset, width), FALSE);
        tempOrigin = origin;
        tempOrigin.X = origin.X + pos - offset;
        CursorPosition(&tempOrigin);
    }
    while(ch != RETURN_KEY && ch != ESCAPE_KEY);
    // clear on escape
    if (ch == ESCAPE_KEY)
    {
        CursorPosition(&origin);
        Prints(string(width, ' '));
    }
    tempOrigin.X = 0;
    tempOrigin.Y++;
    CursorPosition(&tempOrigin);
#ifdef JLIB_MUTEXED
    pthread_mutex_unlock(&mutex);
#endif
    return ret;
}

bool ConsoleCore::Wait()
{
    while (!_kbhit())
        Sleep(1);
    int ch = _getch();
    if (ch == ESCAPE_KEY)
        return false;
    if (ch == KB_EXTENDED_KEY) // control key, so need to get again
        _getch();
    return true;
}
