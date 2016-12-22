#include "Script.h"

#define REGISTER_FUNCTION( decl, func, conv )                                                            \
    do                                                                                                   \
    {                                                                                                    \
        int r;                                                                                           \
        if( !max_poratbility )                                                                           \
            r = engine->RegisterGlobalFunction( decl, asFUNCTION( func ), conv );                        \
        else                                                                                             \
            r = engine->RegisterGlobalFunction( decl, asFUNCTION( func ## _Generic ), asCALL_GENERIC );  \
        RUNTIME_ASSERT( r >= 0 );                                                                        \
    } while( 0 )

#define REGISTER_METHOD( type, decl, func, conv )                                                            \
    do                                                                                                       \
    {                                                                                                        \
        int r;                                                                                               \
        if( !max_poratbility )                                                                               \
            r = engine->RegisterObjectMethod( type, decl, asFUNCTION( func ), conv );                        \
        else                                                                                                 \
            r = engine->RegisterObjectMethod( type, decl, asFUNCTION( func ## _Generic ), asCALL_GENERIC );  \
        RUNTIME_ASSERT( r >= 0 );                                                                            \
    } while( 0 )

#define REGISTER_BEHAVIOUR( type, beh, decl, func, conv )                                                            \
    do                                                                                                               \
    {                                                                                                                \
        int r;                                                                                                       \
        if( !max_poratbility )                                                                                       \
            r = engine->RegisterObjectBehaviour( type, beh, decl, asFUNCTION( func ), conv );                        \
        else                                                                                                         \
            r = engine->RegisterObjectBehaviour( type, beh, decl, asFUNCTION( func ## _Generic ), asCALL_GENERIC );  \
        RUNTIME_ASSERT( r >= 0 );                                                                                    \
    } while( 0 )

#define CREATE_GENERIC_FUNCTION_1( func, arg1 )                    \
    static void func ## _Generic( asIScriptGeneric * gen ) { func( \
                                                                 *(arg1*) gen->GetArgAddress( 0 ) ); }
#define CREATE_GENERIC_FUNCTION_2( func, arg1, arg2 )              \
    static void func ## _Generic( asIScriptGeneric * gen ) { func( \
                                                                 *(arg1*) gen->GetArgAddress( 0 ), *(arg2*) gen->GetArgAddress( 1 ) ); }
#define CREATE_GENERIC_FUNCTION_3( func, arg1, arg2, arg3 )        \
    static void func ## _Generic( asIScriptGeneric * gen ) { func( \
                                                                 *(arg1*) gen->GetArgAddress( 0 ), *(arg2*) gen->GetArgAddress( 1 ), *(arg3*) gen->GetArgAddress( 2 ) ); }

#define CREATE_GENERIC_FUNCTION_1_RET( func, arg1, ret )                                                         \
    static void func ## _Generic( asIScriptGeneric * gen ) { new ( gen->GetAddressOfReturnLocation() )ret( func( \
                                                                                                               *(arg1*) gen->GetArgAddress( 0 ) ) ); }
#define CREATE_GENERIC_FUNCTION_2_RET( func, arg1, arg2, ret )                                                   \
    static void func ## _Generic( asIScriptGeneric * gen ) { new ( gen->GetAddressOfReturnLocation() )ret( func( \
                                                                                                               *(arg1*) gen->GetArgAddress( 0 ), *(arg2*) gen->GetArgAddress( 1 ) ) ); }
#define CREATE_GENERIC_FUNCTION_3_RET( func, arg1, arg2, arg3, ret )                                             \
    static void func ## _Generic( asIScriptGeneric * gen ) { new ( gen->GetAddressOfReturnLocation() )ret( func( \
                                                                                                               *(arg1*) gen->GetArgAddress( 0 ), *(arg2*) gen->GetArgAddress( 1 ), *(arg3*) gen->GetArgAddress( 2 ) ) ); }

static void CScriptArray_InsertFirst( CScriptArray* arr, void* value )
{
    arr->InsertAt( 0, value );
}
CREATE_GENERIC_FUNCTION_2( CScriptArray_InsertFirst, CScriptArray *, void* );

static void CScriptArray_RemoveFirst( CScriptArray* arr )
{
    arr->RemoveAt( 0 );
}
CREATE_GENERIC_FUNCTION_1( CScriptArray_RemoveFirst, CScriptArray* );

static void CScriptArray_Grow( CScriptArray* arr, asUINT numElements )
{
    if( numElements == 0 )
        return;

    arr->Resize( arr->GetSize() + numElements );
}
CREATE_GENERIC_FUNCTION_2( CScriptArray_Grow, CScriptArray *, asUINT );

static void CScriptArray_Reduce( CScriptArray* arr, asUINT numElements )
{
    if( numElements == 0 )
        return;

    asUINT size = arr->GetSize();
    if( numElements > size )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Array size is less than reduce count" );
        return;
    }
    arr->Resize( size - numElements );
}
CREATE_GENERIC_FUNCTION_2( CScriptArray_Reduce, CScriptArray *, asUINT );

static void* CScriptArray_First( CScriptArray* arr )
{
    return arr->At( 0 );
}
CREATE_GENERIC_FUNCTION_1_RET( CScriptArray_First, CScriptArray *, void* );

static void* CScriptArray_Last( CScriptArray* arr )
{
    return arr->At( arr->GetSize() - 1 );
}
CREATE_GENERIC_FUNCTION_1_RET( CScriptArray_Last, CScriptArray *, void* );

static void CScriptArray_Clear( CScriptArray* arr )
{
    if( arr->GetSize() > 0 )
        arr->Resize( 0 );
}
CREATE_GENERIC_FUNCTION_1( CScriptArray_Clear, CScriptArray* );

static bool CScriptArray_Exists( const CScriptArray* arr, void* value )
{
    return arr->Find( 0, value ) != -1;
}
CREATE_GENERIC_FUNCTION_2_RET( CScriptArray_Exists, const CScriptArray *, void*, bool );

static CScriptArray* CScriptArray_Clone( asITypeInfo* ti, const CScriptArray** other )
{
    if( !*other )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Array is null" );
        return nullptr;
    }

    CScriptArray* clone = CScriptArray::Create( ti );
    *clone = **other;
    return clone;
}
CREATE_GENERIC_FUNCTION_2_RET( CScriptArray_Clone, asITypeInfo *, const CScriptArray * *, CScriptArray* );

static void CScriptArray_Set( CScriptArray* arr, const CScriptArray** other )
{
    if( !*other )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Array is null" );
        return;
    }

    *arr = **other;
}
CREATE_GENERIC_FUNCTION_2( CScriptArray_Set, CScriptArray *, const CScriptArray * * );

static void CScriptArray_InsertArrAt( CScriptArray* arr, uint index, const CScriptArray** other )
{
    if( !*other )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Array is null" );
        return;
    }

    arr->InsertAt( index, **other );
}
CREATE_GENERIC_FUNCTION_3( CScriptArray_InsertArrAt, CScriptArray *, uint, const CScriptArray * * );

static void CScriptArray_InsertArrFirst( CScriptArray* arr, const CScriptArray** other )
{
    if( !*other )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Array is null" );
        return;
    }

    arr->InsertAt( 0, **other );
}
CREATE_GENERIC_FUNCTION_2( CScriptArray_InsertArrFirst, CScriptArray *, const CScriptArray * * );

static void CScriptArray_InsertArrLast( CScriptArray* arr, const CScriptArray** other )
{
    if( !*other )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Array is null" );
        return;
    }

    arr->InsertAt( arr->GetSize() - 1, **other );
}
CREATE_GENERIC_FUNCTION_2( CScriptArray_InsertArrLast, CScriptArray *, const CScriptArray * * );

static bool CScriptArray_Equals( CScriptArray* arr, const CScriptArray** other )
{
    if( !*other )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Array is null" );
        return false;
    }

    return *arr == **other;
}
CREATE_GENERIC_FUNCTION_2_RET( CScriptArray_Equals, CScriptArray *, const CScriptArray * *, bool );

void Script::RegisterScriptArrayExtensions( asIScriptEngine* engine )
{
    bool max_poratbility = strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) != nullptr;

    REGISTER_METHOD( "array<T>", "void insertFirst(const T&in)", CScriptArray_InsertFirst, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "void removeFirst()", CScriptArray_RemoveFirst, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "void grow(uint)", CScriptArray_Grow, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "void reduce(uint)", CScriptArray_Reduce, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "T& first()", CScriptArray_First, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "const T& first() const", CScriptArray_First, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "T& last()", CScriptArray_Last, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "const T& last() const", CScriptArray_Last, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "void clear()", CScriptArray_Clear, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "bool exists(const T&in) const", CScriptArray_Exists, asCALL_CDECL_OBJFIRST );
    REGISTER_BEHAVIOUR( "array<T>", asBEHAVE_FACTORY, "array<T>@ f(int& in, const array<T>&in)", CScriptArray_Clone, asCALL_CDECL );
    REGISTER_METHOD( "array<T>", "void set(const array<T>&in)", CScriptArray_Set, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "void insertAt(uint, const array<T>&in)", CScriptArray_InsertArrAt, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "void insertFirst(const array<T>&in)", CScriptArray_InsertArrFirst, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "void insertLast(const array<T>&in)", CScriptArray_InsertArrFirst, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "array<T>", "bool equals(const array<T>&in)", CScriptArray_Equals, asCALL_CDECL_OBJFIRST );
}

static CScriptDict* ScriptDict_Clone( asITypeInfo* ti, const CScriptDict** other )
{
    CScriptDict* clone = CScriptDict::Create( ti );
    if( *other )
        *clone = **other;
    return clone;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptDict_Clone, asITypeInfo *, const CScriptDict * *, CScriptDict* );

static bool ScriptDict_Equals( CScriptDict* dict, const CScriptDict** other )
{
    if( !*other )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Dict is null" );
        return false;
    }

    return *dict == **other;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptDict_Equals, CScriptDict *, const CScriptDict * *, bool );

void Script::RegisterScriptDictExtensions( asIScriptEngine* engine )
{
    bool max_poratbility = strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) != nullptr;

    REGISTER_BEHAVIOUR( "dict<T1,T2>", asBEHAVE_FACTORY, "dict<T1,T2>@ f(int& in, const dict<T1,T2>&in)", ScriptDict_Clone, asCALL_CDECL );
    REGISTER_METHOD( "dict<T1,T2>", "bool equals(const dict<T1,T2>&in)", ScriptDict_Equals, asCALL_CDECL_OBJFIRST );
}

static bool IndexUTF8ToRaw( const string& str, int& index, uint* length = nullptr, uint offset = 0 )
{
    if( index < 0 )
    {
        index = (int) Str::LengthUTF8( str.c_str() ) + index;
        if( index < 0 )
        {
            index = 0;
            if( length )
            {
                if( !str.empty() )
                    Str::DecodeUTF8( str.c_str(), length );
                else
                    *length = 0;
            }
            return false;
        }
    }

    const char* begin = str.c_str() + offset;
    const char* s = begin;
    while( *s )
    {
        uint ch_length;
        Str::DecodeUTF8( s, &ch_length );
        if( index > 0 )
        {
            s += ch_length;
            index--;
        }
        else
        {
            index = (uint) ( s - begin );
            if( length )
                *length = ch_length;
            return true;
        }
    }
    index = (uint) ( s - begin );
    if( length )
        *length = 0;
    return false;
}

static int IndexRawToUTF8( const string& str, int index )
{
    int         result = 0;
    const char* s = str.c_str();
    while( index > 0 && *s )
    {
        uint ch_length;
        Str::DecodeUTF8( s, &ch_length );
        s += ch_length;
        index -= ch_length;
        result++;
    }
    return result;
}

static void ScriptString_Clear( string& str )
{
    str.clear();
}
CREATE_GENERIC_FUNCTION_1( ScriptString_Clear, string );

static string ScriptString_SubString( const string& str, int start, int count )
{
    if( !IndexUTF8ToRaw( str, start ) )
        return "";
    if( count >= 0 )
        IndexUTF8ToRaw( str, count, NULL, start );
    return str.substr( start, count >= 0 ? count : std::string::npos );
}
CREATE_GENERIC_FUNCTION_3_RET( ScriptString_SubString, const string, int, int, string );

static int ScriptString_FindFirst( const string& str, const string& sub, int start )
{
    if( !IndexUTF8ToRaw( str, start ) )
        return -1;
    int pos = (int) str.find( sub, start );
    return pos != -1 ? IndexRawToUTF8( str, pos ) : -1;
}
CREATE_GENERIC_FUNCTION_3_RET( ScriptString_FindFirst, const string, const string, int, int );

static int ScriptString_FindLast( const string& str, const string& sub, int start )
{
    if( !IndexUTF8ToRaw( str, start ) )
        return -1;
    int pos = (int) str.rfind( sub );
    return pos != -1 && pos >= start ? IndexRawToUTF8( str, pos ) : -1;
}
CREATE_GENERIC_FUNCTION_3_RET( ScriptString_FindLast, const string, const string, int, int );

static int ScriptString_FindFirstOf( const string& str, const string& chars, int start )
{
    if( !IndexUTF8ToRaw( str, start ) )
        return -1;
    int pos = (int) str.find_first_of( chars, start );
    return pos != -1 ? IndexRawToUTF8( str, pos ) : -1;
}
CREATE_GENERIC_FUNCTION_3_RET( ScriptString_FindFirstOf, const string, const string, int, int );

static int ScriptString_FindFirstNotOf( const string& str, const string& chars, int start )
{
    if( !IndexUTF8ToRaw( str, start ) )
        return -1;
    int pos =  (int) str.find_first_not_of( chars, start );
    return pos != -1 ? IndexRawToUTF8( str, pos ) : -1;
}
CREATE_GENERIC_FUNCTION_3_RET( ScriptString_FindFirstNotOf, const string, const string, int, int );

static int ScriptString_FindLastOf( const string& str, const string& chars, int start )
{
    if( !IndexUTF8ToRaw( str, start ) )
        return -1;
    int pos = (int) str.find_last_of( chars );
    return pos != -1 && pos >= start ? IndexRawToUTF8( str, pos ) : -1;
}
CREATE_GENERIC_FUNCTION_3_RET( ScriptString_FindLastOf, const string, const string, int, int );

static int ScriptString_FindLastNotOf( const string& str, const string& chars, int start )
{
    if( !IndexUTF8ToRaw( str, start ) )
        return -1;
    int pos = (int) str.find_last_not_of( chars, start );
    return pos != -1 && pos >= start ? IndexRawToUTF8( str, pos ) : -1;
}
CREATE_GENERIC_FUNCTION_3_RET( ScriptString_FindLastNotOf, const string, const string, int, int );

static string ScriptString_GetAt( const string& str, int i )
{
    uint length;
    if( !IndexUTF8ToRaw( str, i, &length ) )
    {
        // Set a script exception
        asIScriptContext* ctx = asGetActiveContext();
        ctx->SetException( "Out of range" );

        // Return a null pointer
        return 0;
    }

    return string( str.c_str() + i, length );
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptString_GetAt, const string, int, string );

static void ScriptString_SetAt( string& str, int i, string& value )
{
    uint length;
    if( !IndexUTF8ToRaw( str, i, &length ) )
    {
        // Set a script exception
        asIScriptContext* ctx = asGetActiveContext();
        ctx->SetException( "Out of range" );
        return;
    }

    if( length )
        str.erase( i, length );
    if( value.length() )
        str.insert( i, value.c_str() );
}
CREATE_GENERIC_FUNCTION_3( ScriptString_SetAt, string, int, string );

static uint ScriptString_Length( const string& str )
{
    return Str::LengthUTF8( str.c_str() );
}
CREATE_GENERIC_FUNCTION_1_RET( ScriptString_Length, const string, uint );

static uint ScriptString_RawLength( const string& str )
{
    return (uint) str.length();
}
CREATE_GENERIC_FUNCTION_1_RET( ScriptString_RawLength, const string, uint );

static void ScriptString_RawResize( string& str, uint length )
{
    str.resize( length );
}
CREATE_GENERIC_FUNCTION_2( ScriptString_RawResize, string, uint );

static uchar ScriptString_RawGet( const string& str, uint index )
{
    return index < (uint) str.length() ? str[ index ] : 0;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptString_RawGet, string, uint, uchar );

static void ScriptString_RawSet( string& str, uint index, uchar value )
{
    if( index < (uint) str.length() )
        str[ index ] = (char) value;
}
CREATE_GENERIC_FUNCTION_3( ScriptString_RawSet, string, int, uchar );

static int ScriptString_ToInt( const string& str, int defaultValue )
{
    const char* p = str.c_str();
    while( *p == ' ' || *p == '\t' )
        ++p;

    char* end_str = NULL;
    int   result;
    if( p[ 0 ] && p[ 0 ] == '0' && ( p[ 1 ] == 'x' || p[ 1 ] == 'X' ) )
        result = (int) strtol( p + 2, &end_str, 16 );
    else
        result = (int) strtol( p, &end_str, 10 );

    if( !end_str || end_str == p )
        return defaultValue;

    while( *end_str == ' ' || *end_str == '\t' )
        ++end_str;
    if( *end_str )
        return defaultValue;

    return result;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptString_ToInt, const string, int, int );

static float ScriptString_ToFloat( const string& str, float defaultValue )
{
    const char* p = str.c_str();
    while( *p == ' ' || *p == '\t' )
        ++p;

    char* end_str = NULL;
    float result = (float) strtod( p, &end_str );

    if( !end_str || end_str == p )
        return defaultValue;

    while( *end_str == ' ' || *end_str == '\t' )
        ++end_str;
    if( *end_str )
        return defaultValue;

    return result;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptString_ToFloat, const string, float, float );

static bool ScriptString_StartsWith( const string& str, const string& other )
{
    if( str.length() < other.length() )
        return false;
    return str.compare( 0, other.length(), other ) == 0;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptString_StartsWith, const string, const string, bool );

static bool ScriptString_EndsWith( const string& str, const string& other )
{
    if( str.length() < other.length() )
        return false;
    return str.compare( str.length() - other.length(), other.length(), other ) == 0;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptString_EndsWith, const string, const string, bool );

static string ScriptString_Lower( const string& str )
{
    string result = str;
    Str::LowerUTF8( (char*) result.c_str() );
    return result;
}
CREATE_GENERIC_FUNCTION_1_RET( ScriptString_Lower, const string, string );

static string ScriptString_Upper( const string& str )
{
    string result = str;
    Str::UpperUTF8( (char*) result.c_str() );
    return result;
}
CREATE_GENERIC_FUNCTION_1_RET( ScriptString_Upper, const string, string );

static CScriptArray* ScriptString_Split( const string& delim, const string& str )
{
    CScriptArray* array = Script::CreateArray( "string[]" );

    // Find the existence of the delimiter in the input string
    int pos = 0, prev = 0, count = 0;
    while( ( pos = (int) str.find( delim, prev ) ) != (int) string::npos )
    {
        // Add the part to the array
        array->Resize( array->GetSize() + 1 );
        ( (string*) array->At( count ) )->assign( &str[ prev ], pos - prev );

        // Find the next part
        count++;
        prev = pos + (int) delim.length();
    }

    // Add the remaining part
    array->Resize( array->GetSize() + 1 );
    ( (string*) array->At( count ) )->assign( &str[ prev ] );

    return array;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptString_Split, const string, const string, CScriptArray* );

static string ScriptString_Join( const CScriptArray** parray, const string& delim )
{
    const CScriptArray* array = *parray;
    if( !array )
    {
        asIScriptContext* ctx = asGetActiveContext();
        if( ctx )
            ctx->SetException( "Array is null" );
        return "";
    }

    // Create the new string
    string str = "";
    if( array->GetSize() )
    {
        int n;
        for( n = 0; n < (int) array->GetSize() - 1; n++ )
        {
            str += *(string*) array->At( n );
            str += delim;
        }

        // Add the last part
        str += *(string*) array->At( n );
    }

    return str;
}
CREATE_GENERIC_FUNCTION_2_RET( ScriptString_Join, const CScriptArray * *, const string, string );

void Script::RegisterScriptStdStringExtensions( asIScriptEngine* engine )
{
    bool max_poratbility = strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) != nullptr;

    REGISTER_METHOD( "string", "void clear()", ScriptString_Clear, asCALL_CDECL_OBJFIRST );

    REGISTER_METHOD( "string", "uint length() const", ScriptString_Length, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "uint rawLength() const", ScriptString_RawLength, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "void rawResize(uint)", ScriptString_RawResize, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "uint8 rawGet(uint) const", ScriptString_RawGet, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "void rawSet(uint, uint8)", ScriptString_RawSet, asCALL_CDECL_OBJFIRST );

    REGISTER_METHOD( "string", "string substr(uint start = 0, int count = -1) const", ScriptString_SubString, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "int findFirst(const string &in, uint start = 0) const", ScriptString_FindFirst, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "int findFirstOf(const string &in, uint start = 0) const", ScriptString_FindFirstOf, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "int findFirstNotOf(const string &in, uint start = 0) const", ScriptString_FindFirstNotOf, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "int findLast(const string &in, int start = -1) const", ScriptString_FindLast, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "int findLastOf(const string &in, int start = -1) const", ScriptString_FindLastOf, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "int findLastNotOf(const string &in, int start = -1) const", ScriptString_FindLastNotOf, asCALL_CDECL_OBJFIRST );

    // Register the index operator, both as a mutator and as an inspector
    REGISTER_METHOD( "string", "string get_opIndex(int) const", ScriptString_GetAt, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "void set_opIndex(int, const string &in)", ScriptString_SetAt, asCALL_CDECL_OBJFIRST );

    // Conversion methods
    REGISTER_METHOD( "string", "int toInt(int defaultValue = 0) const", ScriptString_ToInt, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "float toFloat(float defaultValue = 0) const", ScriptString_ToFloat, asCALL_CDECL_OBJFIRST );

    // Find methods
    REGISTER_METHOD( "string", "bool startsWith(const string &in) const", ScriptString_StartsWith, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "bool endsWith(const string &in) const", ScriptString_EndsWith, asCALL_CDECL_OBJFIRST );

    REGISTER_METHOD( "string", "string lower() const", ScriptString_Lower, asCALL_CDECL_OBJFIRST );
    REGISTER_METHOD( "string", "string upper() const", ScriptString_Upper, asCALL_CDECL_OBJFIRST );

    REGISTER_METHOD( "string", "array<string>@ split(const string &in) const", ScriptString_Split, asCALL_CDECL_OBJLAST );
    REGISTER_FUNCTION( "string join(const array<string>@ &in, const string &in)", ScriptString_Join, asCALL_CDECL );
}
