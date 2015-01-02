#include "KeyValueStore.h"
#include "Logger.h"

#include <sstream>
#include <fstream>

using namespace std;


string KeyValueStore::getString ( const string& key ) const
{
    ASSERT ( types.find ( key ) != types.end() );
    ASSERT ( types.find ( key )->second == Type::String );
    ASSERT ( settings.find ( key ) != settings.end() );

    return settings.find ( key )->second;
}

void KeyValueStore::putString ( const string& key, const string& str )
{
    settings[key] = str;
    types[key] = Type::String;
}

int KeyValueStore::getInteger ( const string& key ) const
{
    ASSERT ( types.find ( key ) != types.end() );
    ASSERT ( types.find ( key )->second == Type::Integer );
    ASSERT ( settings.find ( key ) != settings.end() );

    int i;
    stringstream ss ( settings.find ( key )->second );
    ss >> i;
    return i;
}

void KeyValueStore::setInteger ( const string& key, int i )
{
    settings[key] = format ( i );
    types[key] = Type::Integer;
}

bool KeyValueStore::save ( const string& file ) const
{
    ofstream fout ( file.c_str() );
    bool good = fout.good();

    if ( good )
    {
        fout << endl;
        for ( auto it = settings.begin(); it != settings.end(); ++it )
            fout << ( it == settings.begin() ? "" : "\n" ) << it->first << '=' << it->second << endl;
        good = fout.good();
    }

    fout.close();
    return good;
}

bool KeyValueStore::load ( const string& file )
{
    ifstream fin ( file.c_str() );
    bool good = fin.good();

    if ( good )
    {
        string line;
        while ( getline ( fin, line ) )
        {
            size_t div = line.find ( '=' );
            if ( div == string::npos )
                continue;

            auto it = types.find ( line.substr ( 0, div ) );
            if ( it != types.end() )
            {
                stringstream ss ( line.substr ( div + 1 ) );
                ss >> ws;

                switch ( it->second )
                {
                    case Type::String:
                    {
                        string str;
                        getline ( ss, str );
                        putString ( it->first, str );
                        break;
                    }

                    case Type::Integer:
                    {
                        int i;
                        ss >> i;
                        if ( ss.fail() )
                            continue;
                        setInteger ( it->first, i );
                        break;
                    }

                    default:
                        break;
                }
            }
        }
    }

    fin.close();
    return good;
}
