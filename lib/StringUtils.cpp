#include "StringUtils.h"

using namespace std;


void splitFormat ( const string& fmt, string& first, string& rest )
{
    size_t i;

    for ( i = 0; i < fmt.size(); ++i )
    {
        if ( i + 1 < fmt.size() && fmt[i] == '%' && fmt[i + 1] == '%' )
            ++i;
        else if ( fmt[i] == '%' && ( i + 1 == fmt.size() || fmt[i + 1] != '%' ) )
            break;
    }

    if ( i == fmt.size() - 1 )
    {
        first = "";
        rest = fmt;
        return;
    }

    for ( ++i; i < fmt.size(); ++i )
    {
        if ( ! ( isalnum ( fmt[i] )
                 || fmt[i] == '.'
                 || fmt[i] == '-'
                 || fmt[i] == '+'
                 || fmt[i] == '#' ) )
        {
            break;
        }
    }

    first = fmt.substr ( 0, i );
    rest = ( i < fmt.size() ? fmt.substr ( i ) : "" );
}

string formatAsHex ( const string& bytes )
{
    if ( bytes.empty() )
        return "";

    string str;
    for ( char c : bytes )
        str += format ( "%02x ", ( unsigned char ) c );

    return str.substr ( 0, str.size() - 1 );
}

string formatAsHex ( const void *bytes, size_t len )
{
    if ( len == 0 )
        return "";

    string str;
    for ( size_t i = 0; i < len; ++i )
        str += format ( "%02x ", static_cast<const unsigned char *> ( bytes ) [i] );

    return str.substr ( 0, str.size() - 1 );
}

string trimmed ( string str, const string& ws )
{
    // trim trailing spaces
    size_t endpos = str.find_last_not_of ( ws );
    if ( string::npos != endpos )
        str = str.substr ( 0, endpos + 1 );

    // trim leading spaces
    size_t startpos = str.find_first_not_of ( ws );
    if ( string::npos != startpos )
        str = str.substr ( startpos );

    return str;
}

vector<string> split ( const string& str, const string& delim )
{
    vector<string> result;

    string copy = str;

    for ( ;; )
    {
        size_t i = copy.find_first_of ( delim );

        if ( i == string::npos )
            break;

        result.push_back ( copy.substr ( 0, i ) );
        copy = copy.substr ( i + delim.size() );
    }

    result.push_back ( copy );

    return result;
}
