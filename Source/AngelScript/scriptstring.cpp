#include "scriptstring.h"
#include "scriptarray.h"
#include <Common.h>
#include <Debugger.h>
#include <Text.h>

#define assert( x )

bool ScriptString::indexByteToUTF8( int& index, uint* length, uint offset )
{
    if( index < 0 )
    {
        index = (int) lengthUTF8() + index;
        if( index < 0 )
        {
            index = 0;
            if( length )
            {
                if( !buffer.empty() )
                    Str::DecodeUTF8( c_str(), length );
                else
                    *length = 0;
            }
            return false;
        }
    }

    const char* str_begin = buffer.c_str() + offset;
    const char* str = str_begin;
    while( *str )
    {
        uint ch_length;
        Str::DecodeUTF8( str, &ch_length );
        if( index > 0 )
        {
            str += ch_length;
            index--;
        }
        else
        {
            index = (uint) ( str - str_begin );
            if( length )
                *length = ch_length;
            return true;
        }
    }
    index = (uint) ( str - str_begin );
    if( length )
        *length = 0;
    return false;
}

// --------------
// constructors
// --------------

ScriptString::ScriptString()
{
    // Count the first reference
    refCount = 1;
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, sizeof( ScriptString ) + (uint) buffer.capacity() );
}

ScriptString::ScriptString( const char* s, uint len )
{
    refCount = 1;
    buffer.assign( s, len );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, sizeof( ScriptString ) + (uint) buffer.capacity() );
}

ScriptString::ScriptString( const char* s )
{
    refCount = 1;
    buffer.assign( s, strlen( s ) );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, sizeof( ScriptString ) + (uint) buffer.capacity() );
}

ScriptString::ScriptString( const string& s )
{
    refCount = 1;
    buffer = s;
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, sizeof( ScriptString ) + (uint) buffer.capacity() );
}

ScriptString::ScriptString( const ScriptString& s )
{
    refCount = 1;
    buffer = s.buffer;
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, sizeof( ScriptString ) + (uint) buffer.capacity() );
}

ScriptString::~ScriptString()
{
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, -(int) ( sizeof( ScriptString ) + (uint) buffer.capacity() ) );
}

void ScriptString::assign( const char* buf, uint count )
{
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, -(int) buffer.capacity() );
    buffer.assign( buf, count );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, (uint) buffer.capacity() );
}

void ScriptString::assign( const char* buf )
{
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, -(int) buffer.capacity() );
    buffer.assign( buf );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, (uint) buffer.capacity() );
}

void ScriptString::append( const char* buf, uint count )
{
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, -(int) buffer.capacity() );
    buffer.append( buf, count );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, (uint) buffer.capacity() );
}

void ScriptString::append( const char* buf )
{
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, -(int) buffer.capacity() );
    buffer.append( buf );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, (uint) buffer.capacity() );
}

void ScriptString::reserve( uint count )
{
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, -(int) buffer.capacity() );
    buffer.reserve( count );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, (uint) buffer.capacity() );
}

void ScriptString::rawResize( uint count )
{
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, -(int) buffer.capacity() );
    buffer.resize( count );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, (uint) buffer.capacity() );
}

uint ScriptString::lengthUTF8() const
{
    return Str::LengthUTF8( buffer.c_str() );
}

// --------------------
// reference counting
// --------------------

void ScriptString::AddRef() const
{
    refCount++;
}

void ScriptString::Release() const
{
    if( --refCount == 0 )
        delete this;
}

// -----------------
// string opCmp, opEquals string
// -----------------

static bool StringEquals( const ScriptString& a, const ScriptString& b )
{
    return a.c_std_str() == b.c_std_str();
}

static int StringCmp( const ScriptString& a, const ScriptString& b )
{
    int cmp = 0;
    if( a.c_std_str() < b.c_std_str() )
        cmp = -1;
    else if( a.c_std_str() > b.c_std_str() )
        cmp = 1;
    return cmp;
}

// -----------------
// string + string
// -----------------

ScriptString* operator+( const ScriptString& a, const ScriptString& b )
{
    // Return a new object as a script handle
    ScriptString* str = new ScriptString();

    // Avoid unnecessary memory copying by first reserving the full memory buffer, then concatenating
    str->reserve( a.length() + b.length() );
    *str += a;
    *str += b;

    return str;
}

// ----------------
// string = value
// ----------------

static ScriptString& AssignUIntToString( uint i, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%u", i );
    dest = buf;
    return dest;
}

static ScriptString& AssignIntToString( int i, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%d", i );
    dest = buf;
    return dest;
}

static ScriptString& AssignFloatToString( float f, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%g", f );
    dest = buf;
    return dest;
}

static ScriptString& AssignDoubleToString( double f, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%g", f );
    dest = buf;
    return dest;
}

static ScriptString& AssignBoolToString( bool b, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%s", b ? "true" : "false" );
    dest = buf;
    return dest;
}

// -----------------
// string += value
// -----------------

static ScriptString& AddAssignUIntToString( uint i, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%u", i );
    dest += buf;
    return dest;
}

static ScriptString& AddAssignIntToString( int i, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%d", i );
    dest += buf;
    return dest;
}

static ScriptString& AddAssignFloatToString( float f, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%g", f );
    dest += buf;
    return dest;
}

static ScriptString& AddAssignDoubleToString( double f, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%g", f );
    dest += buf;
    return dest;
}

static ScriptString& AddAssignBoolToString( bool b, ScriptString& dest )
{
    char buf[ 100 ];
    sprintf( buf, "%s", b ? "true" : "false" );
    dest += buf;
    return dest;
}

// ----------------
// string + value
// ----------------

static ScriptString* AddStringUInt( const ScriptString& str, uint i )
{
    char          buf[ 100 ];
    sprintf( buf, "%u", i );
    ScriptString* str_ = new ScriptString( str );
    *str_ += buf;
    return str_;
}

static ScriptString* AddStringInt( const ScriptString& str, int i )
{
    char          buf[ 100 ];
    sprintf( buf, "%d", i );
    ScriptString* str_ = new ScriptString( str );
    *str_ += buf;
    return str_;
}

static ScriptString* AddStringFloat( const ScriptString& str, float f )
{
    char          buf[ 100 ];
    sprintf( buf, "%g", f );
    ScriptString* str_ = new ScriptString( str );
    *str_ += buf;
    return str_;
}

static ScriptString* AddStringDouble( const ScriptString& str, double f )
{
    char          buf[ 100 ];
    sprintf( buf, "%g", f );
    ScriptString* str_ = new ScriptString( str );
    *str_ += buf;
    return str_;
}

static ScriptString* AddStringBool( const ScriptString& str, bool b )
{
    char          buf[ 100 ];
    sprintf( buf, "%s", b ? "true" : "false" );
    ScriptString* str_ = new ScriptString( str );
    *str_ += buf;
    return str_;
}

// ----------------
// value + string
// ----------------

static ScriptString* AddIntString( int i, const ScriptString& str )
{
    char          buf[ 100 ];
    sprintf( buf, "%d", i );
    ScriptString* str_ = new ScriptString( buf );
    *str_ += str;
    return str_;
}

static ScriptString* AddUIntString( uint i, const ScriptString& str )
{
    char          buf[ 100 ];
    sprintf( buf, "%u", i );
    ScriptString* str_ = new ScriptString( buf );
    *str_ += str;
    return str_;
}

static ScriptString* AddFloatString( float f, const ScriptString& str )
{
    char          buf[ 100 ];
    sprintf( buf, "%g", f );
    ScriptString* str_ = new ScriptString( buf );
    *str_ += str;
    return str_;
}

static ScriptString* AddDoubleString( double f, const ScriptString& str )
{
    char          buf[ 100 ];
    sprintf( buf, "%g", f );
    ScriptString* str_ = new ScriptString( buf );
    *str_ += str;
    return str_;
}

static ScriptString* AddBoolString( bool b, const ScriptString& str )
{
    char          buf[ 100 ];
    sprintf( buf, "%s", b ? "true" : "false" );
    ScriptString* str_ = new ScriptString( buf );
    *str_ += str;
    return str_;
}

// ----------
// string[]
// ----------

static ScriptString* GetStringAt( int i, ScriptString& str )
{
    uint length;
    if( !str.indexByteToUTF8( i, &length ) )
    {
        // Set a script exception
        asIScriptContext* ctx = asGetActiveContext();
        ctx->SetException( "Out of range" );

        // Return a null pointer
        return 0;
    }

    return new ScriptString( str.c_str() + i, length );
}

static void SetStringAt( int i, ScriptString& value, ScriptString& str )
{
    uint length;
    if( !str.indexByteToUTF8( i, &length ) )
    {
        // Set a script exception
        asIScriptContext* ctx = asGetActiveContext();
        ctx->SetException( "Out of range" );
        return;
    }

    string& buffer = *(string*) &str.c_std_str();
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, -(int) buffer.capacity() );
    if( length )
        buffer.erase( i, length );
    if( value.length() )
        buffer.insert( i, value.c_str() );
    MEMORY_PROCESS( MEMORY_SCRIPT_STRING, (uint) buffer.capacity() );
}

// -----------------------
// AngelScript functions
// -----------------------

// This is the string factory that creates new strings for the script based on string literals
static ScriptString* StringFactory( asUINT length, const char* s )
{
    return new ScriptString( s, length );
}

// This is the default string factory, that is responsible for creating empty string objects, e.g. when a variable is declared
static ScriptString* StringDefaultFactory()
{
    // Allocate and initialize with the default constructor
    return new ScriptString();
}

static ScriptString* StringCopyFactory( const ScriptString& other )
{
    // Allocate and initialize with the copy constructor
    return new ScriptString( other );
}

// This is where we register the string type
void RegisterScriptStringUtils( asIScriptEngine* engine );
void RegisterScriptString( asIScriptEngine* engine )
{
    int r;

    // Register the type
    r = engine->RegisterObjectType( "string", 0, asOBJ_REF );
    assert( r >= 0 );

    // Register the object operator overloads
    // Note: We don't have to register the destructor, since the object uses reference counting
    r = engine->RegisterObjectBehaviour( "string", asBEHAVE_FACTORY,    "string @f()",                 asFUNCTION( StringDefaultFactory ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "string", asBEHAVE_FACTORY,    "string @f(const string &in)", asFUNCTION( StringCopyFactory ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "string", asBEHAVE_ADDREF,     "void f()",                    asMETHOD( ScriptString, AddRef ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "string", asBEHAVE_RELEASE,    "void f()",                    asMETHOD( ScriptString, Release ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string &opAssign(const string &in)", asMETHODPR( ScriptString, operator=, ( const ScriptString & ), ScriptString & ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string &opAddAssign(const string &in)", asMETHODPR( ScriptString, operator+=, ( const ScriptString & ), ScriptString & ), asCALL_THISCALL );
    assert( r >= 0 );

    // Register the factory to return a handle to a new string
    // Note: We must register the string factory after the basic behaviours,
    // otherwise the library will not allow the use of object handles for this type
    r = engine->RegisterStringFactory( "string@", asFUNCTION( StringFactory ), asCALL_CDECL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "string", "bool opEquals(const string &in) const", asFUNCTION( StringEquals ), asCALL_CDECL_OBJFIRST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "int opCmp(const string &in) const", asFUNCTION( StringCmp ), asCALL_CDECL_OBJFIRST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd(const string &in) const", asFUNCTIONPR( operator+, ( const ScriptString &, const ScriptString & ), ScriptString* ), asCALL_CDECL_OBJFIRST );
    assert( r >= 0 );

    // Register the index operator, both as a mutator and as an inspector
    r = engine->RegisterObjectMethod( "string", "string@ get_opIndex(int) const", asFUNCTION( GetStringAt ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "void set_opIndex(int, const string &in) const", asFUNCTION( SetStringAt ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );

    // Register the object methods
    r = engine->RegisterObjectMethod( "string", "uint length() const", asMETHOD( ScriptString, lengthUTF8 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "uint rawLength() const", asMETHOD( ScriptString, length ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "void rawResize(uint)", asMETHODPR( ScriptString, rawResize, ( uint ), void ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "uint8 rawGet(uint) const", asMETHODPR( ScriptString, rawGet, ( uint ), char ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "void rawSet(uint, uint8)", asMETHODPR( ScriptString, rawSet, ( uint, char ), void ), asCALL_THISCALL );
    assert( r >= 0 );

    // TODO: Add factory  string(const string &in str, int repeatCount)

    // TODO: Add explicit type conversion via constructor and value cast

    // TODO: Add parseInt and parseDouble. Two versions, one without parameter, one with an outparm that returns the number of characters parsed.

    // Automatic conversion from values
    r = engine->RegisterObjectMethod( "string", "string &opAssign(double)", asFUNCTION( AssignDoubleToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string &opAddAssign(double)", asFUNCTION( AddAssignDoubleToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd(double) const", asFUNCTION( AddStringDouble ), asCALL_CDECL_OBJFIRST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd_r(double) const", asFUNCTION( AddDoubleString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "string", "string &opAssign(float)", asFUNCTION( AssignFloatToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string &opAddAssign(float)", asFUNCTION( AddAssignFloatToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd(float) const", asFUNCTION( AddStringFloat ), asCALL_CDECL_OBJFIRST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd_r(float) const", asFUNCTION( AddFloatString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "string", "string &opAssign(int)", asFUNCTION( AssignIntToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string &opAddAssign(int)", asFUNCTION( AddAssignIntToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd(int) const", asFUNCTION( AddStringInt ), asCALL_CDECL_OBJFIRST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd_r(int) const", asFUNCTION( AddIntString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "string", "string &opAssign(uint)", asFUNCTION( AssignUIntToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string &opAddAssign(uint)", asFUNCTION( AddAssignUIntToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd(uint) const", asFUNCTION( AddStringUInt ), asCALL_CDECL_OBJFIRST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd_r(uint) const", asFUNCTION( AddUIntString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "string", "string &opAssign(bool)", asFUNCTION( AssignBoolToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string &opAddAssign(bool)", asFUNCTION( AddAssignBoolToString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd(bool) const", asFUNCTION( AddStringBool ), asCALL_CDECL_OBJFIRST );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "string", "string@ opAdd_r(bool) const", asFUNCTION( AddBoolString ), asCALL_CDECL_OBJLAST );
    assert( r >= 0 );

    RegisterScriptStringUtils( engine );
}

// This function returns a string containing the substring of the input string
// determined by the starting index and count of characters.
ScriptString* StringSubString( ScriptString* str, int start, uint count )
{
    if( !str->indexByteToUTF8( start ) )
        return new ScriptString( "" );
    int count_ = (int) count;
    str->indexByteToUTF8( count_, NULL, start );
    return new ScriptString( str->c_std_str().substr( start, count_ ) );
}

// This function returns the index of the first position where the substring
// exists in the input string. If the substring doesn't exist in the input
// string -1 is returned.
int StringFindFirst( ScriptString* str, ScriptString* sub, int start )
{
    if( !str->indexByteToUTF8( start ) )
        return -1;
    return (int) str->c_std_str().find( sub->c_std_str(), start );
}

// This function returns the index of the last position where the substring
// exists in the input string. If the substring doesn't exist in the input
// string -1 is returned.
int StringFindLast( ScriptString* str, ScriptString* sub, int start )
{
    if( !str->indexByteToUTF8( start ) )
        return -1;
    return (int) str->c_std_str().rfind( sub->c_std_str(), start );
}

// This function returns the index of the first character that is in
// the specified set of characters. If no such character is found -1 is
// returned.
int StringFindFirstOf( ScriptString* str, ScriptString* chars, int start )
{
    if( !str->indexByteToUTF8( start ) )
        return -1;
    return (int) str->c_std_str().find_first_of( chars->c_std_str(), start );
}

// This function returns the index of the first character that is not in
// the specified set of characters. If no such character is found -1 is
// returned.
int StringFindFirstNotOf( ScriptString* str, ScriptString* chars, int start )
{
    if( !str->indexByteToUTF8( start ) )
        return -1;
    return (int) str->c_std_str().find_first_not_of( chars->c_std_str(), start );
}

// This function returns the index of the last character that is in
// the specified set of characters. If no such character is found -1 is
// returned.
int StringFindLastOf( ScriptString* str, ScriptString* chars, int start )
{
    if( !str->indexByteToUTF8( start ) )
        return -1;
    return (int) str->c_std_str().find_last_of( chars->c_std_str(), start );
}

// This function returns the index of the last character that is not in
// the specified set of characters. If no such character is found -1 is
// returned.
int StringFindLastNotOf( ScriptString* str, ScriptString* chars, int start )
{
    if( !str->indexByteToUTF8( start ) )
        return -1;
    return (int) str->c_std_str().find_last_not_of( chars->c_std_str(), start );
}

// This function takes an input string and splits it into parts by looking
// for a specified delimiter. Example:
//
// string str = "A|B||D";
// string@[]@ array = split(str, "|");
//
// The resulting array has the following elements:
//
// {"A", "B", "", "D"}
ScriptArray* StringSplit( ScriptString* str, ScriptString* delim )
{
    // Obtain a pointer to the engine
    asIScriptContext* ctx = asGetActiveContext();
    asIScriptEngine*  engine = ctx->GetEngine();

    // TODO: This should only be done once
    // TODO: This assumes that ScriptArray was already registered
    asIObjectType* arrayType = engine->GetObjectTypeById( engine->GetTypeIdByDecl( "array<string@>" ) );

    // Create the array object
    ScriptArray* array = new ScriptArray( 0, arrayType );

    // Find the existence of the delimiter in the input string
    int pos = 0, prev = 0, count = 0;
    while( ( pos = (int) str->c_std_str().find( delim->c_std_str(), prev ) ) != (int) std::string::npos )
    {
        // Add the part to the array
        ScriptString* part = new ScriptString();
        part->assign( &str->c_str()[ prev ], pos - prev );
        array->Resize( array->GetSize() + 1 );
        *(ScriptString**) array->At( count ) = part;

        // Find the next part
        count++;
        prev = pos + (int) delim->length();
    }

    // Add the remaining part
    ScriptString* part = new ScriptString();
    part->assign( &str->c_str()[ prev ] );
    array->Resize( array->GetSize() + 1 );
    *(ScriptString**) array->At( count ) = part;

    // Return the array by handle
    return array;
}

// This function takes an input string and splits it into parts by looking
// for a specified delimiter. Example:
//
// string str = "A| B\t\t||D| E|F   ";
// string@[]@ array = splitEx(str, "|");
//
// The resulting array has the following elements:
//
// {"A", "B", "D", "E", "F"}
ScriptArray* StringSplitEx( ScriptString* str, ScriptString* delim )
{
    // Obtain a pointer to the engine
    asIScriptContext* ctx = asGetActiveContext();
    asIScriptEngine*  engine = ctx->GetEngine();

    // TODO: This should only be done once
    // TODO: This assumes that ScriptArray was already registered
    asIObjectType* arrayType = engine->GetObjectTypeById( engine->GetTypeIdByDecl( "array<string@>" ) );

    // Create the array object
    ScriptArray* array = new ScriptArray( 0, arrayType );

    // Find the existence of the delimiter in the input string
    const char* cstr = str->c_str();
    int         pos = 0, prev = 0, count = 0;
    while( ( pos = (int) str->c_std_str().find( delim->c_std_str(), prev ) ) != (int) std::string::npos )
    {
        // Manage part
        int pos_ = pos;
        for( int i = prev; i < pos && ( cstr[ i ] == ' ' || cstr[ i ] == '\t' || cstr[ i ] == '\r' || cstr[ i ] == '\n' ); i++ )
            prev++;
        for( int i = pos - 1; i > prev && ( cstr[ i ] == ' ' || cstr[ i ] == '\t' || cstr[ i ] == '\r' || cstr[ i ] == '\n' ); i-- )
            pos--;
        if( prev == pos )
        {
            prev = pos_ + (int) delim->length();
            continue;
        }

        // Add the part to the array
        ScriptString* part = new ScriptString();
        part->assign( &str->c_str()[ prev ], pos - prev );
        array->Resize( array->GetSize() + 1 );
        *(ScriptString**) array->At( count ) = part;

        // Find the next part
        count++;
        prev = pos_ + (int) delim->length();
    }

    // Add the remaining part
    ScriptString* part = new ScriptString();
    part->assign( &str->c_str()[ prev ] );
    array->Resize( array->GetSize() + 1 );
    *(ScriptString**) array->At( count ) = part;

    // Return the array by handle
    return array;
}

// This function takes as input an array of string handles as well as a
// delimiter and concatenates the array elements into one delimited string.
// Example:
//
// string@[] array = {"A", "B", "", "D"};
// string str = join(array, "|");
//
// The resulting string is:
//
// "A|B||D"
ScriptString* StringJoin( ScriptArray* array, ScriptString* delim )
{
    // Create the new string
    ScriptString* str = new ScriptString();
    int           n;
    for( n = 0; n < (int) array->GetSize() - 1; n++ )
    {
        ScriptString* part = *(ScriptString**) array->At( n );
        *str += *part;
        *str += *delim;
    }

    // Add the last part
    ScriptString* part = *(ScriptString**) array->At( n );
    *str += *part;

    // Return the string
    return str;
}

ScriptString* StringStrLwr( ScriptString* str )
{
    std::string str_ = str->c_std_str();
    Str::LowerUTF8( (char*) str_.c_str() );
    return new ScriptString( str_ );
}

ScriptString* StringStrUpr( ScriptString* str )
{
    std::string str_ = str->c_std_str();
    Str::UpperUTF8( (char*) str_.c_str() );
    return new ScriptString( str_ );
}

// TODO: Implement the following functions
//
//       int64    parseInt(const string &in str, int &out bytesParsed);
//       double   parseDouble(const string &in str, int &out bytesParsed);
//       string @ formatString(int64, const string &in format);  // should use sprintf to format the string
//       string @ formatDouble(double, const string &in format);
//
//       int16    byteStringToInt16(const string &in str, int start);
//       int32    byteStringToInt32(const string &in str, int start);
//       int64    byteStringtoInt64(const string &in str, int start);
//       float    byteStringToFloat(const string &in str, int start);
//       double   byteStringToDouble(const string &in str, int start);
//       string @ int16ToByteString(int16);
//       string @ int32ToByteString(int32);
//       string @ int64ToByteString(int64);
//       string @ floatToByteString(float);
//       string @ doubleToByteString(double);

void RegisterScriptStringUtils( asIScriptEngine* engine )
{
    int r;

    r = engine->RegisterGlobalFunction( "string@ substring(const string &in, int, uint)", asFUNCTION( StringSubString ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "int findFirst(const string &in, const string &in, int start = 0)", asFUNCTION( StringFindFirst ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "int findLast(const string &in, const string &in, int start = 0)", asFUNCTION( StringFindLast ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "int findFirstOf(const string &in, const string &in, int start = 0)", asFUNCTION( StringFindFirstOf ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "int findFirstNotOf(const string &in, const string &in, int start = 0)", asFUNCTION( StringFindFirstNotOf ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "int findLastOf(const string &in, const string &in, int start = 0)", asFUNCTION( StringFindLastOf ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "int findLastNotOf(const string &in, const string &in, int start = 0)", asFUNCTION( StringFindLastNotOf ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "array<string@>@ split(const string &in, const string &in)", asFUNCTION( StringSplit ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "array<string@>@ splitEx(const string &in, const string &in)", asFUNCTION( StringSplitEx ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "string@ join(const array<string@> &in, const string &in)", asFUNCTION( StringJoin ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "string@ strlwr(const string &in)", asFUNCTION( StringStrLwr ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterGlobalFunction( "string@ strupr(const string &in)", asFUNCTION( StringStrUpr ), asCALL_CDECL );
    assert( r >= 0 );
}
