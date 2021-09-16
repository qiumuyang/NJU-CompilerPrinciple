%{
    #include "tree.h"
    #define YYSTYPE treeNode*
    #include "header.h"
    
    #define LEX_RET(id) do { \
       treeNode* ret = newNode(id, #id, LexicalType, yylineno); \
       strcpy(ret->str, yytext); \
       yylval = ret; \
       return id; \
    } while (0)
    #define LEX_ERR(errid) do { \
        if (unhandled > 0) { \
            fprintf(stderr, "%s.\n", errmsgB[SYN_ERR]); \
            unhandled--; \
        } \
        if (yylineno != preverr) { \
        fprintf(stderr, "Error type A at Line %d: %s \'%s\'.\n", \
                yylineno, errmsgA[errid], yytext); \
        yyperr = true; \
        preverr = yylineno; \
        } \
    } while (0)
    // LEX_RET allocs a new node for current token
    // and passes the node pointer to yylval in Bison
    // #id treats id literally as char []
    int yycolumn = 1;
    #define YY_USER_ACTION \
    yylloc.first_line = yylloc.last_line = yylineno; \
    yylloc.first_column = yycolumn; \
    yylloc.last_column = yycolumn + yyleng - 1; \
    yycolumn += yyleng;
%}

%option yylineno
ws          [ \t\r]+
digit       [0-9]
oct         0[0-7]+
hex         0[xX][0-9A-Fa-f]+
_alpha      [a-zA-Z_]
id          {_alpha}({_alpha}|{digit}){0,30}
inv_id      {digit}({_alpha}|{digit})*
decimal     0|[1-9]{digit}{0,9}
exponent    [eE][+-]?{digit}+
normfloat   {digit}+\.{digit}+
scifloat    ({digit}+\.{digit}*|{digit}*\.{digit}+){exponent}
float       {normfloat}|{scifloat}
%%
{ws}        { /* do nothing */ }
"\n"        { yycolumn = 1; }
";"         { LEX_RET(SEMI); }
","         { LEX_RET(COMMA); }
">="        { LEX_RET(RELOP); }
"<="        { LEX_RET(RELOP); }
"=="        { LEX_RET(RELOP); }
"!="        { LEX_RET(RELOP); }
">"         { LEX_RET(RELOP); }
"<"         { LEX_RET(RELOP); }
"="         { LEX_RET(ASSIGNOP); }
"+"         { LEX_RET(PLUS); }
"-"         { LEX_RET(MINUS); }
"*"         { LEX_RET(STAR); }
"/"         { LEX_RET(DIV); }
"&&"        { LEX_RET(AND); }
"||"        { LEX_RET(OR); }
"."         { LEX_RET(DOT); }
"!"         { LEX_RET(NOT); }
"int"       { LEX_RET(TYPE); }
"float"     { LEX_RET(TYPE); }
"("         { LEX_RET(LP); }
")"         { LEX_RET(RP); }
"["         { LEX_RET(LB); }
"]"         { LEX_RET(RB); }
"{"         { LEX_RET(LC); }
"}"         { LEX_RET(RC); }
"struct"    { LEX_RET(STRUCT); }
"return"    { LEX_RET(RETURN); }
"if"        { LEX_RET(IF); }
"else"      { LEX_RET(ELSE); }
"while"     { LEX_RET(WHILE); }
{id}        { LEX_RET(ID); }
{decimal}   { LEX_RET(INT); }
{oct}       { LEX_ERR(NOT_IMP); LEX_RET(INT); }
{hex}       { LEX_ERR(NOT_IMP); LEX_RET(INT); }
{float}     { LEX_RET(FLOAT); }
{inv_id}    { LEX_ERR(INV_ID); LEX_RET(ID);}
.           { LEX_ERR(INV_CHR); }

%%