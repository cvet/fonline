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

#include "preprocessor.h"
#include <list>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string.h>

namespace Preprocessor
{
    /************************************************************************/
    /* Line number translator                                               */
    /************************************************************************/

    class LineNumberTranslator
    {
public:
        struct Entry
        {
            std::string  File;
            unsigned int StartLine;
            unsigned int Offset;
        };

        std::vector< Entry > lines;

        Entry& Search( unsigned int linenumber );
        void   AddLineRange( const std::string& file, unsigned int start_line, unsigned int offset );
        void   Store( std::vector< unsigned char >& data );
        void   Restore( const std::vector< unsigned char >& data );
    };

    /************************************************************************/
    /* Lexems                                                               */
    /************************************************************************/

    enum LexemType
    {
        IDENTIFIER,             // Names which can be expanded.
        COMMA,                  // ,
        SEMICOLON,
        OPEN,                   // {[(
        CLOSE,                  // }])
        PREPROCESSOR,           // Begins with #
        NEWLINE,
        WHITESPACE,
        IGNORE,
        COMMENT,
        STRING,
        NUMBER,
        BACKSLASH
    };

    class Lexem
    {
public:
        std::string Value;
        LexemType   Type;
    };

    const std::string Numbers = "0123456789";
    const std::string IdentifierStart = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const std::string IdentifierBody = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const std::string HexNumbers = "0123456789abcdefABCDEF";
    const std::string Trivials = ",;\n\r\t [{(]})";

    const LexemType   TrivialTypes[] =
    {
        COMMA,
        SEMICOLON,
        NEWLINE,
        WHITESPACE,
        WHITESPACE,
        WHITESPACE,
        OPEN,
        OPEN,
        OPEN,
        CLOSE,
        CLOSE,
        CLOSE
    };

    typedef std::list< Lexem >  LexemList;
    typedef LexemList::iterator LLITR;

    std::string IntToString( int i );
    bool        SearchString( std::string str, char in );
    bool        IsNumber( char in );
    bool        IsIdentifierStart( char in );
    bool        IsIdentifierBody( char in );
    bool        IsTrivial( char in );
    bool        IsHex( char in );
    char*       ParseIdentifier( char* start, char* end, Lexem& out );
    char*       ParseStringLiteral( char* start, char* end, char quote, Lexem& out );
    char*       ParseCharacterLiteral( char* start, char* end, Lexem& out );
    char*       ParseFloatingPoint( char* start, char* end, Lexem& out );
    char*       ParseHexConstant( char* start, char* end, Lexem& out );
    char*       ParseNumber( char* start, char* end, Lexem& out );
    char*       ParseBlockComment( char* start, char* end, Lexem& out );
    char*       ParseLineComment( char* start, char* end, Lexem& out );
    char*       ParseLexem( char* start, char* end, Lexem& out );
    int         Lex( char* begin, char* end, std::list< Lexem >& results );

    /************************************************************************/
    /* Define table                                                         */
    /************************************************************************/

    typedef std::map< std::string, int > ArgSet;

    class DefineEntry
    {
public:
        LexemList Lexems;
        ArgSet    Arguments;
    };

    typedef std::map< std::string, DefineEntry > DefineTable;

    /************************************************************************/
    /* Expressions                                                          */
    /************************************************************************/

    void PreprocessLexem( LLITR it, LexemList& lexems );
    bool IsOperator( const Lexem& lexem );
    bool IsIdentifier( const Lexem& lexem );
    bool IsLeft( const Lexem& lexem );
    bool IsRight( const Lexem& lexem );
    int  OperPrecedence( const Lexem& lexem );
    bool OperLeftAssoc( const Lexem& lexem );

    /************************************************************************/
    /* Preprocess                                                           */
    /************************************************************************/

    void        PrintMessage( Context* ctx, const std::string& msg );
    void        PrintErrorMessage( Context* ctx, const std::string& errmsg );
    void        PrintWarningMessage( Context* ctx, const std::string& warnmsg );
    std::string RemoveQuotes( Context* ctx, const std::string& in );
    void        SetPragmaCallback( Context* ctx, PragmaCallback* callback );
    void        CallPragma( Context* ctx, const PragmaInstance& pragma );
    std::string PrependRootPath( Context* ctx, const std::string& filename );
    LLITR       ParsePreprocessor( Context* ctx, LexemList& lexems, LLITR itr, LLITR end );
    LLITR       ParseStatement( Context* ctx, LLITR itr, LLITR end, LexemList& dest );
    LLITR       ParseDefineArguments( Context* ctx, LLITR itr, LLITR end, LexemList& lexems, std::vector< LexemList >& args );
    LLITR       ExpandDefine( Context* ctx, LLITR itr, LLITR ent, LexemList& lexems, DefineTable& define_table );
    void        ParseDefine( Context* ctx, DefineTable& define_table, LexemList& def_lexems );
    LLITR       ParseIfDef( Context* ctx, LLITR itr, LLITR end );
    void        ParseIf( Context* ctx, LexemList& directive, std::string& name_out );
    void        ParseUndef( Context* ctx, DefineTable& define_table, const std::string& identifier );
    bool        ConvertExpression( Context* ctx, LexemList& expression, LexemList& output );
    int         EvaluateConvertedExpression( Context* ctx, DefineTable& define_table, LexemList& expr );
    bool        EvaluateExpression( Context* ctx, DefineTable& define_table, LexemList& directive );
    std::string AddPaths( Context* ctx, const std::string& first, const std::string& second );
    void        ParsePragma( Context* ctx, LexemList& args );
    void        ParseTextLine( Context* ctx, LexemList& directive, std::string& message );
    void        SetLineMacro( Context* ctx, DefineTable& define_table, unsigned int line );
    void        SetFileMacro( Context* ctx, DefineTable& define_table, const std::string& file );
    void        RecursivePreprocess( Context* ctx, std::string filename, FileLoader& file_source, LexemList& lexems, DefineTable& define_table );
    void        PrintLexemList(Context* ctx, LexemList& out, OutStream& destination );

    class Context
    {
    public:
        DefineTable                CustomDefines;
        OutStream*                 Errors = NULL;
        unsigned int               ErrorsCount = 0;
        PragmaCallback*            CurPragmaCallback = NULL;
        LineNumberTranslator*      LNT = NULL;
        std::string                RootFile;
        std::string                RootPath;
        std::string                CurrentFile;
        unsigned int               CurrentLine = 0;
        unsigned int               LinesThisFile = 0;
        std::vector< std::string > FilesPreprocessed;
    };
}

/************************************************************************/
/* Line number translator                                               */
/************************************************************************/

Preprocessor::LineNumberTranslator::Entry& Preprocessor::LineNumberTranslator::Search( unsigned int line_number )
{
    for( size_t i = 1; i < lines.size(); ++i )
    {
        if( line_number < lines[ i ].StartLine )
        {
            // Found the first block after our line
            return lines[ i - 1 ];
        }
    }
    return lines.back();     // Line must be in last block
}

void Preprocessor::LineNumberTranslator::AddLineRange( const std::string& file, unsigned int start_line, unsigned int offset )
{
    Entry e;
    e.File = file;
    e.StartLine = start_line;
    e.Offset = offset;
    lines.push_back( e );
}

void Preprocessor::LineNumberTranslator::Store( std::vector< unsigned char >& data )
{
    // Calculate size
    size_t size = sizeof( unsigned int );
    for( size_t i = 0; i < lines.size(); i++ )
    {
        Entry& entry = lines[ i ];
        size += sizeof( unsigned int ) * 3 + entry.File.length();
    }

    // Write data
    data.resize( size );
    unsigned char* cur = &data[ 0 ];
    unsigned int   tmp;
    memcpy( cur, &( tmp = (unsigned int) lines.size() ), sizeof( unsigned int ) );
    cur += sizeof( unsigned int );
    for( size_t i = 0; i < lines.size(); i++ )
    {
        Entry& entry = lines[ i ];
        memcpy( cur, &( tmp = (unsigned int) entry.File.length() ), sizeof( unsigned int ) );
        cur += sizeof( unsigned int );
        memcpy( cur, entry.File.c_str(), entry.File.length() );
        cur += entry.File.length();
        memcpy( cur, &( tmp = (unsigned int) entry.StartLine ), sizeof( unsigned int ) );
        cur += sizeof( unsigned int );
        memcpy( cur, &( tmp = (unsigned int) entry.Offset ), sizeof( unsigned int ) );
        cur += sizeof( unsigned int );
    }
}

void Preprocessor::LineNumberTranslator::Restore( const std::vector< unsigned char >& data )
{
    // Read data
    const unsigned char* cur = &data[ 0 ];
    unsigned int         count;
    memcpy( &count, cur, sizeof( unsigned int ) );
    cur += sizeof( unsigned int );
    lines.resize( count );
    for( size_t i = 0; i < lines.size(); i++ )
    {
        Entry& entry = lines[ i ];
        memcpy( &count, cur, sizeof( unsigned int ) );
        entry.File.resize( count );
        cur += sizeof( unsigned int );
        memcpy( &entry.File[ 0 ], cur, entry.File.length() );
        cur += entry.File.length();
        memcpy( &entry.StartLine, cur, sizeof( unsigned int ) );
        cur += sizeof( unsigned int );
        memcpy( &entry.Offset, cur, sizeof( unsigned int ) );
        cur += sizeof( unsigned int );
    }
}

/************************************************************************/
/* Preprocess                                                           */
/************************************************************************/

void Preprocessor::PrintMessage( Context* ctx, const std::string& msg )
{
    ( *ctx->Errors ) << ctx->CurrentFile << " (" << ctx->LinesThisFile << ") " << msg << "\n";
}

void Preprocessor::PrintErrorMessage( Context* ctx, const std::string& errmsg )
{
    std::string err = "Error";
    if( !( errmsg.empty() || errmsg.length() == 0 ) )
    {
        err += ": ";
        err += errmsg;
    }

    PrintMessage( ctx, err );
    ctx->ErrorsCount++;
}

void Preprocessor::PrintWarningMessage( Context* ctx, const std::string& warnmsg )
{
    std::string warn = "Warning";
    if( !( warnmsg.empty() || warnmsg.length() == 0 ) )
    {
        warn += ": ";
        warn += warnmsg;
    }

    PrintMessage( ctx, warn );
}

std::string Preprocessor::RemoveQuotes( Context* ctx, const std::string& in )
{
    return in.substr( 1, in.size() - 2 );
}

void Preprocessor::SetPragmaCallback( Context* ctx, PragmaCallback* callback )
{
    ctx->CurPragmaCallback = callback;
}

void Preprocessor::CallPragma( Context* ctx, const PragmaInstance& pragma )
{
    if( ctx->CurPragmaCallback )
        ctx->CurPragmaCallback->CallPragma( pragma );
}

std::string Preprocessor::PrependRootPath( Context* ctx, const std::string& filename )
{
    if( filename == ctx->RootFile )
        return ctx->RootFile;
    return ctx->RootPath + filename;
}

Preprocessor::LLITR Preprocessor::ParsePreprocessor( Context* ctx, LexemList& lexems, LLITR itr, LLITR end )
{
    unsigned int spaces = 0;
    LLITR        prev = itr;
    while( itr != end )
    {
        if( itr->Type == NEWLINE )
        {
            if( prev == itr || prev->Type != BACKSLASH )
                break;
            itr->Type = WHITESPACE;
            itr->Value = " ";
            spaces++;
        }
        if (itr->Type != WHITESPACE)
			prev = itr;
        ++itr;
    }
    if( spaces )
    {
        Lexem newline;
        newline.Type = NEWLINE;
        newline.Value = "\n";
        prev = itr;
        itr++;
        while( spaces-- )
            lexems.insert( itr, newline );
        return prev;
    }
    return itr;
}

Preprocessor::LLITR Preprocessor::ParseStatement( Context* ctx, LLITR itr, LLITR end, LexemList& dest )
{
    int depth = 0;
    while( itr != end )
    {
        if( itr->Value == "," && depth == 0 )
            return itr;
        if( itr->Type == CLOSE && depth == 0 )
            return itr;
        if( itr->Type == SEMICOLON && depth == 0 )
            return itr;
        dest.push_back( *itr );
        if( itr->Type == OPEN )
            depth++;
        if( itr->Type == CLOSE )
        {
            if( depth == 0 )
                PrintErrorMessage( ctx, "Mismatched braces while parsing statement." );
            depth--;
        }
        ++itr;
    }
    return itr;
}

Preprocessor::LLITR Preprocessor::ParseDefineArguments( Context* ctx, LLITR itr, LLITR end, LexemList& lexems, std::vector< LexemList >& args )
{
    if( itr == end || itr->Value != "(" )
    {
        PrintErrorMessage( ctx, "Expected argument list." );
        return itr;
    }
    LLITR begin_erase = itr;
    ++itr;

    while( itr != end )
    {
        LexemList argument;
        LLITR     prev = itr;
        itr = ParseStatement( ctx, itr, end, argument );
        if( itr == prev )
            return itr;

        args.push_back( argument );

        if( itr == end )
        {
            PrintErrorMessage( ctx, "0x0FA1 Unexpected end of file." );
            return itr;
        }
        if( itr->Value == "," )
        {
            ++itr;
            if( itr == end )
            {
                PrintErrorMessage( ctx, "0x0FA2 Unexpected end of file." );
                return itr;
            }
            continue;
        }
        if( itr->Value == ")" )
        {
            ++itr;
            break;
        }
    }

    return lexems.erase( begin_erase, itr );
}

Preprocessor::LLITR Preprocessor::ExpandDefine( Context* ctx, LLITR itr, LLITR end, LexemList& lexems, DefineTable& define_table )
{
    DefineTable::iterator define_entry = define_table.find( itr->Value );
    if( define_entry == define_table.end() )
        return ++itr;

    const auto itr_value = itr->Value;

    itr = lexems.erase( itr );

    if( define_entry->second.Arguments.size() == 0 )
    {
        return lexems.insert( itr,
                       define_entry->second.Lexems.begin(),
                       define_entry->second.Lexems.end() );
    }

    // define has arguments.
    std::vector< LexemList > arguments;
    itr = ParseDefineArguments( ctx, itr, end, lexems, arguments );

    if( define_entry->second.Arguments.size() != arguments.size() )
    {
        PrintErrorMessage( ctx, "Didn't supply right number of arguments to define '" + itr_value + "'." );
        return end;
    }

    LexemList temp_list( define_entry->second.Lexems.begin(), define_entry->second.Lexems.end() );

    LLITR     tli = temp_list.begin();
    while( tli != temp_list.end() )
    {
        ArgSet::iterator arg = define_entry->second.Arguments.find( tli->Value );
        if( arg == define_entry->second.Arguments.end() )
        {
            ++tli;
            continue;
        }

        tli = temp_list.erase( tli );
        temp_list.insert( tli, arguments[ arg->second ].begin(), arguments[ arg->second ].end() );
    }

    return lexems.insert( itr, temp_list.begin(), temp_list.end() );
    // expand arguments in templist.
}

void Preprocessor::ParseDefine( Context* ctx, DefineTable& define_table, LexemList& def_lexems )
{
    def_lexems.pop_front();     // remove #define directive
    if( def_lexems.empty() )
    {
        PrintErrorMessage( ctx, "Define directive without arguments." );
        return;
    }
    Lexem name = *def_lexems.begin();
    if( name.Type != IDENTIFIER )
    {
        PrintErrorMessage( ctx, "Define's name was not an identifier." );
        return;
    }
    def_lexems.pop_front();

    while( !def_lexems.empty() )
    {
        LexemType lexem_type = def_lexems.begin()->Type;
        if( lexem_type == BACKSLASH || lexem_type == NEWLINE || lexem_type == WHITESPACE )
            def_lexems.pop_front();
        else
            break;
    }

    DefineEntry def;
    if( !def_lexems.empty() )
    {
        if( def_lexems.begin()->Type == PREPROCESSOR && def_lexems.begin()->Value == "#" )
        {
            // Macro has arguments
            def_lexems.pop_front();
            if( def_lexems.empty() )
            {
                PrintErrorMessage( ctx, "Expected arguments." );
                return;
            }
            if( def_lexems.begin()->Value != "(" )
            {
                PrintErrorMessage( ctx, "Expected arguments." );
                return;
            }
            def_lexems.pop_front();

            int num_args = 0;
            while( !def_lexems.empty() && def_lexems.begin()->Value != ")" )
            {
                if( def_lexems.begin()->Type != IDENTIFIER )
                {
                    PrintErrorMessage( ctx, "Expected identifier." );
                    return;
                }
                def.Arguments[ def_lexems.begin()->Value ] = num_args;
                def_lexems.pop_front();
                if( !def_lexems.empty() && def_lexems.begin()->Value == "," )
                {
                    def_lexems.pop_front();
                }
                num_args++;
            }

            if( !def_lexems.empty() )
            {
                if( def_lexems.begin()->Value != ")" )
                {
                    PrintErrorMessage( ctx, "Expected closing parantheses." );
                    return;
                }
                def_lexems.pop_front();
            }
            else
            {
                PrintErrorMessage( ctx, "0x0FA3 Unexpected end of line." );
                return;
            }
        }

        LLITR dlb = def_lexems.begin();
        while( dlb != def_lexems.end() )
        {
            if( dlb->Value == "##" && dlb->Type == IGNORE )
                dlb->Value = "";
            dlb = ExpandDefine( ctx, dlb, def_lexems.end(), def_lexems, define_table );
        }
    }

    def.Lexems = def_lexems;
    define_table[ name.Value ] = def;
}

Preprocessor::LLITR Preprocessor::ParseIfDef( Context* ctx, LLITR itr, LLITR end )
{
    int  depth = 0;
    int  newlines = 0;
    bool found_end = false;
    while( itr != end )
    {
        if( itr->Type == NEWLINE )
            newlines++;
        else if( itr->Type == PREPROCESSOR )
        {
            if( itr->Value == "#endif" && depth == 0 )
            {
                ++itr;
                found_end = true;
                break;
            }
            if( itr->Value == "#ifdef" || itr->Value == "#ifndef" || itr->Value == "#if" )
                depth++;
            if( itr->Value == "#endif" && depth > 0 )
                depth--;
        }
        ++itr;
    }
    if( itr == end && !found_end )
    {
        PrintErrorMessage( ctx, "0x0FA4 Unexpected end of file." );
        return itr;
    }
    while( newlines > 0 )
    {
        --itr;
        itr->Type = NEWLINE;
        itr->Value = "\n";
        --newlines;
    }
    return itr;
}

void Preprocessor::ParseIf( Context* ctx, LexemList& directive, std::string& name_out )
{
    directive.pop_front();
    if( directive.empty() )
    {
        PrintErrorMessage( ctx, "Expected argument." );
        return;
    }
    name_out = directive.begin()->Value;
    directive.pop_front();
    if( !directive.empty() )
        PrintErrorMessage( ctx, "Too many arguments." );
}

void Preprocessor::ParseUndef( Context* ctx, DefineTable& define_table, const std::string& identifier )
{
    auto it = define_table.find( identifier );
    if( it != define_table.end() )
        define_table.erase( it );
}

bool Preprocessor::ConvertExpression( Context* ctx, LexemList& expression, LexemList& output )
{
    if( expression.empty() )
    {
        PrintMessage( ctx, "Empty expression." );
        return true;
    }

    // Convert to RPN
    std::vector< Lexem > stack;
    for( LLITR it = expression.begin(); it != expression.end(); ++it )
    {
        PreprocessLexem( it, expression );
        Lexem& lexem = *it;
        if( IsIdentifier( lexem ) )
        {
            output.push_back( lexem );
        }
        else if( IsOperator( lexem ) )
        {
            while( stack.size() )
            {
                Lexem& lex = stack.back();
                if( IsOperator( lex ) &&
                    ( ( OperLeftAssoc( lexem ) && ( OperPrecedence( lexem ) <= OperPrecedence( lex ) ) ) ||
                      ( OperPrecedence( lexem ) < OperPrecedence( lex ) ) ) )
                {
                    output.push_back( lex );
                    stack.pop_back();
                }
                else
                    break;
            }
            stack.push_back( lexem );
        }
        else if( IsLeft( lexem ) )
        {
            stack.push_back( lexem );
        }
        else if( IsRight( lexem ) )
        {
            bool foundLeft = false;
            while( stack.size() )
            {
                Lexem& lex = stack.back();
                if( IsLeft( lex ) )
                {
                    foundLeft = true;
                    break;
                }
                else
                {
                    output.push_back( lex );
                    stack.pop_back();
                }
            }

            if( !foundLeft )
            {
                PrintErrorMessage( ctx, "Mismatched parentheses." );
                return false;
            }

            stack.pop_back();
        }
        else
        {
            PrintErrorMessage( ctx, "Unknown token: " + lexem.Value );
            return false;
        }
    }

    // input end, flush the stack
    while( stack.size() )
    {
        Lexem& lex = stack.back();
        if( IsLeft( lex ) || IsRight( lex ) )
        {
            PrintErrorMessage( ctx, "Mismatched parentheses." );
            return false;
        }
        output.push_back( lex );
        stack.pop_back();
    }
    return true;
}

int Preprocessor::EvaluateConvertedExpression( Context* ctx, DefineTable& define_table, LexemList& expr )
{
    std::vector< int > stack;

    while( expr.size() )
    {
        Lexem lexem = expr.front();
        expr.pop_front();

        if( IsIdentifier( lexem ) )
        {
            if( lexem.Type == NUMBER )
                stack.push_back( atoi( lexem.Value.c_str() ) );
            else
            {
                LexemList ll;
                ll.push_back( lexem );
                LLITR res = ExpandDefine( ctx, ll.begin(), ll.end(), ll, define_table );
                if( res != ll.begin() )
                {
                    PrintErrorMessage( ctx, "Unknown identifier in #if: " + lexem.Value + "." );
                    return 0;
                }
                LexemList out;
                bool      success = ConvertExpression( ctx, ll, out );
                if( !success )
                {
                    PrintErrorMessage( ctx, "Error while expanding macros." );
                    return 0;
                }
                stack.push_back( EvaluateConvertedExpression( ctx, define_table, out ) );
            }
        }
        else if( IsOperator( lexem ) )
        {
            if( lexem.Value == "!" )
            {
                if( !stack.size() )
                {
                    PrintErrorMessage( ctx, "Syntax error in #if: no argument for ! operator." );
                    return 0;
                }
                stack.back() = stack.back() != 0 ? 0 : 1;
            }
            else
            {
                if( stack.size() < 2 )
                {
                    PrintErrorMessage( ctx, "Syntax error in #if: not enough arguments for " + lexem.Value + " operator." );
                    return 0;
                }
                int rhs = stack.back();
                stack.pop_back();
                int lhs = stack.back();
                stack.pop_back();

                std::string op = lexem.Value;
                if( op == "*" )
                    stack.push_back( lhs *  rhs );
                if( op == "/" )
                    stack.push_back( lhs /  rhs );
                if( op == "%" )
                    stack.push_back( lhs %  rhs );
                if( op == "+" )
                    stack.push_back( lhs +  rhs );
                if( op == "-" )
                    stack.push_back( lhs -  rhs );
                if( op == "<" )
                    stack.push_back( lhs <  rhs );
                if( op == "<=" )
                    stack.push_back( lhs <= rhs );
                if( op == ">" )
                    stack.push_back( lhs >  rhs );
                if( op == ">=" )
                    stack.push_back( lhs >= rhs );
                if( op == "==" )
                    stack.push_back( lhs == rhs );
                if( op == "!=" )
                    stack.push_back( lhs != rhs );
                if( op == "&&" )
                    stack.push_back( ( lhs != 0 && rhs != 0 ) ? 1 : 0 );
                if( op == "||" )
                    stack.push_back( ( lhs != 0 || rhs != 0 ) ? 1 : 0 );
            }
        }
        else
        {
            PrintErrorMessage( ctx, "Internal error on lexem " + lexem.Value + "." );
            return 0;
        }
    }
    if( stack.size() == 1 )
        return stack.back();
    PrintErrorMessage( ctx, "Invalid #if expression." );
    return 0;
}

bool Preprocessor::EvaluateExpression( Context* ctx, DefineTable& define_table, LexemList& directive )
{
    LexemList output;
    directive.pop_front();
    bool      success = ConvertExpression( ctx, directive, output );
    if( !success )
        return false;
    return EvaluateConvertedExpression( ctx, define_table, output ) != 0;
}

std::string Preprocessor::AddPaths( Context* ctx, const std::string& first, const std::string& second )
{
    std::string result;
    size_t      slash_pos = first.find_last_of( '/' );
    if( slash_pos == 0 || slash_pos >= first.size() )
        return second;
    result = first.substr( 0, slash_pos + 1 );
    result += second;
    return result;
}

void Preprocessor::ParsePragma( Context* ctx, LexemList& args )
{
    args.pop_front();
    if( args.empty() )
    {
        PrintErrorMessage( ctx, "Pragmas need arguments." );
        return;
    }
    std::string p_name = args.begin()->Value;
    args.pop_front();
    std::string p_args;
    while( !args.empty() )
    {
        if( args.begin()->Type == STRING )
            p_args += RemoveQuotes( ctx, args.begin()->Value );
        else
            p_args += args.begin()->Value;

        args.pop_front();

        if( !args.empty() )
            p_args += " ";
    }

    PragmaInstance pi;
    pi.Name = p_name;
    pi.Text = p_args;
    pi.CurrentFile = ctx->CurrentFile;
    pi.CurrentFileLine = ctx->LinesThisFile;
    pi.RootFile = ctx->RootFile;
    pi.GlobalLine = ctx->CurrentLine;
    if( ctx->CurPragmaCallback )
        ctx->CurPragmaCallback->CallPragma( pi );
}

void Preprocessor::ParseTextLine( Context* ctx, LexemList& directive, std::string& message )
{
    message = "";
    directive.pop_front();
    if( directive.empty() )
        return;

    bool first = true;
    while( !directive.empty() )
    {
        if( !first )
            message += " ";
        else
            first = false;
        message += directive.begin()->Value;
        directive.pop_front();
    }
}

void Preprocessor::SetLineMacro( Context* ctx, DefineTable& define_table, unsigned int line )
{
    DefineEntry       def;
    Lexem             l;
    l.Type = NUMBER;
    std::stringstream sstr;
    sstr << line;
    sstr >> l.Value;
    def.Lexems.push_back( l );
    define_table[ "__LINE__" ] = def;
}

void Preprocessor::SetFileMacro( Context* ctx, DefineTable& define_table, const std::string& file )
{
    DefineEntry def;
    Lexem       l;
    l.Type = STRING;
    l.Value = std::string( "\"" ) + file + "\"";
    def.Lexems.push_back( l );
    define_table[ "__FILE__" ] = def;
}

void Preprocessor::RecursivePreprocess( Context* ctx, std::string filename, FileLoader& file_source, LexemList& lexems, DefineTable& define_table )
{
    size_t      slash = filename.find_last_of( "/\\" );
    std::string current_file = filename.substr( slash != std::string::npos ? slash + 1 : 0 );
    if( std::find( ctx->FilesPreprocessed.begin(), ctx->FilesPreprocessed.end(), current_file ) != ctx->FilesPreprocessed.end() )
        return;
    ctx->FilesPreprocessed.push_back( current_file );
    ctx->CurrentFile = current_file;

    unsigned int start_line = ctx->CurrentLine;
    ctx->LinesThisFile = 0;
    SetFileMacro( ctx, define_table, ctx->CurrentFile );
    SetLineMacro( ctx, define_table, ctx->LinesThisFile );

    std::vector< char > data;
    std::string         file_path;
    bool                loaded = file_source.LoadFile(ctx->RootPath, filename, data, file_path );
    if( !loaded )
    {
        PrintErrorMessage( ctx, std::string( "Could not open file " ) + ctx->RootPath + filename );
        return;
    }

    if( data.size() == 0 )
    {
        file_source.FileLoaded();
        return;
    }

    char* d_end = &data[ data.size() - 1 ];
    Lex( &data[ 0 ], ++d_end, lexems );

    LLITR itr = lexems.begin();
    LLITR end = lexems.end();
    LLITR old = end;
    while( itr != end )
    {
        if( itr->Type == NEWLINE )
        {
            if( itr != old )
            {
                ctx->CurrentLine++;
                ctx->LinesThisFile++;
                SetLineMacro( ctx, define_table, ctx->LinesThisFile );
            }
            old = itr;
            ++itr;
        }
        else if( itr->Type == PREPROCESSOR )
        {
            LLITR     start_of_line = itr;
            LLITR     end_of_line = ParsePreprocessor( ctx, lexems, itr, end );

            LexemList directive( start_of_line, end_of_line );
            itr = lexems.erase( start_of_line, end_of_line );

            for (auto it = directive.begin(); it != directive.end();)
            {
                if (it->Type == WHITESPACE)
                    it = directive.erase(it);
                else
                    ++it;
            }

            std::string value = directive.begin()->Value;
            if( value == "#define" )
            {
                ParseDefine( ctx, define_table, directive );
            }
            else if( value == "#ifdef" )
            {
                std::string           def_name;
                ParseIf( ctx, directive, def_name );
                DefineTable::iterator dti = define_table.find( def_name );
                if( dti == define_table.end() )
                {
                    LLITR splice_to = ParseIfDef( ctx, itr, end );
                    itr = lexems.erase( itr, splice_to );
                }
            }
            else if( value == "#ifndef" )
            {
                std::string           def_name;
                ParseIf( ctx, directive, def_name );
                DefineTable::iterator dti = define_table.find( def_name );
                if( dti != define_table.end() )
                {
                    LLITR splice_to = ParseIfDef( ctx, itr, end );
                    itr = lexems.erase( itr, splice_to );
                }
            }
            else if( value == "#if" )
            {
                bool satisfied = EvaluateExpression( ctx, define_table, directive ) != 0;
                if( !satisfied )
                {
                    LLITR splice_to = ParseIfDef( ctx, itr, end );
                    itr = lexems.erase( itr, splice_to );
                }
            }
            else if( value == "#endif" )
            {
                // ignore
            }
            else if( value == "#undef" )
            {
                directive.pop_front();

                if( directive.empty() )
                    PrintErrorMessage( ctx, "Undef directive without arguments." );
                else if( directive.size() > 1 )
                    PrintErrorMessage( ctx, "Undef directive multiple arguments." );
                else if( directive.begin()->Type != IDENTIFIER )
                    PrintErrorMessage( ctx, "Undef's name was not an identifier." );
                else
                    ParseUndef( ctx, define_table, directive.begin()->Value );
            }
            else if( value == "#include" )
            {
                if( ctx->LNT )
                    ctx->LNT->AddLineRange( file_path, start_line, ctx->CurrentLine - ctx->LinesThisFile );
                unsigned int save_lines_this_file = ctx->LinesThisFile;
                std::string  file_name;
                ParseIf( ctx, directive, file_name );

                LexemList next_file;
                RecursivePreprocess( ctx, AddPaths( ctx, filename, RemoveQuotes( ctx, file_name ) ), file_source, next_file, define_table );
                lexems.splice( itr, next_file );
                start_line = ctx->CurrentLine;
                ctx->LinesThisFile = save_lines_this_file;
                ctx->CurrentFile = current_file;
                SetFileMacro( ctx, define_table, ctx->CurrentFile );
                SetLineMacro( ctx, define_table, ctx->LinesThisFile );
            }
            else if( value == "#pragma" )
            {
                ParsePragma( ctx, directive );
            }
            else if( value == "#message" )
            {
                std::string message;
                ParseTextLine( ctx, directive, message );
                PrintMessage( ctx, message );
            }
            else if( value == "#warning" )
            {
                std::string warning;
                ParseTextLine( ctx, directive, warning );
                PrintWarningMessage( ctx, warning );
            }
            else if( value == "#error" )
            {
                std::string error;
                ParseTextLine( ctx, directive, error );
                PrintErrorMessage( ctx, error );
            }
            else
            {
                PrintErrorMessage( ctx, "Unknown directive '" + value + "'." );
            }
        }
        else if( itr->Type == IDENTIFIER )
        {
            itr = ExpandDefine( ctx, itr, end, lexems, define_table );
        }
        else
        {
            ++itr;
        }
    }

    if( ctx->LNT )
        ctx->LNT->AddLineRange( file_path, start_line, ctx->CurrentLine - ctx->LinesThisFile );

    file_source.FileLoaded();
}

Preprocessor::Context* Preprocessor::CreateContext()
{
	return new Context();
}

void Preprocessor::DeleteContext( Context* ctx ) noexcept
{
	if (ctx)
	{
		if (ctx->LNT)
			delete ctx->LNT;
		delete ctx;
	}
}

int Preprocessor::Preprocess( Context* ctx, std::string file_path, OutStream& result, OutStream* errors, FileLoader* loader )
{
    static OutStream  null_stream;
    static FileLoader default_loader;

    if( ctx->LNT )
        delete ctx->LNT;
    ctx->LNT = new LineNumberTranslator();

    ctx->CurrentFile = "ERROR";
    ctx->CurrentLine = 0;

    ctx->Errors = ( errors ? errors : &null_stream );
    ctx->ErrorsCount = 0;

    ctx->FilesPreprocessed.clear();

    size_t n = file_path.find_last_of( "\\/" );
    ctx->RootFile = ( n != std::string::npos ? file_path.substr( n + 1 ) : file_path );
    ctx->RootPath = ( n != std::string::npos ? file_path.substr( 0, n + 1 ) : "" );

    DefineTable define_table = ctx->CustomDefines;
    LexemList   lexems;

    RecursivePreprocess( ctx, ctx->RootFile, loader ? *loader : default_loader, lexems, define_table );
    PrintLexemList( ctx, lexems, result );
    return ctx->ErrorsCount;
}

void Preprocessor::Define( Context* ctx, const std::string& str )
{
    if( str.length() == 0 )
        return;

    Undef( ctx, str.substr( 0, str.find_first_of( " \t" ) ) );

    std::string data = "#define ";
    data += str;
    char*       d_end = &data[ data.length() - 1 ];
    LexemList   lexems;
    Lex( &data[ 0 ], ++d_end, lexems );

    for (auto it = lexems.begin(); it != lexems.end();)
    {
        if (it->Type == WHITESPACE)
            it = lexems.erase(it);
        else
            ++it;
    }

    ParseDefine( ctx, ctx->CustomDefines, lexems );
}

void Preprocessor::Undef( Context* ctx, const std::string& str )
{
    auto it = ctx->CustomDefines.find( str );
    if( it != ctx->CustomDefines.end() )
        ctx->CustomDefines.erase( it );
}

void Preprocessor::UndefAll( Context* ctx )
{
    ctx->CustomDefines.clear();
}

bool Preprocessor::IsDefined( Context* ctx, const std::string& str, std::string* value /* = NULL */ )
{
    for( DefineTable::iterator it = ctx->CustomDefines.begin(), end = ctx->CustomDefines.end(); it != end; ++it )
    {
        if( it->first == str )
        {
            if( value )
            {
                for( auto& lex : it->second.Lexems )
                {
                    if( !value->empty() )
                        *value += " ";
                    *value += lex.Value;
                }
            }
            return true;
        }
    }
    return false;
}

Preprocessor::LineNumberTranslator* Preprocessor::GetLineNumberTranslator( Context* ctx )
{
    return new LineNumberTranslator( *ctx->LNT );
}

void Preprocessor::DeleteLineNumberTranslator( LineNumberTranslator* lnt )
{
    delete lnt;
}

void Preprocessor::StoreLineNumberTranslator( LineNumberTranslator* lnt, std::vector< unsigned char >& data )
{
    lnt->Store( data );
}

Preprocessor::LineNumberTranslator* Preprocessor::RestoreLineNumberTranslator( const std::vector< unsigned char >& data )
{
    LineNumberTranslator* lnt = new LineNumberTranslator();
    lnt->Restore( data );
    return lnt;
}

const std::string& Preprocessor::ResolveOriginalFile( Context* ctx, unsigned int line_number )
{
    static std::string error = "ERROR";
    return ctx->LNT ? ctx->LNT->Search( line_number ).File : error;
}

unsigned int Preprocessor::ResolveOriginalLine( Context* ctx, unsigned int line_number )
{
    return ctx->LNT ? line_number - ctx->LNT->Search( line_number ).Offset : 0;
}

const std::string& Preprocessor::ResolveOriginalFile( unsigned int line_number, LineNumberTranslator* lnt )
{
    static std::string error = "ERROR";
    return lnt ? lnt->Search( line_number ).File : error;
}

unsigned int Preprocessor::ResolveOriginalLine( unsigned int line_number, LineNumberTranslator* lnt )
{
    return lnt ? line_number - lnt->Search( line_number ).Offset : 0;
}

void Preprocessor::PrintLexemList( Context* ctx, LexemList& out, OutStream& destination )
{
	bool need_a_space = false;
    for( LLITR itr = out.begin(); itr != out.end(); ++itr )
    {
        if( itr->Type == IDENTIFIER || itr->Type == NUMBER )
        {
            if( need_a_space )
                destination << " ";
            destination << itr->Value;
            need_a_space = true;
        }
        else if( itr->Type == BACKSLASH )
        {
            destination << " ";
            need_a_space = false;
        }
        else
        {
            destination << itr->Value;
            need_a_space = false;
        }
    }
}

/************************************************************************/
/* File loader                                                          */
/************************************************************************/

bool Preprocessor::FileLoader::LoadFile( const std::string& dir, const std::string& file_name, std::vector< char >& data, std::string& file_path )
{
    FILE* fs = fopen( ( dir + file_name ).c_str(), "rb" );
    if( !fs )
        return false;

    fseek( fs, 0, SEEK_END );
    int len = (int) ftell( fs );
    fseek( fs, 0, SEEK_SET );

    data.resize( len );

    if( !fread( &data[ 0 ], len, 1, fs ) )
    {
        fclose( fs );
        return false;
    }
    fclose( fs );

    file_path = dir + file_name;
    return true;
}

/************************************************************************/
/* Expressions                                                          */
/************************************************************************/

void Preprocessor::PreprocessLexem( LLITR it, LexemList& lexems )
{
    LLITR next = it;
    ++next;
    if( next == lexems.end() )
        return;
    Lexem&      l1 = *it;
    Lexem&      l2 = *next;
    std::string backup = l1.Value;
    l1.Value += l2.Value;
    if( IsOperator( l1 ) )
    {
        lexems.erase( next );
        return;
    }
    l1.Value = backup;
}

bool Preprocessor::IsOperator( const Lexem& lexem )
{
    return ( lexem.Value == "+" || lexem.Value == "-" ||
             lexem.Value == "/"  || lexem.Value == "*" ||
             lexem.Value == "!"  || lexem.Value == "%" ||
             lexem.Value == "==" || lexem.Value == "!=" ||
             lexem.Value == ">"  || lexem.Value == "<" ||
             lexem.Value == ">=" || lexem.Value == "<=" ||
             lexem.Value == "||" || lexem.Value == "&&" );
}

bool Preprocessor::IsIdentifier( const Lexem& lexem )
{
    return ( !IsOperator( lexem ) && lexem.Type == IDENTIFIER ) || lexem.Type == NUMBER;
}

bool Preprocessor::IsLeft( const Lexem& lexem )
{
    return lexem.Type == OPEN && lexem.Value == "(";
}

bool Preprocessor::IsRight( const Lexem& lexem )
{
    return lexem.Type == CLOSE && lexem.Value == ")";
}

int Preprocessor::OperPrecedence( const Lexem& lexem )
{
    std::string op = lexem.Value;
    if( op == "!" )
        return 7;
    else if( op == "*" || op == "/" || op == "%" )
        return 6;
    else if( op == "+" || op == "-" )
        return 5;
    else if( op == "<" || op == "<=" || op == ">" || op == ">=" )
        return 4;
    else if( op == "==" || op == "!=" )
        return 3;
    else if( op == "&&" )
        return 2;
    else if( op == "||" )
        return 1;
    return 0;
}

bool Preprocessor::OperLeftAssoc( const Lexem& lexem )
{
    return lexem.Value != "!";
}

/************************************************************************/
/* Lexems                                                               */
/************************************************************************/

std::string Preprocessor::IntToString( int i )
{
    std::stringstream sstr;
    sstr << i;
    std::string       r;
    sstr >> r;
    return r;
}

bool Preprocessor::SearchString( std::string str, char in )
{
    return ( str.find_first_of( in ) < str.length() );
}

bool Preprocessor::IsNumber( char in )
{
    return SearchString( Numbers, in );
}

bool Preprocessor::IsIdentifierStart( char in )
{
    return SearchString( IdentifierStart, in );
}

bool Preprocessor::IsIdentifierBody( char in )
{
    return SearchString( IdentifierBody, in );
}

bool Preprocessor::IsTrivial( char in )
{
    return SearchString( Trivials, in );
}

bool Preprocessor::IsHex( char in )
{
    return SearchString( HexNumbers, in );
}

char* Preprocessor::ParseIdentifier( char* start, char* end, Lexem& out )
{
    out.Type = IDENTIFIER;
    out.Value += *start;
    while( true )
    {
        ++start;
        if( start == end )
            return start;
        if( IsIdentifierBody( *start ) )
        {
            out.Value += *start;
        }
        else
        {
            return start;
        }
    }
};

char* Preprocessor::ParseStringLiteral( char* start, char* end, char quote, Lexem& out )
{
    out.Type = STRING;
    out.Value += *start;
    while( true )
    {
        ++start;
        if( start == end )
            return start;
        out.Value += *start;
        // End of string literal?
        if( *start == quote )
            return ++start;
        // Escape sequence? - Really only need to handle \"
        if( *start == '\\' )
        {
            ++start;
            if( start == end )
                return start;
            out.Value += *start;
        }
    }
}

char* Preprocessor::ParseCharacterLiteral( char* start, char* end, Lexem& out )
{
    ++start;
    if( start == end )
        return start;
    out.Type = NUMBER;
    if( *start == '\\' )
    {
        ++start;
        if( start == end )
            return start;
        if( *start == 'n' )
            out.Value = IntToString( '\n' );
        if( *start == 't' )
            out.Value = IntToString( '\t' );
        if( *start == 'r' )
            out.Value = IntToString( '\r' );
        ++start;
        if( start == end )
            return start;
        ++start;
        return start;
    }
    else
    {
        out.Value = IntToString( *start );
        ++start;
        if( start == end )
            return start;
        ++start;
        return start;
    }
}

char* Preprocessor::ParseFloatingPoint( char* start, char* end, Lexem& out )
{
    out.Value += *start;
    while( true )
    {
        ++start;
        if( start == end )
            return start;
        if( !IsNumber( *start ) )
        {
            if( *start == 'f' )
            {
                out.Value += *start;
                ++start;
                return start;
            }
            return start;
        }
        else
        {
            out.Value += *start;
        }
    }
}

char* Preprocessor::ParseHexConstant( char* start, char* end, Lexem& out )
{
    out.Value += *start;
    while( true )
    {
        ++start;
        if( start == end )
            return start;
        if( IsHex( *start ) )
        {
            out.Value += *start;
        }
        else
        {
            return start;
        }
    }
}

char* Preprocessor::ParseNumber( char* start, char* end, Lexem& out )
{
    out.Type = NUMBER;
    out.Value += *start;
    while( true )
    {
        ++start;
        if( start == end )
            return start;
        if( IsNumber( *start ) )
        {
            out.Value += *start;
        }
        else if( *start == '.' )
        {
            return ParseFloatingPoint( start, end, out );
        }
        else if( *start == 'x' )
        {
            return ParseHexConstant( start, end, out );
        }
        else
        {
            return start;
        }
    }
}

char* Preprocessor::ParseBlockComment( char* start, char* end, Lexem& out )
{
    out.Type = COMMENT;
    out.Value += "/*";

    int newlines = 0;
    while( true )
    {
        ++start;
        if( start == end )
            break;
        if( *start == '\n' )
        {
            newlines++;
        }
        out.Value += *start;
        if( *start == '*' )
        {
            ++start;
            if( start == end )
                break;
            if( *start == '/' )
            {
                out.Value += *start;
                ++start;
                break;
            }
            else
            {
                --start;
            }
        }
    }
    while( newlines > 0 )
    {
        --start;
        *start = '\n';
        --newlines;
    }
    return start;
}

char* Preprocessor::ParseLineComment( char* start, char* end, Lexem& out )
{
    out.Type = COMMENT;
    out.Value += "//";

    while( true )
    {
        ++start;
        if( start == end )
            return start;
        out.Value += *start;
        if( *start == '\n' )
            return start;
    }
}

char* Preprocessor::ParseLexem( char* start, char* end, Lexem& out )
{
    if( start == end )
        return start;
    char current_char = *start;

    if( IsTrivial( current_char ) )
    {
        out.Value += current_char;
        out.Type = TrivialTypes[ Trivials.find_first_of( current_char ) ];
        return ++start;
    }

    if( IsIdentifierStart( current_char ) )
        return ParseIdentifier( start, end, out );

    if( current_char == '#' )
    {
        out.Value = "#";
        ++start;
        if( *start == '#' )
        {
            out.Value = "##";
            out.Type = IGNORE;
            return ( ++start );
        }
        while( start != end && ( *start == ' ' || *start == '\t' ) )
            ++start;
        if( start != end && IsIdentifierStart( *start ) )
            start = ParseIdentifier( start, end, out );
        out.Type = PREPROCESSOR;
        return start;
    }

    if( IsNumber( current_char ) )
        return ParseNumber( start, end, out );
    if( current_char == '\"' )
        return ParseStringLiteral( start, end, '\"', out );
    if( current_char == '\'' )
        return ParseStringLiteral( start, end, '\'', out );  // Todo: set optional ParseCharacterLiteral?
    if( current_char == '/' )
    {
        // Need to see if it's a comment.
        ++start;
        if( start == end )
            return start;
        if( *start == '*' )
            return ParseBlockComment( start, end, out );
        if( *start == '/' )
            return ParseLineComment( start, end, out );
        // Not a comment - let default code catch it as MISC
        --start;
    }
    if( current_char == '\\' )
    {
        out.Type = BACKSLASH;
        return ++start;
    }

    out.Value = std::string( 1, current_char );
    out.Type = IGNORE;
    return ++start;
}

int Preprocessor::Lex( char* begin, char* end, std::list< Lexem >& results )
{
    while( true )
    {
        Lexem current_lexem;
        begin = ParseLexem( begin, end, current_lexem );

        if( current_lexem.Type != COMMENT )
            results.push_back( current_lexem );

        if( begin == end )
            return 0;
    }
}
