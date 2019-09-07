#include "Common.h"
#include "Log.h"
#include "Testing.h"
#include "Timer.h"
#include "Crypt.h"
#include "Script.h"
#include "StringUtils.h"
#include "FileUtils.h"
#include "IniFile.h"
#include "NetBuffer.h"
#include "SDL.h"

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
STATIC_ASSERT( sizeof( size_t ) == sizeof( void* ) );
STATIC_ASSERT( sizeof( uint64 ) >= sizeof( void* ) );

// Some checks from AngelScript config
#include "as_config.h"
#ifdef AS_BIG_ENDIAN
# error "Big Endian architectures not supported."
#endif

IniFile* MainConfig;
StrVec   ProjectFiles;

void InitialSetup( const string& app_name, uint argc, char** argv )
{
    // Exceptions catcher
    CatchExceptions( app_name, FONLINE_VERSION );

    // Disable SIGPIPE signal
    #ifndef FO_WINDOWS
    signal( SIGPIPE, SIG_IGN );
    #endif

    // Timer
    Timer::Init();

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

        // Make single line
        GameOpt.CommandLine += argv[ i ];
        if( i < argc - 1 )
            GameOpt.CommandLine += " ";
    }

    // Store start directory
    if( !GameOpt.WorkDir.empty() )
    {
        #ifdef FO_WINDOWS
        SetCurrentDirectoryW( _str( GameOpt.WorkDir ).toWideChar().c_str() );
        #else
        int r = chdir( GameOpt.WorkDir.c_str() );
        UNUSED_VARIABLE( r );
        #endif
    }
    #ifdef FO_WINDOWS
    wchar_t buf[ TEMP_BUF_SIZE ];
    GetCurrentDirectoryW( TEMP_BUF_SIZE, buf );
    GameOpt.WorkDir = _str().parseWideChar( buf );
    #else
    char  buf[ TEMP_BUF_SIZE ];
    char* r = getcwd( buf, sizeof( buf ) );
    UNUSED_VARIABLE( r );
    GameOpt.WorkDir = buf;
    #endif

    // Injected config
    static char InternalConfig[ 5022 ] =
    {
        "###InternalConfig###\0"
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
    MainConfig = new IniFile();
    MainConfig->AppendStr( InternalConfig );

    // File configs
    string config_dir;
    MainConfig->AppendFile( CONFIG_NAME );
    for( string& config : configs )
    {
        config_dir = _str( config ).extractDir();
        MainConfig->AppendFile( config );
    }

    // Command line config
    for( uint i = 0; i < argc; i++ )
    {
        string arg = argv[ i ];
        if( arg.length() < 2 || arg[ 0 ] != '-' )
            continue;

        string arg_value;
        while( i < argc - 1 && argv[ i + 1 ][ 0 ] != '-' )
        {
            if( arg_value.length() > 0 )
                arg_value += " ";
            arg_value += argv[ i + 1 ];
            i++;
        }

        MainConfig->SetStr( "", arg.substr( 1 ), arg_value );
    }

    // Cache project files
    #if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
    string project_files = MainConfig->GetStr( "", "ProjectFiles" );
    for( string project_path : _str( project_files ).split( ';' ) )
    {
        project_path = _str( config_dir ).combinePath( project_path ).normalizePathSlashes().resolvePath();
        if( !project_path.empty() && project_path.back() != '/' && project_path.back() != '\\' )
            project_path += "/";
        ProjectFiles.push_back( _str( project_path ).normalizePathSlashes() );
    }
    #endif
	
    // Fix Mono path
    string mono_path = MainConfig->GetStr( "", "MonoPath" );
    mono_path = _str( GameOpt.WorkDir ).combinePath( mono_path ).normalizeLineEndings().resolvePath();
    MainConfig->SetStr( "", "MonoPath", mono_path );
}

// Default randomizer
static Randomizer DefaultRandomizer;
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

void ShowErrorMessage( const string& message, const string& traceback )
{
    #ifndef FO_NO_GRAPHIC
    # if defined ( FO_WEB ) || defined ( FO_ANDROID ) || defined ( FO_IOS )
    SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "FOnline Error", message.c_str(), nullptr );

    # else
    string verb_message = message;
    #  ifdef FO_WINDOWS
    string most_recent = "most recent call first";
    #  else
    string most_recent = "most recent call last";
    #  endif
    if( !traceback.empty() )
        verb_message += _str( "\n\nTraceback ({}):\n{}", most_recent, traceback );

    SDL_MessageBoxButtonData copy_button;
    SDL_zero( copy_button );
    copy_button.buttonid = 0;
    copy_button.text = "Copy";

    SDL_MessageBoxButtonData close_button;
    SDL_zero( close_button );
    close_button.buttonid = 1;
    close_button.text = "Close";

    // Workaround for strange button focus behaviour
    #  ifdef FO_WINDOWS
    copy_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    copy_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    #  else
    close_button.flags |= SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
    close_button.flags |= SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT;
    #  endif

    const SDL_MessageBoxButtonData buttons[] = { close_button, copy_button };
    SDL_MessageBoxData             data;
    SDL_zero( data );
    data.flags = SDL_MESSAGEBOX_ERROR;
    data.title = "FOnline Error";
    data.message = verb_message.c_str();
    data.numbuttons = 2;
    data.buttons = buttons;

    int buttonid;
    while( !SDL_ShowMessageBox( &data, &buttonid ) && buttonid == 0 )
        SDL_SetClipboardText( verb_message.c_str() );
    # endif
    #endif
}

int ConvertParamValue( const string& str, bool& fail )
{
    if( str.empty() )
    {
        WriteLog( "Empty parameter value.\n" );
        fail = true;
        return false;
    }

    if( str[ 0 ] == '@' && str[ 1 ] )
        return _str( str.substr( 1 ) ).toHash();
    if( _str( str ).isNumber() )
        return _str( str ).toInt();
    if( _str( str ).compareIgnoreCase( "true" ) )
        return 1;
    if( _str( str ).compareIgnoreCase( "false" ) )
        return 0;

    return Script::GetEnumValue( str, fail );
}

// Hex offset
#define HEX_OFFSET_SIZE    ( ( MAX_HEX_OFFSET * MAX_HEX_OFFSET / 2 + MAX_HEX_OFFSET / 2 ) * DIRS_COUNT )
static int    CurHexOffset = 0; // 0 - none, 1 - hexagonal, 2 - square
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

#if defined ( FONLINE_CLIENT ) || defined ( FONLINE_EDITOR )
IntVec MainWindowKeyboardEvents;
StrVec MainWindowKeyboardEventsText;
IntVec MainWindowMouseEvents;

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
    # define GETOPTIONS_CMD_LINE_INT( opt, str_id )                            \
        do { string str = MainConfig->GetStr( "", str_id ); if( !str.empty() ) \
                 opt = MainConfig->GetInt( "", str_id ); } while( 0 )
    # define GETOPTIONS_CMD_LINE_BOOL( opt, str_id )                           \
        do { string str = MainConfig->GetStr( "", str_id ); if( !str.empty() ) \
                 opt = MainConfig->GetInt( "", str_id ) != 0; } while( 0 )
    # define GETOPTIONS_CMD_LINE_BOOL_ON( opt, str_id )                        \
        do { string str = MainConfig->GetStr( "", str_id ); if( !str.empty() ) \
                 opt = true; } while( 0 )
    # define GETOPTIONS_CMD_LINE_STR( opt, str_id )                            \
        do { string str = MainConfig->GetStr( "", str_id ); if( !str.empty() ) \
                 opt = str; } while( 0 )
    # define GETOPTIONS_CHECK( val_, min_, max_, def_ )                 \
        do { int val__ = (int) val_; if( val__ < min_ || val__ > max_ ) \
                 val_ = def_; } while( 0 )

    string buf;
    # define READ_CFG_STR_DEF( cfg, key, def_val )    buf = MainConfig->GetStr( "", key, def_val )

    // Data files
    File::ClearDataFiles();
    File::InitDataFiles( "$Basic" );
    # ifndef FONLINE_EDITOR
    #  if defined ( FO_IOS )
    File::InitDataFiles( "../../Documents/" );
    #  elif defined ( FO_ANDROID )
    File::InitDataFiles( "$Bundle" );
    File::InitDataFiles( SDL_AndroidGetInternalStoragePath() );
    File::InitDataFiles( SDL_AndroidGetExternalStoragePath() );
    #  elif defined ( FO_WEB )
    File::InitDataFiles( "./Data/" );
    File::InitDataFiles( "./PersistentData/" );
    #  else
    File::InitDataFiles( CLIENT_DATA );
    #  endif
    # endif

    // Cached configuration
    MainConfig->AppendFile( CONFIG_NAME );

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
    GameOpt.ProxyType = MainConfig->GetInt( "", "ProxyType", 0 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ProxyType, "ProxyType" );
    GETOPTIONS_CHECK( GameOpt.ProxyType, 0, 3, 0 );
    GameOpt.ProxyPort = MainConfig->GetInt( "", "ProxyPort", 8080 );
    GETOPTIONS_CMD_LINE_INT( GameOpt.ProxyPort, "ProxyPort" );
    GETOPTIONS_CHECK( GameOpt.ProxyPort, 0, 0xFFFF, 1080 );

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
    READ_CFG_STR_DEF( *MainConfig, "ProxyHost", "localhost" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyHost" );
    GameOpt.ProxyHost = buf;
    READ_CFG_STR_DEF( *MainConfig, "ProxyUser", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyUser" );
    GameOpt.ProxyUser = buf;
    READ_CFG_STR_DEF( *MainConfig, "ProxyPass", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "ProxyPass" );
    GameOpt.ProxyPass = buf;
    READ_CFG_STR_DEF( *MainConfig, "KeyboardRemap", "" );
    GETOPTIONS_CMD_LINE_STR( buf, "KeyboardRemap" );
    GameOpt.KeyboardRemap = buf;

    // Logging
    bool logging = MainConfig->GetInt( "", "Logging", 1 ) != 0;
    GETOPTIONS_CMD_LINE_BOOL( logging, "Logging" );
    if( !logging )
    {
        WriteLog( "File logging off.\n" );
        LogToFile( "" );
    }

    # ifdef FONLINE_EDITOR
    Script::SetRunTimeout( 0, 0 );
    # endif
}
#endif

#if defined ( FONLINE_SERVER ) || defined ( FONLINE_EDITOR )
int  ServerGameSleep = 10;
bool AllowServerNativeCalls = false;
bool AllowClientNativeCalls = false;

void GetServerOptions()
{
    ServerGameSleep = MainConfig->GetInt( "", "GameSleep", 10 );
    AllowServerNativeCalls = ( MainConfig->GetInt( "", "AllowServerNativeCalls", 1 ) != 0 );
    AllowClientNativeCalls = ( MainConfig->GetInt( "", "AllowClientNativeCalls", 0 ) != 0 );
}
#endif

static void AddPropertyCallback( void ( * function )( void*, void*, void*, void* ) )
{
    PropertyRegistrator::GlobalSetCallbacks.push_back( (NativeCallback) function );
}

GameOptions GameOpt;
GameOptions::GameOptions()
{
    WorkDir = "";
    CommandLine = "";

    FullSecondStart = 0;
    FullSecond = 0;
    GameTimeTick = 0;
    YearStartFTHi = 0;
    YearStartFTLo = 0;

    DisableTcpNagle = false;
    DisableZlibCompression = false;
    FloodSize = 2048;
    NoAnswerShuffle = false;
    DialogDemandRecheck = false;
    SneakDivider = 6;
    LookMinimum = 6;
    DeadHitPoints = -6;

    Breaktime = 1200;
    TimeoutTransfer = 3;
    TimeoutBattle = 10;
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
    ForceRebuildResources = false;

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

    #ifdef FO_WEB
    WebBuild = true;
    #else
    WebBuild = false;
    #endif
    #ifdef FO_WINDOWS
    WindowsBuild = true;
    #else
    WindowsBuild = false;
    #endif
    #ifdef FO_LINUX
    LinuxBuild = true;
    #else
    LinuxBuild = false;
    #endif
    #ifdef FO_MAC
    MacOsBuild = true;
    #else
    MacOsBuild = false;
    #endif
    #ifdef FO_ANDROID
    AndroidBuild = true;
    #else
    AndroidBuild = false;
    #endif
    #ifdef FO_IOS
    IOsBuild = true;
    #else
    IOsBuild = false;
    #endif

    DesktopBuild = WindowsBuild || LinuxBuild || MacOsBuild;
    TabletBuild = AndroidBuild || IOsBuild;

    #ifdef FO_WINDOWS
    if( GetSystemMetrics( SM_TABLETPC ) != 0 )
    {
        DesktopBuild = false;
        TabletBuild = true;
    }
    #endif

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
    ProxyType = 0;
    ProxyHost = "";
    ProxyPort = 0;
    ProxyUser = "";
    ProxyPass = "";
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
    ScreenWidth = 1024;
    ScreenHeight = 768;
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

    struct StrWrapper
    {
        static string GetName( hash h )          { return _str().parseHash( h ); }
        static hash   GetHash( const string& s ) { return _str( s ).toHash(); }
    };
    GetNameByHash = &StrWrapper::GetName;
    GetHashByName = &StrWrapper::GetHash;

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
    SetLogCallback = [] ( void ( * func )( const char* ), bool enable ) {
        LogToFunc( "SetLogCallback", [ func ] ( const string &s ) { func( s.c_str() );
                   }, enable );
    };
    AddPropertyCallback = &::AddPropertyCallback;
}

TwoBitMask::TwoBitMask()
{
    memset( this, 0, sizeof( TwoBitMask ) );
}

TwoBitMask::TwoBitMask( uint width_2bit, uint height_2bit, uchar* ptr )
{
    if( !width_2bit )
        width_2bit = 1;
    if( !height_2bit )
        height_2bit = 1;

    width = width_2bit;
    height = height_2bit;
    widthBytes = width / 4;

    if( width % 4 )
        widthBytes++;

    if( ptr )
    {
        isAlloc = false;
        data = ptr;
    }
    else
    {
        isAlloc = true;
        data = new uchar[ widthBytes * height ];
        Fill( 0 );
    }
}

TwoBitMask::~TwoBitMask()
{
    if( isAlloc )
        delete[] data;
    data = nullptr;
}

void TwoBitMask::Set2Bit( uint x, uint y, int val )
{
    if( x >= width || y >= height )
        return;

    uchar& b = data[ y * widthBytes + x / 4 ];
    int    bit = ( x % 4 * 2 );

    UNSETFLAG( b, 3 << bit );
    SETFLAG( b, ( val & 3 ) << bit );
}

int TwoBitMask::Get2Bit( uint x, uint y )
{
    if( x >= width || y >= height )
        return 0;

    return ( data[ y * widthBytes + x / 4 ] >> ( x % 4 * 2 ) ) & 3;
}

void TwoBitMask::Fill( int fill )
{
    memset( data, fill, widthBytes * height );
}

uchar* TwoBitMask::GetData()
{
    return data;
}

void Randomizer::GenerateState()
{
    static unsigned int mag01[ 2 ] = { 0x0, 0x9908B0DF };
    unsigned int        num;
    int                 i = 0;

    for( ; i < periodN - periodM; i++ )
    {
        num = ( rndNumbers[ i ] & 0x80000000 ) | ( rndNumbers[ i + 1 ] & 0x7FFFFFFF );
        rndNumbers[ i ] = rndNumbers[ i + periodM ] ^ ( num >> 1 ) ^ mag01[ num & 0x1 ];
    }

    for( ; i < periodN - 1; i++ )
    {
        num = ( rndNumbers[ i ] & 0x80000000 ) | ( rndNumbers[ i + 1 ] & 0x7FFFFFFF );
        rndNumbers[ i ] = rndNumbers[ i + ( periodM - periodN ) ] ^ ( num >> 1 ) ^ mag01[ num & 0x1 ];
    }

    num = ( rndNumbers[ periodN - 1 ] & 0x80000000 ) | ( rndNumbers[ 0 ] & 0x7FFFFFFF );
    rndNumbers[ periodN - 1 ] = rndNumbers[ periodM - 1 ] ^ ( num >> 1 ) ^ mag01[ num & 0x1 ];

    rndIter = 0;
}

Randomizer::Randomizer()
{
    Generate();
}

Randomizer::Randomizer( unsigned int seed )
{
    Generate( seed );
}

void Randomizer::Generate()
{
    Generate( (unsigned int) time( nullptr ) );
}

void Randomizer::Generate( unsigned int seed )
{
    rndNumbers[ 0 ] = seed;

    for( int i = 1; i < periodN; i++ )
        rndNumbers[ i ] = ( 1812433253 * ( rndNumbers[ i - 1 ] ^ ( rndNumbers[ i - 1 ] >> 30 ) ) + i );

    GenerateState();
}

int Randomizer::Random( int minimum, int maximum )
{
    if( rndIter >= periodN )
        GenerateState();

    unsigned int num = rndNumbers[ rndIter++ ];
    num ^= ( num >> 11 );
    num ^= ( num << 7 ) & 0x9D2C5680;
    num ^= ( num << 15 ) & 0xEFC60000;
    num ^= ( num >> 18 );

    #ifdef FO_X86
    return minimum + (int) ( (double) num * (double) ( 1.0 / 4294967296.0 ) * (double) ( maximum - minimum + 1 ) );
    #else // FO_X64
    return minimum + (int) ( (int64) num * (int64) ( maximum - minimum + 1 ) / (int64) 0x100000000 );
    #endif
}

struct CmdDef
{
    char  Name[ 20 ];
    uchar Id;
};

static const CmdDef CmdList[] =
{
    { "exit", CMD_EXIT },
    { "myinfo", CMD_MYINFO },
    { "gameinfo", CMD_GAMEINFO },
    { "id", CMD_CRITID },
    { "move", CMD_MOVECRIT },
    { "disconnect", CMD_DISCONCRIT },
    { "toglobal", CMD_TOGLOBAL },
    { "prop", CMD_PROPERTY },
    { "getaccess", CMD_GETACCESS },
    { "additem", CMD_ADDITEM },
    { "additemself", CMD_ADDITEM_SELF },
    { "ais", CMD_ADDITEM_SELF },
    { "addnpc", CMD_ADDNPC },
    { "addloc", CMD_ADDLOCATION },
    { "reloadscripts", CMD_RELOADSCRIPTS },
    { "reloadclientscripts", CMD_RELOAD_CLIENT_SCRIPTS },
    { "rcs", CMD_RELOAD_CLIENT_SCRIPTS },
    { "runscript", CMD_RUNSCRIPT },
    { "run", CMD_RUNSCRIPT },
    { "reloadprotos", CMD_RELOAD_PROTOS },
    { "regenmap", CMD_REGENMAP },
    { "reloaddialogs", CMD_RELOADDIALOGS },
    { "loaddialog", CMD_LOADDIALOG },
    { "reloadtexts", CMD_RELOADTEXTS },
    { "settime", CMD_SETTIME },
    { "ban", CMD_BAN },
    { "deleteself", CMD_DELETE_ACCOUNT },
    { "changepassword", CMD_CHANGE_PASSWORD },
    { "changepass", CMD_CHANGE_PASSWORD },
    { "log", CMD_LOG },
    { "exec", CMD_DEV_EXEC },
    { "func", CMD_DEV_FUNC },
    { "gvar", CMD_DEV_GVAR },
};

bool PackNetCommand( const string& str, NetBuffer* pbuf, std::function< void(const string&) > logcb, const string& name )
{
    string args = _str( str ).trim();
    string cmd_str = args;
    size_t space = cmd_str.find( ' ' );
    if( space != string::npos )
    {
        cmd_str = args.substr( 0, space );
        args.erase( 0, cmd_str.length() );
    }
    istringstream args_str( args );

    uchar         cmd = 0;
    for( uint cur_cmd = 0; cur_cmd < sizeof( CmdList ) / sizeof( CmdDef ); cur_cmd++ )
        if( _str( cmd_str ).compareIgnoreCase( CmdList[ cur_cmd ].Name ) )
            cmd = CmdList[ cur_cmd ].Id;
    if( !cmd )
        return false;

    uint msg = NETMSG_SEND_COMMAND;
    uint msg_len = sizeof( msg ) + sizeof( msg_len ) + sizeof( cmd );

    RUNTIME_ASSERT( pbuf );
    NetBuffer& buf = *pbuf;

    switch( cmd )
    {
    case CMD_EXIT:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_MYINFO:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_GAMEINFO:
    {
        int type;
        if( !( args_str >> type ) )
        {
            logcb( "Invalid arguments. Example: gameinfo type." );
            break;
        }
        msg_len += sizeof( type );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << type;
    }
    break;
    case CMD_CRITID:
    {
        string name;
        if( !( args_str >> name ) )
        {
            logcb( "Invalid arguments. Example: id name." );
            break;
        }
        msg_len += NetBuffer::StringLenSize;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name;
    }
    break;
    case CMD_MOVECRIT:
    {
        uint   crid;
        ushort hex_x;
        ushort hex_y;
        if( !( args_str >> crid >> hex_x >> hex_y ) )
        {
            logcb( "Invalid arguments. Example: move crid hx hy." );
            break;
        }
        msg_len += sizeof( crid ) + sizeof( hex_x ) + sizeof( hex_y );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf << hex_x;
        buf << hex_y;
    }
    break;
    case CMD_DISCONCRIT:
    {
        uint crid;
        if( !( args_str >> crid ) )
        {
            logcb( "Invalid arguments. Example: disconnect crid." );
            break;
        }
        msg_len += sizeof( crid );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
    }
    break;
    case CMD_TOGLOBAL:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_PROPERTY:
    {
        uint   crid;
        string property_name;
        int    property_value;
        if( !( args_str >> crid >> property_name >> property_value ) )
        {
            logcb( "Invalid arguments. Example: prop crid prop_name value." );
            break;
        }
        msg_len += sizeof( uint ) + NetBuffer::StringLenSize + (uint) property_name.length() + sizeof( int );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << crid;
        buf << property_name;
        buf << property_value;
    }
    break;
    case CMD_GETACCESS:
    {
        string name_access;
        string pasw_access;
        if( !( args_str >> name_access >> pasw_access ) )
        {
            logcb( "Invalid arguments. Example: getaccess name password." );
            break;
        }
        name_access = _str( name_access ).replace( '*', ' ' );
        pasw_access = _str( pasw_access ).replace( '*', ' ' );
        msg_len += NetBuffer::StringLenSize * 2 + (uint) ( name_access.length() + pasw_access.length() );
        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name_access;
        buf << pasw_access;
    }
    break;
    case CMD_ADDITEM:
    {
        ushort hex_x;
        ushort hex_y;
        string proto_name;
        uint   count;
        if( !( args_str >> hex_x >> hex_y >> proto_name >> count ) )
        {
            logcb( "Invalid arguments. Example: additem hx hy name count." );
            break;
        }
        hash pid = _str( proto_name ).toHash();
        msg_len += sizeof( hex_x ) + sizeof( hex_y ) + sizeof( pid ) + sizeof( count );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << hex_x;
        buf << hex_y;
        buf << pid;
        buf << count;
    }
    break;
    case CMD_ADDITEM_SELF:
    {
        string proto_name;
        uint   count;
        if( !( args_str >> proto_name >> count ) )
        {
            logcb( "Invalid arguments. Example: additemself name count." );
            break;
        }
        hash pid = _str( proto_name ).toHash();
        msg_len += sizeof( pid ) + sizeof( count );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << pid;
        buf << count;
    }
    break;
    case CMD_ADDNPC:
    {
        ushort hex_x;
        ushort hex_y;
        uchar  dir;
        string proto_name;
        if( !( args_str >> hex_x >> hex_y >> dir >> proto_name ) )
        {
            logcb( "Invalid arguments. Example: addnpc hx hy dir name." );
            break;
        }
        hash pid = _str( proto_name ).toHash();
        msg_len += sizeof( hex_x ) + sizeof( hex_y ) + sizeof( dir ) + sizeof( pid );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << hex_x;
        buf << hex_y;
        buf << dir;
        buf << pid;
    }
    break;
    case CMD_ADDLOCATION:
    {
        ushort wx;
        ushort wy;
        string proto_name;
        if( !( args_str >> wx >> wy >> proto_name ) )
        {
            logcb( "Invalid arguments. Example: addloc wx wy name." );
            break;
        }
        hash pid = _str( proto_name ).toHash();
        msg_len += sizeof( wx ) + sizeof( wy ) + sizeof( pid );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << wx;
        buf << wy;
        buf << pid;
    }
    break;
    case CMD_RELOADSCRIPTS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RELOAD_CLIENT_SCRIPTS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RUNSCRIPT:
    {
        string func_name;
        uint   param0, param1, param2;
        if( !( args_str >> func_name >> param0 >> param1 >> param2 ) )
        {
            logcb( "Invalid arguments. Example: runscript module::func param0 param1 param2." );
            break;
        }
        msg_len += NetBuffer::StringLenSize + (uint) func_name.length() + sizeof( uint ) * 3;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << func_name;
        buf << param0;
        buf << param1;
        buf << param2;
    }
    break;
    case CMD_RELOAD_PROTOS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_REGENMAP:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_RELOADDIALOGS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_LOADDIALOG:
    {
        string dlg_name;
        if( !( args_str >> dlg_name ) )
        {
            logcb( "Invalid arguments. Example: loaddialog name." );
            break;
        }
        msg_len += NetBuffer::StringLenSize + (uint) dlg_name.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << dlg_name;
    }
    break;
    case CMD_RELOADTEXTS:
    {
        buf << msg;
        buf << msg_len;
        buf << cmd;
    }
    break;
    case CMD_SETTIME:
    {
        int multiplier;
        int year;
        int month;
        int day;
        int hour;
        int minute;
        int second;
        if( !( args_str >> multiplier >> year >> month >> day >> hour >> minute >> second ) )
        {
            logcb( "Invalid arguments. Example: settime tmul year month day hour minute second." );
            break;
        }
        msg_len += sizeof( multiplier ) + sizeof( year ) + sizeof( month ) + sizeof( day ) + sizeof( hour ) + sizeof( minute ) + sizeof( second );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << multiplier;
        buf << year;
        buf << month;
        buf << day;
        buf << hour;
        buf << minute;
        buf << second;
    }
    break;
    case CMD_BAN:
    {
        string params;
        string name;
        uint   ban_hours;
        string info;
        args_str >> params;
        if( !args_str.fail() )
            args_str >> name;
        if( !args_str.fail() )
            args_str >> ban_hours;
        if( !args_str.fail() )
            info = args_str.str();
        if( !_str( params ).compareIgnoreCase( "add" ) && !_str( params ).compareIgnoreCase( "add+" ) &&
            !_str( params ).compareIgnoreCase( "delete" ) && !_str( params ).compareIgnoreCase( "list" ) )
        {
            logcb( "Invalid arguments. Example: ban [add,add+,delete,list] [user] [hours] [comment]." );
            break;
        }
        name = _str( name ).replace( '*', ' ' ).trim();
        info = _str( info ).replace( '$', '*' ).trim();
        msg_len += NetBuffer::StringLenSize * 3 + (uint) ( name.length() + params.length() + info.length() ) + sizeof( ban_hours );

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << name;
        buf << params;
        buf << ban_hours;
        buf << info;
    }
    break;
    case CMD_DELETE_ACCOUNT:
    {
        if( name.empty() )
        {
            logcb( "Can't execute this command." );
            break;
        }

        string pass;
        if( !( args_str >> pass ) )
        {
            logcb( "Invalid arguments. Example: deleteself user_password." );
            break;
        }
        pass = _str( pass ).replace( '*', ' ' );
        string pass_hash = Crypt.ClientPassHash( name, pass );
        msg_len += PASS_HASH_SIZE;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( pass_hash.c_str(), PASS_HASH_SIZE );
    }
    break;
    case CMD_CHANGE_PASSWORD:
    {
        if( name.empty() )
        {
            logcb( "Can't execute this command." );
            break;
        }

        string pass;
        string new_pass;
        if( !( args_str >> pass >> new_pass ) )
        {
            logcb( "Invalid arguments. Example: changepassword current_password new_password." );
            break;
        }
        pass = _str( pass ).replace( '*', ' ' );

        // Check the new password's validity
        uint pass_len = _str( new_pass ).lengthUtf8();
        if( pass_len < MIN_NAME || pass_len < GameOpt.MinNameLength || pass_len > MAX_NAME || pass_len > GameOpt.MaxNameLength )
        {
            logcb( "Invalid new password." );
            break;
        }

        string pass_hash = Crypt.ClientPassHash( name, pass );
        new_pass = _str( new_pass ).replace( '*', ' ' );
        string new_pass_hash = Crypt.ClientPassHash( name, new_pass );
        msg_len += PASS_HASH_SIZE * 2;

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf.Push( pass_hash.c_str(), PASS_HASH_SIZE );
        buf.Push( new_pass_hash.c_str(), PASS_HASH_SIZE );
    }
    break;
    case CMD_LOG:
    {
        string flags;
        if( !( args_str >> flags ) )
        {
            logcb( "Invalid arguments. Example: log flag. Valid flags: '+' attach, '-' detach, '--' detach all." );
            break;
        }
        msg_len += NetBuffer::StringLenSize + (uint) flags.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << flags;
    }
    break;
    case CMD_DEV_EXEC:
    case CMD_DEV_FUNC:
    case CMD_DEV_GVAR:
    {
        if( args.empty() )
            break;

        msg_len += NetBuffer::StringLenSize + (uint) args.length();

        buf << msg;
        buf << msg_len;
        buf << cmd;
        buf << args;
    }
    break;
    default:
        return false;
    }

    return true;
}
