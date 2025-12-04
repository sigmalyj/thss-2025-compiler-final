lexer grammar SysYLexer;

CONST     : 'const';
INT       : 'int';
VOID      : 'void';
IF        : 'if';
ELSE      : 'else';
WHILE     : 'while';
BREAK     : 'break';
CONTINUE  : 'continue';
RETURN    : 'return';

ADD       : '+';
SUB       : '-';
MUL       : '*';
DIV       : '/';
MOD       : '%';
EQ        : '==';
NEQ       : '!=';
LE        : '<=';
GE        : '>=';
ASSIGN    : '=';
LT        : '<';
GT        : '>';
NOT       : '!';
AND       : '&&';
OR        : '||';

LPAREN    : '(';
RPAREN    : ')';
LBRACE    : '{';
RBRACE    : '}';
LBRACK    : '[';
RBRACK    : ']';
COMMA     : ',';
SEMICOLON : ';';

IDENT     : [_a-zA-Z] [_a-zA-Z0-9]*;

INTLIT
	: DECIMAL
	| OCTAL
	| HEX
	;

fragment DECIMAL : '0' | [1-9] [0-9]*;
fragment OCTAL   : '0' [0-7]+;
fragment HEX     : ('0x' | '0X') [0-9a-fA-F]+;

WS : [ \t\r\n]+ -> skip;
LINE_COMMENT : '//' ~[\r\n]* -> skip;
BLOCK_COMMENT : '/*' .*? '*/' -> skip;