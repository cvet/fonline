#include "Common.h"
#include "IniParser.h"
#include "Crypt.h"
#include "FileManager.h"

IniParser::IniParser()
{
    //
}

void IniParser::AppendStr( const char* buf )
{
    ParseStr( buf );
}

bool IniParser::AppendFile( const char* fname )
{
    FileManager fm;
    if( !fm.LoadFile( fname ) )
        return false;

    ParseStr( fm.GetCStr() );
    return true;
}

void IniParser::ParseStr( const char* str )
{
    StrMap* cur_app;
    auto    it_app = appKeyValues.find( "" );
    if( it_app == appKeyValues.end() )
    {
        auto it = appKeyValues.insert( PAIR( string( "" ), StrMap() ) );
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

    istrstream istr( str );
    string     line;
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
            auto it = appKeyValues.insert( PAIR( buf, StrMap() ) );
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

            // Parse key value
            char* begin = &line[ 0 ];

            // Text format {}{}{}
            string key;
            string value;
            if( begin[ 0 ] == '{' )
            {
                char* pbuf = begin;

                // Skip '{'
                pbuf++;
                if( !*pbuf )
                    continue;

                // Goto '}'
                const char* num1_begin = pbuf;
                Str::GoTo( pbuf, '}', false );
                if( !*pbuf )
                    continue;
                *pbuf = 0;
                pbuf++;

                // Parse number
                uint num1 = (uint) ( _str( num1_begin ).isNumber() ? _str( num1_begin ).toInt() : _str( num1_begin ).toHash() );

                // Skip '{'
                Str::GoTo( pbuf, '{', true );
                if( !*pbuf )
                    continue;

                // Goto '}'
                const char* num2_begin = pbuf;
                Str::GoTo( pbuf, '}', false );
                if( !*pbuf )
                    continue;

                *pbuf = 0;
                pbuf++;

                // Parse number
                uint num2 = ( num2_begin[ 0 ] ? ( _str( num2_begin ).isNumber() ? _str( num2_begin ).toInt() : _str( num2_begin ).toHash() ) : 0 );

                // Goto '{'
                Str::GoTo( pbuf, '{', true );
                if( !*pbuf )
                    continue;

                // Find '}'
                char* _pbuf = pbuf;
                Str::GoTo( pbuf, '}', false );
                if( !*pbuf )
                    continue;

                *pbuf = 0;
                pbuf++;
                uint num = num1 + num2;
                if( !num )
                    continue;

                key = _str( "{}", num );
                value = _pbuf;
            }
            else
            {
                // Key value format
                char* separator = Str::Substring( begin, "=" );
                if( !separator )
                    continue;

                // Parse key value
                key = string( begin, (size_t) ( separator - begin ) );
                value = separator + 1;
                key = _str( key ).trim();
                value = _str( value ).trim();
            }

            // Store entire
            if( !key.empty() )
                ( *cur_app )[ key ] = std::move( value );
        }
    }
    RUNTIME_ASSERT( istr.eof() );

    // Store current section content
    if( collectContent )
        ( *cur_app )[ "" ] = app_content;
}

bool IniParser::SaveFile( const char* fname )
{
    string str;
    str.reserve( 10000000 );
    for( auto& app_it : appKeyValuesOrder )
    {
        auto& app = *app_it;
        str.append( string( "[" ).append( app.first ).append( "]" ).append( "\n" ) );
        for( const auto& kv : app.second )
            if( !kv.first.empty() )
                str.append( string( kv.first ).append( " = " ).append( kv.second ).append( "\n" ) );
        str.append( "\n" );
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

string* IniParser::GetRawValue( const char* app_name, const char* key_name )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return nullptr;

    auto it_key = it_app->second.find( key_name );
    if( it_key == it_app->second.end() )
        return nullptr;

    return &it_key->second;
}

const char* IniParser::GetStr( const char* app_name, const char* key_name, const char* def_val /* = nullptr */ )
{
    string* str = GetRawValue( app_name, key_name );
    return str ? str->c_str() : def_val;
}

int IniParser::GetInt( const char* app_name, const char* key_name, int def_val /* = 0 */  )
{
    string* str = GetRawValue( app_name, key_name );
    if( str && str->length() == 4 && Str::CompareCase( str->c_str(), "true" ) )
        return 1;
    if( str && str->length() == 5 && Str::CompareCase( str->c_str(), "false" ) )
        return 0;
    return str ? _str( *str ).toInt() : def_val;
}

void IniParser::SetStr( const char* app_name, const char* key_name, const char* val )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
    {
        StrMap key_values;
        key_values[ key_name ] = val;
        auto   it = appKeyValues.insert( PAIR( string( app_name ), key_values ) );
        appKeyValuesOrder.push_back( it );
    }
    else
    {
        it_app->second[ key_name ] = val;
    }
}

void IniParser::SetInt( const char* app_name, const char* key_name, int val )
{
    SetStr( app_name, key_name, _str( "{}", val ).c_str() );
}

StrMap& IniParser::GetApp( const char* app_name )
{
    auto it = appKeyValues.find( app_name );
    RUNTIME_ASSERT( it != appKeyValues.end() );
    return it->second;
}

void IniParser::GetApps( const char* app_name, PStrMapVec& key_values )
{
    size_t count = appKeyValues.count( app_name );
    auto   it = appKeyValues.find( app_name );
    key_values.reserve( key_values.size() + count );
    for( size_t i = 0; i < count; i++, it++ )
        key_values.push_back( &it->second );
}

StrMap& IniParser::SetApp( const char* app_name )
{
    auto it = appKeyValues.insert( PAIR( string( app_name ), StrMap() ) );
    appKeyValuesOrder.push_back( it );
    return it->second;
}

bool IniParser::IsApp( const char* app_name )
{
    auto it_app = appKeyValues.find( app_name );
    return it_app != appKeyValues.end();
}

bool IniParser::IsKey( const char* app_name, const char* key_name )
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

void IniParser::GotoNextApp( const char* app_name )
{
    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return;

    auto it = std::find( appKeyValuesOrder.begin(), appKeyValuesOrder.end(), it_app );
    RUNTIME_ASSERT( it != appKeyValuesOrder.end() );
    appKeyValuesOrder.erase( it );
    appKeyValues.erase( it_app );
}

const StrMap* IniParser::GetAppKeyValues( const char* app_name )
{
    auto it_app = appKeyValues.find( app_name );
    return it_app != appKeyValues.end() ? &it_app->second : nullptr;
}

const char* IniParser::GetAppContent( const char* app_name )
{
    RUNTIME_ASSERT( collectContent );

    auto it_app = appKeyValues.find( app_name );
    if( it_app == appKeyValues.end() )
        return nullptr;

    auto it_key = it_app->second.find( "" );
    return it_key != it_app->second.end() ? it_key->second.c_str() : nullptr;
}
