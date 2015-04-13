#include "KeyValueStore.h"
#include "StringUtils.h"
#include "Logger.h"

#include <sstream>
#include <fstream>

using namespace std;


bool KeyValueStore::hasString ( const string& key ) const
{
    return ( types.find ( key ) != types.end() )
           && ( types.find ( key )->second == Type::String )
           && ( strings.find ( key ) != strings.end() );
}

string KeyValueStore::getString ( const string& key ) const
{
    ASSERT ( types.find ( key ) != types.end() );
    ASSERT ( types.find ( key )->second == Type::String );
    ASSERT ( strings.find ( key ) != strings.end() );

    return strings.find ( key )->second;
}

void KeyValueStore::putString ( const string& key, const string& value )
{
    types[key] = Type::String;
    strings[key] = value;
}

bool KeyValueStore::hasInteger ( const string& key ) const
{
    return ( types.find ( key ) != types.end() )
           && ( types.find ( key )->second == Type::Integer )
           && ( integers.find ( key ) != integers.end() );
}

int KeyValueStore::getInteger ( const string& key ) const
{
    ASSERT ( types.find ( key ) != types.end() );
    ASSERT ( types.find ( key )->second == Type::Integer );
    ASSERT ( integers.find ( key ) != integers.end() );

    return integers.find ( key )->second;
}

void KeyValueStore::setInteger ( const string& key, int value )
{
    types[key] = Type::Integer;
    integers[key] = value;
}

bool KeyValueStore::hasDouble ( const string& key ) const
{
    return ( types.find ( key ) != types.end() )
           && ( types.find ( key )->second == Type::Double )
           && ( doubles.find ( key ) != doubles.end() );
}

double KeyValueStore::getDouble ( const string& key ) const
{
    ASSERT ( types.find ( key ) != types.end() );
    ASSERT ( types.find ( key )->second == Type::Double );
    ASSERT ( doubles.find ( key ) != doubles.end() );

    return doubles.find ( key )->second;
}

void KeyValueStore::setDouble ( const string& key, double value )
{
    types[key] = Type::Double;
    doubles[key] = value;
}

bool KeyValueStore::save ( const string& file ) const
{
    ofstream fout ( file.c_str() );
    bool good = fout.good();

    if ( good )
    {
        for ( const auto& kv : types )
        {
            fout << "\n" << kv.first << '=';

            switch ( kv.second )
            {
                case Type::String:
                    fout << strings.find ( kv.first )->second;
                    break;

                case Type::Integer:
                    fout << integers.find ( kv.first )->second;
                    break;

                case Type::Double:
                    fout << doubles.find ( kv.first )->second;
                    break;

                default:
                    ASSERT_IMPOSSIBLE;
                    break;
            }

            fout << endl;
        }

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
            vector<string> parts = split ( line, "=" );

            if ( parts.size() != 2 )
                continue;

            const auto it = types.find ( parts[0] );

            if ( it != types.end() )
            {
                switch ( it->second )
                {
                    case Type::String:
                        putString ( it->first, parts[1] );
                        break;

                    case Type::Integer:
                        setInteger ( it->first, lexical_cast<int> ( trimmed ( parts[1] ) ) );
                        break;

                    case Type::Double:
                        setDouble ( it->first, lexical_cast<double> ( trimmed ( parts[1] ) ) );
                        break;

                    default:
                        break;
                }
            }
        }
    }

    fin.close();
    return good;
}
