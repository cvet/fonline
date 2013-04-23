#include "StdAfx.h"
#include "Common.h"
#include "Exception.h"
#include "Crypt.h"
#include "Script.h"
#include "Text.h"
#include <math.h>
#include "FileManager.h"
#include "IniParser.h"
#include "Version.h"
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
char   CommandLine[ MAX_FOTEXT ] = { 0 };
char** CommandLineArgValues = NULL;
uint   CommandLineArgCount = 0;
void SetCommandLine( uint argc, char** argv )
{
    CommandLineArgCount = argc;
    CommandLineArgValues = new char*[ CommandLineArgCount ];
    for( uint i = 0; i < argc; i++ )
    {
        Str::Append( CommandLine, argv[ i ] );
        Str::Append( CommandLine, " " );
        CommandLineArgValues[ i ] = Str::Duplicate( argv[ i ] );
    }
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
    #ifdef FO_WINDOWS
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
    #endif

    // Todo: linux need it?
}

void ShowMessage( const char* message )
{
    #ifdef FO_WINDOWS
    MessageBox( NULL, message, "FOnline", MB_OK );
    #else
    // Todo: Linux
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
/* Config file                                                          */
/************************************************************************/

const char* GetConfigFileName()
{
    // Default config names
    #if defined ( FONLINE_SERVER )
    static char config_name[ MAX_FOPATH ] = { "FOnlineServer.cfg\0--default-server-config--" };
    #elif defined ( FONLINE_MAPPER )
    static char config_name[ MAX_FOPATH ] = { "Mapper.cfg\0--default-mapper-config--" };
    #else // FONLINE_CLIENT and others
    static char config_name[ MAX_FOPATH ] = { "FOnline.cfg\0--default-client-config--" };
    #endif

    // Extract config name from current exe
    static bool processed = false;
    if( !processed )
    {
        // Call once
        processed = true;

        // Get full path
        char module_name[ MAX_FOPATH ];
        #ifdef FO_WINDOWS
        if( !GetModuleFileName( NULL, module_name, sizeof( module_name ) ) )
            return config_name;
        #else
        // Todo: Linux CommandLineArgValues[0] ?
        #endif

        // Change extension
        char* ext = (char*) FileManager::GetExtension( module_name );
        if( !ext )
            return config_name;
        Str::Copy( ext, 4, "cfg" );

        // Check availability
        if( !FileExist( module_name ) )
            return config_name;

        // Get file name
        const char* name = NULL;
        for( size_t i = 0, j = Str::Length( module_name ); i < j; i++ )
            if( module_name[ i ] == DIR_SLASH_C )
                name = &module_name[ i + 1 ];
        if( !name )
            return config_name;

        // Set as main
        Str::Copy( config_name, name );
    }

    return config_name;
}

/************************************************************************/
/* Window name                                                          */
/************************************************************************/

const char* GetWindowName()
{
    // Default config names
    #if defined ( FONLINE_SERVER )
    static char window_name[ MAX_FOPATH ] = { "FOnline Server\0--default-server-name--" };
    int         path_type = PT_SERVER_ROOT;
    #elif defined ( FONLINE_MAPPER )
    static char window_name[ MAX_FOPATH ] = { "FOnline Mapper\0--default-mapper-name--" };
    int         path_type = PT_MAPPER_ROOT;
    #else // FONLINE_CLIENT and others
    static char window_name[ MAX_FOPATH ] = { "FOnline\0--default-client-name--" };
    int         path_type = PT_ROOT;
    #endif

    // Extract config name from current exe
    static bool processed = false;
    if( !processed )
    {
        // Call once
        processed = true;

        // Take name from config file
        IniParser cfg;
        cfg.LoadFile( GetConfigFileName(), path_type );
        if( !cfg.IsLoaded() )
            return window_name;

        // 'WindowName' section
        char str[ MAX_FOPATH ];
        #if !defined ( FONLINE_CLIENT )
        if( !cfg.GetStr( "WindowName", "", str ) || !str[ 0 ] )
            return window_name;
        #else
        if( !cfg.GetStr( CLIENT_CONFIG_APP, "WindowName", "", str ) || !str[ 0 ] )
            return window_name;
        #endif
        Str::Copy( window_name, str );

        // Singleplayer appendix
        if( Singleplayer )
            Str::Append( window_name, " Singleplayer" );

        // Mapper appendix
        #if defined ( FONLINE_MAPPER )
        Str::Append( window_name, " " );
        Str::Append( window_name, MAPPER_VERSION_STR );
        #endif
    }

    return window_name;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )

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
    IniParser cfg_mapper;
    cfg_mapper.LoadFile( GetConfigFileName(), PT_MAPPER_ROOT );

    cfg_mapper.GetStr( "ClientPath", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ClientPath" );
    FileManager::FormatPath( buf );
    GameOpt.ClientPath = buf;
    if( GameOpt.ClientPath.length() && GameOpt.ClientPath.c_str()[ GameOpt.ClientPath.length() - 1 ] != DIR_SLASH_C )
        GameOpt.ClientPath += DIR_SLASH_S;
    cfg_mapper.GetStr( "ServerPath", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ServerPath" );
    FileManager::FormatPath( buf );
    GameOpt.ServerPath = buf;
    if( GameOpt.ServerPath.length() && GameOpt.ServerPath.c_str()[ GameOpt.ServerPath.length() - 1 ] != DIR_SLASH_C )
        GameOpt.ServerPath += DIR_SLASH_S;

    FileManager::SetDataPath( GameOpt.ClientPath.c_str() );

    // Client config
    IniParser cfg;
    cfg_mapper.GetStr( "ClientName", "FOnline", buf );
    Str::Append( buf, ".cfg" );
    cfg.LoadFile( buf, PT_ROOT );
    # else
    IniParser cfg;
    cfg.LoadFile( GetConfigFileName(), PT_ROOT );
    # endif

    // Language
    cfg.GetStr( CLIENT_CONFIG_APP, "Language", "russ", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "Language" );
    Str::Lower( buf );
    if( Str::Compare( buf, "russ" ) )
        SetExceptionsRussianText();

    // Int / Bool
    GameOpt.OpenGLDebug = cfg.GetInt( CLIENT_CONFIG_APP, "OpenGLDebug", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.OpenGLDebug, "-OpenGLDebug" );
    GameOpt.AssimpLogging = cfg.GetInt( CLIENT_CONFIG_APP, "AssimpLogging", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AssimpLogging, "-AssimpLogging" );
    GameOpt.FullScreen = cfg.GetInt( CLIENT_CONFIG_APP, "FullScreen", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.FullScreen, "-FullScreen" );
    GameOpt.VSync = cfg.GetInt( CLIENT_CONFIG_APP, "VSync", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.VSync, "-VSync" );
    GameOpt.Light = cfg.GetInt( CLIENT_CONFIG_APP, "Light", 20 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.Light, "-Light" );
    GETOPTIONS_CHECK( GameOpt.Light, 0, 100, 20 );
    GameOpt.ScrollDelay = cfg.GetInt( CLIENT_CONFIG_APP, "ScrollDelay", 10 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScrollDelay, "-ScrollDelay" );
    GETOPTIONS_CHECK( GameOpt.ScrollDelay, 0, 100, 10 );
    GameOpt.ScrollStep = cfg.GetInt( CLIENT_CONFIG_APP, "ScrollStep", 12 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScrollStep, "-ScrollStep" );
    GETOPTIONS_CHECK( GameOpt.ScrollStep, 4, 32, 12 );
    GameOpt.TextDelay = cfg.GetInt( CLIENT_CONFIG_APP, "TextDelay", 3000 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.TextDelay, "-TextDelay" );
    GETOPTIONS_CHECK( GameOpt.TextDelay, 1000, 3000, 30000 );
    GameOpt.DamageHitDelay = cfg.GetInt( CLIENT_CONFIG_APP, "DamageHitDelay", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.DamageHitDelay, "-OptDamageHitDelay" );
    GETOPTIONS_CHECK( GameOpt.DamageHitDelay, 0, 30000, 0 );
    GameOpt.ScreenWidth = cfg.GetInt( CLIENT_CONFIG_APP, "ScreenWidth", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScreenWidth, "-ScreenWidth" );
    GETOPTIONS_CHECK( GameOpt.ScreenWidth, 100, 30000, 800 );
    GameOpt.ScreenHeight = cfg.GetInt( CLIENT_CONFIG_APP, "ScreenHeight", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ScreenHeight, "-ScreenHeight" );
    GETOPTIONS_CHECK( GameOpt.ScreenHeight, 100, 30000, 600 );
    GameOpt.MultiSampling = cfg.GetInt( CLIENT_CONFIG_APP, "MultiSampling", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.MultiSampling, "-MultiSampling" );
    GETOPTIONS_CHECK( GameOpt.MultiSampling, -1, 16, -1 );
    GameOpt.AlwaysOnTop = cfg.GetInt( CLIENT_CONFIG_APP, "AlwaysOnTop", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AlwaysOnTop, "-AlwaysOnTop" );
    GameOpt.FlushVal = cfg.GetInt( CLIENT_CONFIG_APP, "FlushValue", 50 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.FlushVal, "-FlushValue" );
    GETOPTIONS_CHECK( GameOpt.FlushVal, 1, 1000, 50 );
    GameOpt.BaseTexture = cfg.GetInt( CLIENT_CONFIG_APP, "BaseTexture", 1024 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.BaseTexture, "-BaseTexture" );
    GETOPTIONS_CHECK( GameOpt.BaseTexture, 128, 8192, 1024 );
    GameOpt.FixedFPS = cfg.GetInt( CLIENT_CONFIG_APP, "FixedFPS", 100 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.FixedFPS, "-FixedFPS" );
    GETOPTIONS_CHECK( GameOpt.FixedFPS, -10000, 10000, 100 );
    GameOpt.MsgboxInvert = cfg.GetInt( CLIENT_CONFIG_APP, "InvertMessBox", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.MsgboxInvert, "-InvertMessBox" );
    GameOpt.MessNotify = cfg.GetInt( CLIENT_CONFIG_APP, "WinNotify", true ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.MessNotify, "-WinNotify" );
    GameOpt.SoundNotify = cfg.GetInt( CLIENT_CONFIG_APP, "SoundNotify", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.SoundNotify, "-SoundNotify" );
    GameOpt.Port = cfg.GetInt( CLIENT_CONFIG_APP, "RemotePort", 4000 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.Port, "-RemotePort" );
    GETOPTIONS_CHECK( GameOpt.Port, 0, 0xFFFF, 4000 );
    GameOpt.ProxyType = cfg.GetInt( CLIENT_CONFIG_APP, "ProxyType", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ProxyType, "-ProxyType" );
    GETOPTIONS_CHECK( GameOpt.ProxyType, 0, 3, 0 );
    GameOpt.ProxyPort = cfg.GetInt( CLIENT_CONFIG_APP, "ProxyPort", 8080 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ProxyPort, "-ProxyPort" );
    GETOPTIONS_CHECK( GameOpt.ProxyPort, 0, 0xFFFF, 1080 );
    GameOpt.AlwaysRun = cfg.GetInt( CLIENT_CONFIG_APP, "AlwaysRun", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( GameOpt.AlwaysRun, "-AlwaysRun" );
    GameOpt.DefaultCombatMode = cfg.GetInt( CLIENT_CONFIG_APP, "DefaultCombatMode", COMBAT_MODE_ANY );
    GETOPTIONS_CMD_LINE_INT( GameOpt.DefaultCombatMode, "-DefaultCombatMode" );
    GETOPTIONS_CHECK( GameOpt.DefaultCombatMode, COMBAT_MODE_ANY, COMBAT_MODE_TURN_BASED, COMBAT_MODE_ANY );
    GameOpt.IndicatorType = cfg.GetInt( CLIENT_CONFIG_APP, "IndicatorType", COMBAT_MODE_ANY );
    GETOPTIONS_CMD_LINE_INT( GameOpt.IndicatorType, "-IndicatorType" );
    GETOPTIONS_CHECK( GameOpt.IndicatorType, INDICATOR_LINES, INDICATOR_BOTH, INDICATOR_LINES );
    GameOpt.DoubleClickTime = cfg.GetInt( CLIENT_CONFIG_APP, "DoubleClickTime", COMBAT_MODE_ANY );
    GETOPTIONS_CMD_LINE_INT( GameOpt.DoubleClickTime, "-DoubleClickTime" );
    GETOPTIONS_CHECK( GameOpt.DoubleClickTime, 0, 1000, 0 );
    GameOpt.CombatMessagesType = cfg.GetInt( CLIENT_CONFIG_APP, "CombatMessagesType", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.CombatMessagesType, "-CombatMessagesType" );
    GETOPTIONS_CHECK( GameOpt.CombatMessagesType, 0, 1, 0 );
    GameOpt.Animation3dFPS = cfg.GetInt( CLIENT_CONFIG_APP, "Animation3dFPS", 10 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.Animation3dFPS, "-Animation3dFPS" );
    GETOPTIONS_CHECK( GameOpt.Animation3dFPS, 0, 1000, 10 );
    GameOpt.Animation3dSmoothTime = cfg.GetInt( CLIENT_CONFIG_APP, "Animation3dSmoothTime", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.Animation3dSmoothTime, "-Animation3dSmoothTime" );
    GETOPTIONS_CHECK( GameOpt.Animation3dSmoothTime, 0, 10000, 250 );

    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.HelpInfo, "-HelpInfo" );
    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.DebugInfo, "-DebugInfo" );
    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.DebugNet, "-DebugNet" );
    GETOPTIONS_CMD_LINE_BOOL_ON( GameOpt.DebugSprites, "-DebugSprites" );

    // Str
    cfg.GetStr( CLIENT_CONFIG_APP, "FonlineDataPath", DIR_SLASH_SD "data", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-FonlineDataPath" );
    FileManager::FormatPath( buf );
    GameOpt.FoDataPath = buf;
    cfg.GetStr( CLIENT_CONFIG_APP, "RemoteHost", "localhost", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-RemoteHost" );
    GameOpt.Host = buf;
    cfg.GetStr( CLIENT_CONFIG_APP, "ProxyHost", "localhost", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ProxyHost" );
    GameOpt.ProxyHost = buf;
    cfg.GetStr( CLIENT_CONFIG_APP, "ProxyUser", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ProxyUser" );
    GameOpt.ProxyUser = buf;
    cfg.GetStr( CLIENT_CONFIG_APP, "ProxyPass", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-ProxyPass" );
    GameOpt.ProxyPass = buf;
    cfg.GetStr( CLIENT_CONFIG_APP, "UserName", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-UserName" );
    GameOpt.Name = buf;
    cfg.GetStr( CLIENT_CONFIG_APP, "KeyboardRemap", "", buf );
    GETOPTIONS_CMD_LINE_STR( buf, "-KeyboardRemap" );
    GameOpt.KeyboardRemap = buf;

    // Logging
    bool logging = cfg.GetInt( CLIENT_CONFIG_APP, "Logging", 1 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "-Logging" );
    if( !logging )
    {
        WriteLog( "File logging off.\n" );
        LogToFile( NULL );
    }

    logging = cfg.GetInt( CLIENT_CONFIG_APP, "LoggingDebugOutput", 0 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "-LoggingDebugOutput" );
    if( logging )
        LogToDebugOutput( true );

    logging = cfg.GetInt( CLIENT_CONFIG_APP, "LoggingTime", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "-LoggingTime" );
    LogWithTime( logging );
    logging = cfg.GetInt( CLIENT_CONFIG_APP, "LoggingThread", false ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "-LoggingThread" );
    LogWithThread( logging );

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
bool WorldSaveManager = true;
bool LogicMT = false;

void GetServerOptions()
{
    IniParser cfg;
    cfg.LoadFile( GetConfigFileName(), PT_SERVER_ROOT );
    ServerGameSleep = cfg.GetInt( "GameSleep", 10 );
    Script::SetConcurrentExecution( cfg.GetInt( "ScriptConcurrentExecution", 0 ) != 0 );
    WorldSaveManager = ( cfg.GetInt( "WorldSaveManager", 1 ) == 1 );
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
        Str::Format( str, # code ", %d, "message, code ); break

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
    Str::Format( str, "%d", errno );
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

    DisableTcpNagle = false;
    DisableZlibCompression = false;
    FloodSize = 2048;
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

    StartSpecialPoints = 40;
    StartTagSkillPoints = 3;

    SkillMaxValue = 300;
    SkillModAdd2 = 100;
    SkillModAdd3 = 125;
    SkillModAdd4 = 150;
    SkillModAdd5 = 175;
    SkillModAdd6 = 200;

    AbsoluteOffsets = true;
    SkillBegin = 200;
    SkillEnd = 217;
    TimeoutBegin = 230;
    TimeoutEnd = 249;
    KillBegin = 260;
    KillEnd = 278;
    PerkBegin = 300;
    PerkEnd = 435;
    AddictionBegin = 470;
    AddictionEnd = 476;
    KarmaBegin = 480;
    KarmaEnd = 495;
    DamageBegin = 500;
    DamageEnd = 506;
    TraitBegin = 550;
    TraitEnd = 565;
    ReputationBegin = 570;
    ReputationEnd = 599;

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
    MapRoofOffsX = -8;
    MapRoofOffsY = -66;
    MapRoofSkipSize = 2;
    MapCameraAngle = 25.7f;
    MapSmoothPath = true;
    MapDataPrefix = "art/geometry/fallout_";

    // Client and Mapper
    Quit = false;
    #ifdef FO_D3D
    OpenGLRendering = false;
    #else
    OpenGLRendering = true;
    #endif
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
    DebugSprites = false;
    FullScreen = false;
    VSync = false;
    FlushVal = 100;
    BaseTexture = 256;
    Light = 0;
    Host = "localhost";
    Port = 0;
    ProxyType = 0;
    ProxyHost = "";
    ProxyPort = 0;
    ProxyUser = "";
    ProxyPass = "";
    Name = "";
    ScrollDelay = 10;
    ScrollStep = 1;
    ScrollCheck = true;
    FoDataPath = "";
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
    MultiSampling = 0;
    MouseScroll = true;
    IndicatorType = INDICATOR_LINES;
    DoubleClickTime = 0;
    RoofAlpha = 200;
    HideCursor = false;
    DisableLMenu = false;
    DisableMouseEvents = false;
    DisableKeyboardEvents = false;
    HidePassword = true;
    PlayerOffAppendix = "_off";
    CombatMessagesType = 0;
    Animation3dSmoothTime = 250;
    Animation3dFPS = 10;
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

    // Mapper
    ClientPath = DIR_SLASH_SD;
    ServerPath = DIR_SLASH_SD;
    ShowCorners = false;
    ShowSpriteCuts = false;
    ShowDrawOrder = false;
    SplitTilesCollection = true;

    // Engine data
    CritterChangeParameter = NULL;
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

    // Callbacks
    GetUseApCost = NULL;
    GetAttackDistantion = NULL;
    GetRainOffset = NULL;
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

THREAD char Thread::threadName[ 64 ] = { 0 };
UIntStrMap  Thread::threadNames;
Mutex       Thread::threadNamesLocker;

void* ThreadBeginExecution( void* args )
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
    pthread_attr_init( &threadAttr );
}

Thread::~Thread()
{
    pthread_attr_destroy( &threadAttr );
}

bool Thread::Start( void ( * func )( void* ), const char* name, void* arg /* = NULL */ )
{
    void** args = (void**) malloc( sizeof( void* ) * 3 );
    char*  name_ = Str::Duplicate( name );
    args[ 0 ] = (void*) func, args[ 1 ] = arg, args[ 2 ] = name_;
    isStarted = ( pthread_create( &threadId, &threadAttr, ThreadBeginExecution, args ) == 0 );
    return isStarted;
}

void Thread::Wait()
{
    if( isStarted )
        pthread_join( threadId, NULL );
    isStarted = false;
}

void Thread::Finish()
{
    if( isStarted )
        pthread_cancel( threadId );
    isStarted = false;
}

#ifdef FO_WINDOWS
HANDLE Thread::GetWindowsHandle()
{
    return pthread_getw32threadhandle_np( threadId );
}
#endif

#ifndef FO_WINDOWS
pid_t Thread::GetPid()
{
    return (pid_t) threadId;
}
#endif

uint Thread::GetCurrentId()
{
    #ifdef FO_WINDOWS
    return (uint) GetCurrentThreadId();
    #else
    return (uint) pthread_self();
    #endif
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
    #ifdef FO_WINDOWS
    ::Sleep( ms );
    #else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = ( ms % 1000 ) * 1000000;
    while( nanosleep( &req, &req ) == -1 && errno == EINTR )
        continue;
    #endif
}

void Thread_Sleep( uint ms ) // Used in Mutex.h as extern function
{
    Thread::Sleep( ms );
}

/************************************************************************/
/* FOWindow                                                             */
/************************************************************************/

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_MAPPER )

int FOWindow::handle( int event )
{
    // Keyboard
    if( event == FL_KEYDOWN || event == FL_KEYUP )
    {
        KeyboardEvents.push_back( event );
        KeyboardEvents.push_back( Fl::event_key() );
        KeyboardEventsText.push_back( Fl::event_text() );
        return 1;
    }
    else if( event == FL_PASTE )
    {
        KeyboardEvents.push_back( FL_KEYDOWN );
        KeyboardEvents.push_back( 0 );
        KeyboardEventsText.push_back( Fl::event_text() );
        KeyboardEvents.push_back( FL_KEYUP );
        KeyboardEvents.push_back( 0 );
        KeyboardEventsText.push_back( Fl::event_text() );
        return 1;
    }
    // Mouse
    else if( event == FL_PUSH || event == FL_RELEASE || ( event == FL_MOUSEWHEEL && Fl::event_dy() != 0 ) )
    {
        MouseEvents.push_back( event );
        MouseEvents.push_back( Fl::event_button() );
        MouseEvents.push_back( Fl::event_dy() );
        return 1;
    }

    // Focus
    if( event == FL_FOCUS )
        Focused = true;
    if( event == FL_UNFOCUS )
        Focused = false;

    return 0;
}

#endif

/************************************************************************/
/*                                                                      */
/************************************************************************/

// Deprecated stuff
#include <FileManager.h>
#include <Text.h>
#include <Item.h>

bool     ListsLoaded = false;
PCharVec LstNames[ PATH_LIST_COUNT ];

void LoadList( const char* lst_name, int path_type )
{
    FileManager fm;
    if( !fm.LoadFile( lst_name, PT_ROOT ) )
        return;

    char        str[ 1024 ];
    uint        str_cnt = 0;
    const char* path = FileManager::GetPath( path_type );

    PCharVec&   lst = LstNames[ path_type ];
    for( uint i = 0, j = (uint) lst.size(); i < j; i++ )
        SAFEDELA( lst[ i ] );
    lst.clear();

    while( fm.GetLine( str, 1023 ) )
    {
        // Lower text
        Str::Lower( str );

        // Skip comments
        if( !Str::Length( str ) || str[ 0 ] == '#' || str[ 0 ] == ';' )
            continue;

        // New value of line
        if( str[ 0 ] == '*' )
        {
            str_cnt = atoi( &str[ 1 ] );
            continue;
        }

        // Find ext
        char* ext = (char*) FileManager::GetExtension( str );
        if( !ext )
        {
            str_cnt++;
            WriteLogF( _FUNC_, " - Extension not found in line<%s>, skip.\n", str );
            continue;
        }

        // Cut off comments
        int j = 0;
        while( ext[ j ] && ext[ j ] != ' ' )
            j++;
        ext[ j ] = '\0';

        // Create name
        uint  len = Str::Length( path ) + Str::Length( str ) + 1;
        char* rec = new char[ len ];
        Str::Copy( rec, len, path );
        Str::Copy( rec, len, str );

        // Check for size
        if( str_cnt >= lst.size() )
            lst.resize( str_cnt + 1 );

        // Add
        lst[ str_cnt ] = rec;
        str_cnt++;
    }
}

string GetPicName( uint lst_num, ushort pic_num )
{
    if( pic_num >= LstNames[ lst_num ].size() || !LstNames[ lst_num ][ pic_num ] )
        return "";
    return string( LstNames[ lst_num ][ pic_num ] );
}

string Deprecated_GetPicName( int pid, int type, ushort pic_num )
{
    if( !ListsLoaded )
    {
        LoadList( "data" DIR_SLASH_S "deprecated_lists" DIR_SLASH_S "tiles.lst", PT_ART_TILES );
        LoadList( "data" DIR_SLASH_S "deprecated_lists" DIR_SLASH_S "items.lst", PT_ART_ITEMS );
        LoadList( "data" DIR_SLASH_S "deprecated_lists" DIR_SLASH_S "scenery.lst", PT_ART_SCENERY );
        LoadList( "data" DIR_SLASH_S "deprecated_lists" DIR_SLASH_S "walls.lst", PT_ART_WALLS );
        LoadList( "data" DIR_SLASH_S "deprecated_lists" DIR_SLASH_S "misc.lst", PT_ART_MISC );
        LoadList( "data" DIR_SLASH_S "deprecated_lists" DIR_SLASH_S "intrface.lst", PT_ART_INTRFACE );
        LoadList( "data" DIR_SLASH_S "deprecated_lists" DIR_SLASH_S "inven.lst", PT_ART_INVEN );
        ListsLoaded = true;
    }

    if( pid == -1 )
        return GetPicName( PT_ART_INTRFACE, pic_num );
    if( pid == -2 )
        return GetPicName( PT_ART_TILES, pic_num );
    if( pid == -3 )
        return GetPicName( PT_ART_INVEN, pic_num );

    if( type == ITEM_TYPE_DOOR )
        return GetPicName( PT_ART_SCENERY, pic_num );                       // For doors from Scenery
    else if( pid == SP_MISC_SCRBLOCK || SP_MISC_GRID_MAP( pid ) || SP_MISC_GRID_GM( pid ) )
        return GetPicName( PT_ART_MISC, pic_num );                          // For exit grids from Misc
    else if( pid >= 4000 && pid <= 4200 )
        return GetPicName( PT_ART_MISC, pic_num );                          // From Misc
    else if( type >= ITEM_TYPE_ARMOR && type <= ITEM_TYPE_DOOR )
        return GetPicName( PT_ART_ITEMS, pic_num );                         // From Items
    else if( type == ITEM_TYPE_GENERIC || type == ITEM_TYPE_GRID )
        return GetPicName( PT_ART_SCENERY, pic_num );                       // From Scenery
    else if( type == ITEM_TYPE_WALL )
        return GetPicName( PT_ART_WALLS, pic_num );                         // From Walls
    return "";
}

uint Deprecated_GetPicHash( int pid, int type, ushort pic_num )
{
    string name = Deprecated_GetPicName( pid, type, pic_num );
    if( !name.length() )
        return 0;
    return Str::GetHash( name.c_str() );
}

void Deprecated_CondExtToAnim2( uchar cond, uchar cond_ext, uint& anim2ko, uint& anim2dead )
{
    if( cond == COND_KNOCKOUT )
    {
        if( cond_ext == 2 )
            anim2ko = ANIM2_IDLE_PRONE_FRONT;  // COND_KNOCKOUT_FRONT
        anim2ko = ANIM2_IDLE_PRONE_BACK;       // COND_KNOCKOUT_BACK
    }
    else if( cond == COND_DEAD )
    {
        switch( cond_ext )
        {
        case 1:
            anim2dead = ANIM2_DEAD_FRONT; // COND_DEAD_FRONT
        case 2:
            anim2dead = ANIM2_DEAD_BACK;  // COND_DEAD_BACK
        case 3:
            anim2dead = 112;              // COND_DEAD_BURST -> ANIM2_DEAD_BURST
        case 4:
            anim2dead = 110;              // COND_DEAD_BLOODY_SINGLE -> ANIM2_DEAD_BLOODY_SINGLE
        case 5:
            anim2dead = 111;              // COND_DEAD_BLOODY_BURST -> ANIM2_DEAD_BLOODY_BURST
        case 6:
            anim2dead = 113;              // COND_DEAD_PULSE -> ANIM2_DEAD_PULSE
        case 7:
            anim2dead = 114;              // COND_DEAD_PULSE_DUST -> ANIM2_DEAD_PULSE_DUST
        case 8:
            anim2dead = 115;              // COND_DEAD_LASER -> ANIM2_DEAD_LASER
        case 9:
            anim2dead = 117;              // COND_DEAD_EXPLODE -> ANIM2_DEAD_EXPLODE
        case 10:
            anim2dead = 116;              // COND_DEAD_FUSED -> ANIM2_DEAD_FUSED
        case 11:
            anim2dead = 118;              // COND_DEAD_BURN -> ANIM2_DEAD_BURN
        case 12:
            anim2dead = 118;              // COND_DEAD_BURN2 -> ANIM2_DEAD_BURN
        case 13:
            anim2dead = 119;              // COND_DEAD_BURN_RUN -> ANIM2_DEAD_BURN_RUN
        default:
            anim2dead = ANIM2_DEAD_FRONT;
        }
    }
}

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
