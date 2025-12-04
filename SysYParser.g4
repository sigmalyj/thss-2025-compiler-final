parser grammar SysYParser;

options {
    tokenVocab = SysYLexer;
}

compUnit
    : (decl | funcDef)+ EOF
    ;

decl
    : constDecl
    | varDecl
    ;

constDecl
    : CONST bType constDef (COMMA constDef)* SEMICOLON
    ;

bType
    : INT
    ;

constDef
    : IDENT (LBRACK constExp RBRACK)* ASSIGN constInitVal
    ;

constInitVal
    : constExp
    | LBRACE (constInitVal (COMMA constInitVal)*)? RBRACE
    ;

varDecl
    : bType varDef (COMMA varDef)* SEMICOLON
    ;

varDef
    : IDENT (LBRACK constExp RBRACK)* (ASSIGN initVal)?
    ;

initVal
    : exp
    | LBRACE (initVal (COMMA initVal)*)? RBRACE
    ;

funcDef
    : funcType IDENT LPAREN funcFParams? RPAREN block
    ;

funcType
    : VOID
    | INT
    ;

funcFParams
    : funcFParam (COMMA funcFParam)*
    ;

funcFParam
    : bType IDENT (LBRACK RBRACK (LBRACK constExp RBRACK)*)?
    ;

block
    : LBRACE blockItem* RBRACE
    ;

blockItem
    : decl
    | stmt
    ;

stmt
    : lVal ASSIGN exp SEMICOLON
    | block
    | IF LPAREN cond RPAREN stmt (ELSE stmt)?
    | WHILE LPAREN cond RPAREN stmt
    | BREAK SEMICOLON
    | CONTINUE SEMICOLON
    | RETURN exp? SEMICOLON
    | exp? SEMICOLON
    ;

exp
    : addExp
    ;

cond
    : lorExp
    ;

lorExp
    : lorExp OR landExp
    | landExp
    ;

landExp
    : landExp AND eqExp
    | eqExp
    ;

eqExp
    : eqExp (EQ | NEQ) relExp
    | relExp
    ;

relExp
    : relExp (LT | GT | LE | GE) addExp
    | addExp
    ;

addExp
    : addExp (ADD | SUB) mulExp
    | mulExp
    ;

mulExp
    : mulExp (MUL | DIV | MOD) unaryExp
    | unaryExp
    ;

unaryExp
    : primaryExp
    | IDENT LPAREN funcRParams? RPAREN
    | (ADD | SUB | NOT) unaryExp
    ;

primaryExp
    : LPAREN exp RPAREN
    | lVal
    | number
    ;

lVal
    : IDENT (LBRACK exp RBRACK)*
    ;

number
    : INTLIT
    ;

funcRParams
    : exp (COMMA exp)*
    ;

constExp
    : addExp
    ;