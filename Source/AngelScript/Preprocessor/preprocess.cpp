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

#include "lex.h"
#include "preprocess.h"
#include "lexem_list.h"
#include "define_table.h"

#include <list>
#include <map>
#include <string>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace Preprocessor;

namespace {
	DefineTable application_specified;

	OutStream* error_stream;

	PragmaCallback* pragma_callback;

	LineNumberTranslator::Table* LNT;
	std::string root_file;
	std::string current_file;
	unsigned int current_line;
	unsigned int lines_this_file;
	unsigned int number_of_errors;
	bool ProcessPragmas;
	std::vector<std::string> FileDependencies;
	std::vector<std::string> Pragmas;
    std::set<std::string> PragmasAdded;

	void PrintMessage( const std::string& msg )
	{
		(*error_stream) << current_file << " (" << lines_this_file
			<< ") " << msg << "\n";
	}

	void PrintErrorMessage(const std::string& errmsg)
	{
		std::string err = "Error";
		if( !(errmsg.empty() || errmsg.length() == 0) )
		{
			err += ": ";
			err += errmsg;
		}

		PrintMessage( err );
		number_of_errors++;
	}

	void PrintWarningMessage(const std::string& warnmsg)
	{
		std::string warn = "Warning";
		if( !(warnmsg.empty() || warnmsg.length() == 0) )
		{
			warn += ": ";
			warn += warnmsg;
		}

		PrintMessage( warn );
	}

	std::string removeQuotes(const std::string& in)
	{
		return in.substr(1,in.size()-2);
	}
}

void Preprocessor::SetPragmaCallback(PragmaCallback* callback)
{
	pragma_callback = callback;
}

void Preprocessor::CallPragma(const std::string& name, const PragmaInstance& instance)
{
	if(pragma_callback) pragma_callback->CallPragma(name,instance);
}

class Preprocessor::LineNumberTranslator::Table
{
public:
	struct Entry
	{
		std::string file;
		unsigned int start_line;
		unsigned int offset;
	};

	std::vector<Entry> lines;


	//Assuming blocks were entered in the proper order.
	Entry& search(unsigned int linenumber)
	{
		for (size_t i = 1; i < lines.size(); ++i)
		{
			if (linenumber < lines[i].start_line)
			{
				//Found the first block after our line.
				return lines[i-1];
			}
		}
		return lines[lines.size()-1]; //Line must be in last block.
	}

	void AddLineRange(const std::string& file, unsigned int start_line, unsigned int offset)
	{
		Entry e;
		e.file = file;
		e.start_line = start_line;
		e.offset = offset;
		lines.push_back(e);
	}
};

std::string Preprocessor::LineNumberTranslator::ResolveOriginalFile(unsigned int linenumber)
{
	if (!pimple) return "ERROR";
	return pimple->search(linenumber).file;
}

unsigned int Preprocessor::LineNumberTranslator::ResolveOriginalLine(unsigned int linenumber)
{
	if (!pimple) return 0;
	return linenumber - pimple->search(linenumber).offset;
}

Preprocessor::LineNumberTranslator::LineNumberTranslator() : pimple(0) {}

Preprocessor::LineNumberTranslator::~LineNumberTranslator()
{
	delete pimple;
}

void Preprocessor::LineNumberTranslator::SetTable(Preprocessor::LineNumberTranslator::Table *t)
{
	delete pimple;
	pimple = t;
}

static LLITR findEndOfLine(LLITR ITR, LLITR END)
{
	LLITR prev = ITR;
	while(ITR != END)
	{
		if(ITR->type == NEWLINE && (prev == ITR || prev->type != BACKSLASH)) break;
		prev = ITR;
		++ITR;
	}
	return ITR;
}

static LLITR parseStatement(LLITR ITR, LLITR END, LexemList& dest)
{
	int depth = 0;
	while (ITR != END)
	{
		if (ITR->value == "," && depth == 0) return ITR;
		if (ITR->type == CLOSE && depth == 0) return ITR;
		if (ITR->type == SEMICOLON && depth == 0) return ITR;
		dest.push_back(*ITR);
		if (ITR->type == OPEN) depth++;
		if (ITR->type == CLOSE) {
			if (depth == 0) PrintErrorMessage("Mismatched braces while parsing statement.");
			depth--;
		}
		++ITR;
	}
	return ITR;
}

static LLITR parseDefineArguments(LLITR ITR, LLITR END, LexemList& lexems, std::vector<LexemList>& args)
{
	if (ITR == END || ITR->value != "(")
	{
		PrintErrorMessage("Expected argument list.");
		return ITR;
	}
	LLITR begin_erase = ITR;
	++ITR;

	while (ITR != END)
	{
		LexemList argument;
		LLITR prev = ITR;
		ITR = parseStatement(ITR,END,argument);
		if(ITR == prev) return ITR;

		args.push_back(argument);

		if (ITR == END)
		{
			PrintErrorMessage("0x0FA1 Unexpected end of file.");
			return ITR;
		}
		if (ITR->value == ",")
		{
			++ITR;
			if (ITR == END) {
				PrintErrorMessage("0x0FA2 Unexpected end of file.");
				return ITR;
			}
			continue;
		}
		if (ITR->value == ")")
		{
			++ITR;
			break;
		}
	}

	return lexems.erase(begin_erase,ITR);
}

static LLITR expandDefine(LLITR ITR, LLITR END, LexemList& lexems, DefineTable& define_table)
{
	DefineTable::iterator define_entry = define_table.find(ITR->value);
	if (define_entry == define_table.end()) return ++ITR;
	ITR = lexems.erase(ITR);

	LLITR ITR_BEGIN = ITR;
	ITR_BEGIN--;

	if (define_entry->second.arguments.size() == 0)
	{
		lexems.insert(ITR,
			define_entry->second.lexems.begin(),
			define_entry->second.lexems.end());

		return ITR_BEGIN;
	}

	//define has arguments.
	std::vector<LexemList> arguments;
	ITR = parseDefineArguments(ITR,END,lexems,arguments);

	if (define_entry->second.arguments.size() != arguments.size())
	{
		PrintErrorMessage("Didn't supply right number of arguments to define.");
		return END;
	}

	LexemList temp_list(define_entry->second.lexems.begin(),define_entry->second.lexems.end());

	LLITR TLI = temp_list.begin();
	while (TLI != temp_list.end())
	{
		ArgSet::iterator arg = define_entry->second.arguments.find(TLI->value);
		if (arg == define_entry->second.arguments.end())
		{
			++TLI;
			continue;
		}

		TLI = temp_list.erase(TLI);
		temp_list.insert(TLI,arguments[arg->second].begin(),arguments[arg->second].end());
	}

	lexems.insert(ITR,temp_list.begin(),temp_list.end());

	return ITR_BEGIN;
	//expand arguments in templist.
}

static void parseDefine(DefineTable& define_table, LexemList& def_lexems)
{
	def_lexems.pop_front();	//remove #define directive
	if (def_lexems.empty()) {
		PrintErrorMessage("Define directive without arguments.");
		return;
	}
	Lexem name = *def_lexems.begin();
	if (name.type != IDENTIFIER)
	{
		PrintErrorMessage("Define's name was not an identifier.");
		return;
	}
	def_lexems.pop_front();

	DefineEntry def;

	while(!def_lexems.empty() && (def_lexems.begin()->type == BACKSLASH || def_lexems.begin()->type == NEWLINE))
		def_lexems.pop_front();

	if (!def_lexems.empty())
	{
		if (def_lexems.begin()->type == PREPROCESSOR && def_lexems.begin()->value == "#")
		{
			//Macro has arguments
			def_lexems.pop_front();
			if (def_lexems.empty())
			{
				PrintErrorMessage("Expected arguments.");
				return;
			}
			if (def_lexems.begin()->value != "(")
			{
				PrintErrorMessage("Expected arguments.");
				return;
			}
			def_lexems.pop_front();

			int num_args = 0;
			while(!def_lexems.empty() && def_lexems.begin()->value != ")")
			{
				if (def_lexems.begin()->type != IDENTIFIER)
				{
					PrintErrorMessage("Expected identifier.");
					return;
				}
				def.arguments[def_lexems.begin()->value] = num_args;
				def_lexems.pop_front();
				if (!def_lexems.empty() && def_lexems.begin()->value == ",")
				{
					def_lexems.pop_front();
				}
				num_args++;
			}

			if (!def_lexems.empty())
			{
				if (def_lexems.begin()->value != ")")
				{
					PrintErrorMessage("Expected closing parantheses.");
					return;
				}
				def_lexems.pop_front();
			}
			else
			{
				PrintErrorMessage("0x0FA3 Unexpected end of line.");
				return;
			}
		}

		LLITR DLB = def_lexems.begin();
		while (DLB != def_lexems.end())
		{
			if( DLB->value == "##" && DLB->type == IGNORE )
				DLB->value = "";
			DLB = expandDefine(DLB,def_lexems.end(),def_lexems,define_table);
		}
	}

	def.lexems = def_lexems;
	define_table[name.value] = def;
}

static LLITR parseIfDef(LLITR ITR, LLITR END)
{
	int depth = 0;
	int newlines = 0;
	bool found_end = false;
	while (ITR != END)
	{
		if (ITR->type == NEWLINE) newlines++;
		else if (ITR->type == PREPROCESSOR)
		{
			if (ITR->value == "#endif" && depth == 0)
			{
				++ITR;
				found_end = true;
				break;
			}
			if (ITR->value == "#ifdef" || ITR->value == "#ifndef") depth++;
			if (ITR->value == "#endif" && depth > 0) depth--;
		}
		++ITR;
	}
	if (ITR == END && !found_end)
	{
		PrintErrorMessage("0x0FA4 Unexpected end of file.");
		return ITR;
	}
	while (newlines > 0)
	{
		--ITR;
		ITR->type = NEWLINE;
		ITR->value = "\n";
		--newlines;
	}
	return ITR;
}

static void parseIf(LexemList& directive, std::string& name_out)
{
	directive.pop_front();
	if (directive.empty())
	{
		PrintErrorMessage("Expected argument.");
		return;
	}
	name_out = directive.begin()->value;
	directive.pop_front();
	if (!directive.empty()) PrintErrorMessage("Too many arguments.");
}

static std::string addPaths(const std::string& first, const std::string& second)
{
	std::string result;
	size_t slash_pos = first.find_last_of('/');
	if (slash_pos == 0 || slash_pos >= first.size()) return second;
	result = first.substr(0,slash_pos+1);
	result += second;
	return result;
}

static void parsePragma(LexemList& args)
{
	args.pop_front();
	if (args.empty())
	{
		PrintErrorMessage("Pragmas need arguments.");
		return;
	}
	std::string p_name = args.begin()->value;
	args.pop_front();
	std::string p_args;
	if (!args.empty())
	{
		if (args.begin()->type != STRING)
			PrintErrorMessage("Pragma parameter should be a string literal.");
		p_args = removeQuotes(args.begin()->value);
		args.pop_front();
	}
	if (!args.empty()) PrintErrorMessage("Too many parameters to pragma.");

    std::string pragma_line = p_name + p_args;
    if( !PragmasAdded.count( pragma_line ) )
    {
        PragmasAdded.insert( pragma_line );

	    Pragmas.push_back(p_name);
	    Pragmas.push_back(p_args);

	    Preprocessor::PragmaInstance PI;
	    PI.text = p_args;
	    PI.current_file = current_file;
	    PI.current_file_line = lines_this_file;
	    PI.root_file = root_file;
	    PI.global_line = current_line;
	    if(pragma_callback) CallPragma(p_name,PI);
    }
}

static void parseTextLine(LexemList& directive, std::string& message )
{
	message = "";
	directive.pop_front();
	if( directive.empty() )
		return;
	else
	{
		bool first = true;
		while( !directive.empty() )
		{
			if( !first )
				message += " ";
			else
				first = false;
			message += directive.begin()->value;
			directive.pop_front();
		}
	}
}

static void setLineMacro(DefineTable& define_table, unsigned int line)
{
	DefineEntry def;
	Lexem l;
	l.type = NUMBER;
	std::stringstream sstr;
	sstr << line;
	sstr >> l.value;
	def.lexems.push_back(l);
	define_table["__LINE__"] = def;
}

static void setFileMacro(DefineTable& define_table, const std::string& file)
{
	DefineEntry def;
	Lexem l;
	l.type = STRING;
	l.value = std::string("\"")+file+"\"";
	def.lexems.push_back(l);
	define_table["__FILE__"] = def;
}

static void recursivePreprocess(
	std::string filename,
	FileSource& file_source,
	LexemList& lexems,
	DefineTable& define_table)
{
	std::vector<char> data;

	unsigned int start_line = current_line;
	lines_this_file = 0;
	current_file = filename;
	setFileMacro(define_table,current_file);
	setLineMacro(define_table,lines_this_file);

	bool loaded = file_source.LoadFile(filename,data);

	if (!loaded)
	{
		PrintErrorMessage(std::string("Could not open file ")+filename);
		return;
	}

	if (data.size() == 0) return;
	char* d_end = &data[data.size()-1];
	++d_end;
	lex(&data[0],d_end,lexems);

	LexemList::iterator ITR = lexems.begin();
	LexemList::iterator END = lexems.end();
	LLITR old=END;
	while ( ITR != END )
	{
		if (ITR->type == NEWLINE)
		{
			if(ITR!=old)
			{
				current_line++;
				lines_this_file++;
				setLineMacro(define_table,lines_this_file);
			}
			old=ITR;
			++ITR;
		}
		else if (ITR->type == PREPROCESSOR)
		{
			LLITR start_of_line = ITR;
			LLITR end_of_line = findEndOfLine(ITR,END);

			LexemList directive(start_of_line,end_of_line);

			if(!ProcessPragmas && directive.begin()->value=="#pragma")
			{
				ITR=end_of_line;
				Lexem wspace;
				wspace.type=WHITESPACE;
				wspace.value=" ";
				for(LLITR it=start_of_line;it!=end_of_line;)
				{
					++it;
					it=lexems.insert(it,wspace);
					++it;
				}
				continue;
			}

			ITR = lexems.erase(start_of_line,end_of_line);

			std::string value = directive.begin()->value;
			if (value == "#define")
			{
				parseDefine(define_table,directive);
			}
			else if (value == "#ifdef")
			{
				std::string def_name;
				parseIf(directive,def_name);
				DefineTable::iterator DTI = define_table.find(def_name);
				if (DTI == define_table.end())
				{
					LLITR splice_to = parseIfDef(ITR,END);
					ITR = lexems.erase(ITR,splice_to);
				}
			}
			else if (value == "#ifndef")
			{
				std::string def_name;
				parseIf(directive,def_name);
				DefineTable::iterator DTI = define_table.find(def_name);
				if (DTI != define_table.end())
				{
					LLITR splice_to = parseIfDef(ITR,END);
					ITR = lexems.erase(ITR,splice_to);
				}
			}
			else if (value == "#endif")
			{
				//ignore
			}
			else if (value == "#include")
			{
				if (LNT) LNT->AddLineRange(filename,start_line,current_line-lines_this_file);
				unsigned int save_lines_this_file = lines_this_file;
				std::string file_name;
				parseIf(directive,file_name);

				std::string file_name_=removeQuotes(file_name);
				if(std::find(FileDependencies.begin(),FileDependencies.end(),file_name_)==FileDependencies.end())
					FileDependencies.push_back(file_name_);

				LexemList next_file;
				recursivePreprocess(
					addPaths(filename,file_name_),
					file_source,
					next_file,
					define_table);
				lexems.splice(ITR,next_file);
				start_line = current_line;
				lines_this_file = save_lines_this_file;
				current_file = filename;
				setFileMacro(define_table,current_file);
				setLineMacro(define_table,lines_this_file);
			}
			else if (value == "#pragma")
			{
				parsePragma(directive);
			}
			else if( value == "#message" )
			{
				std::string message;
				parseTextLine( directive, message );
				PrintMessage(message);
			}
			else if(value == "#warning")
			{
				std::string warning;
				parseTextLine(directive,warning);
				PrintWarningMessage(warning);
			}
			else if( value == "#error" )
			{
				std::string error;
				parseTextLine( directive, error );
				PrintErrorMessage( error );
			}
			else
			{
				PrintErrorMessage("Unknown directive.");
			}
		}
		else if (ITR->type == IDENTIFIER)
		{
			ITR = expandDefine(ITR,END,lexems,define_table);
		}
		else { ++ITR; }
	}

	if (LNT) LNT->AddLineRange(filename,start_line,current_line-lines_this_file);
}

int Preprocessor::Preprocess(
	std::string source_file,
	FileSource& file_source,
	OutStream& destination,
	bool process_pragmas,
	OutStream* err,
	LineNumberTranslator* trans)
{
	if (trans) LNT = new LineNumberTranslator::Table;
	else LNT = 0;
	current_file = "ERROR";
	current_line = 0;
	DefineTable define_table = application_specified;
	LexemList lexems;
	static NullOutStream null_err;
	error_stream = (err ? err : &null_err);
	number_of_errors = 0;
	root_file = source_file;
	ProcessPragmas = process_pragmas;
	FileDependencies.clear();
	Pragmas.clear();
    PragmasAdded.clear();

	recursivePreprocess(source_file,file_source,lexems,define_table);
	printLexemList(lexems,destination);

	if (trans)
	{
		trans->SetTable(LNT);
		LNT = 0;
	}
	return number_of_errors;
}

void Preprocessor::Define(const std::string& str)
{
	if (str.length() == 0) return;
	std::string data = "#define ";
	data += str;
	char* d_end = &data[data.length()-1];
	++d_end;
	LexemList lexems;
	lex(&data[0],d_end,lexems);

	::parseDefine(application_specified,lexems);
}

void Preprocessor::Undefine(const std::string& str)
{
	for(DefineTable::iterator it=application_specified.begin(),end=application_specified.end();it!=end;++it)
	{
		if((*it).first==str)
		{
			application_specified.erase(it);
			Undefine(str);
			break;
		}
	}
}

bool Preprocessor::IsDefined(const std::string& str)
{
	for(DefineTable::iterator it=application_specified.begin(),end=application_specified.end();it!=end;++it)
	{
		if((*it).first==str) return true;
	}
	return false;
}

void Preprocessor::UndefAll()
{
	application_specified.clear();
}

const std::vector<std::string>& Preprocessor::GetFileDependencies()
{
	return FileDependencies;
}

const std::vector<std::string>& Preprocessor::GetParsedPragmas()
{
	return Pragmas;
}

void Preprocessor::SetParsedPragmas( std::vector<std::string>& pragmas )
{
	PragmasAdded.clear();
	for( size_t i = 0, j = pragmas.size(); i < j; i += 2 )
		PragmasAdded.insert( pragmas[ i ] + pragmas[ i + 1 ] );
	Pragmas = pragmas;
}
