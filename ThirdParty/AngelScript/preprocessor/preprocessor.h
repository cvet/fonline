/*
   Preprocessor 0.7
   Copyright (c) 2005 Anthony Casteel
   Copyright (c) 2015 Anton "Cvet" Tsvetinsky, Grzegorz "Atom" Jagiella

   This software is provided 'as-is', without any express or implied
   warranty. In no event will the authors be held liable for any
   damages arising from the use of this software.

   Permission is granted to anyone to use this software for any
   purpose, including commercial applications, and to alter it and
   redistribute it freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you
      must not claim that you wrote the original software. If you use
      this software in a product, an acknowledgment in the product
      documentation would be appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and
      must not be misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
      distribution.

   The original version of this library can be located at:
   http://www.angelcode.com/angelscript/
   under addons & utilities or at
   http://www.omnisu.com

   Anthony Casteel
   jm@omnisu.com
 */

/*
 * This version has been modified and improved by Anton "Cvet" Tsvetinsky and Rotators team.
 * http://github.com/rotators/angelscript-preprocessor/
 */

#pragma once

#include <stdio.h>
#include <string>
#include <sstream>
#include <vector>

#define PREPROCESSOR_VERSION_STRING    "0.7"

namespace Preprocessor
{
    /************************************************************************/
    /* Preprocess                                                           */
    /************************************************************************/

    class Context;
    class OutStream;
    class FileLoader;
    class PragmaCallback;
    class LineNumberTranslator;
    class PragmaInstance;

    Context* CreateContext();
    void     DeleteContext( Context* ctx ) noexcept;

    // Preprocess
    int Preprocess( Context* ctx, std::string file_path, OutStream& result, OutStream* errors = NULL, FileLoader* loader = NULL );

    // Pre preprocess settings
    void Define( Context* ctx, const std::string& str );
    void Undef( Context* ctx, const std::string& str );
    void UndefAll( Context* ctx );
    bool IsDefined( Context* ctx, const std::string& str, std::string* value = NULL );
    void SetPragmaCallback( Context* ctx, PragmaCallback* callback );
    void CallPragma( Context* ctx, const PragmaInstance& pragma );

    // Post preprocess settings
    LineNumberTranslator* GetLineNumberTranslator( Context* ctx );
    void                  DeleteLineNumberTranslator( LineNumberTranslator* lnt );
    void                  StoreLineNumberTranslator( LineNumberTranslator* lnt, std::vector< unsigned char >& data );
    LineNumberTranslator* RestoreLineNumberTranslator( const std::vector< unsigned char >& data );
    const std::string&    ResolveOriginalFile( Context* ctx, unsigned int line_number );
    unsigned int          ResolveOriginalLine( Context* ctx, unsigned int line_number );
    const std::string&    ResolveOriginalFile( unsigned int line_number, LineNumberTranslator* lnt );
    unsigned int          ResolveOriginalLine( unsigned int line_number, LineNumberTranslator* lnt );

    /************************************************************************/
    /* Streams                                                              */
    /************************************************************************/

    class OutStream
    {
public:
        virtual ~OutStream() {}
        virtual void Write( const char*, size_t ) {}

        OutStream& operator<<( const std::string& in )
        {
            Write( in.c_str(), in.length() );
            return *this;
        }
        OutStream& operator<<( const char* in )
        {
            return operator<<( std::string( in ) );
        }
        template< typename T >
        OutStream& operator<<( const T& in )
        {
            std::stringstream strstr;
            strstr << in;
            std::string       str;
            strstr >> str;
            Write( str.c_str(), str.length() );
            return *this;
        }
    };

    class StringOutStream: public OutStream
    {
public:
        std::string String;

        virtual ~StringOutStream() {}
        virtual void Write( const char* str, size_t len )
        {
            String.append( str, len );
        }
    };

    /************************************************************************/
    /* Loader                                                               */
    /************************************************************************/

    class FileLoader
    {
public:
        virtual ~FileLoader() {}
        virtual bool LoadFile( const std::string& dir, const std::string& file_name, std::vector< char >& data, std::string& file_path );
        virtual void FileLoaded() {}
    };

    /************************************************************************/
    /* Pragmas                                                              */
    /************************************************************************/

    class PragmaInstance
    {
public:
        std::string  Name;
        std::string  Text;
        std::string  CurrentFile;
        unsigned int CurrentFileLine;
        std::string  RootFile;
        unsigned int GlobalLine;
    };

    class PragmaCallback
    {
public:
        virtual ~PragmaCallback() {}
        virtual void CallPragma( const PragmaInstance& pragma ) = 0;
    };
};
