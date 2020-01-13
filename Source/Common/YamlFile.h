#pragma once

#include "Common.h"

/*
   class YamlFile
   {
        Data::Collection

    using ValuesMap = multimap< string, StrMap >;
    using ValuesMapItVec = vector< ValuesMap::const_iterator >;

    ValuesMap      appKeyValues;
    ValuesMapItVec appKeyValuesOrder;
    bool           collectContent = false;

    void    ParseStr( const string& str );
    string* GetRawValue( const string& app_name, const string& key_name );

   public:
    IniFile();
    void CollectContent() { collectContent = true; }
    void AppendStr( const string& buf );
    bool AppendFile( const string& fname );
    bool SaveFile( const string& fname );
    void Clear();
    bool IsLoaded();

    string GetStr( const string& app_name, const string& key_name, const string& def_val = "" );
    int    GetInt( const string& app_name, const string& key_name, int def_val = 0 );

    void SetStr( const string& app_name, const string& key_name, const string& val );
    void SetInt( const string& app_name, const string& key_name, int val );

    StrMap& GetApp( const string& app_name );
    void    GetApps( const string& app_name, PStrMapVec& key_values );
    StrMap& SetApp( const string& app_name );

    bool IsApp( const string& app_name );
    bool IsKey( const string& app_name, const string& key_name );

    void          GetAppNames( StrSet& apps );
    void          GotoNextApp( const string& app_name );
    const StrMap* GetAppKeyValues( const string& app_name );
    string        GetAppContent( const string& app_name );
   };
 */
