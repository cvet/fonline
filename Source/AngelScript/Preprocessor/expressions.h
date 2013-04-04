#include <string>
#include "lex.h"
#include "lexem_list.h"

namespace Preprocessor
{
	void preprocessLexem(LLITR it, LexemList& lexems);
	bool isOperator(const Lexem& lexem);
	bool isIdentifier(const Lexem& lexem);
	bool isLeft(const Lexem& lexem);
	bool isRight(const Lexem& lexem);
	int operPrecedence(const Lexem& lexem);
	bool operLeftAssoc(const Lexem& lexem);
} // end namespace Preprocessor