#include "scriptfile.h"
#include <new>
#include <assert.h>
#include <string>
#include <string.h>
#include <stdio.h>

// ---------------------------
// Compilation settings
//

// Set this flag to turn on/off write support
//  0 = off
//  1 = on

#define AS_WRITE_OPS    1

#ifdef _WIN32_WCE
# include <windows.h> // For GetModuleFileName
# ifdef GetObject
#  undef GetObject
# endif
#endif

using namespace std;

ScriptFile* ScriptFile_Factory()
{
    return new ScriptFile();
}

void RegisterScriptFile_Native( asIScriptEngine* engine )
{
    int r;

    r = engine->RegisterObjectType( "file", 0, asOBJ_REF );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "file", asBEHAVE_FACTORY, "file @f()", asFUNCTION( ScriptFile_Factory ), asCALL_CDECL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "file", asBEHAVE_ADDREF, "void f()", asMETHOD( ScriptFile, AddRef ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectBehaviour( "file", asBEHAVE_RELEASE, "void f()", asMETHOD( ScriptFile, Release ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectMethod( "file", "int open(const string &in, const string &in)", asMETHOD( ScriptFile, Open ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int close()", asMETHOD( ScriptFile, Close ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int getSize() const", asMETHOD( ScriptFile, GetSize ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "bool isEndOfFile() const", asMETHOD( ScriptFile, IsEOF ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int readString(uint, string &out)", asMETHOD( ScriptFile, ReadString ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int readLine(string &out)", asMETHOD( ScriptFile, ReadLine ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int64 readInt(uint)", asMETHOD( ScriptFile, ReadInt ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "uint64 readUInt(uint)", asMETHOD( ScriptFile, ReadUInt ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "float readFloat()", asMETHOD( ScriptFile, ReadFloat ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "double readDouble()", asMETHOD( ScriptFile, ReadDouble ), asCALL_THISCALL );
    assert( r >= 0 );
    #if AS_WRITE_OPS == 1
    r = engine->RegisterObjectMethod( "file", "int writeString(const string &in)", asMETHOD( ScriptFile, WriteString ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int writeInt(int64, uint)", asMETHOD( ScriptFile, WriteInt ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int writeUInt(uint64, uint)", asMETHOD( ScriptFile, WriteUInt ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int writeFloat(float)", asMETHOD( ScriptFile, WriteFloat ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int writeDouble(double)", asMETHOD( ScriptFile, WriteDouble ), asCALL_THISCALL );
    assert( r >= 0 );
    #endif
    r = engine->RegisterObjectMethod( "file", "int getPos() const", asMETHOD( ScriptFile, GetPos ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int setPos(int)", asMETHOD( ScriptFile, SetPos ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int movePos(int)", asMETHOD( ScriptFile, MovePos ), asCALL_THISCALL );
    assert( r >= 0 );

    r = engine->RegisterObjectProperty( "file", "bool mostSignificantByteFirst", asOFFSET( ScriptFile, mostSignificantByteFirst ) );
    assert( r >= 0 );

    // ////////////////////////////////////////////////////////////////////////
    r = engine->RegisterObjectMethod( "file", "string@ readWord()", asMETHOD( ScriptFile, ReadWord ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "int readNumber()", asMETHOD( ScriptFile, ReadNumber ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "uint8 readUint8()", asMETHOD( ScriptFile, ReadUint8 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "uint16 readUint16()", asMETHOD( ScriptFile, ReadUint16 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "uint32 readUint32()", asMETHOD( ScriptFile, ReadUint32 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "uint64 readUint64()", asMETHOD( ScriptFile, ReadUint64 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "uint readData(uint count, uint8[]& data)", asMETHOD( ScriptFile, ReadData ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "bool writeUint8(uint8 data)", asMETHOD( ScriptFile, WriteUint8 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "bool writeUint16(uint16 data)", asMETHOD( ScriptFile, WriteUint16 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "bool writeUint32(uint32 data)", asMETHOD( ScriptFile, WriteUint32 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "bool writeUint64(uint64 data)", asMETHOD( ScriptFile, WriteUint64 ), asCALL_THISCALL );
    assert( r >= 0 );
    r = engine->RegisterObjectMethod( "file", "bool writeData(uint8[]& data, uint count)", asMETHOD( ScriptFile, WriteData ), asCALL_THISCALL );
    assert( r >= 0 );
    // ////////////////////////////////////////////////////////////////////////
}

// ////////////////////////////////////////////////////////////////////////
ScriptString* ScriptFile::ReadWord()
{
    if( file == 0 )
        return 0;

    // Read the string
    char buf[ 1024 ];
    fscanf( file, "%s", buf );
    return new ScriptString( buf );
}

int ScriptFile::ReadNumber()
{
    if( file == 0 )
        return 0;

    // Read the string
    int result = 0;
    fscanf( file, "%d", &result );
    return result;
}

unsigned char ScriptFile::ReadUint8()
{
    if( !file )
        return 0;
    unsigned char data;
    if( !fread( &data, sizeof( data ), 1, file ) )
        return 0;
    return data;
}

unsigned short ScriptFile::ReadUint16()
{
    if( !file )
        return 0;
    unsigned short data;
    if( !fread( &data, sizeof( data ), 1, file ) )
        return 0;
    return data;
}

unsigned int ScriptFile::ReadUint32()
{
    if( !file )
        return 0;
    unsigned int data;
    if( !fread( &data, sizeof( data ), 1, file ) )
        return 0;
    return data;
}

asQWORD ScriptFile::ReadUint64()
{
    if( !file )
        return 0;
    asQWORD data;
    if( !fread( &data, sizeof( data ), 1, file ) )
        return 0;
    return data;
}

unsigned int ScriptFile::ReadData( unsigned int count, ScriptArray& data )
{
    if( !file )
        return 0;

    if( !count )
    {
        unsigned int pos = ftell( file );
        fseek( file, 0, SEEK_END );
        count = ftell( file ) - pos;
        fseek( file, pos, SEEK_SET );
        if( !count )
            return 0;
    }

    unsigned int size = data.GetSize();
    data.Resize( size + count );
    unsigned int r = (unsigned int) fread( data.At( size ), 1, count, file );
    if( r < count )
        data.Resize( size + r );
    return r;
}

bool ScriptFile::WriteUint8( unsigned char data )
{
    if( !file )
        return false;
    return fwrite( &data, sizeof( data ), 1, file ) != 0;
}

bool ScriptFile::WriteUint16( unsigned short data )
{
    if( !file )
        return false;
    return fwrite( &data, sizeof( data ), 1, file ) != 0;
}

bool ScriptFile::WriteUint32( unsigned int data )
{
    if( !file )
        return false;
    return fwrite( &data, sizeof( data ), 1, file ) != 0;
}

bool ScriptFile::WriteUint64( asQWORD data )
{
    if( !file )
        return false;
    return fwrite( &data, sizeof( data ), 1, file ) != 0;
}

bool ScriptFile::WriteData( ScriptArray& data, unsigned int count )
{
    if( !file )
        return false;
    if( !count )
        count = data.GetSize();
    else if( count > data.GetSize() )
        return false;
    if( !count )
        return false;
    return fwrite( data.At( 0 ), count, 1, file ) != 0;
}
// ////////////////////////////////////////////////////////////////////////

void RegisterScriptFile_Generic( asIScriptEngine* engine )
{}

void RegisterScriptFile( asIScriptEngine* engine )
{
    if( strstr( asGetLibraryOptions(), "AS_MAX_PORTABILITY" ) )
        RegisterScriptFile_Generic( engine );
    else
        RegisterScriptFile_Native( engine );
}

ScriptFile::ScriptFile()
{
    refCount = 1;
    file = 0;
    mostSignificantByteFirst = false;
}

ScriptFile::~ScriptFile()
{
    Close();
}

void ScriptFile::AddRef() const
{
    ++refCount;
}

void ScriptFile::Release() const
{
    if( --refCount == 0 )
        delete this;
}

int ScriptFile::Open( const ScriptString& filename, const ScriptString& mode )
{
    // Close the previously opened file handle
    if( file )
        Close();

    std::string myFilename = filename.c_std_str();

    // Validate the mode
    string m;
    #if AS_WRITE_OPS == 1
    if( mode.c_std_str() != "r" && mode.c_std_str() != "w" && mode.c_std_str() != "a" )
    #else
    if( mode.c_std_str() != "r" )
    #endif
        return -1;
    else
        m = mode.c_std_str();

    #ifdef _WIN32_WCE
    // no relative pathing on CE
    char         buf[ MAX_PATH ];
    static TCHAR apppath[ MAX_PATH ] = TEXT( "" );
    if( !apppath[ 0 ] )
    {
        GetModuleFileName( NULL, apppath, MAX_PATH );

        int appLen = _tcslen( apppath );
        while( appLen > 1 )
        {
            if( apppath[ appLen - 1 ] == TEXT( '\\' ) )
                break;
            appLen--;
        }

        // Terminate the string after the trailing backslash
        apppath[ appLen ] = TEXT( '\0' );
    }
    # ifdef _UNICODE
    wcstombs( buf, apppath, wcslen( apppath ) + 1 );
    # else
    memcpy( buf, apppath, strlen( apppath ) );
    # endif
    myFilename = buf + myFilename;
    #endif


    // By default windows translates "\r\n" to "\n", but we want to read the file as-is.
    m += "b";

    // Open the file
    #if _MSC_VER >= 1400 && !defined ( __S3E__ )
    // MSVC 8.0 / 2005 introduced new functions
    // Marmalade doesn't use these, even though it uses the MSVC compiler
    fopen_s( &file, myFilename.c_str(), m.c_str() );
    #else
    file = fopen( myFilename.c_str(), m.c_str() );
    #endif
    if( file == 0 )
        return -1;

    return 0;
}

int ScriptFile::Close()
{
    if( file == 0 )
        return -1;

    fclose( file );
    file = 0;

    return 0;
}

int ScriptFile::GetSize() const
{
    if( file == 0 )
        return -1;

    int pos = ftell( file );
    fseek( file, 0, SEEK_END );
    int size = ftell( file );
    fseek( file, pos, SEEK_SET );

    return size;
}

int ScriptFile::GetPos() const
{
    if( file == 0 )
        return -1;

    return ftell( file );
}

int ScriptFile::SetPos( int pos )
{
    if( file == 0 )
        return -1;

    int r = fseek( file, pos, SEEK_SET );

    // Return -1 on error
    return r ? -1 : 0;
}

int ScriptFile::MovePos( int delta )
{
    if( file == 0 )
        return -1;

    int r = fseek( file, delta, SEEK_CUR );

    // Return -1 on error
    return r ? -1 : 0;
}

int ScriptFile::ReadString( unsigned int length, ScriptString& str )
{
    if( file == 0 )
        return 0;

    // Read the string
    str.rawResize( length );
    int size = (int) fread( (char*) str.c_str(), 1, length, file );
    str.rawResize( size );

    return size;
}

int ScriptFile::ReadLine( ScriptString& str )
{
    if( file == 0 )
        return 0;

    // Read until the first new-line character
    str = "";
    char buf[ 256 ];

    do
    {
        // Get the current position so we can determine how many characters were read
        int start = ftell( file );

        // Set the last byte to something different that 0, so that we can check if the buffer was filled up
        buf[ 255 ] = 1;

        // Read the line (or first 255 characters, which ever comes first)
        char* r = fgets( buf, 256, file );
        if( r == 0 )
            break;

        // Get the position after the read
        int end = ftell( file );

        // Add the read characters to the output buffer
        str.append( buf, end - start );
    }
    while( !feof( file ) && buf[ 255 ] == 0 && buf[ 254 ] != '\n' );

    return int( str.length() );
}

asINT64 ScriptFile::ReadInt( asUINT bytes )
{
    if( file == 0 )
        return 0;

    if( bytes > 8 )
        bytes = 8;
    if( bytes == 0 )
        return 0;

    unsigned char buf[ 8 ];
    size_t        r = fread( buf, bytes, 1, file );
    if( r == 0 )
        return 0;

    asINT64 val = 0;
    if( mostSignificantByteFirst )
    {
        unsigned int n = 0;
        for( ; n < bytes; n++ )
            val |= asQWORD( buf[ n ] ) << ( ( bytes - n - 1 ) * 8 );
        if( buf[ 0 ] & 0x80 )
            for( ; n < 8; n++ )
                val |= asQWORD( 0xFF ) << ( n * 8 );
    }
    else
    {
        unsigned int n = 0;
        for( ; n < bytes; n++ )
            val |= asQWORD( buf[ n ] ) << ( n * 8 );
        if( buf[ 0 ] & 0x80 )
            for( ; n < 8; n++ )
                val |= asQWORD( 0xFF ) << ( n * 8 );
    }

    return val;
}

asQWORD ScriptFile::ReadUInt( asUINT bytes )
{
    if( file == 0 )
        return 0;

    if( bytes > 8 )
        bytes = 8;
    if( bytes == 0 )
        return 0;

    unsigned char buf[ 8 ];
    size_t        r = fread( buf, bytes, 1, file );
    if( r == 0 )
        return 0;

    asQWORD val = 0;
    if( mostSignificantByteFirst )
    {
        unsigned int n = 0;
        for( ; n < bytes; n++ )
            val |= asQWORD( buf[ n ] ) << ( ( bytes - n - 1 ) * 8 );
    }
    else
    {
        unsigned int n = 0;
        for( ; n < bytes; n++ )
            val |= asQWORD( buf[ n ] ) << ( n * 8 );
    }

    return val;
}

float ScriptFile::ReadFloat()
{
    if( file == 0 )
        return 0;

    unsigned char buf[ 4 ];
    size_t        r = fread( buf, 4, 1, file );
    if( r == 0 )
        return 0;

    asUINT val = 0;
    if( mostSignificantByteFirst )
    {
        unsigned int n = 0;
        for( ; n < 4; n++ )
            val |= asUINT( buf[ n ] ) << ( ( 3 - n ) * 8 );
    }
    else
    {
        unsigned int n = 0;
        for( ; n < 4; n++ )
            val |= asUINT( buf[ n ] ) << ( n * 8 );
    }

    return *reinterpret_cast< float* >( &val );
}

double ScriptFile::ReadDouble()
{
    if( file == 0 )
        return 0;

    unsigned char buf[ 8 ];
    size_t        r = fread( buf, 8, 1, file );
    if( r == 0 )
        return 0;

    asQWORD val = 0;
    if( mostSignificantByteFirst )
    {
        unsigned int n = 0;
        for( ; n < 8; n++ )
            val |= asQWORD( buf[ n ] ) << ( ( 7 - n ) * 8 );
    }
    else
    {
        unsigned int n = 0;
        for( ; n < 8; n++ )
            val |= asQWORD( buf[ n ] ) << ( n * 8 );
    }

    return *reinterpret_cast< double* >( &val );
}

bool ScriptFile::IsEOF() const
{
    if( file == 0 )
        return true;

    return feof( file ) ? true : false;
}

#if AS_WRITE_OPS == 1
int ScriptFile::WriteString( const ScriptString& str )
{
    if( file == 0 )
        return -1;

    // Write the entire string
    size_t r = fwrite( str.c_str(), 1, str.length(), file );

    return int(r);
}

int ScriptFile::WriteInt( asINT64 val, asUINT bytes )
{
    if( file == 0 )
        return 0;

    unsigned char buf[ 8 ];
    if( mostSignificantByteFirst )
    {
        for( unsigned int n = 0; n < bytes; n++ )
            buf[ n ] = ( val >> ( ( bytes - n - 1 ) * 8 ) ) & 0xFF;
    }
    else
    {
        for( unsigned int n = 0; n < bytes; n++ )
            buf[ n ] = ( val >> ( n * 8 ) ) & 0xFF;
    }

    size_t r = fwrite( &buf, bytes, 1, file );
    return int(r);
}

int ScriptFile::WriteUInt( asQWORD val, asUINT bytes )
{
    if( file == 0 )
        return 0;

    unsigned char buf[ 8 ];
    if( mostSignificantByteFirst )
    {
        for( unsigned int n = 0; n < bytes; n++ )
            buf[ n ] = ( val >> ( ( bytes - n - 1 ) * 8 ) ) & 0xFF;
    }
    else
    {
        for( unsigned int n = 0; n < bytes; n++ )
            buf[ n ] = ( val >> ( n * 8 ) ) & 0xFF;
    }

    size_t r = fwrite( &buf, bytes, 1, file );
    return int(r);
}

int ScriptFile::WriteFloat( float f )
{
    if( file == 0 )
        return 0;

    unsigned char buf[ 4 ];
    asUINT        val = *reinterpret_cast< asUINT* >( &f );
    if( mostSignificantByteFirst )
    {
        for( unsigned int n = 0; n < 4; n++ )
            buf[ n ] = ( val >> ( ( 3 - n ) * 4 ) ) & 0xFF;
    }
    else
    {
        for( unsigned int n = 0; n < 4; n++ )
            buf[ n ] = ( val >> ( n * 8 ) ) & 0xFF;
    }

    size_t r = fwrite( &buf, 4, 1, file );
    return int(r);
}

int ScriptFile::WriteDouble( double d )
{
    if( file == 0 )
        return 0;

    unsigned char buf[ 8 ];
    asQWORD       val = *reinterpret_cast< asQWORD* >( &d );
    if( mostSignificantByteFirst )
    {
        for( unsigned int n = 0; n < 8; n++ )
            buf[ n ] = ( val >> ( ( 7 - n ) * 8 ) ) & 0xFF;
    }
    else
    {
        for( unsigned int n = 0; n < 8; n++ )
            buf[ n ] = ( val >> ( n * 8 ) ) & 0xFF;
    }

    size_t r = fwrite( &buf, 8, 1, file );
    return int(r);
}
#endif
