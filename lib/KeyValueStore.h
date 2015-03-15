#pragma once

#include <map>
#include <string>


class KeyValueStore
{
    enum class Type : uint8_t { String, Integer };

    std::map<std::string, std::string> settings;

    std::map<std::string, Type> types;

public:

    std::string getString ( const std::string& key ) const;
    void putString ( const std::string& key, const std::string& str );

    int getInteger ( const std::string& key ) const;
    void setInteger ( const std::string& key, int i );

    bool save ( const std::string& file ) const;
    bool load ( const std::string& file );
};

