/*
   Preprocessor 0.5
   Copyright (c) 2005 Anthony Casteel

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

#include <string>
#include "lex.h"
#include <cassert>
#include <sstream>

using namespace Preprocessor;

//This should be implemented using boost::lexical_cast.
static std::string IntToString(int i)
{
	std::stringstream sstr;
	sstr << i;
	std::string r;
	sstr >> r;
	return r;
}

std::string numbers = "0123456789";
std::string identifierStart = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
std::string identifierBody = "_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
std::string hexnumbers = "0123456789abcdefABCDEF";

std::string trivials = ",;\n\r\t [{(]})";

LexemType trivial_types[] =
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

static bool searchString(std::string str, char in)
{
	return (str.find_first_of(in) < str.length());
}

static bool isNumber(char in) { return searchString(numbers,in); }
static bool isIdentifierStart(char in)	{ return searchString(identifierStart,in); }
static bool isIdentifierBody(char in) { return searchString(identifierBody,in); }
static bool isTrivial(char in) { return searchString(trivials,in); }
static bool isHex(char in) { return searchString(hexnumbers,in); }

static char* parseIdentifier(char* start, char* end, Lexem& out)
{
	out.type = IDENTIFIER;
	out.value += *start;
	while(true)
	{
		++start;
		if (start == end) return start;
		if (isIdentifierBody(*start))
		{
			out.value += *start;
		} 
		else
		{
			return start;
		}
	}
};

static char* parseStringLiteral(char* start, char* end, Lexem& out)
{
	out.type = STRING;
	out.value += *start;
	while(true)
	{
		++start;
		if (start == end) return start;
		out.value += *start;
		//End of string literal?
		if (*start == '\"') return ++start;
		//Escape sequence? - Really only need to handle \"
		if (*start == '\\')	
		{
			++start;
			if (start == end) return start;
			out.value += *start;
		}
	}
}

static char* parseCharacterLiteral(char* start, char* end, Lexem& out)
{
	++start;
	if (start == end) return start;
	out.type = NUMBER;
	if (*start == '\\')
	{
		++start;
		if (start == end) return start;
		if (*start == 'n') out.value = IntToString('\n');
		if (*start == 't') out.value = IntToString('\t');
		if (*start == 'r') out.value = IntToString('\r');
		++start; 
		if (start == end) return start;
		++start;
		return start;
	} 
	else 
	{
		out.value = IntToString(*(unsigned char*)start);
		++start;
		if (start == end) return start;
		++start;
		return start;
	}
}

static char* parseFloatingPoint(char* start, char* end, Lexem& out)
{
	out.value += *start;
	while (true)
	{
		++start;
		if (start == end) return start;
		if (!isNumber(*start))
		{
			if (*start == 'f')
			{
				out.value += *start;
				++start;
				return start;
			}
			return start;
		} else {
			out.value += *start;
		}
	}
}

static char* parseHexConstant(char* start, char* end, Lexem& out)
{
	out.value += *start;
	while (true)
	{ 
		++start;
		if (start == end) return start;
		if (isHex(*start))
		{
			out.value += *start;
		} else {
			return start;
		}
	}
}

static char* parseNumber(char* start, char* end, Lexem& out)
{
	out.type = NUMBER;
	out.value += *start;
	while(true)
	{
		++start;
		if (start == end) return start;
		if (isNumber(*start)) {
			out.value += *start;
		} else if (*start == '.') {
			return parseFloatingPoint(start,end,out);
		} else if (*start == 'x') {
			return parseHexConstant(start,end,out);
		} else {
			return start;
		}
	}
}

static char* parseBlockComment(char* start, char* end, Lexem& out)
{
	out.type = COMMENT;
	out.value += "/*";

	int newlines = 0;
	while(true)
	{
		++start;
		if (start == end) break;
		if (*start == '\n')
		{
			newlines++;
		}
		out.value += *start;
		if (*start == '*')
		{
			++start;
			if (start == end) break;
			if (*start == '/') 
			{
				out.value += *start;
				++start;
				break;
			} else 
			{
				--start;
			}
		}
	}
	while (newlines > 0) 
	{
		--start;
		*start = '\n';
		--newlines;
	}
	return start;
}

static char* parseLineComment(char* start, char* end, Lexem& out)
{
	out.type = COMMENT;
	out.value += "//";
	
	while(true)
	{
		++start;
		if (start == end) return start;
		out.value += *start;
		if (*start == '\n') return start;
	}
}


static char* parseLexem(char* start, char* end, Lexem& out) 
{	
	if (start == end) return start;
	char current_char = *start;
	
	if (isTrivial(current_char)) 
	{
		out.value += current_char;
		out.type = trivial_types[trivials.find_first_of(current_char)];
		return ++start;
	}
	
	if (isIdentifierStart(current_char)) return parseIdentifier(start,end,out);
	
	if (current_char == '#')
	{
		out.value = "#";
		++start;
		if( *start == '#' )
		{
			out.value = "##";
			out.type = IGNORE;
			return( ++start );
		}
		while (start != end && (*start == ' ' || *start == '\t'))
			++start;
		if (start != end && isIdentifierStart(*start))
			start = parseIdentifier(start,end,out);
		out.type = PREPROCESSOR;
		return start;
	}
	
	if (isNumber(current_char)) return parseNumber(start,end,out);
	if (current_char == '\"') return parseStringLiteral(start,end,out);
	if (current_char == '\'') return parseCharacterLiteral(start,end,out);	
	if (current_char == '/')
	{
		//Need to see if it's a comment.
		++start;
		if (start == end) return start;
		if (*start == '*') return parseBlockComment(start,end,out);
		if (*start == '/') return parseLineComment(start,end,out);
		//Not a comment - let default code catch it as MISC
		--start;
	}
	if (current_char == '\\')
	{
		out.type = BACKSLASH;
		return ++start;
	}
				
	out.value = current_char;
	out.type = IGNORE;
	return ++start;
}

int Preprocessor::lex(char* begin, char* end, std::list<Lexem>& results)
{
	assert(begin != 0 && end != 0);
	assert(begin <= end);
	while (true)
	{
		Lexem current_lexem;
		begin = parseLexem(begin,end,current_lexem);
		if (current_lexem.type != WHITESPACE &&
			current_lexem.type != COMMENT ) results.push_back(current_lexem);
		if (begin == end) return 0;
	}
};