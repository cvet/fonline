#include "Script.h"

static void CScriptArray_InsertFirst( CScriptArray* arr, void* value )
{
    arr->InsertAt( 0, value );
}

static void CScriptArray_RemoveFirst( CScriptArray* arr )
{
    arr->RemoveAt( 0 );
}

static void CScriptArray_Grow( CScriptArray* arr, asUINT numElements )
{
    if( numElements == 0 )
        return;

    arr->Resize( arr->GetSize() + numElements );
}

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

static void* CScriptArray_First( CScriptArray* arr )
{
    return arr->At( 0 );
}

static void* CScriptArray_Last( CScriptArray* arr )
{
    return arr->At( arr->GetSize() - 1 );
}

static void CScriptArray_Clear( CScriptArray* arr )
{
    if( arr->GetSize() > 0 )
        arr->Resize( 0 );
}

static bool CScriptArray_Exists( const CScriptArray* arr, void* value )
{
    return arr->Find( 0, value ) != -1;
}

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

void Script::RegisterScriptArrayExtensions( asIScriptEngine* engine )
{
    int r = engine->RegisterObjectMethod( "array<T>", "void insertFirst(const T&in)", asFUNCTION( CScriptArray_InsertFirst ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "void removeFirst()", asFUNCTION( CScriptArray_RemoveFirst ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "void grow(uint)", asFUNCTION( CScriptArray_Grow ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "void reduce(uint)", asFUNCTION( CScriptArray_Reduce ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "T& first()", asFUNCTION( CScriptArray_First ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "const T& first() const", asFUNCTION( CScriptArray_First ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "T& last()", asFUNCTION( CScriptArray_Last ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "const T& last() const", asFUNCTION( CScriptArray_Last ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "void clear()", asFUNCTION( CScriptArray_Clear ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "bool exists(const T&in) const", asFUNCTION( CScriptArray_Exists ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectBehaviour( "array<T>", asBEHAVE_FACTORY, "array<T>@ f(int& in, const array<T>&in)", asFUNCTION( CScriptArray_Clone ), asCALL_CDECL );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "void set(const array<T>&in)", asFUNCTION( CScriptArray_Set ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "void insertAt(uint, const array<T>&in)", asFUNCTION( CScriptArray_InsertArrAt ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "void insertFirst(const array<T>&in)", asFUNCTION( CScriptArray_InsertArrFirst ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "array<T>", "void insertLast(const array<T>&in)", asFUNCTION( CScriptArray_InsertArrFirst ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
}

static CScriptDict* ScriptDict_Clone( asITypeInfo* ti, const CScriptDict** other )
{
    CScriptDict* clone = CScriptDict::Create( ti );
    if( *other )
        *clone = **other;
    return clone;
}

void Script::RegisterScriptDictExtensions( asIScriptEngine* engine )
{
    int r = engine->RegisterObjectBehaviour( "dict<T1,T2>", asBEHAVE_FACTORY, "dict<T1,T2>@ f(int& in, const dict<T1,T2>&in)", asFUNCTION( ScriptDict_Clone ), asCALL_CDECL );
    RUNTIME_ASSERT( r >= 0 );
}

static void ScriptString_Clear( string& str )
{
    str.clear();
}

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

static bool ScriptString_StartsWith( const string& str, const string& other )
{
    if( str.length() < other.length() )
        return false;
    return str.compare( 0, other.length(), other ) == 0;
}

static bool ScriptString_EndsWith( const string& str, const string& other )
{
    if( str.length() < other.length() )
        return false;
    return str.compare( str.length() - other.length(), other.length(), other ) == 0;
}

static string ScriptString_Lower( const string& str )
{
    string result = str;
    std::transform( result.begin(), result.end(), result.begin(), tolower );
    return result;
}

static string ScriptString_Upper( const string& str )
{
    string result = str;
    std::transform( result.begin(), result.end(), result.begin(), toupper );
    return result;
}

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

void Script::RegisterScriptStdStringExtensions( asIScriptEngine* engine )
{
    int r = engine->RegisterObjectMethod( "string", "void clear()", asFUNCTION( ScriptString_Clear ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );

    // Conversion methods
    r = engine->RegisterObjectMethod( "string", "int toInt(int defaultValue = 0) const", asFUNCTION( ScriptString_ToInt ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "float toFloat(float defaultValue = 0) const", asFUNCTION( ScriptString_ToFloat ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );

    // Find methods
    r = engine->RegisterObjectMethod( "string", "bool startsWith(const string &in) const", asFUNCTION( ScriptString_StartsWith ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "bool endsWith(const string &in) const", asFUNCTION( ScriptString_EndsWith ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );

    r = engine->RegisterObjectMethod( "string", "string lower() const", asFUNCTION( ScriptString_Lower ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string upper() const", asFUNCTION( ScriptString_Upper ), asCALL_CDECL_OBJFIRST );
    RUNTIME_ASSERT( r >= 0 );

    r = engine->RegisterObjectMethod( "string", "array<string>@ split(const string &in) const", asFUNCTION( ScriptString_Split ), asCALL_CDECL_OBJLAST );
    RUNTIME_ASSERT( r >= 0 );
    r = engine->RegisterGlobalFunction( "string join(const array<string>@ &in, const string &in)", asFUNCTION( ScriptString_Join ), asCALL_CDECL );
    RUNTIME_ASSERT( r >= 0 );
}
