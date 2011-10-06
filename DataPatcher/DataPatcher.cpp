#include "stdafx.h"
#include "..\Source\SHA2\sha2.h"

void setPassword( const char* client, const char* password );
void patchSaves();
vector< string > getFiles( const char* query );
void swapChar( char* str, char from, char to );
void setCurrentPath( int path, const char* pathAdd ); // path: 0 - server, 1 - client
void xor( char* data, int len, char* xorKey, int xorLen );
void decryptPassword( char* data, int len, unsigned int key );
void clientPassHash( const char* name, const char* pass, char* pass_hash );

int main( int argc, char* argv[] )
{
    // Locale
    setlocale( LC_ALL, "Russian" );

    // Help
    printf( "FOnline save patcher v.1.2\n" );
    printf( "Commands:\n" );
    printf( " setPassword <clientName> <newPassword>\n  - change password for client (use '*' instead spaces)\n" );
    printf( " patchSaves\n  - patch all client files to actual state\n" );
    printf( " exit\n  - close program\n" );

    // Command loop
    while( true )
    {
        printf( ">" );
        char cmd[ 1024 ];
        scanf( "%s", cmd );
        if( !_stricmp( cmd, "setPassword" ) )
        {
            char clientName[ 1024 ];
            char newPassword[ 1024 ];
            scanf( "%s%s", clientName, newPassword );
            OemToAnsi(clientName, clientName);
            OemToAnsi(newPassword, newPassword);
            swapChar( clientName, '*', ' ' );
            swapChar( newPassword, '*', ' ' );
            setCurrentPath( 0, "save\\clients\\" );
            setPassword( clientName, newPassword );
        }
        else if( !_stricmp( cmd, "patchSaves" ) )
        {
            setCurrentPath( 0, "save\\clients\\" );
            patchSaves();
        }
        else if( !_stricmp( cmd, "exit" ) )
        {
            break;
        }
    }

	return 0;
}

void setPassword( const char* client, const char* password )
{
    char fileName[ 1024 ];
    sprintf( fileName, "%s.client", client );

    WIN32_FIND_DATA fd;
    auto h = FindFirstFile( fileName, &fd );
    if( h != INVALID_HANDLE_VALUE )
    {
        FindClose( h );

        FILE* f = fopen( fileName, "r+b" );
        char signature[ 4 ];
        fread( signature, sizeof( signature ), 1, f );
        if( signature[ 0 ] == 'F' && signature[ 1 ] == 'O' && signature[ 2 ] == 0 )
        {
            char name[ 1024 ];
            strcpy( name, fd.cFileName );
            *strstr( name, ".client" ) = 0;
            char hashPassword[ 32 ];
            clientPassHash( name, password, hashPassword );
            fseek( f, 4, SEEK_SET );
            fwrite( hashPassword, sizeof( hashPassword ), 1, f );

            printf( "Password changed\n" );
        }
        else
        {
            printf( "Wrong file format, try to 'patch' first\n" );
        }
        fclose( f );
    }
    else
    {
        printf( "File '%s' not found\n", fileName );
    }
}

void patchSaves()
{
    vector< string > files = getFiles( "*.client" );
    for( auto it = files.begin(); it != files.end(); it++ )
    {
        printf( "Patch %s...", it->c_str() );
        char fileName[ 1024 ];
        sprintf( fileName, "%s", it->c_str() );

        FILE* f = fopen( fileName, "rb" );
        if( f )
        {
            fseek( f, 0, SEEK_END );
            int fileSize = ftell( f );
            fseek( f, 0, SEEK_SET );

            char signature[ 4 ];
            if( fread( signature, sizeof( signature ), 1, f ) )
            {
                const int actualVersion = 2;

                // Old format
                if( !( signature[ 0 ] == 'F' && signature[ 1 ] == 'O' && signature[ 2 ] == 0 ) )
                {
                    // Get data
                    char* fileData = new char[ fileSize + 1 ];
                    fseek( f, 0, SEEK_SET );
                    fread( fileData, 1, fileSize, f );
                    fclose( f );

                    // Convert old passwords
                    unsigned int& oldPasswordKey = *(unsigned int*) &fileData[ 4307 ];
                    if( oldPasswordKey )
                    {
                        char oldPassword[ 31 ];
                        memcpy( oldPassword, fileData, 31 );
                        decryptPassword( oldPassword, 31, *(unsigned int*) &fileData[ 4307 ] );
                        if( oldPassword[ 0 ]  && !oldPassword[ 27 ] && !oldPassword[ 28 ] && !oldPassword[ 29 ] && !oldPassword[ 30 ] )
                            memcpy( fileData, oldPassword, 31 );
                        oldPasswordKey = 0;
                    }
                    if( fileData[ 0 ] && !fileData[ 27 ] && !fileData[ 28 ] && !fileData[ 29 ] && !fileData[ 30 ] )
                    {
                        char oldPassword[ 31 ];
                        strcpy( oldPassword, fileData );
                        fileSize++;
                        for( int i = fileSize; i > 0; i-- )
                            fileData[ i ] = fileData[ i - 1 ];
                        memset( fileData, 0, 32 );
                        char name[ 1024 ];
                        strcpy( name, it->c_str() );
                        *strstr( name, ".client" ) = 0;
                        clientPassHash( name, oldPassword, fileData );
                        strcpy( fileData, oldPassword );
                    }

                    // Write new format
                    f = fopen( fileName, "wb" );
                    char signature[ 4 ] = { 'F', 'O', 0, actualVersion };
                    fwrite( signature, sizeof( signature ), 1, f );
                    fwrite( fileData, 1, fileSize, f );
                    delete[] fileData;

                    printf( "done\n" );
                }
                // Version 1
                else if( signature[ 3 ] == 1 )
                {
                    // Add extra zero byte
                    char* fileData = new char[ fileSize + 1 ];
                    fseek( f, 0, SEEK_SET );
                    fread( fileData, 1, fileSize, f );
                    fclose( f );
                    fileData[ fileSize ] = 0;
                    fileSize++;
                    f = fopen( fileName, "wb" );
                    fileData[ 3 ] = actualVersion;
                    fwrite( fileData, 1, fileSize, f );
                    delete[] fileData;

                    printf( "done\n" );
                }
                // Actual state
                else
                {
                    printf( "already patched\n" );
                }
            }
            else
            {
                printf( "can't read file\n" );
            }

            fclose( f );
        }
        else
        {
            printf( "can't open file\n" );
        }
    }

    printf( "Files patched\n" );
}

vector< string > getFiles( const char* query )
{
    vector< string > files;

    WIN32_FIND_DATA fd;
    auto h = FindFirstFile( query, &fd );
    while( h != INVALID_HANDLE_VALUE )
    {
        if( !( fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) )
            files.push_back( fd.cFileName );

        if( !FindNextFile( h, &fd ) )
            break;
    }
    FindClose( h );

    return files;
}

void swapChar( char* str, char from, char to )
{
    while( *str )
    {
        if( *str == from )
            *str = to;
        str++;
    }
}

void setCurrentPath( int path, const char* pathAdd ) // path: 0 - server, 1 - client
{
    // Work directory
    char pathStr[ 1024 ];
    const char* key = path == 0 ? "ServerPath" : "ClientPath";
    GetPrivateProfileString( "Options", key, ".\\", pathStr, 1024, ".\\DataPatcher.cfg" );
    if( pathStr[ strlen( pathStr ) - 1 ] != '\\' )
        pathStr[ strlen( pathStr ) - 1 ] = '\\';
    if( pathAdd )
        strcat( pathStr, pathAdd );
    SetCurrentDirectory( pathStr );
}

void xor( char* data, int len, char* xorKey, int xorLen )
{
    for( int i = 0, k = 0; i < len; ++i, ++k )
    {
        if( k >= xorLen )
            k = 0;
        data[ i ] ^= xorKey[ k ];
    }
}

void decryptPassword( char* data, int len, unsigned int key )
{
    for( int i = 10; i < len + 10; i++ )
        xor( &data[ i - 10 ], 1, (char*) &i, 1 );
    xor( data, len, (char*) &key, sizeof( key ) );
    for( int i = 0; i < ( len - 1 ) / 2; i++ )
        swap( data[ i ], data[ len - 1 - i ] );
    int slen = data[ len - 1 ];
    for( int i = slen; i < len; i++ )
        data[ i ] = 0;
    data[ len - 1 ] = 0;
}

void clientPassHash( const char* name, const char* pass, char* pass_hash )
{
    const int MAX_NAME = 30;
    char* bld = new char[ MAX_NAME + 1 ];
    memset( bld, 0, MAX_NAME + 1 );
    int passLen = strlen( pass );
    int nameLen = strlen( name );
    if( passLen > MAX_NAME )
        passLen = MAX_NAME;
    strcpy( bld, pass );
    if( passLen < MAX_NAME )
        bld[ passLen++ ] = '*';
    if( nameLen )
    {
        for( ; passLen < MAX_NAME; passLen++ )
            bld[ passLen ] = tolower( name[ passLen % nameLen ] );
    }
    sha256( (const unsigned char*) bld, MAX_NAME, (unsigned char*) pass_hash );
    delete[] bld;
}
