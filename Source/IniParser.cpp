#include "Common.h"
#include "IniParser.h"
#include "Crypt.h"
#include "FileManager.h"

IniParser::IniParser()
{
    //
}

void IniParser::AppendStr( const string& buf )
{
    ParseStr( buf );
}

bool IniParser::AppendFile( const string& fname )
{
    FileManager fm;
    if( !fm.LoadFile( fname ) )
        return false;

    ParseStr( fm.GetCStr() );
    return true;
}

void IniParser::ParseStr( const string& str )
{
    StrMap* cur_app;
    auto    it_app = appKeyValues.find( "" );
    if( it_app == appKeyValues.end() )
    {
        auto it = appKeyValues.insert( std::make_pair( "", StrMap() ) );
        appKeyValuesOrder.push_back( it );
        cur_app = &it->second;
    }
    else
    {
        cur_app = &it_app->second;
    }

    string app_content;
    if( collectContent )
        app_content.reserve( 0xFFFF );

    istringstream istr( str );
    string        line;
    while( std::getline( istr, line, '\n' ) )
    {
        // New section
        if( line[ 0 ] == '[' )
        {
            // Parse name
            size_t end = line.find( ']' );
            if( end == string::npos )
                continue;

            string buf = _str( line.substr( 1, end - 1 ) ).trim();
            if( buf.empty() )
                continue;

            // Store current section content
            if( collectContent )
            {
                ( *cur_app )[ "" ] = app_content;
                app_content.clear();
            }

            // Add new section
            auto it = appKeyValues.insert( std::make_pair( buf, StrMap() ) );
            appKeyValuesOrder.push_back( it );
            cur_app = &it->second;
        }
        // Section content
        else
        {
            // Store raw content
            if( collectContent )
                app_content.append( line ).append( "\n" );

            // Cut comments
            line = _str( line ).trim();
            char* comment = (char*) Str::Substring( line, "#" );
            if( comment )
                *comment = 0;

            // Text format {}{}{}
            if( line[ 0 ] == '{' )
            {
                uint   num = 0;
                size_t offset = 0;
                for( int i = 0; i < 3; i++ )
                {
                    size_t first = line.find( '{', offset );
                    size_t last = line.find( '}', first );
                    if( first == string::npos || last == string::npos )
                        break;

                    string str = line.substr( first + 1, last - first - 1 );
                    offset = last + 1;
                    if( i == 0 && !num )
                        num = ( _str( str ).isNumber() ? _str( str ).toInt() : _str( str ).toHash() );
                    else if( i == 1 && num )
                        num += ( !str.empty() ? ( _str( str ).isNumber() ? _str( str ).toInt() : _str( str ).toHash() ) : 0 );
                    else if( i == 2 && num )
                        ( *cur_app )[ _str( "{}", num ) ] = str;
                }
            }
            else
            {
                // Key value format
                size_t separator = line.find( '=' );
                if( separator != string::npos )
                    ( *cur_app )[ _str( line.substr( 0, separator ) ).trim() ] = _str( line.substr( separator + 1 ) ).trim();
            }
        }
    }
    RUNTIME_ASSERT( istr.eof() );

    // Store current section content
    if( collectContent )
        ( *cur_app )[ "" ] = app_content;
}

bool IniParser::SaveFile( const string& fname )
{
    string str;
    str.reserve( 10000000 );
    for( auto& app_it : appKeyValuesOrder )
    {
        auto& app = *app_it;
        str += _str( "[{}]\n", app.first );
        for( const auto& kv : app.second )
            if( !kv.first.empty() )
                str += _str( "{} = {}\n", kv.first, kv.second );
        str += "\n";
    }

    FileManager f;
    f.LoadStream( (uchar*) str.c_str(), (uint) str.length() );
    f.SwitchToWrite();
    return f.SaveFile( fname );
}

void IniParser::Clear()
{
    appKeyValues.clear();
    appKeyValuesOrder.clear();
}

bool IniParser::IsLoaded()
{
    return !appKeyValues.empty();
}

string* IniParser::GetRawValue( const string& app_name, const string& key_name )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return nullptr;

    auto it_key = it_app->second.find( key_name );
    if( it_key == it_app->second.end() )
        return nullptr;

    return &it_key->second;
}

string IniParser::GetStr( const string& app_name, const string& key_name, const string& def_val /* = "" */ )
{
    string* str = GetRawValue( app_name, key_name );
    return str ? *str : def_val;
}

int IniParser::GetInt( const string& app_name, const string& key_name, int def_val /* = 0 */  )
{
    string* str = GetRawValue( app_name, key_name );
    if( str && str->length() == 4 && _str( *str ).compareIgnoreCase( "true" ) )
        return 1;
    if( str && str->length() == 5 && _str( *str ).compareIgnoreCase( "false" ) )
        return 0;
    return str ? _str( *str ).toInt() : def_val;
}

void IniParser::SetStr( const string& app_name, const string& key_name, const string& val )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
    {
        StrMap key_values;
        key_values[ key_name ] = val;
        auto   it = appKeyValues.insert( std::make_pair( app_name, key_values ) );
        appKeyValuesOrder.push_back( it );
    }
    else
    {
        it_app->second[ key_name ] = val;
    }
}

void IniParser::SetInt( const string& app_name, const string& key_name, int val )
{
    SetStr( app_name, key_name, _str( "{}", val ) );
}

StrMap& IniParser::GetApp( const string& app_name )
{
    auto it = appKeyValues.find( app_name );
    RUNTIME_ASSERT( it != appKeyValues.end() );
    return it->second;
}

void IniParser::GetApps( const string& app_name, PStrMapVec& key_values )
{
    size_t count = appKeyValues.count( app_name );
    auto   it = appKeyValues.find( app_name );
    key_values.reserve( key_values.size() + count );
    for( size_t i = 0; i < count; i++, it++ )
        key_values.push_back( &it->second );
}

StrMap& IniParser::SetApp( const string& app_name )
{
    auto it = appKeyValues.insert( std::make_pair( app_name, StrMap() ) );
    appKeyValuesOrder.push_back( it );
    return it->second;
}

bool IniParser::IsApp( const string& app_name )
{
    auto it_app = appKeyValues.find( app_name );
    return it_app != appKeyValues.end();
}

bool IniParser::IsKey( const string& app_name, const string& key_name )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return false;

    return it_app->second.find( key_name ) != it_app->second.end();
}

void IniParser::GetAppNames( StrSet& apps )
{
    for( const auto& kv : appKeyValues )
        apps.insert( kv.first );
}

void IniParser::GotoNextApp( const string& app_name )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return;

    auto it = std::find( appKeyValuesOrder.begin(), appKeyValuesOrder.end(), it_app );
    RUNTIME_ASSERT( it != appKeyValuesOrder.end() );
    appKeyValuesOrder.erase( it );
    appKeyValues.erase( it_app );
}

const StrMap* IniParser::GetAppKeyValues( const string& app_name )
{
    auto it_app = appKeyValues.find( app_name );
    return it_app != appKeyValues.end() ? &it_app->second : nullptr;
}

string IniParser::GetAppContent( const string& app_name )
{
    RUNTIME_ASSERT( collectContent );

    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return nullptr;

    auto it_key = it_app->second.find( "" );
    return it_key != it_app->second.end() ? it_key->second : nullptr;
}
