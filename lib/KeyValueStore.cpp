#include "KeyValueStore.hpp"
#include "StringUtils.hpp"
#include "Logger.hpp"

#include <sstream>
#include <fstream>

using namespace std;


bool KeyValueStore::hasString ( const string& key ) const
{
    return ( _types.find ( key ) != _types.end() )
           && ( _types.find ( key )->second == Type::String )
           && ( _strings.find ( key ) != _strings.end() );
}

string KeyValueStore::getString ( const string& key ) const
{
    ASSERT ( _types.find ( key ) != _types.end() );
    ASSERT ( _types.find ( key )->second == Type::String );
    ASSERT ( _strings.find ( key ) != _strings.end() );

    return _strings.find ( key )->second;
}

void KeyValueStore::setString ( const string& key, const string& value )
{
    _types[key] = Type::String;
    _strings[key] = value;
}

bool KeyValueStore::hasInteger ( const string& key ) const
{
    return ( _types.find ( key ) != _types.end() )
           && ( _types.find ( key )->second == Type::Integer )
           && ( _integers.find ( key ) != _integers.end() );
}

int KeyValueStore::getInteger ( const string& key ) const
{
    ASSERT ( _types.find ( key ) != _types.end() );
    ASSERT ( _types.find ( key )->second == Type::Integer );
    ASSERT ( _integers.find ( key ) != _integers.end() );

    return _integers.find ( key )->second;
}

void KeyValueStore::setInteger ( const string& key, int value )
{
    _types[key] = Type::Integer;
    _integers[key] = value;
}

bool KeyValueStore::hasDouble ( const string& key ) const
{
    return ( _types.find ( key ) != _types.end() )
           && ( _types.find ( key )->second == Type::Double )
           && ( _doubles.find ( key ) != _doubles.end() );
}

double KeyValueStore::getDouble ( const string& key ) const
{
    ASSERT ( _types.find ( key ) != _types.end() );
    ASSERT ( _types.find ( key )->second == Type::Double );
    ASSERT ( _doubles.find ( key ) != _doubles.end() );

    return _doubles.find ( key )->second;
}

void KeyValueStore::setDouble ( const string& key, double value )
{
    _types[key] = Type::Double;
    _doubles[key] = value;
}

bool KeyValueStore::save ( const string& file ) const
{
    ofstream fout ( file.c_str() );
    bool good = fout.good();

    if ( good )
    {
        for ( const auto& kv : _types )
        {
            fout << "\n" << kv.first << '=';

            switch ( kv.second )
            {
                case Type::String:
                    fout << _strings.find ( kv.first )->second;
                    break;

                case Type::Integer:
                    fout << _integers.find ( kv.first )->second;
                    break;

                case Type::Double:
                    fout << _doubles.find ( kv.first )->second;
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

            const auto it = _types.find ( parts[0] );

            if ( it != _types.end() )
            {
                switch ( it->second )
                {
                    case Type::String:
                        setString ( it->first, parts[1] );
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
