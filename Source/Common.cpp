#include "Common.h"
#include "Exception.h"
#include "Crypt.h"
#include "Script.h"
#include "Text.h"
#include <math.h>
#include "FileManager.h"
#include "IniParser.h"
#include <stdarg.h>

// Check the sizes of base types
STATIC_ASSERT( sizeof( char ) == 1 );
STATIC_ASSERT( sizeof( short ) == 2 );
STATIC_ASSERT( sizeof( int ) == 4 );
STATIC_ASSERT( sizeof( int64 ) == 8 );
STATIC_ASSERT( sizeof( uchar ) == 1 );
STATIC_ASSERT( sizeof( ushort ) == 2 );
STATIC_ASSERT( sizeof( uint ) == 4 );
STATIC_ASSERT( sizeof( uint64 ) == 8 );
STATIC_ASSERT( sizeof( bool ) == 1 );
#if defined ( FO_X86 )
STATIC_ASSERT( sizeof( size_t ) == 4 );
STATIC_ASSERT( sizeof( void* ) == 4 );
#elif defined ( FO_X64 )
STATIC_ASSERT( sizeof( size_t ) == 8 );
STATIC_ASSERT( sizeof( void* ) == 8 );
#endif
STATIC_ASSERT( sizeof( uint64 ) >= sizeof( void* ) );

// Some checks from AngelScript config
#include "AngelScript/sdk/angelscript/source/as_config.h"
#ifdef AS_BIG_ENDIAN
# error "Big Endian architectures not supported."
#endif

#pragma MESSAGE("Add TARGET_HEX.")

/************************************************************************/
/*                                                                      */
/************************************************************************/

IniParser* MainConfig;
StrVec     GameModules;

void InitialSetup( uint argc, char** argv )
{
    // Parse command line args
    StrVec configs;
    for( uint i = 0; i < argc; i++ )
    {
        // Skip path
        if( i == 0 && argv[ 0 ][ 0 ] != '-' )
            continue;

        // Find work path entry
        if( Str::Compare( argv[ i ], "-WorkDir" ) && i < argc - 1 )
            GameOpt.WorkDir = argv[ i + 1 ];
        if( Str::Compare( argv[ i ], "-ServerDir" ) && i < argc - 1 )
            GameOpt.ServerDir = argv[ i + 1 ];

        // Find config path entry
        if( Str::Compare( argv[ i ], "-AddConfig" ) && i < argc - 1 )
            configs.push_back( argv[ i + 1 ] );

        // Make sinle line
        GameOpt.CommandLine += argv[ i ];
        if( i < argc - 1 )
            GameOpt.CommandLine += " ";
    }

    // Store start directory
    if( !GameOpt.WorkDir.empty() )
    {
        #ifdef FO_WINDOWS
        SetCurrentDirectoryW( CharToWideChar( GameOpt.WorkDir ).c_str() );
        #else
        chdir( GameOpt.WorkDir.c_str() );
        #endif
    }
    #ifdef FO_WINDOWS
    wchar_t buf[ TEMP_BUF_SIZE ];
    GetCurrentDirectoryW( TEMP_BUF_SIZE, buf );
    GameOpt.WorkDir = WideCharToChar( buf );
    #else
    char buf[ TEMP_BUF_SIZE ];
    getcwd( buf, sizeof( buf ) );
    GameOpt.WorkDir = buf;
    #endif

    // Injected config
    static char InternalConfig[ 5032 ] =
    {
        "###InternalConfig###5032\0"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
        "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    };
    MainConfig = new IniParser();
    MainConfig->AppendStr( InternalConfig );

    // File configs
    MainConfig->AppendFile( "Cache/" CONFIG_NAME );
    for( string& config : configs )
        MainConfig->AppendFile( config.c_str() );

    // Command line config
    for( uint i = 0; i < argc; i++ )
    {
        const char* arg = argv[ i ];
        uint        arg_len = Str::Length( arg );
        if( arg_len < 2 || arg[ 0 ] != '-' )
            continue;

        string arg_value;
        while( i < argc - 1 && argv[ i + 1 ][ 0 ] != '-' )
        {
            if( arg_value.length() > 0 )
                arg_value += " ";
            arg_value += argv[ i + 1 ];
            i++;
        }

        MainConfig->SetStr( "", arg + 1, arg_value.c_str() );
    }

    // Cache modules
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_MAPPER ) || defined ( FONLINE_SCRIPT_COMPILER )
    const char* modules = MainConfig->GetStr( "", "Modules" );
    RUNTIME_ASSERT( modules );
    StrVec      modules_arr;
    Str::ParseLine( modules, ';', modules_arr, Str::ParseLineDummy );
    for( size_t i = 0; i < modules_arr.size(); i++ )
    {
        size_t pos = modules_arr[ i ].find_last_of( '/' );
        RUNTIME_ASSERT( pos != string::npos );

        char name[ MAX_FOTEXT ];
        Str::Copy( name, modules_arr[ i ].substr( pos + 1 ).c_str() );
        char dir[ MAX_FOTEXT ];
        Str::Copy( dir, modules_arr[ i ].substr( 0, pos + 1 ).c_str() );
        ResolvePath( dir );

        char full_dir[ MAX_FOTEXT ];
        Str::Copy( full_dir, modules_arr[ i ].c_str() );
        Str::Append( full_dir, "/" );
        ResolvePath( full_dir );
        Str::Replacement( full_dir, '\\', '/' );
        GameModules.push_back( string( full_dir ) );
    }
    #endif
}

// Default randomizer
Randomizer DefaultRandomizer;
int Random( int minimum, int maximum )
{
    return DefaultRandomizer.Random( minimum, maximum );
}

// Math stuff
int Procent( int full, int peace )
{
    if( !full )
        return 0;
    int procent = peace * 100 / full;
    return CLAMP( procent, 0, 100 );
}

uint NumericalNumber( uint num )
{
    if( num & 1 )
        return num * ( num / 2 + 1 );
    else
        return num * num / 2 + num / 2;
}

uint DistSqrt( int x1, int y1, int x2, int y2 )
{
    int dx = x1 - x2;
    int dy = y1 - y2;
    return (uint) sqrt( double(dx * dx + dy * dy) );
}

uint DistGame( int x1, int y1, int x2, int y2 )
{
    if( GameOpt.MapHexagonal )
    {
        int dx = ( x1 > x2 ? x1 - x2 : x2 - x1 );
        if( !( x1 & 1 ) )
        {
            if( y2 <= y1 )
            {
                int rx = y1 - y2 - dx / 2;
                return dx + ( rx > 0 ? rx : 0 );
            }
            else
            {
                int rx = y2 - y1 - ( dx + 1 ) / 2;
                return dx + ( rx > 0 ? rx : 0 );
            }
        }
        else
        {
            if( y2 >= y1 )
            {
                int rx = y2 - y1 - dx / 2;
                return dx + ( rx > 0 ? rx : 0 );
            }
            else
            {
                int rx = y1 - y2 - ( dx + 1 ) / 2;
                return dx + ( rx > 0 ? rx : 0 );
            }
        }
    }
    else
    {
        int dx = abs( x2 - x1 );
        int dy = abs( y2 - y1 );
        return MAX( dx, dy );
    }
}

int GetNearDir( int x1, int y1, int x2, int y2 )
{
    int dir = 0;

    if( GameOpt.MapHexagonal )
    {
        if( x1 & 1 )
        {
            if( x1 > x2 && y1 > y2 )
                dir = 0;
            else if( x1 > x2 && y1 == y2 )
                dir = 1;
            else if( x1 == x2 && y1 < y2 )
                dir = 2;
            else if( x1 < x2 && y1 == y2 )
                dir = 3;
            else if( x1 < x2 && y1 > y2 )
                dir = 4;
            else if( x1 == x2 && y1 > y2 )
                dir = 5;
        }
        else
        {
            if( x1 > x2 && y1 == y2 )
                dir = 0;
            else if( x1 > x2 && y1 < y2 )
                dir = 1;
            else if( x1 == x2 && y1 < y2 )
                dir = 2;
            else if( x1 < x2 && y1 < y2 )
                dir = 3;
            else if( x1 < x2 && y1 == y2 )
                dir = 4;
            else if( x1 == x2 && y1 > y2 )
                dir = 5;
        }
    }
    else
    {
        if( x1 > x2 && y1 == y2 )
            dir = 0;
        else if( x1 > x2 && y1 < y2 )
            dir = 1;
        else if( x1 == x2 && y1 < y2 )
            dir = 2;
        else if( x1 < x2 && y1 < y2 )
            dir = 3;
        else if( x1 < x2 && y1 == y2 )
            dir = 4;
        else if( x1 < x2 && y1 > y2 )
            dir = 5;
        else if( x1 == x2 && y1 > y2 )
            dir = 6;
        else if( x1 > x2 && y1 > y2 )
            dir = 7;
    }

    return dir;
}

int GetFarDir( int x1, int y1, int x2, int y2 )
{
    if( GameOpt.MapHexagonal )
    {
        float hx = (float) x1;
        float hy = (float) y1;
        float tx = (float) x2;
        float ty = (float) y2;
        float nx = 3 * ( tx - hx );
        float ny = ( ty - hy ) * SQRT3T2_FLOAT - ( float(x2 & 1) - float(x1 & 1) ) * SQRT3_FLOAT;
        float dir = 180.0f + RAD2DEG* atan2f( ny, nx );

        if( dir >= 60.0f  && dir < 120.0f )
            return 5;
        if( dir >= 120.0f && dir < 180.0f )
            return 4;
        if( dir >= 180.0f && dir < 240.0f )
            return 3;
        if( dir >= 240.0f && dir < 300.0f )
            return 2;
        if( dir >= 300.0f )
            return 1;
        return 0;
    }
    else
    {
        float dir = 180.0f + RAD2DEG* atan2( (float) ( x2 - x1 ), (float) ( y2 - y1 ) );

        if( dir >= 22.5f  && dir < 67.5f )
            return 7;
        if( dir >= 67.5f  && dir < 112.5f )
            return 0;
        if( dir >= 112.5f && dir < 157.5f )
            return 1;
        if( dir >= 157.5f && dir < 202.5f )
            return 2;
        if( dir >= 202.5f && dir < 247.5f )
            return 3;
        if( dir >= 247.5f && dir < 292.5f )
            return 4;
        if( dir >= 292.5f && dir < 337.5f )
            return 5;
        return 6;
    }
}

int GetFarDir( int x1, int y1, int x2, int y2, float offset )
{
    if( GameOpt.MapHexagonal )
    {
        float hx = (float) x1;
        float hy = (float) y1;
        float tx = (float) x2;
        float ty = (float) y2;
        float nx = 3 * ( tx - hx );
        float ny = ( ty - hy ) * SQRT3T2_FLOAT - ( float(x2 & 1) - float(x1 & 1) ) * SQRT3_FLOAT;
        float dir = 180.0f + RAD2DEG* atan2f( ny, nx ) + offset;
        if( dir < 0.0f )
            dir = 360.0f - fmod( -dir, 360.0f );
        else if( dir >= 360.0f )
            dir = fmod( dir, 360.0f );

        if( dir >= 60.0f  && dir < 120.0f )
            return 5;
        if( dir >= 120.0f && dir < 180.0f )
            return 4;
        if( dir >= 180.0f && dir < 240.0f )
            return 3;
        if( dir >= 240.0f && dir < 300.0f )
            return 2;
        if( dir >= 300.0f )
            return 1;
        return 0;
    }
    else
    {
        float dir = 180.0f + RAD2DEG* atan2( (float) ( x2 - x1 ), (float) ( y2 - y1 ) ) + offset;
        if( dir < 0.0f )
            dir = 360.0f - fmod( -dir, 360.0f );
        else if( dir >= 360.0f )
            dir = fmod( dir, 360.0f );

        if( dir >= 22.5f  && dir < 67.5f )
            return 7;
        if( dir >= 67.5f  && dir < 112.5f )
            return 0;
        if( dir >= 112.5f && dir < 157.5f )
            return 1;
        if( dir >= 157.5f && dir < 202.5f )
            return 2;
        if( dir >= 202.5f && dir < 247.5f )
            return 3;
        if( dir >= 247.5f && dir < 292.5f )
            return 4;
        if( dir >= 292.5f && dir < 337.5f )
            return 5;
        return 6;
    }
}

bool CheckDist( ushort x1, ushort y1, ushort x2, ushort y2, uint dist )
{
    return DistGame( x1, y1, x2, y2 ) <= dist;
}

int ReverseDir( int dir )
{
    int dirs_count = DIRS_COUNT;
    return ( dir + dirs_count / 2 ) % dirs_count;
}

void GetStepsXY( float& sx, float& sy, int x1, int y1, int x2, int y2 )
{
    float dx = (float) abs( x2 - x1 );
    float dy = (float) abs( y2 - y1 );

    sx = 1.0f;
    sy = 1.0f;

    dx < dy ? sx = dx / dy : sy = dy / dx;

    if( x2 < x1 )
        sx = -sx;
    if( y2 < y1 )
        sy = -sy;
}

void ChangeStepsXY( float& sx, float& sy, float deq )
{
    float                      rad = deq * PI_FLOAT / 180.0f;
    sx = sx * cos( rad ) - sy* sin( rad );
    sy = sx * sin( rad ) + sy* cos( rad );
}

bool MoveHexByDir( ushort& hx, ushort& hy, uchar dir, ushort maxhx, ushort maxhy )
{
    int hx_ = hx;
    int hy_ = hy;
    MoveHexByDirUnsafe( hx_, hy_, dir );

    if( hx_ >= 0 && hx_ < maxhx && hy_ >= 0 && hy_ < maxhy )
    {
        hx = hx_;
        hy = hy_;
        return true;
    }
    return false;
}

void MoveHexByDirUnsafe( int& hx, int& hy, uchar dir )
{
    if( GameOpt.MapHexagonal )
    {
        switch( dir )
        {
        case 0:
            hx--;
            if( !( hx & 1 ) )
                hy--;
            break;
        case 1:
            hx--;
            if( hx & 1 )
                hy++;
            break;
        case 2:
            hy++;
            break;
        case 3:
            hx++;
            if( hx & 1 )
                hy++;
            break;
        case 4:
            hx++;
            if( !( hx & 1 ) )
                hy--;
            break;
        case 5:
            hy--;
            break;
        default:
            return;
        }
    }
    else
    {
        switch( dir )
        {
        case 0:
            hx--;
            break;
        case 1:
            hx--;
            hy++;
            break;
        case 2:
            hy++;
            break;
        case 3:
            hx++;
            hy++;
            break;
        case 4:
            hx++;
            break;
        case 5:
            hx++;
            hy--;
            break;
        case 6:
            hy--;
            break;
        case 7:
            hx--;
            hy--;
            break;
        default:
            return;
        }
    }
}

bool IntersectCircleLine( int cx, int cy, int radius, int x1, int y1, int x2, int y2 )
{
    int x01 = x1 - cx;
    int y01 = y1 - cy;
    int x02 = x2 - cx;
    int y02 = y2 - cy;
    int dx = x02 - x01;
    int dy = y02 - y01;
    int a = dx * dx + dy * dy;
    int b = 2 * ( x01 * dx + y01 * dy );
    int c = x01 * x01 + y01 * y01 - radius * radius;
    if( -b < 0 )
        return c < 0;
    if( -b < 2 * a )
        return 4 * a * c - b * b < 0;
    return a + b + c < 0;
}

void ShowMessage( const char* message )
{
    #if defined ( FO_CLIENT ) || defined ( FO_MAPPER )
    SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "FOnline", message, nullptr );
    #else
    # ifdef FO_WINDOWS
    MessageBoxW( nullptr, CharToWideChar( message ).c_str(), L"FOnline", MB_OK );
    # else
    // Todo: Linux
    # endif
    #endif
}

int ConvertParamValue( const char* str, bool& fail )
{
    if( !str[ 0 ] )
    {
        WriteLog( "Empty parameter value.\n" );
        fail = true;
        return false;
    }

    if( str[ 0 ] == '@' && str[ 1 ] )
        return Str::GetHash( str + 1 );
    if( Str::IsNumber( str ) )
        return Str::AtoI( str );
    if( Str::CompareCase( str, "true" ) )
        return 1;
    if( Str::CompareCase( str, "false" ) )
        return 0;

    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    return Script::GetEnumValue( str, fail );
    #else
    return 0;
    #endif
}

/************************************************************************/
/* Hex offsets                                                          */
/************************************************************************/

// Hex offset
#define HEX_OFFSET_SIZE    ( ( MAX_HEX_OFFSET * MAX_HEX_OFFSET / 2 + MAX_HEX_OFFSET / 2 ) * DIRS_COUNT )
int           CurHexOffset = 0; // 0 - none, 1 - hexagonal, 2 - square
static short* SXEven = nullptr;
static short* SYEven = nullptr;
static short* SXOdd = nullptr;
static short* SYOdd = nullptr;

void InitializeHexOffsets()
{
    SAFEDELA( SXEven );
    SAFEDELA( SYEven );
    SAFEDELA( SXOdd );
    SAFEDELA( SYOdd );

    if( GameOpt.MapHexagonal )
    {
        CurHexOffset = 1;
        SXEven = new short[ HEX_OFFSET_SIZE ];
        SYEven = new short[ HEX_OFFSET_SIZE ];
        SXOdd = new short[ HEX_OFFSET_SIZE ];
        SYOdd = new short[ HEX_OFFSET_SIZE ];

        int pos = 0;
        int xe = 0, ye = 0, xo = 1, yo = 0;
        for( int i = 0; i < MAX_HEX_OFFSET; i++ )
        {
            MoveHexByDirUnsafe( xe, ye, 0 );
            MoveHexByDirUnsafe( xo, yo, 0 );

            for( int j = 0; j < 6; j++ )
            {
                int dir = ( j + 2 ) % 6;
                for( int k = 0; k < i + 1; k++ )
                {
                    SXEven[ pos ] = xe;
                    SYEven[ pos ] = ye;
                    SXOdd[ pos ] = xo - 1;
                    SYOdd[ pos ] = yo;
                    pos++;
                    MoveHexByDirUnsafe( xe, ye, dir );
                    MoveHexByDirUnsafe( xo, yo, dir );
                }
            }
        }
    }
    else
    {
        CurHexOffset = 2;
        SXEven = SXOdd = new short[ HEX_OFFSET_SIZE ];
        SYEven = SYOdd = new short[ HEX_OFFSET_SIZE ];

        int pos = 0;
        int hx = 0, hy = 0;
        for( int i = 0; i < MAX_HEX_OFFSET; i++ )
        {
            MoveHexByDirUnsafe( hx, hy, 0 );

            for( int j = 0; j < 5; j++ )
            {
                int dir = 0, steps = 0;
                switch( j )
                {
                case 0:
                    dir = 2;
                    steps = i + 1;
                    break;
                case 1:
                    dir = 4;
                    steps = ( i + 1 ) * 2;
                    break;
                case 2:
                    dir = 6;
                    steps = ( i + 1 ) * 2;
                    break;
                case 3:
                    dir = 0;
                    steps = ( i + 1 ) * 2;
                    break;
                case 4:
                    dir = 2;
                    steps = i + 1;
                    break;
                default:
                    break;
                }

                for( int k = 0; k < steps; k++ )
                {
                    SXEven[ pos ] = hx;
                    SYEven[ pos ] = hy;
                    pos++;
                    MoveHexByDirUnsafe( hx, hy, dir );
                }
            }
        }
    }
}

void GetHexOffsets( bool odd, short*& sx, short*& sy )
{
    if( CurHexOffset != ( GameOpt.MapHexagonal ? 1 : 2 ) )
        InitializeHexOffsets();
    sx = ( odd ? SXOdd : SXEven );
    sy = ( odd ? SYOdd : SYEven );
}

void GetHexInterval( int from_hx, int from_hy, int to_hx, int to_hy, int& x, int& y )
{
    if( GameOpt.MapHexagonal )
    {
        int dx = to_hx - from_hx;
        int dy = to_hy - from_hy;
        x = dy * ( GameOpt.MapHexWidth / 2 ) - dx * GameOpt.MapHexWidth;
        y = dy * GameOpt.MapHexLineHeight;
        if( from_hx & 1 )
        {
            if( dx > 0 )
                dx++;
        }
        else if( dx < 0 )
            dx--;
        dx /= 2;
        x += ( GameOpt.MapHexWidth / 2 ) * dx;
        y += GameOpt.MapHexLineHeight * dx;
    }
    else
    {
        int dx = to_hx - from_hx;
        int dy = to_hy - from_hy;
        x = ( dy - dx ) * GameOpt.MapHexWidth / 2;
        y = ( dy + dx ) * GameOpt.MapHexLineHeight;
    }
}

#ifdef FO_WINDOWS
std::wstring CharToWideChar( const char* str )
{
    int len = (int) strlen( str );
    if( !len )
        return L"";
    wchar_t* buf = (wchar_t*) alloca( len * sizeof( wchar_t ) * 2 );
    int      r = MultiByteToWideChar( CP_UTF8, 0, str, len, buf, len );
    return std::wstring( buf, r );
}

std::wstring CharToWideChar( const std::string& str )
{
    int len = (int) str.length();
    if( !len )
        return L"";
    wchar_t* buf = (wchar_t*) alloca( len * sizeof( wchar_t ) * 2 );
    int      r = MultiByteToWideChar( CP_UTF8, 0, str.c_str(), len, buf, len );
    return std::wstring( buf, r );
}

std::string WideCharToChar( const wchar_t* str )
{
    int len = (int) wcslen( str );
    if( !len )
        return "";
    char* buf = (char*) alloca( UTF8_BUF_SIZE( len ) );
    int   r = WideCharToMultiByte( CP_UTF8, 0, str, len, buf, len * 4, nullptr, nullptr );
    return std::string( buf, r );
}
#endif

/************************************************************************/
/*                                                                      */
/************************************************************************/
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )

SDL_Window*   MainWindow = nullptr;
SDL_GLContext GLContext = nullptr;
IntVec        MainWindowKeyboardEvents;
StrVec        MainWindowKeyboardEventsText;
IntVec        MainWindowMouseEvents;

uint GetColorDay( int* day_time, uchar* colors, int game_time, int* light )
{
    uchar result[ 3 ];
    int   color_r[ 4 ] = { colors[ 0 ], colors[ 1 ], colors[ 2 ], colors[ 3 ] };
    int   color_g[ 4 ] = { colors[ 4 ], colors[ 5 ], colors[ 6 ], colors[ 7 ] };
    int   color_b[ 4 ] = { colors[ 8 ], colors[ 9 ], colors[ 10 ], colors[ 11 ] };

    game_time %= 1440;
    int time, duration;
    if( game_time >= day_time[ 0 ] && game_time < day_time[ 1 ] )
    {
        time = 0;
        game_time -= day_time[ 0 ];
        duration = day_time[ 1 ] - day_time[ 0 ];
    }
    else if( game_time >= day_time[ 1 ] && game_time < day_time[ 2 ] )
    {
        time = 1;
        game_time -= day_time[ 1 ];
        duration = day_time[ 2 ] - day_time[ 1 ];
    }
    else if( game_time >= day_time[ 2 ] && game_time < day_time[ 3 ] )
    {
        time = 2;
        game_time -= day_time[ 2 ];
        duration = day_time[ 3 ] - day_time[ 2 ];
    }
    else
    {
        time = 3;
        if( game_time >= day_time[ 3 ] )
            game_time -= day_time[ 3 ];
        else
            game_time += 1440 - day_time[ 3 ];
        duration = ( 1440 - day_time[ 3 ] ) + day_time[ 0 ];
    }

    if( !duration )
        duration = 1;
    result[ 0 ] = color_r[ time ] + ( color_r[ time < 3 ? time + 1 : 0 ] - color_r[ time ] ) * game_time / duration;
    result[ 1 ] = color_g[ time ] + ( color_g[ time < 3 ? time + 1 : 0 ] - color_g[ time ] ) * game_time / duration;
    result[ 2 ] = color_b[ time ] + ( color_b[ time < 3 ? time + 1 : 0 ] - color_b[ time ] ) * game_time / duration;

    if( light )
    {
        int max_light = ( MAX( MAX( MAX( color_r[ 0 ], color_r[ 1 ] ), color_r[ 2 ] ), color_r[ 3 ] ) +
                          MAX( MAX( MAX( color_g[ 0 ], color_g[ 1 ] ), color_g[ 2 ] ), color_g[ 3 ] ) +
                          MAX( MAX( MAX( color_b[ 0 ], color_b[ 1 ] ), color_b[ 2 ] ), color_b[ 3 ] ) ) / 3;
        int min_light = ( MIN( MIN( MIN( color_r[ 0 ], color_r[ 1 ] ), color_r[ 2 ] ), color_r[ 3 ] ) +
                          MIN( MIN( MIN( color_g[ 0 ], color_g[ 1 ] ), color_g[ 2 ] ), color_g[ 3 ] ) +
                          MIN( MIN( MIN( color_b[ 0 ], color_b[ 1 ] ), color_b[ 2 ] ), color_b[ 3 ] ) ) / 3;
        int cur_light = ( result[ 0 ] + result[ 1 ] + result[ 2 ] ) / 3;
        *light = Procent( max_light - min_light, max_light - cur_light );
        *light = CLAMP( *light, 0, 100 );
    }

    return ( result[ 0 ] << 16 ) | ( result[ 1 ] << 8 ) | ( result[ 2 ] );
}

void GetClientOptions()
{
    // Defines
    # define GETOPTIONS_CMD_LINE_INT( opt, str_id )                        \
        do { const char* str = MainConfig->GetStr( "", str_id ); if( str ) \
                 opt = MainConfig->GetInt( "", str_id ); } while( 0 )
    # define GETOPTIONS_CMD_LINE_BOOL( opt, str_id )                       \
        do { const char* str = MainConfig->GetStr( "", str_id ); if( str ) \
                 opt = MainConfig->GetInt( "", str_id ) != 0; } while( 0 )
    # define GETOPTIONS_CMD_LINE_BOOL_ON( opt, str_id )                    \
        do { const char* str = MainConfig->GetStr( "", str_id ); if( str ) \
                 opt = true; } while( 0 )
    # define GETOPTIONS_CMD_LINE_STR( opt, str_id )                        \
        do { const char* str = MainConfig->GetStr( "", str_id ); if( str ) \
                 Str::Copy( opt, str ); } while( 0 )
    # define GETOPTIONS_CHECK( val_, min_, max_, def_ )                 \
        do { int val__ = (int) val_; if( val__ < min_ || val__ > max_ ) \
                 val_ = def_; } while( 0 )

    char buf[ MAX_FOTEXT ];
    # define READ_CFG_STR_DEF( cfg, key, def_val )    Str::Copy( buf, MainConfig->GetStr( "", key, def_val ) )

    // Data files
    FileManager::InitDataFiles( CLIENT_DATA );
    # if defined ( FO_IOS )
    FileManager::InitDataFiles( "../../Documents/" );
    # elif defined ( FO_ANDROID )
    FileManager::InitDataFiles( SDL_AndroidGetInternalStoragePath() );
    FileManager::InitDataFiles( SDL_AndroidGetExternalStoragePath() );
    # endif

    // Cached configuration
    MainConfig->AppendFile( "Cache/" CONFIG_NAME );

    // Language
    READ_CFG_STR_DEF( *MainConfig, "Language", "russ" );
    GETOPTIONS_CMD_LINE_STR( buf, "Language" );

    // Int / Bool
    GameOpt.OpenGLDebug = MainConfig->GetInt( "", "OpenGLDebug", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.OpenGLDebug, "OpenGLDebug" );
    GameOpt.AssimpLogging = MainConfig->GetInt( "", "AssimpLogging", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AssimpLogging, "AssimpLogging" );
    GameOpt.FullScreen = MainConfig->GetInt( "", "FullScreen", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.FullScreen, "FullScreen" );
    GameOpt.VSync = MainConfig->GetInt( "", "VSync", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.VSync, "VSync" );
    GameOpt.Light = MainConfig->GetInt( "", "Light", 20 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.Light, "Light" );
    GETOPTIONS_CHECK( GameOpt.Light, 0, 100, 20 );
    GameOpt.ScrollDelay = MainConfig->GetInt( "", "ScrollDelay", 10 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScrollDelay, "ScrollDelay" );
    GETOPTIONS_CHECK( GameOpt.ScrollDelay, 0, 100, 10 );
    GameOpt.ScrollStep = MainConfig->GetInt( "", "ScrollStep", 12 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScrollStep, "ScrollStep" );
    GETOPTIONS_CHECK( GameOpt.ScrollStep, 4, 32, 12 );
    GameOpt.TextDelay = MainConfig->GetInt( "", "TextDelay", 3000 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.TextDelay, "TextDelay" );
    GETOPTIONS_CHECK( GameOpt.TextDelay, 1000, 30000, 3000 );
    GameOpt.DamageHitDelay = MainConfig->GetInt( "", "DamageHitDelay", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.DamageHitDelay, "OptDamageHitDelay" );
    GETOPTIONS_CHECK( GameOpt.DamageHitDelay, 0, 30000, 0 );
    GameOpt.ScreenWidth = MainConfig->GetInt( "", "ScreenWidth", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScreenWidth, "ScreenWidth" );
    GETOPTIONS_CHECK( GameOpt.ScreenWidth, 100, 30000, 800 );
    GameOpt.ScreenHeight = MainConfig->GetInt( "", "ScreenHeight", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScreenHeight, "ScreenHeight" );
    GETOPTIONS_CHECK( GameOpt.ScreenHeight, 100, 30000, 600 );
    GameOpt.AlwaysOnTop = MainConfig->GetInt( "", "AlwaysOnTop", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AlwaysOnTop, "AlwaysOnTop" );
    GameOpt.FixedFPS = MainConfig->GetInt( "", "FixedFPS", 100 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.FixedFPS, "FixedFPS" );
    GETOPTIONS_CHECK( GameOpt.FixedFPS, -10000, 10000, 100 );
    GameOpt.MsgboxInvert = MainConfig->GetInt( "", "InvertMessBox", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.MsgboxInvert, "InvertMessBox" );
    GameOpt.MessNotify = MainConfig->GetInt( "", "WinNotify", true ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.MessNotify, "WinNotify" );
    GameOpt.SoundNotify = MainConfig->GetInt( "", "SoundNotify", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.SoundNotify, "SoundNotify" );
    GameOpt.Port = MainConfig->GetInt( "", "RemotePort", 4010 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.Port, "RemotePort" );
    GETOPTIONS_CHECK( GameOpt.Port, 0, 0xFFFF, 4000 );
    GameOpt.UpdateServerPort = MainConfig->GetInt( "", "UpdateServerPort", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.UpdateServerPort, "UpdateServerPort" );
    GETOPTIONS_CHECK( GameOpt.UpdateServerPort, 0, 0xFFFF, 0 );
    GameOpt.ProxyType = MainConfig->GetInt( "", "ProxyType", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ProxyType, "ProxyType" );
    GETOPTIONS_CHECK( GameOpt.ProxyType, 0, 3, 0 );
    GameOpt.ProxyPort = MainConfig->GetInt( "", "ProxyPort", 8080 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ProxyPort, "ProxyPort" );
    GETOPTIONS_CHECK( GameOpt.ProxyPort, 0, 0xFFFF, 1080 );
    GameOpt.AlwaysRun = MainConfig->GetInt( "", "AlwaysRun", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AlwaysRun, "AlwaysRun" );

    uint dct = 500;
    # ifdef FO_WINDOWS
    dct = (uint) GetDoubleClickTime();
    # endif
    GameOpt.DoubleClickTime = MainConfig->GetInt( "", "DoubleClickTime", dct );
    GETOPTIONS_CMD_LINE_INT( GameOpt.DoubleClickTime, "DoubleClickTime" );
    GETOPTIONS_CHECK( GameOpt.DoubleClickTime, 0, 1000, dct );

    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.HelpInfo, "HelpInfo" );
    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.DebugInfo, "DebugInfo" );
    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.DebugNet, "DebugNet" );

    // Str
    READ_CFG_STR_DEF( *MainConfig, "RemoteHost", "localhost" );
    GETOPTIONS_CMD_LINE_STR( buf, "RemoteHost" );
    GameOpt.Host = buf;
    READ_CFG_STR_DEF( *MainConfig, "UpdateServerHost", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "UpdateServerHost" );
    GameOpt.UpdateServerHost = buf;
    READ_CFG_STR_DEF( *MainConfig, "ProxyHost", "localhost" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyHost" );
    GameOpt.ProxyHost = buf;
    READ_CFG_STR_DEF( *MainConfig, "ProxyUser", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyUser" );
    GameOpt.ProxyUser = buf;
    READ_CFG_STR_DEF( *MainConfig, "ProxyPass", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyPass" );
    GameOpt.ProxyPass = buf;
    READ_CFG_STR_DEF( *MainConfig, "UserName", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "UserName" );
    GameOpt.Name = buf;
    READ_CFG_STR_DEF( *MainConfig, "KeyboardRemap", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "KeyboardRemap" );
    GameOpt.KeyboardRemap = buf;

    // Logging
    bool logging = MainConfig->GetInt( "", "Logging", 1 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "Logging" );
    if( !logging )
    {
        WriteLog( "File logging off.\n" );
        LogToFile( nullptr );
    }

    # ifdef FONLINE_MAPPER
    Script::SetRunTimeout( 0, 0 );
    # endif
}

ClientScriptFunctions ClientFunctions;
MapperScriptFunctions MapperFunctions;
#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/
#ifdef FONLINE_SERVER

bool FOQuit = false;
int  ServerGameSleep = 10;
int  MemoryDebugLevel = 10;
uint VarsGarbageTime = 3600000;
bool AllowServerNativeCalls = false;
bool AllowClientNativeCalls = false;

void GetServerOptions()
{
    ServerGameSleep = MainConfig->GetInt( "", "GameSleep", 10 );
    AllowServerNativeCalls = ( MainConfig->GetInt( "", "AllowServerNativeCalls", 1 ) != 0 );
    AllowClientNativeCalls = ( MainConfig->GetInt( "", "AllowClientNativeCalls", 0 ) != 0 );
}

ServerScriptFunctions ServerFunctions;

#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/
#ifdef FO_WINDOWS

string GetLastSocketError()
{
    string result;
    int    error = WSAGetLastError();
    # define CASE_SOCK_ERROR( code, message ) \
    case code:                                \
        result += fmt::format( "{}, {}, {}", # code, code, message ); break

    switch( error )
    {
    default:
        result += fmt::format( "{}, unknown error code.", error );
        break;
        CASE_SOCK_ERROR( WSAEINTR, "A blocking operation was interrupted by a call to WSACancelBlockingCall." );
        CASE_SOCK_ERROR( WSAEBADF, "The file handle supplied is not valid." );
        CASE_SOCK_ERROR( WSAEACCES, "An attempt was made to access a socket in a way forbidden by its access permissions." );
        CASE_SOCK_ERROR( WSAEFAULT, "The system detected an invalid pointer address in attempting to use a pointer argument in a call." );
        CASE_SOCK_ERROR( WSAEINVAL, "An invalid argument was supplied." );
        CASE_SOCK_ERROR( WSAEMFILE, "Too many open sockets." );
        CASE_SOCK_ERROR( WSAEWOULDBLOCK, "A non-blocking socket operation could not be completed immediately." );
        CASE_SOCK_ERROR( WSAEINPROGRESS, "A blocking operation is currently executing." );
        CASE_SOCK_ERROR( WSAEALREADY, "An operation was attempted on a non-blocking socket that already had an operation in progress." );
        CASE_SOCK_ERROR( WSAENOTSOCK, "An operation was attempted on something that is not a socket." );
        CASE_SOCK_ERROR( WSAEDESTADDRREQ, "A required address was omitted from an operation on a socket." );
        CASE_SOCK_ERROR( WSAEMSGSIZE, "A message sent on a datagram socket was larger than the internal message buffer or some other network limit, or the buffer used to receive a datagram into was smaller than the datagram itself." );
        CASE_SOCK_ERROR( WSAEPROTOTYPE, "A protocol was specified in the socket function call that does not support the semantics of the socket type requested." );
        CASE_SOCK_ERROR( WSAENOPROTOOPT, "An unknown, invalid, or unsupported option or level was specified in a getsockopt or setsockopt call." );
        CASE_SOCK_ERROR( WSAEPROTONOSUPPORT, "The requested protocol has not been configured into the system, or no implementation for it exists." );
        CASE_SOCK_ERROR( WSAESOCKTNOSUPPORT, "The support for the specified socket type does not exist in this address family." );
        CASE_SOCK_ERROR( WSAEOPNOTSUPP, "The attempted operation is not supported for the type of object referenced." );
        CASE_SOCK_ERROR( WSAEPFNOSUPPORT, "The protocol family has not been configured into the system or no implementation for it exists." );
        CASE_SOCK_ERROR( WSAEAFNOSUPPORT, "An address incompatible with the requested protocol was used." );
        CASE_SOCK_ERROR( WSAEADDRINUSE, "Only one usage of each socket address (protocol/network address/port) is normally permitted." );
        CASE_SOCK_ERROR( WSAEADDRNOTAVAIL, "The requested address is not valid in its context." );
        CASE_SOCK_ERROR( WSAENETDOWN, "A socket operation encountered a dead network." );
        CASE_SOCK_ERROR( WSAENETUNREACH, "A socket operation was attempted to an unreachable network." );
        CASE_SOCK_ERROR( WSAENETRESET, "The connection has been broken due to keep-alive activity detecting a failure while the operation was in progress." );
        CASE_SOCK_ERROR( WSAECONNABORTED, "An established connection was aborted by the software in your host machine." );
        CASE_SOCK_ERROR( WSAECONNRESET, "An existing connection was forcibly closed by the remote host." );
        CASE_SOCK_ERROR( WSAENOBUFS, "An operation on a socket could not be performed because the system lacked sufficient buffer space or because a queue was full." );
        CASE_SOCK_ERROR( WSAEISCONN, "A connect request was made on an already connected socket." );
        CASE_SOCK_ERROR( WSAENOTCONN, "A request to send or receive data was disallowed because the socket is not connected and (when sending on a datagram socket using a sendto call) no address was supplied." );
        CASE_SOCK_ERROR( WSAESHUTDOWN, "A request to send or receive data was disallowed because the socket had already been shut down in that direction with a previous shutdown call." );
        CASE_SOCK_ERROR( WSAETOOMANYREFS, "Too many references to some kernel object." );
        CASE_SOCK_ERROR( WSAETIMEDOUT, "A connection attempt failed because the connected party did not properly respond after a period of time, or established connection failed because connected host has failed to respond." );
        CASE_SOCK_ERROR( WSAECONNREFUSED, "No connection could be made because the target machine actively refused it." );
        CASE_SOCK_ERROR( WSAELOOP, "Cannot translate name." );
        CASE_SOCK_ERROR( WSAENAMETOOLONG, "Name component or name was too long." );
        CASE_SOCK_ERROR( WSAEHOSTDOWN, "A socket operation failed because the destination host was down." );
        CASE_SOCK_ERROR( WSAEHOSTUNREACH, "A socket operation was attempted to an unreachable host." );
        CASE_SOCK_ERROR( WSAENOTEMPTY, "Cannot remove a directory that is not empty." );
        CASE_SOCK_ERROR( WSAEPROCLIM, "A Windows Sockets implementation may have a limit on the number of applications that may use it simultaneously." );
        CASE_SOCK_ERROR( WSAEUSERS, "Ran out of quota." );
        CASE_SOCK_ERROR( WSAEDQUOT, "Ran out of disk quota." );
        CASE_SOCK_ERROR( WSAESTALE, "File handle reference is no longer available." );
        CASE_SOCK_ERROR( WSAEREMOTE, "Item is not available locally." );
        CASE_SOCK_ERROR( WSASYSNOTREADY, "WSAStartup cannot function at this time because the underlying system it uses to provide network services is currently unavailable." );
        CASE_SOCK_ERROR( WSAVERNOTSUPPORTED, "The Windows Sockets version requested is not supported." );
        CASE_SOCK_ERROR( WSANOTINITIALISED, "Either the application has not called WSAStartup, or WSAStartup failed." );
        CASE_SOCK_ERROR( WSAEDISCON, "Returned by WSARecv or WSARecvFrom to indicate the remote party has initiated a graceful shutdown sequence." );
        CASE_SOCK_ERROR( WSAENOMORE, "No more results can be returned by WSALookupServiceNext." );
        CASE_SOCK_ERROR( WSAECANCELLED, "A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled." );
        CASE_SOCK_ERROR( WSAEINVALIDPROCTABLE, "The procedure call table is invalid." );
        CASE_SOCK_ERROR( WSAEINVALIDPROVIDER, "The requested service provider is invalid." );
        CASE_SOCK_ERROR( WSAEPROVIDERFAILEDINIT, "The requested service provider could not be loaded or initialized." );
        CASE_SOCK_ERROR( WSASYSCALLFAILURE, "A system call that should never fail has failed." );
        CASE_SOCK_ERROR( WSASERVICE_NOT_FOUND, "No such service is known. The service cannot be found in the specified name space." );
        CASE_SOCK_ERROR( WSATYPE_NOT_FOUND, "The specified class was not found." );
        CASE_SOCK_ERROR( WSA_E_NO_MORE, "No more results can be returned by WSALookupServiceNext." );
        CASE_SOCK_ERROR( WSA_E_CANCELLED, "A call to WSALookupServiceEnd was made while this call was still processing. The call has been canceled." );
        CASE_SOCK_ERROR( WSAEREFUSED, "A database query failed because it was actively refused." );
        CASE_SOCK_ERROR( WSAHOST_NOT_FOUND, "No such host is known." );
        CASE_SOCK_ERROR( WSATRY_AGAIN, "This is usually a temporary error during hostname resolution and means that the local server did not receive a response from an authoritative server." );
        CASE_SOCK_ERROR( WSANO_RECOVERY, "A non-recoverable error occurred during a database lookup." );
        CASE_SOCK_ERROR( WSANO_DATA, "The requested name is valid, but no data of the requested type was found." );
        CASE_SOCK_ERROR( WSA_QOS_RECEIVERS, "At least one reserve has arrived." );
        CASE_SOCK_ERROR( WSA_QOS_SENDERS, "At least one path has arrived." );
        CASE_SOCK_ERROR( WSA_QOS_NO_SENDERS, "There are no senders." );
        CASE_SOCK_ERROR( WSA_QOS_NO_RECEIVERS, "There are no receivers." );
        CASE_SOCK_ERROR( WSA_QOS_REQUEST_CONFIRMED, "Reserve has been confirmed." );
        CASE_SOCK_ERROR( WSA_QOS_ADMISSION_FAILURE, "Error due to lack of resources." );
        CASE_SOCK_ERROR( WSA_QOS_POLICY_FAILURE, "Rejected for administrative reasons - bad credentials." );
        CASE_SOCK_ERROR( WSA_QOS_BAD_STYLE, "Unknown or conflicting style." );
        CASE_SOCK_ERROR( WSA_QOS_BAD_OBJECT, "Problem with some part of the filterspec or providerspecific buffer in general." );
        CASE_SOCK_ERROR( WSA_QOS_TRAFFIC_CTRL_ERROR, "Problem with some part of the flowspec." );
        CASE_SOCK_ERROR( WSA_QOS_GENERIC_ERROR, "General QOS error." );
        CASE_SOCK_ERROR( WSA_QOS_ESERVICETYPE, "An invalid or unrecognized service type was found in the flowspec." );
        CASE_SOCK_ERROR( WSA_QOS_EFLOWSPEC, "An invalid or inconsistent flowspec was found in the QOS structure." );
        CASE_SOCK_ERROR( WSA_QOS_EPROVSPECBUF, "Invalid QOS provider-specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_EFILTERSTYLE, "An invalid QOS filter style was used." );
        CASE_SOCK_ERROR( WSA_QOS_EFILTERTYPE, "An invalid QOS filter type was used." );
        CASE_SOCK_ERROR( WSA_QOS_EFILTERCOUNT, "An incorrect number of QOS FILTERSPECs were specified in the FLOWDESCRIPTOR." );
        CASE_SOCK_ERROR( WSA_QOS_EOBJLENGTH, "An object with an invalid ObjectLength field was specified in the QOS provider-specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_EFLOWCOUNT, "An incorrect number of flow descriptors was specified in the QOS structure." );
        CASE_SOCK_ERROR( WSA_QOS_EPOLICYOBJ, "An invalid policy object was found in the QOS provider-specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_EFLOWDESC, "An invalid QOS flow descriptor was found in the flow descriptor list." );
        CASE_SOCK_ERROR( WSA_QOS_EPSFLOWSPEC, "An invalid or inconsistent flowspec was found in the QOS provider specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_EPSFILTERSPEC, "An invalid FILTERSPEC was found in the QOS provider-specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_ESDMODEOBJ, "An invalid shape discard mode object was found in the QOS provider specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_ESHAPERATEOBJ, "An invalid shaping rate object was found in the QOS provider-specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_RESERVED_PETYPE, "A reserved policy element was found in the QOS provider-specific buffer." );
    }
    return result;
}

#else

string GetLastSocketError()
{
    return fmt::format( "{} ({})", strerror( errno ), errno );
}

#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/

static void AddPropertyCallback( void ( * function )( void*, void*, void*, void* ) )
{
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
    PropertyRegistrator::GlobalSetCallbacks.push_back( (NativeCallback) function );
    #endif
}

GameOptions GameOpt;
GameOptions::GameOptions()
{
    WorkDir = "";
    CommandLine = "";

    YearStart = 2246;
    Year = 2246;
    Month = 1;
    Day = 2;
    Hour = 14;
    Minute = 5;
    Second = 0;
    FullSecondStart = 0;
    FullSecond = 0;
    TimeMultiplier = 0;
    GameTimeTick = 0;

    Singleplayer = false;
    DisableTcpNagle = false;
    DisableZlibCompression = false;
    FloodSize = 2048;
    BruteForceTick = 100;
    NoAnswerShuffle = false;
    DialogDemandRecheck = false;
    SneakDivider = 6;
    LookMinimum = 6;
    CritterIdleTick = 10000;
    DeadHitPoints = -6;

    Breaktime = 1200;
    TimeoutTransfer = 3;
    TimeoutBattle = 10;
    RtAlwaysRun = false;
    RunOnCombat = false;
    RunOnTransfer = false;
    GlobalMapWidth = 28;
    GlobalMapHeight = 30;
    GlobalMapZoneLength = 50;
    BagRefreshTime = 60;
    WhisperDist = 2;
    ShoutDist = 200;
    LookChecks = 0;
    LookDir[ 0 ] = 0;
    LookDir[ 1 ] = 20;
    LookDir[ 2 ] = 40;
    LookDir[ 3 ] = 60;
    LookDir[ 4 ] = 60;
    LookSneakDir[ 0 ] = 90;
    LookSneakDir[ 1 ] = 60;
    LookSneakDir[ 2 ] = 30;
    LookSneakDir[ 3 ] = 0;
    LookSneakDir[ 4 ] = 0;
    RegistrationTimeout = 5;
    AccountPlayTime = 0;
    ScriptRunSuspendTimeout = 30000;
    ScriptRunMessageTimeout = 10000;
    TalkDistance = 3;
    NpcMaxTalkers = 1;
    MinNameLength = 4;
    MaxNameLength = 12;
    DlgTalkMinTime = 0;
    DlgBarterMinTime = 0;
    MinimumOfflineTime = 180000;
    GameServer = false;
    UpdateServer = false;

    MapHexagonal = true;
    MapHexWidth = 32;
    MapHexHeight = 16;
    MapHexLineHeight = 12;
    MapTileOffsX = -8;
    MapTileOffsY = 32;
    MapTileStep = 2;
    MapRoofOffsX = -8;
    MapRoofOffsY = -66;
    MapRoofSkipSize = 2;
    MapCameraAngle = 25.7f;
    MapSmoothPath = true;
    MapDataPrefix = "art/geometry/fallout_";

    // Client and Mapper
    Quit = false;
    WaitPing = false;
    OpenGLRendering = true;
    OpenGLDebug = false;
    AssimpLogging = false;
    MouseX = 0;
    MouseY = 0;
    LastMouseX = 0;
    LastMouseY = 0;
    ScrOx = 0;
    ScrOy = 0;
    ShowTile = true;
    ShowRoof = true;
    ShowItem = true;
    ShowScen = true;
    ShowWall = true;
    ShowCrit = true;
    ShowFast = true;
    ShowPlayerNames = false;
    ShowNpcNames = false;
    ShowCritId = false;
    ScrollKeybLeft = false;
    ScrollKeybRight = false;
    ScrollKeybUp = false;
    ScrollKeybDown = false;
    ScrollMouseLeft = false;
    ScrollMouseRight = false;
    ScrollMouseUp = false;
    ScrollMouseDown = false;
    ShowGroups = true;
    HelpInfo = false;
    DebugInfo = false;
    DebugNet = false;
    Enable3dRendering = false;
    FullScreen = false;
    VSync = false;
    Light = 0;
    Host = "localhost";
    Port = 4000;
    UpdateServerHost = "";
    UpdateServerPort = 0;
    ProxyType = 0;
    ProxyHost = "";
    ProxyPort = 0;
    ProxyUser = "";
    ProxyPass = "";
    Name = "";
    ScrollDelay = 10;
    ScrollStep = 1;
    ScrollCheck = true;
    FixedFPS = 100;
    FPS = 0;
    PingPeriod = 2000;
    Ping = 0;
    MsgboxInvert = false;
    MessNotify = true;
    SoundNotify = true;
    AlwaysOnTop = false;
    TextDelay = 3000;
    DamageHitDelay = 0;
    ScreenWidth = 800;
    ScreenHeight = 600;
    MultiSampling = -1;
    MouseScroll = true;
    DoubleClickTime = 0;
    RoofAlpha = 200;
    HideCursor = false;
    ShowMoveCursor = false;
    DisableMouseEvents = false;
    DisableKeyboardEvents = false;
    HidePassword = true;
    PlayerOffAppendix = "_off";
    Animation3dSmoothTime = 150;
    Animation3dFPS = 30;
    RunModMul = 1;
    RunModDiv = 3;
    RunModAdd = 0;
    MapZooming = false;
    SpritesZoom = 1.0f;
    SpritesZoomMax = MAX_ZOOM;
    SpritesZoomMin = MIN_ZOOM;
    memzero( EffectValues, sizeof( EffectValues ) );
    AlwaysRun = false;
    AlwaysRunMoveDist = 1;
    AlwaysRunUseDist = 5;
    KeyboardRemap = "";
    CritterFidgetTime = 50000;
    Anim2CombatBegin = 0;
    Anim2CombatIdle = 0;
    Anim2CombatEnd = 0;
    RainTick = 60;
    RainSpeedX = 0;
    RainSpeedY = 15;
    ConsoleHistorySize = 20;
    SoundVolume = 100;
    MusicVolume = 100;
    RegName = "";
    RegPassword = "";
    ChosenLightColor = 0;
    ChosenLightDistance = 4;
    ChosenLightIntensity = 2500;
    ChosenLightFlags = 0;

    // Mapper
    ServerDir = "";
    ShowCorners = false;
    ShowSpriteCuts = false;
    ShowDrawOrder = false;
    SplitTilesCollection = true;

    // Engine data
    ClientMap = nullptr;
    ClientMapLight = nullptr;
    ClientMapWidth = 0;
    ClientMapHeight = 0;

    GetDrawingSprites = nullptr;
    GetSpriteInfo = nullptr;
    GetSpriteColor = nullptr;
    IsSpriteHit = nullptr;

    GetNameByHash = &Str::GetName;
    GetHashByName = &Str::GetHash;

    ScriptBind = nullptr;
    ScriptPrepare = nullptr;
    ScriptSetArgInt8 = nullptr;
    ScriptSetArgInt16 = nullptr;
    ScriptSetArgInt = nullptr;
    ScriptSetArgInt64 = nullptr;
    ScriptSetArgUInt8 = nullptr;
    ScriptSetArgUInt16 = nullptr;
    ScriptSetArgUInt = nullptr;
    ScriptSetArgUInt64 = nullptr;
    ScriptSetArgBool = nullptr;
    ScriptSetArgFloat = nullptr;
    ScriptSetArgDouble = nullptr;
    ScriptSetArgObject = nullptr;
    ScriptSetArgAddress = nullptr;
    ScriptRunPrepared = nullptr;
    ScriptGetReturnedInt8 = nullptr;
    ScriptGetReturnedInt16 = nullptr;
    ScriptGetReturnedInt = nullptr;
    ScriptGetReturnedInt64 = nullptr;
    ScriptGetReturnedUInt8 = nullptr;
    ScriptGetReturnedUInt16 = nullptr;
    ScriptGetReturnedUInt = nullptr;
    ScriptGetReturnedUInt64 = nullptr;
    ScriptGetReturnedBool = nullptr;
    ScriptGetReturnedFloat = nullptr;
    ScriptGetReturnedDouble = nullptr;
    ScriptGetReturnedObject = nullptr;
    ScriptGetReturnedAddress = nullptr;

    Random = &::Random;
    GetTick = &Timer::FastTick;
    SetLogCallback = &LogToFunc;
    AddPropertyCallback = &::AddPropertyCallback;
}

/************************************************************************/
/* File logger                                                          */
/************************************************************************/

FileLogger::FileLogger( const char* fname )
{
    logFile = fopen( fname, "wb" );
    startTick = Timer::FastTick();
}

FileLogger::~FileLogger()
{
    if( logFile )
    {
        fclose( logFile );
        logFile = nullptr;
    }
}

void FileLogger::Write( const char* fmt, ... )
{
    if( logFile )
    {
        fprintf( logFile, "%10u) ", ( Timer::FastTick() - startTick ) / 1000 );
        va_list list;
        va_start( list, fmt );
        vfprintf( logFile, fmt, list );
        va_end( list );
    }
}

/************************************************************************/
/* Single player                                                        */
/************************************************************************/

#ifdef FO_WINDOWS

# define INTERPROCESS_DATA_SIZE    ( OFFSETOF( InterprocessData, mapFileMutex ) )

HANDLE InterprocessData::Init()
{
    SECURITY_ATTRIBUTES sa = { sizeof( sa ), nullptr, TRUE };
    mapFile = CreateFileMapping( INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, INTERPROCESS_DATA_SIZE + sizeof( mapFileMutex ), nullptr );
    if( !mapFile )
        return nullptr;
    mapFilePtr = nullptr;

    mapFileMutex = CreateMutex( &sa, FALSE, nullptr );
    if( !mapFileMutex )
        return nullptr;

    if( !Lock() )
        return nullptr;
    memzero( this, INTERPROCESS_DATA_SIZE );
    ( (InterprocessData*) mapFilePtr )->mapFileMutex = mapFileMutex;
    Unlock();
    return mapFile;
}

void InterprocessData::Finish()
{
    if( mapFile )
        CloseHandle( mapFile );
    if( mapFileMutex )
        CloseHandle( mapFileMutex );
    mapFile = nullptr;
    mapFilePtr = nullptr;
}

bool InterprocessData::Attach( HANDLE map_file )
{
    if( !map_file )
        return false;
    mapFile = map_file;
    mapFilePtr = nullptr;

    // Read mutex handle
    void* ptr = MapViewOfFile( mapFile, FILE_MAP_WRITE, 0, 0, 0 );
    if( !ptr )
        return false;
    mapFileMutex = ( (InterprocessData*) ptr )->mapFileMutex;
    UnmapViewOfFile( ptr );
    if( !mapFileMutex )
        return false;

    return Refresh();
}

bool InterprocessData::Lock()
{
    if( !mapFile )
        return false;

    uint result = WaitForSingleObject( mapFileMutex, INFINITE );
    if( result != WAIT_OBJECT_0 )
        return false;

    mapFilePtr = MapViewOfFile( mapFile, FILE_MAP_WRITE, 0, 0, 0 );
    if( !mapFilePtr )
        return false;
    memcpy( this, mapFilePtr, INTERPROCESS_DATA_SIZE );
    return true;
}

void InterprocessData::Unlock()
{
    if( !mapFile || !mapFilePtr )
        return;

    memcpy( mapFilePtr, this, INTERPROCESS_DATA_SIZE );
    UnmapViewOfFile( mapFilePtr );
    mapFilePtr = nullptr;

    ReleaseMutex( mapFileMutex );
}

bool InterprocessData::Refresh()
{
    if( !Lock() )
        return false;
    Unlock();
    return true;
}

void* SingleplayerClientProcess = nullptr;
#endif

bool             Singleplayer = false;
InterprocessData SingleplayerData;

/************************************************************************/
/*                                                                      */
/************************************************************************/

// Preprocessor output formatting
int InsertTabs( string& str, int cur_pos, int level )
{
    if( cur_pos < 0 || cur_pos >= (int) str.length() )
        return (int) str.size();

    int i = cur_pos;
    for( ; i < (int) str.length() - 1; i++ )
    {
        if( str[ i ] == '\n' )
        {
            int k = 0;
            if( str[ i + 1 ] == '}' )
                k++;
            for( ; k < level; ++k )
            {
                i++;
                str.insert( str.begin() + i, '\t' );
            }
        }
        else if( str[ i ] == '{' )
        {
            i = InsertTabs( str, i + 1, level + 1 );
        }
        else if( str[ i ] == '}' )
        {
            return i;
        }
    }

    return i;
}
