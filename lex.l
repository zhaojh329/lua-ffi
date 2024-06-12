%option noyywrap nodefault yylineno
%option noinput nounput
%option noyy_scan_string
%option noyyget_in noyyset_in noyyget_out noyyset_out
%option noyyget_debug noyyset_debug

%x COMMENT

%{
#include "token.h"
char *lex_err;
%}

%%

"/*"                    { BEGIN(COMMENT); }
<COMMENT>"*/"           { BEGIN(INITIAL); }
<COMMENT>([^*]|\n)+|.
<COMMENT><<EOF>>        { lex_err = "Unterminated comment"; return 0; }
"//".*\n                { /* skip for single line comment */ }

[0-9]+                  { return TOK_INTEGER; }

"typedef"               { return TOK_TYPEDEF; }
"struct"                { return TOK_STRUCT; }
"union"                 { return TOK_UNION; }

"const"                 { return TOK_CONST; }
"signed"                { return TOK_SIGNED; }
"unsigned"              { return TOK_UNSIGNED; }

"void"                  { return TOK_VOID; }
"bool"                  { return TOK_BOOL; }
"char"                  { return TOK_CHAR; }
"short"                 { return TOK_SHORT; }
"int"                   { return TOK_INT; }
"long"                  { return TOK_LONG; }
"float"                 { return TOK_FLOAT; }
"double"                { return TOK_DOUBLE; }

"int8_t"                { return TOK_INT8_T; }
"int16_t"               { return TOK_INT16_T; }
"int32_t"               { return TOK_INT32_T; }
"int64_t"               { return TOK_INT64_T; }
"uint8_t"               { return TOK_UINT8_T; }
"uint16_t"              { return TOK_UINT16_T; }
"uint32_t"              { return TOK_UINT32_T; }
"uint64_t"              { return TOK_UINT64_T; }

"ino_t"                 { return TOK_INO_T; }
"dev_t"                 { return TOK_DEV_T; }
"gid_t"                 { return TOK_GID_T; }
"mode_t"                { return TOK_MODE_T; }
"nlink_t"               { return TOK_NLINK_T; }
"uid_t"                 { return TOK_UID_T; }
"off_t"                 { return TOK_OFF_T; }
"pid_t"                 { return TOK_PID_T; }
"size_t"                { return TOK_SIZE_T; }
"ssize_t"               { return TOK_SSIZE_T; }
"useconds_t"            { return TOK_USECONDS_T; }
"suseconds_t"           { return TOK_SUSECONDS_T; }
"blksize_t"             { return TOK_BLKSIZE_T; }
"blkcnt_t"              { return TOK_BLKCNT_T; }
"time_t"                { return TOK_TIME_T; }

[\?\*(){}\[\],;]        { return yytext[0]; }
[_a-zA-Z][_a-zA-Z0-9]*  { return TOK_NAME; }
"..."                   { return TOK_VAL; }
[ \t\n]+                { /* ignore all spaces */ }
.                       { lex_err = "Unrecognized character"; return 0; }
<<EOF>>                 { return 0; }
