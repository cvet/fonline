#include "expressions.h"

namespace Preprocessor
{

void preprocessLexem(LLITR it, LexemList& lexems)
{
	LLITR next = it;
	++next;
	if(next == lexems.end()) return;
	Lexem& l1 = *it;
	Lexem& l2 = *next;
	std::string backup = l1.value;
	l1.value += l2.value;
	if(isOperator(l1))
	{
		lexems.erase(next);
		return;
	}
	l1.value = backup;
}

bool isOperator(const Lexem& lexem)
{
	return (lexem.value == "+" || lexem.value == "-"
		|| lexem.value == "/"  || lexem.value == "*"
		|| lexem.value == "!"  || lexem.value == "%"
		|| lexem.value == "==" || lexem.value == "!="
		|| lexem.value == ">"  || lexem.value == "<"
		|| lexem.value == ">=" || lexem.value == "<="
		|| lexem.value == "||" || lexem.value == "&&" );
}

bool isIdentifier(const Lexem& lexem)
{
	return !isOperator(lexem) && lexem.type == IDENTIFIER || lexem.type == NUMBER;
}

bool isLeft(const Lexem& lexem)
{
	return lexem.type == OPEN && lexem.value == "(";
}

bool isRight(const Lexem& lexem)
{
	return lexem.type == CLOSE && lexem.value == ")";
}

int operPrecedence(const Lexem& lexem)
{
	std::string op = lexem.value;
	if(op == "!") return 7;
	else if(op == "*" || op == "/" || op == "%") return 6;
	else if(op == "+" || op == "-") return 5;
	else if(op == "<" || op == "<=" || op == ">" || op == ">=") return 4;
	else if(op == "==" || op == "!=") return 3;
	else if(op == "&&") return 2;
	else if(op == "||") return 1;
	return 0;
}

bool operLeftAssoc(const Lexem& lexem)
{
	return lexem.value != "!";
}

} // end namespace Preprocessor