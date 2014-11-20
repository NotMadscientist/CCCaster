#pragma once

#include "Logger.h"

#include <string>
#include <iostream>


struct Exception
{
    std::string msg;

    Exception() {}
    Exception ( const std::string& msg ) : msg ( msg ) {}

    virtual std::string str() const;
};


struct WindowsException : public Exception
{
    int code = 0;

    WindowsException() {}
    WindowsException ( int code );

    std::string str() const;
};


std::ostream& operator<< ( std::ostream& os, const Exception& exception );


#define LOG_AND_THROW_STRING(FORMAT, ...)                                                                           \
    do {                                                                                                            \
        ::Exception err = format ( FORMAT, ## __VA_ARGS__ );                                                        \
        LOG ( FORMAT, ## __VA_ARGS__ );                                                                             \
        throw err;                                                                                                  \
    } while ( 0 )


#define LOG_AND_THROW_ERROR(EXCEPTION, FORMAT, ...)                                                                 \
    do {                                                                                                            \
        LOG ( "%s; " FORMAT, EXCEPTION, ## __VA_ARGS__ );                                                           \
        throw EXCEPTION;                                                                                            \
    } while ( 0 )
