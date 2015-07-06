#pragma once

#include <map>
#include <unordered_map>
#include <string>


class KeyValueStore
{
public:

    bool hasString ( const std::string& key ) const;

    std::string getString ( const std::string& key ) const;

    void putString ( const std::string& key, const std::string& value );


    bool hasInteger ( const std::string& key ) const;

    int getInteger ( const std::string& key ) const;

    void setInteger ( const std::string& key, int value );


    bool hasDouble ( const std::string& key ) const;

    double getDouble ( const std::string& key ) const;

    void setDouble ( const std::string& key, double value );


    bool save ( const std::string& file ) const;

    bool load ( const std::string& file );

private:

    enum class Type : uint8_t { String, Integer, Double };

    std::map<std::string, Type> types;

    std::unordered_map<std::string, std::string> strings;

    std::unordered_map<std::string, int> integers;

    std::unordered_map<std::string, double> doubles;
};

