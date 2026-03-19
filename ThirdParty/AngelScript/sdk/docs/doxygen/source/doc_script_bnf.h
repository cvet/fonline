/**


\page doc_script_bnf Script language grammar

This is language grammar in Extended Backus-Naur Form (EBNF).

\note The <a href="https://github.com/GuntherRademacher/rr" target="_blank">Railroad Diagram Generator</a> can be used to generate an online navigational railroad diagram.



<pre>
SCRIPT        ::= (IMPORT | ENUM | TYPEDEF | CLASS | MIXIN | INTERFACE | FUNCDEF | VIRTPROP | VAR | FUNC | NAMESPACE | USING | ';')*
IMPORT        ::= 'import' TYPE '&'? IDENTIFIER PARAMLIST FUNCATTR 'from' STRING ';'
USING         ::= 'using' 'namespace' IDENTIFIER ('::' IDENTIFIER)* ';'
NAMESPACE     ::= 'namespace' IDENTIFIER ('::' IDENTIFIER)* '{' SCRIPT '}'
ENUM          ::= ('shared' | 'external')* 'enum' IDENTIFIER (';' | ('{' IDENTIFIER ('=' EXPR)? (',' IDENTIFIER ('=' EXPR)?)* '}'))
FUNCDEF       ::= ('external' | 'shared')* 'funcdef' TYPE '&'? IDENTIFIER PARAMLIST ';'
FUNC          ::= ('shared' | 'external')* ('private' | 'protected')? (((TYPE '&'?) | '~'))? IDENTIFIER PARAMLIST 'const'? FUNCATTR (';' | STATBLOCK)
VIRTPROP      ::= ('private' | 'protected')? TYPE '&'? IDENTIFIER '{' (('get' | 'set') 'const'? FUNCATTR (STATBLOCK | ';'))* '}'
INTERFACE     ::= ('external' | 'shared')* 'interface' IDENTIFIER (';' | ((':' IDENTIFIER (',' IDENTIFIER)*)? '{' (VIRTPROP | INTFMTHD)* '}'))
MIXIN         ::= 'mixin' CLASS
CLASS         ::= ('shared' | 'abstract' | 'final' | 'external')* 'class' IDENTIFIER (';' | ((':' IDENTIFIER (',' IDENTIFIER)*)? '{' (VIRTPROP | FUNC | VAR | FUNCDEF)* '}'))
VAR           ::= ('private'|'protected')? TYPE IDENTIFIER (( '=' (INITLIST | EXPR)) | ARGLIST)? (',' IDENTIFIER (( '=' (INITLIST | EXPR)) | ARGLIST)?)* ';'
TYPEDEF       ::= 'typedef' PRIMTYPE IDENTIFIER ';'
INTFMTHD      ::= TYPE '&'? IDENTIFIER PARAMLIST 'const'? ';'
STATBLOCK     ::= '{' (VAR | STATEMENT | USING)* '}'
PARAMLIST     ::= '(' ('void' | (TYPE TYPEMOD ('...' | IDENTIFIER? ('=' EXPR)?) (',' TYPE TYPEMOD ('...' | IDENTIFIER? ('=' EXPR)?))*))? ')'
TYPEMOD       ::= ('&' ('in' | 'out' | 'inout')?)?
TYPE          ::= 'const'? SCOPE DATATYPE TEMPLTYPELIST? ( ('[' ']') | ('@' 'const'?) )*
TEMPLTYPELIST ::= '<' TYPE (',' TYPE)* '>'
INITLIST      ::= '{' (ASSIGN | INITLIST)? (',' (ASSIGN | INITLIST)?)* '}'
SCOPE         ::= '::'? (IDENTIFIER '::')* (IDENTIFIER TEMPLTYPELIST? '::')?
DATATYPE      ::= (IDENTIFIER | PRIMTYPE | '?' | 'auto')
PRIMTYPE      ::= 'void' | 'int' | 'int8' | 'int16' | 'int32' | 'int64' | 'uint' | 'uint8' | 'uint16' | 'uint32' | 'uint64' | 'float' | 'double' | 'bool'
FUNCATTR      ::= ('override' | 'final' | 'explicit' | 'property' | 'delete')*
STATEMENT     ::= (IF | FOR | FOREACH | WHILE | RETURN | STATBLOCK | BREAK | CONTINUE | DOWHILE | SWITCH | EXPRSTAT | TRY)
EXPRSTAT      ::= ASSIGN? ';'
SWITCH        ::= 'switch' '(' ASSIGN ')' '{' CASE* '}'
IF            ::= 'if' '(' ASSIGN ')' STATEMENT ('else' STATEMENT)?
TRY           ::= 'try' STATBLOCK 'catch' STATBLOCK
FOR           ::= 'for' '(' (VAR | EXPRSTAT) EXPRSTAT (ASSIGN (',' ASSIGN)*)? ')' STATEMENT
FOREACH       ::= 'foreach' '(' TYPE IDENTIFIER (',' TYPE INDENTIFIER)* ':' ASSIGN ')' STATEMENT
WHILE         ::= 'while' '(' ASSIGN ')' STATEMENT
DOWHILE       ::= 'do' STATEMENT 'while' '(' ASSIGN ')' ';'
RETURN        ::= 'return' ASSIGN? ';'
BREAK         ::= 'break' ';'
CONTINUE      ::= 'continue' ';'
EXPR          ::= EXPRTERM (EXPROP EXPRTERM)*
CASE          ::= (('case' EXPR) | 'default') ':' STATEMENT*
EXPRTERM      ::= ((TYPE '=')? INITLIST) | (EXPRPREOP* EXPRVALUE EXPRPOSTOP*)
EXPRVALUE     ::= 'void' | CONSTRUCTCALL | FUNCCALL | VARACCESS | CAST | LITERAL | '(' ASSIGN ')' | LAMBDA
CONSTRUCTCALL ::= TYPE ARGLIST
EXPRPREOP     ::= '-' | '+' | '!' | '++' | '--' | '~' | '@'
EXPRPOSTOP    ::= ('.' (FUNCCALL | IDENTIFIER)) | ('[' (IDENTIFIER ':')? ASSIGN (',' (IDENTIFIER ':')? ASSIGN)* ']') | ARGLIST | '++' | '--'
CAST          ::= 'cast' '<' TYPE '>' '(' ASSIGN ')'
LITERAL       ::= NUMBER | STRING | BITS | 'true' | 'false' | 'null'
LAMBDA        ::= 'function' '(' ((TYPE TYPEMOD)? IDENTIFIER? (',' (TYPE TYPEMOD)? IDENTIFIER?)*)? ')' STATBLOCK
FUNCCALL      ::= SCOPE IDENTIFIER TEMPLTYPELIST? ARGLIST
VARACCESS     ::= SCOPE IDENTIFIER
ARGLIST       ::= '(' (IDENTIFIER ':')? ASSIGN (',' (IDENTIFIER ':')? ASSIGN)* ')'
ASSIGN        ::= CONDITION ( ASSIGNOP ASSIGN )?
CONDITION     ::= EXPR ('?' ASSIGN ':' ASSIGN)?
EXPROP        ::= MATHOP | COMPOP | LOGICOP | BITOP
MATHOP        ::= '+' | '-' | '*' | '/' | '\%' | '**'
COMPOP        ::= '==' | '!=' | '<' | '<=' | '>' | '>=' | 'is' | '!is'
LOGICOP       ::= '&&' | '||' | '^^' | 'and' | 'or' | 'xor'
BITOP         ::= '&' | '|' | '^' | '<<' | '>>' | '>>>'
ASSIGNOP      ::= '=' | '+=' | '-=' | '*=' | '/=' | '|=' | '&=' | '^=' | '%=' | '**=' | '<<=' | '>>=' | '>>>='
IDENTIFIER    ::= [A-Za-z_][A-Za-z0-9_]*            // single token:  starts with letter or _, can include any letter and digit, same as in C++
NUMBER        ::= [0-9]+("."[0-9]+)?                // single token:  includes integers and real numbers, same as C++ 
STRING        ::= '"' ("\". | [^"\#x0D\#x0A\\])* '"'   // single token:  single quoted ', double quoted ", or heredoc multi-line string """
BITS          ::= '0'[bBoOdDxX][0-9A-Fa-f]+         // single token:  binary 0b or 0B, octal 0o or 0O, decimal 0d or 0D, hexadecimal 0x or 0X
COMMENT       ::= ('//'[^\#x0A]*) | ('/*'[^*]*'*/')  // single token:  starts with // and ends with new line or starts with /* and ends with */
WHITESPACE    ::= [ \#x09\#x0A\#x0D]+                  // single token:  spaces, tab, carriage return, line feed, and UTF8 byte-order-mark
</pre>

\todo update NUMBER and BITS with number separators
\todo update NUMBER with exponent
\todo update ENUM with underlying type


*/
