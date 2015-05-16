#include "Common.h"
#include "Exception.h"
#include "Crypt.h"
#include "Script.h"
#include "Text.h"
#include <math.h>
#include "FileManager.h"
#include "IniParser.h"
#include <stdarg.h>

#pragma MESSAGE("Add TARGET_HEX.")

/************************************************************************/
/*                                                                      */
/************************************************************************/

#ifndef FO_WINDOWS
// Mutex static stuff
bool                Mutex::attrInitialized = false;
pthread_mutexattr_t Mutex::mutexAttr;
#endif

// Command line
char CommandLine[ MAX_FOTEXT ] = { 0 };
void SetCommandLine( uint argc, char** argv )
{
    for( uint i = 0; i < argc; i++ )
    {
        if( i == 0 && argv[ 0 ][ 0 ] != '-' )
            continue;
        Str::Append( CommandLine, argv[ i ] );
        Str::Append( CommandLine, " " );
    }

    GameOpt.CommandLine = ScriptString::Create( CommandLine );
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

void RestoreMainDirectory()
{
    #if defined ( FO_WINDOWS )
    // Get executable file path
    char path[ MAX_FOPATH ] = { 0 };
    GetModuleFileName( GetModuleHandle( NULL ), path, MAX_FOPATH );

    // Cut off executable name
    int last = 0;
    for( int i = 0; path[ i ]; i++ )
        if( path[ i ] == DIR_SLASH_C )
            last = i;
    path[ last + 1 ] = 0;

    // Set executable directory
    SetCurrentDirectory( path );
    #elif defined ( FO_LINUX )
    // Read symlink to executable
    char buf[ MAX_FOPATH ];
    if( readlink( "/proc/self/exe", buf, MAX_FOPATH ) != -1 ||    // Linux
        readlink( "/proc/curproc/file", buf, MAX_FOPATH ) != -1 ) // FreeBSD
    {
        string            sbuf = buf;
        string::size_type pos = sbuf.find_last_of( DIR_SLASH_C );
        if( pos != string::npos )
        {
            buf[ pos ] = 0;
            chdir( buf );
        }
    }
    #elif defined ( FO_OSX_MAC )
    chdir( "./FOnline.app/Contents/Resources/Client" );
    #elif defined ( FO_OSX_IOS )
    chdir( "./Client" );
    #endif
}

void ShowMessage( const char* message )
{
    #if defined ( FO_CLIENT ) || defined ( FO_MAPPER )
    SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "FOnline", message, NULL );
    #else
    # ifdef FO_WINDOWS
    wchar_t message_wc[ MAX_FOTEXT ];
    MultiByteToWideChar( CP_UTF8, 0, message, -1, message_wc, MAX_FOPATH );
    MessageBoxW( NULL, message_wc, L"FOnline", MB_OK );
    # else
    // Todo: Linux
    # endif
    #endif
}

uint GetDoubleClickTicks()
{
    #ifdef FO_WINDOWS
    return (uint) GetDoubleClickTime();
    #else
    // Todo: Linux
    return 500;
    #endif
}

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )
# include "ConstantsManager.h"
int64 ConvertParamValue( const char* str )
{
    if( !str[ 0 ] )
        return 0;

    if( str[ 0 ] == '$' )
    {
        # ifndef FONLINE_SCRIPT_COMPILER
        return ConstantsManager::GetDefineValue( str + 1 );
        # else
        return 0;
        # endif
    }
    if( !Str::IsNumber( str ) )
        return Str::GetHash( str );
    return Str::AtoI64( str );
}
#else
int64 ConvertParamValue( const char* str )
{
    if( !Str::IsNumber( str ) )
        return Str::GetHash( str );
    return Str::AtoI64( str );
}
#endif

/************************************************************************/
/* Hex offsets                                                          */
/************************************************************************/

// Hex offset
#define HEX_OFFSET_SIZE    ( ( MAX_HEX_OFFSET * MAX_HEX_OFFSET / 2 + MAX_HEX_OFFSET / 2 ) * DIRS_COUNT )
int           CurHexOffset = 0; // 0 - none, 1 - hexagonal, 2 - square
static short* SXEven = NULL;
static short* SYEven = NULL;
static short* SXOdd = NULL;
static short* SYOdd = NULL;

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

/************************************************************************/
/* Window name                                                          */
/************************************************************************/

const char* GetWindowName()
{
    // Default config names
    #if defined ( FONLINE_SERVER )
    static char window_name[ MAX_FOPATH ] = { "FOnline Server\0--default-server-name--" };
    #elif defined ( FONLINE_MAPPER )
    static char window_name[ MAX_FOPATH ] = { "FOnline Mapper\0--default-mapper-name--" };
    #else // FONLINE_CLIENT and others
    static char window_name[ MAX_FOPATH ] = { "FOnline\0--default-client-name--" };
    #endif

    // Extract config name from current exe
    static bool processed = false;
    if( !processed )
    {
        // Call once
        processed = true;

        // Take name from config file
        #if defined ( FONLINE_SERVER )
        IniParser& cfg = IniParser::GetServerConfig();
        #elif defined ( FONLINE_MAPPER )
        IniParser& cfg = IniParser::GetMapperConfig();
        #else // FONLINE_CLIENT and others
        IniParser& cfg = IniParser::GetClientConfig();
        #endif
        if( !cfg.IsLoaded() )
            return window_name;

        // 'WindowName' section
        char str[ MAX_FOPATH ];
        #if !defined ( FONLINE_CLIENT )
        if( !cfg.GetStr( "WindowName", "", str ) || !str[ 0 ] )
            return window_name;
        #else
        if( !cfg.GetStr( "WindowName", "", str ) || !str[ 0 ] )
            return window_name;
        #endif
        Str::Copy( window_name, str );

        // Singleplayer appendix
        if( Singleplayer )
            Str::Append( window_name, " Singleplayer" );

        // Mapper appendix
        #if defined ( FONLINE_MAPPER )
        Str::Append( window_name, " v." );
        Str::Append( window_name, Str::ItoA( FONLINE_VERSION ) );
        #endif
    }

    return window_name;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )

SDL_Window*   MainWindow = NULL;
SDL_GLContext GLContext = NULL;
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
    # define GETOPTIONS_CMD_LINE_INT( opt, str_id )                       \
        do { char* str = Str::Substring( CommandLine, str_id ); if( str ) \
                 opt = atoi( str + Str::Length( str_id ) + 1 ); } while( 0 )
    # define GETOPTIONS_CMD_LINE_BOOL( opt, str_id )                      \
        do { char* str = Str::Substring( CommandLine, str_id ); if( str ) \
                 opt = atoi( str + Str::Length( str_id ) + 1 ) != 0; } while( 0 )
    # define GETOPTIONS_CMD_LINE_BOOL_ON( opt, str_id )                   \
        do { char* str = Str::Substring( CommandLine, str_id ); if( str ) \
                 opt = true; } while( 0 )
    # define GETOPTIONS_CMD_LINE_STR( opt, str_id )                       \
        do { char* str = Str::Substring( CommandLine, str_id ); if( str ) \
                 sscanf( str + Str::Length( str_id ) + 1, "%s", opt ); } while( 0 )
    # define GETOPTIONS_CHECK( val_, min_, max_, def_ )                 \
        do { int val__ = (int) val_; if( val__ < min_ || val__ > max_ ) \
                 val_ = def_; } while( 0 )

    char buf[ MAX_FOTEXT ];

    // Load config file
    # ifdef FONLINE_MAPPER
    IniParser& cfg_mapper = IniParser::GetMapperConfig();
    cfg_mapper.GetStr( "ClientPath", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ClientPath" );
    FileManager::FormatPath( buf );
    *GameOpt.ClientPath = buf;
    if( GameOpt.ClientPath->length() && GameOpt.ClientPath->c_str()[ GameOpt.ClientPath->length() - 1 ] != DIR_SLASH_C )
        *GameOpt.ClientPath += DIR_SLASH_S;
    cfg_mapper.GetStr( "ServerPath", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ServerPath" );
    FileManager::FormatPath( buf );
    *GameOpt.ServerPath = buf;
    if( GameOpt.ServerPath->length() && GameOpt.ServerPath->c_str()[ GameOpt.ServerPath->length() - 1 ] != DIR_SLASH_C )
        *GameOpt.ServerPath += DIR_SLASH_S;

    // Server and client data
    FileManager::InitDataFiles( GameOpt.ServerPath->c_str() );
    FileManager::InitDataFiles( ( GameOpt.ClientPath->c_std_str() + "data" + DIR_SLASH_S ).c_str() );
    # endif

    // Client config
    IniParser& cfg = IniParser::GetClientConfig();

    // Language
    cfg.GetStr( "Language", "russ", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "Language" );
    Str::Lower( buf );
    if( Str::Compare( buf, "russ" ) )
        SetExceptionsRussianText();

    // Int / Bool
    GameOpt.OpenGLDebug = cfg.GetInt( "OpenGLDebug", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.OpenGLDebug, "-OpenGLDebug" );
    GameOpt.AssimpLogging = cfg.GetInt( "AssimpLogging", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AssimpLogging, "-AssimpLogging" );
    GameOpt.FullScreen = cfg.GetInt( "FullScreen", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.FullScreen, "-FullScreen" );
    GameOpt.VSync = cfg.GetInt( "VSync", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.VSync, "-VSync" );
    GameOpt.Light = cfg.GetInt( "Light", 20 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.Light, "-Light" );
    GETOPTIONS_CHECK( GameOpt.Light, 0, 100, 20 );
    GameOpt.ScrollDelay = cfg.GetInt( "ScrollDelay", 10 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScrollDelay, "-ScrollDelay" );
    GETOPTIONS_CHECK( GameOpt.ScrollDelay, 0, 100, 10 );
    GameOpt.ScrollStep = cfg.GetInt( "ScrollStep", 12 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScrollStep, "-ScrollStep" );
    GETOPTIONS_CHECK( GameOpt.ScrollStep, 4, 32, 12 );
    GameOpt.TextDelay = cfg.GetInt( "TextDelay", 3000 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.TextDelay, "-TextDelay" );
    GETOPTIONS_CHECK( GameOpt.TextDelay, 1000, 30000, 3000 );
    GameOpt.DamageHitDelay = cfg.GetInt( "DamageHitDelay", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.DamageHitDelay, "-OptDamageHitDelay" );
    GETOPTIONS_CHECK( GameOpt.DamageHitDelay, 0, 30000, 0 );
    GameOpt.ScreenWidth = cfg.GetInt( "ScreenWidth", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScreenWidth, "-ScreenWidth" );
    GETOPTIONS_CHECK( GameOpt.ScreenWidth, 100, 30000, 800 );
    GameOpt.ScreenHeight = cfg.GetInt( "ScreenHeight", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScreenHeight, "-ScreenHeight" );
    GETOPTIONS_CHECK( GameOpt.ScreenHeight, 100, 30000, 600 );
    GameOpt.AlwaysOnTop = cfg.GetInt( "AlwaysOnTop", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AlwaysOnTop, "-AlwaysOnTop" );
    GameOpt.FixedFPS = cfg.GetInt( "FixedFPS", 100 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.FixedFPS, "-FixedFPS" );
    GETOPTIONS_CHECK( GameOpt.FixedFPS, -10000, 10000, 100 );
    GameOpt.MsgboxInvert = cfg.GetInt( "InvertMessBox", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.MsgboxInvert, "-InvertMessBox" );
    GameOpt.MessNotify = cfg.GetInt( "WinNotify", true ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.MessNotify, "-WinNotify" );
    GameOpt.SoundNotify = cfg.GetInt( "SoundNotify", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.SoundNotify, "-SoundNotify" );
    GameOpt.Port = cfg.GetInt( "RemotePort", 4000 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.Port, "-RemotePort" );
    GETOPTIONS_CHECK( GameOpt.Port, 0, 0xFFFF, 4000 );
    GameOpt.UpdateServerPort = cfg.GetInt( "UpdateServerPort", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.UpdateServerPort, "-UpdateServerPort" );
    GETOPTIONS_CHECK( GameOpt.UpdateServerPort, 0, 0xFFFF, 0 );
    GameOpt.ProxyType = cfg.GetInt( "ProxyType", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ProxyType, "-ProxyType" );
    GETOPTIONS_CHECK( GameOpt.ProxyType, 0, 3, 0 );
    GameOpt.ProxyPort = cfg.GetInt( "ProxyPort", 8080 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ProxyPort, "-ProxyPort" );
    GETOPTIONS_CHECK( GameOpt.ProxyPort, 0, 0xFFFF, 1080 );
    GameOpt.AlwaysRun = cfg.GetInt( "AlwaysRun", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AlwaysRun, "-AlwaysRun" );
    GameOpt.DefaultCombatMode = cfg.GetInt( "DefaultCombatMode", COMBAT_MODE_ANY );
    GETOPTIONS_CMD_LINE_INT( GameOpt.DefaultCombatMode, "-DefaultCombatMode" );
    GETOPTIONS_CHECK( GameOpt.DefaultCombatMode, COMBAT_MODE_ANY, COMBAT_MODE_TURN_BASED, COMBAT_MODE_ANY );
    GameOpt.IndicatorType = cfg.GetInt( "IndicatorType", COMBAT_MODE_ANY );
    GETOPTIONS_CMD_LINE_INT( GameOpt.IndicatorType, "-IndicatorType" );
    GETOPTIONS_CHECK( GameOpt.IndicatorType, INDICATOR_LINES, INDICATOR_BOTH, INDICATOR_LINES );
    GameOpt.DoubleClickTime = cfg.GetInt( "DoubleClickTime", 400 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.DoubleClickTime, "-DoubleClickTime" );
    GETOPTIONS_CHECK( GameOpt.DoubleClickTime, 0, 1000, 0 );
    GameOpt.CombatMessagesType = cfg.GetInt( "CombatMessagesType", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.CombatMessagesType, "-CombatMessagesType" );
    GETOPTIONS_CHECK( GameOpt.CombatMessagesType, 0, 1, 0 );

    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.HelpInfo, "-HelpInfo" );
    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.DebugInfo, "-DebugInfo" );
    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.DebugNet, "-DebugNet" );

    // Str
    cfg.GetStr( "RemoteHost", "localhost", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-RemoteHost" );
    *GameOpt.Host = buf;
    cfg.GetStr( "UpdateServerHost", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-UpdateServerHost" );
    *GameOpt.UpdateServerHost = buf;
    cfg.GetStr( "ProxyHost", "localhost", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ProxyHost" );
    *GameOpt.ProxyHost = buf;
    cfg.GetStr( "ProxyUser", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ProxyUser" );
    *GameOpt.ProxyUser = buf;
    cfg.GetStr( "ProxyPass", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ProxyPass" );
    *GameOpt.ProxyPass = buf;
    cfg.GetStr( "UserName", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-UserName" );
    *GameOpt.Name = buf;
    cfg.GetStr( "KeyboardRemap", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-KeyboardRemap" );
    *GameOpt.KeyboardRemap = buf;

    // Logging
    bool logging = cfg.GetInt( "Logging", 1 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "-Logging" );
    if( !logging )
    {
        WriteLog( "File logging off.\n" );
        LogToFile( NULL );
    }

    logging = cfg.GetInt( "LoggingDebugOutput", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "-LoggingDebugOutput" );
    if( logging )
        LogToDebugOutput( true );

    logging = cfg.GetInt( "LoggingTime", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "-LoggingTime" );
    LogWithTime( logging );
    logging = cfg.GetInt( "LoggingThread", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "-LoggingThread" );
    LogWithThread( logging );

    # ifdef FONLINE_MAPPER
    Script::SetRunTimeout( 0, 0 );
    # endif

    # ifdef FO_OSX_IOS
    GameOpt.ScreenWidth = 1024;
    GameOpt.ScreenHeight = 768;
    GameOpt.FixedFPS = 0;
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
bool WorldSaveManager = true;
bool LogicMT = false;
bool AllowServerNativeCalls = false;
bool AllowClientNativeCalls = false;

void GetServerOptions()
{
    IniParser& cfg = IniParser::GetServerConfig();
    ServerGameSleep = cfg.GetInt( "GameSleep", 10 );
    Script::SetConcurrentExecution( cfg.GetInt( "ScriptConcurrentExecution", 0 ) != 0 );
    WorldSaveManager = ( cfg.GetInt( "WorldSaveManager", 1 ) != 0 );
    AllowServerNativeCalls = ( cfg.GetInt( "AllowServerNativeCalls", 1 ) != 0 );
    AllowClientNativeCalls = ( cfg.GetInt( "AllowClientNativeCalls", 0 ) != 0 );
}

ServerScriptFunctions ServerFunctions;

#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/
#ifdef FO_WINDOWS

const char* GetLastSocketError()
{
    static THREAD char str[ MAX_FOTEXT ];
    int                error = WSAGetLastError();
    # define CASE_SOCK_ERROR( code, message ) \
    case code:                                \
        Str::Format( str, "%s, %d, %s", # code, code, message ); break

    switch( error )
    {
    default:
        Str::Format( str, "%d, unknown error code.", error );
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
//	CASE_SOCK_ERROR(WSA_QOS_EUNKOWNPSOBJ,"An unrecognized object was found in the QOS provider-specific buffer.");
        CASE_SOCK_ERROR( WSA_QOS_EPOLICYOBJ, "An invalid policy object was found in the QOS provider-specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_EFLOWDESC, "An invalid QOS flow descriptor was found in the flow descriptor list." );
        CASE_SOCK_ERROR( WSA_QOS_EPSFLOWSPEC, "An invalid or inconsistent flowspec was found in the QOS provider specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_EPSFILTERSPEC, "An invalid FILTERSPEC was found in the QOS provider-specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_ESDMODEOBJ, "An invalid shape discard mode object was found in the QOS provider specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_ESHAPERATEOBJ, "An invalid shaping rate object was found in the QOS provider-specific buffer." );
        CASE_SOCK_ERROR( WSA_QOS_RESERVED_PETYPE, "A reserved policy element was found in the QOS provider-specific buffer." );
    }
    return str;
}

#else

const char* GetLastSocketError()
{
    static THREAD char str[ MAX_FOTEXT ];
    Str::Format( str, "%s", strerror( errno ) );
    return str;
}

#endif
/************************************************************************/
/*                                                                      */
/************************************************************************/

GameOptions GameOpt;
GameOptions::GameOptions()
{
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
    FixBoyDefaultExperience = 50;
    SneakDivider = 6;
    LevelCap = 99;
    LevelCapAddExperience = false;
    LookNormal = 20;
    LookMinimum = 6;
    GlobalMapMaxGroupCount = 10;
    CritterIdleTick = 10000;
    TurnBasedTick = 30000;
    DeadHitPoints = -6;

    Breaktime = 1200;
    TimeoutTransfer = 3;
    TimeoutBattle = 10;
    ApRegeneration = 10000;
    RtApCostCritterWalk = 0;
    RtApCostCritterRun = 1;
    RtApCostMoveItemContainer = 0;
    RtApCostMoveItemInventory = 2;
    RtApCostPickItem = 1;
    RtApCostDropItem = 1;
    RtApCostReloadWeapon = 2;
    RtApCostPickCritter = 1;
    RtApCostUseItem = 3;
    RtApCostUseSkill = 2;
    RtAlwaysRun = false;
    TbApCostCritterMove = 1;
    TbApCostMoveItemContainer = 0;
    TbApCostMoveItemInventory = 1;
    TbApCostPickItem = 3;
    TbApCostDropItem = 1;
    TbApCostReloadWeapon = 2;
    TbApCostPickCritter = 3;
    TbApCostUseItem = 3;
    TbApCostUseSkill = 3;
    TbAlwaysRun = false;
    ApCostAimEyes = 1;
    ApCostAimHead = 1;
    ApCostAimGroin = 1;
    ApCostAimTorso = 1;
    ApCostAimArms = 1;
    ApCostAimLegs = 1;
    RunOnCombat = false;
    RunOnTransfer = false;
    RunOnTurnBased = false;
    GlobalMapWidth = 28;
    GlobalMapHeight = 30;
    GlobalMapZoneLength = 50;
    GlobalMapMoveTime = 500;
    BagRefreshTime = 60;
    AttackAnimationsMinDist = 0;
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
    LookWeight = 200;
    CustomItemCost = false;
    RegistrationTimeout = 5;
    AccountPlayTime = 0;
    LoggingVars = false;
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
    GenerateWorldDisabled = false;
    BuildMapperScripts = false;
    CommandLine = ScriptString::Create();

    StartSpecialPoints = 40;
    StartTagSkillPoints = 3;

    SkillMaxValue = 300;
    SkillModAdd2 = 100;
    SkillModAdd3 = 125;
    SkillModAdd4 = 150;
    SkillModAdd5 = 175;
    SkillModAdd6 = 200;

    ReputationLoved = 30;
    ReputationLiked = 15;
    ReputationAccepted = 1;
    ReputationNeutral = 0;
    ReputationAntipathy = -14;
    ReputationHated = -29;

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
    MapDataPrefix = ScriptString::Create( "art/geometry/fallout_" );

    // Client and Mapper
    Quit = false;
    OpenGLRendering = true;
    OpenGLDebug = false;
    AssimpLogging = false;
    MouseX = 0;
    MouseY = 0;
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
    Host = ScriptString::Create( "localhost" );
    Port = 4000;
    UpdateServerHost = ScriptString::Create();
    UpdateServerPort = 0;
    ProxyType = 0;
    ProxyHost = ScriptString::Create();
    ProxyPort = 0;
    ProxyUser = ScriptString::Create();
    ProxyPass = ScriptString::Create();
    Name = ScriptString::Create();
    ScrollDelay = 10;
    ScrollStep = 1;
    ScrollCheck = true;
    FixedFPS = 100;
    FPS = 0;
    PingPeriod = 2000;
    Ping = 0;
    MsgboxInvert = false;
    DefaultCombatMode = COMBAT_MODE_ANY;
    MessNotify = true;
    SoundNotify = true;
    AlwaysOnTop = false;
    TextDelay = 3000;
    DamageHitDelay = 0;
    ScreenWidth = 800;
    ScreenHeight = 600;
    MultiSampling = -1;
    MouseScroll = true;
    IndicatorType = INDICATOR_LINES;
    DoubleClickTime = 0;
    RoofAlpha = 200;
    HideCursor = false;
    DisableLMenu = false;
    DisableMouseEvents = false;
    DisableKeyboardEvents = false;
    HidePassword = true;
    PlayerOffAppendix = ScriptString::Create( "_off" );
    CombatMessagesType = 0;
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
    KeyboardRemap = ScriptString::Create();
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
    RegName = ScriptString::Create();
    RegPassword = ScriptString::Create();
    ChosenLightColor = 0;
    ChosenLightDistance = 4;
    ChosenLightIntensity = 2500;
    ChosenLightFlags = 0;

    // Mapper
    ClientPath = ScriptString::Create( DIR_SLASH_SD );
    ServerPath = ScriptString::Create( DIR_SLASH_SD );
    ShowCorners = false;
    ShowSpriteCuts = false;
    ShowDrawOrder = false;
    SplitTilesCollection = true;

    // Engine data
    CritterTypes = NULL;

    ClientMap = NULL;
    ClientMapLight = NULL;
    ClientMapWidth = 0;
    ClientMapHeight = 0;

    GetDrawingSprites = NULL;
    GetSpriteInfo = NULL;
    GetSpriteColor = NULL;
    IsSpriteHit = NULL;

    GetNameByHash = NULL;
    GetHashByName = NULL;

    ScriptLoadModule = NULL;
    ScriptBind = NULL;
    ScriptPrepare = NULL;
    ScriptSetArgInt8 = NULL;
    ScriptSetArgInt16 = NULL;
    ScriptSetArgInt = NULL;
    ScriptSetArgInt64 = NULL;
    ScriptSetArgUInt8 = NULL;
    ScriptSetArgUInt16 = NULL;
    ScriptSetArgUInt = NULL;
    ScriptSetArgUInt64 = NULL;
    ScriptSetArgBool = NULL;
    ScriptSetArgFloat = NULL;
    ScriptSetArgDouble = NULL;
    ScriptSetArgObject = NULL;
    ScriptSetArgAddress = NULL;
    ScriptRunPrepared = NULL;
    ScriptGetReturnedInt8 = NULL;
    ScriptGetReturnedInt16 = NULL;
    ScriptGetReturnedInt = NULL;
    ScriptGetReturnedInt64 = NULL;
    ScriptGetReturnedUInt8 = NULL;
    ScriptGetReturnedUInt16 = NULL;
    ScriptGetReturnedUInt = NULL;
    ScriptGetReturnedUInt64 = NULL;
    ScriptGetReturnedBool = NULL;
    ScriptGetReturnedFloat = NULL;
    ScriptGetReturnedDouble = NULL;
    ScriptGetReturnedObject = NULL;
    ScriptGetReturnedAddress = NULL;

    Random = &::Random;
    GetTick = &Timer::FastTick;
    SetLogCallback = &LogToFunc;
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
        logFile = NULL;
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
    SECURITY_ATTRIBUTES sa = { sizeof( sa ), NULL, TRUE };
    mapFile = CreateFileMapping( INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, INTERPROCESS_DATA_SIZE + sizeof( mapFileMutex ), NULL );
    if( !mapFile )
        return NULL;
    mapFilePtr = NULL;

    mapFileMutex = CreateMutex( &sa, FALSE, NULL );
    if( !mapFileMutex )
        return NULL;

    if( !Lock() )
        return NULL;
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
    mapFile = NULL;
    mapFilePtr = NULL;
}

bool InterprocessData::Attach( HANDLE map_file )
{
    if( !map_file )
        return false;
    mapFile = map_file;
    mapFilePtr = NULL;

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
    mapFilePtr = NULL;

    ReleaseMutex( mapFileMutex );
}

bool InterprocessData::Refresh()
{
    if( !Lock() )
        return false;
    Unlock();
    return true;
}

void* SingleplayerClientProcess = NULL;
#endif

bool             Singleplayer = false;
InterprocessData SingleplayerData;

/************************************************************************/
/* Thread                                                               */
/************************************************************************/

#if !defined ( FONLINE_NPCEDITOR ) && !defined ( FONLINE_MRFIXIT )

THREAD char Thread::threadName[ 64 ] = { 0 };
SizeTStrMap Thread::threadNames;
Mutex       Thread::threadNamesLocker;

# ifdef FO_WINDOWS
DWORD WINAPI ThreadBeginExecution( void* args )
# else
void* ThreadBeginExecution( void* args )
# endif
{
    void** args_ = (void**) args;
    void   ( * func )( void* ) = ( void ( * )( void* ) )args_[ 0 ];
    void*  func_arg = args_[ 1 ];
    char*  name = (char*) args_[ 2 ];
    Thread::SetCurrentName( name );
    delete[] name;
    free( args );
    func( func_arg );
    return NULL;
}

Thread::Thread()
{
    isStarted = false;
}

Thread::~Thread()
{
    Finish();
}

bool Thread::Start( void ( * func )( void* ), const char* name, void* arg /* = NULL */ )
{
    void** args = (void**) malloc( sizeof( void* ) * 3 );
    char*  name_ = Str::Duplicate( name );
    args[ 0 ] = (void*) func, args[ 1 ] = arg, args[ 2 ] = name_;
    # ifdef FO_WINDOWS
    threadId = CreateThread( NULL, 0, ThreadBeginExecution, args, 0, NULL );
    # else
    isStarted = ( pthread_create( &threadId, NULL, ThreadBeginExecution, args ) == 0 );
    # endif
    return isStarted;
}

void Thread::Wait()
{
    if( isStarted )
    {
        # ifdef FO_WINDOWS
        WaitForSingleObject( threadId, INFINITE );
        # else
        pthread_join( threadId, NULL );
        # endif
        isStarted = false;
    }
}

void Thread::Finish()
{
    if( isStarted )
    {
        isStarted = false;
        # ifdef FO_WINDOWS
        TerminateThread( threadId, NULL );
        # else
        pthread_cancel( threadId );
        # endif
    }
}

# if defined ( FO_WINDOWS )
HANDLE Thread::GetWindowsHandle()
{
    return threadId;
}
# elif defined ( FO_LINUX )
pid_t Thread::GetPid()
{
    return (pid_t) threadId;
}
# endif

size_t Thread::GetCurrentId()
{
    # ifdef FO_WINDOWS
    return (size_t) GetCurrentThreadId();
    # else
    return (size_t) pthread_self();
    # endif
}

void Thread::SetCurrentName( const char* name )
{
    if( threadName[ 0 ] )
        return;

    Str::Copy( threadName, name );
    SCOPE_LOCK( threadNamesLocker );
    threadNames.insert( PAIR( GetCurrentId(), string( threadName ) ) );
}

const char* Thread::GetCurrentName()
{
    return threadName;
}

const char* Thread::FindName( uint thread_id )
{
    SCOPE_LOCK( threadNamesLocker );
    auto it = threadNames.find( thread_id );
    return it != threadNames.end() ? ( *it ).second.c_str() : NULL;
}

void Thread::Sleep( uint ms )
{
    # ifdef FO_WINDOWS
    ::Sleep( ms );
    # else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = ( ms % 1000 ) * 1000000;
    while( nanosleep( &req, &req ) == -1 && errno == EINTR )
        continue;
    # endif
}

void Thread_Sleep( uint ms ) // Used in Mutex.h as extern function
{
    Thread::Sleep( ms );
}

#endif

/************************************************************************/
/* Properties                                                           */
/************************************************************************/

PROPERTIES_IMPL( GlobalVars );
GlobalVars::GlobalVars(): Props( PropertiesRegistrator ) {}
GlobalVars* Globals;

PROPERTIES_IMPL( ClientMap );
ClientMap::ClientMap(): Props( PropertiesRegistrator ) {}
ClientMap* ClientCurMap;

PROPERTIES_IMPL( ClientLocation );
ClientLocation::ClientLocation(): Props( PropertiesRegistrator ) {}
ClientLocation* ClientCurLocation;

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

void FormatPreprocessorOutput( string& str )
{
    // Combine long line breaks
    for( int i = 0; i < (int) str.length() - 2; ++i )
        if( str[ i ] == '\n' && str[ i + 1 ] == '\n' && str[ i + 2 ] == '\n' )
            str[ i ] = ' ';

    // Add tabulations for {}
    for( int i = 0; i < (int) str.length() - 1; ++i )
        if( str[ i ] == '{' )
            i = InsertTabs( str, i + 1, 1 );
}

// Deprecated stuff
// item pids conversion
static bool       PidMapsGenerated = false;
static StrMap     PidNameToName;
static UIntStrMap PidIdToName;
static UIntStrMap PidCrIdToName;

#include "ItemManager.h"
static void GeneratePidMaps()
{
    if( PidMapsGenerated )
        return;
    PidMapsGenerated = true;

    FileManager pid_map;
    if( pid_map.LoadFile( "ItemPidsConversion.txt", PT_SERVER_CONFIGS ) )
    {
        istrstream str( (char*) pid_map.GetBuf() );
        string     new_name, old_name;
        uint       old_pid;
        while( !str.eof() )
        {
            str >> new_name >> old_name >> old_pid;
            PidNameToName.insert( PAIR( old_name, new_name ) );
            PidIdToName.insert( PAIR( old_pid, new_name ) );
        }
    }
    if( pid_map.LoadFile( "CritterPidsConversion.txt", PT_SERVER_CONFIGS ) )
    {
        istrstream str( (char*) pid_map.GetBuf() );
        string     new_name;
        uint       old_pid;
        while( !str.eof() )
        {
            str >> new_name >> old_pid;
            PidCrIdToName.insert( PAIR( old_pid, new_name ) );
        }
    }
}

const char* ConvertProtoIdByInt( uint pid )
{
    GeneratePidMaps();
    auto it = PidIdToName.find( pid );
    return it != PidIdToName.end() ? it->second.c_str() : NULL;
}

const char* ConvertProtoIdByStr( const char* pid )
{
    GeneratePidMaps();
    auto it = PidNameToName.find( pid );
    return it != PidNameToName.end() ? it->second.c_str() : NULL;
}

const char* ConvertProtoCritterIdByInt( uint pid )
{
    GeneratePidMaps();
    auto it = PidCrIdToName.find( pid );
    return it != PidCrIdToName.end() ? it->second.c_str() : NULL;
}
