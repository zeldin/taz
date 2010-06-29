%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "taz.h"
#include "aggram.h"

extern struct mempool textpool;

char *fixup(char *);
%}

comment \/\/
ws	[ \t]
number	(0[xX][0-9a-fA-f]+)|([1-9][0-9]*)|(0[0-7]*)
class	[A-Z]
var	[a-z]
var_	[a-z_]

%s ENUM
%x IGN T0 T1 T2

%%

<*>{comment}	BEGIN(IGN);

{number}	{ yylval.num=strtol(yytext, NULL, 0); return T_NUMBER; }

<ENUM>([^ \t\n}=\\]|\\.)+	{ yylval.str=poolstring(&textpool, fixup(yytext)); return T_ENUMNAME; }

\"[^"\n]*\"	{
	yytext[yyleng-1]='\0';
	yylval.str=yytext+1;
	return T_STRING;
}

@incdir		return T_INCDIR;
@include	return T_INCLUDE;

enum		return T_ENUM;
default		return T_DEFAULT;
numeric		return T_NUMERIC;
template	return T_TEMPLATE;
bits		return T_BITS;
zpadto		return T_ZPADTO;
spadto		return T_SPADTO;
signed		return T_SIGNED;
unsigned	return T_UNSIGNED;
wraparound	return T_WRAPAROUND;
relative        return T_RELATIVE;
set		return T_SET;

\=		return T_EQ;
\{		{ BEGIN(ENUM); return T_ENUMSTART; }
\}		{ BEGIN(INITIAL); return T_ENUMEND; }
\{\{		{ BEGIN(T0); return T_TEMPLSTART; }
<T0,T1,T2>\}\}	{ BEGIN(INITIAL); return T_TEMPLEND; }

\({class}[0-9]*\)	{ yytext[yyleng-1]='\0'; yylval.str=yytext+1;
				return T_CLASSDEFNAME; }

<T0>{ws}+	;
<T0>\n		current_lineno++;
<T1>{ws}+	{ BEGIN(T0); return T_TEMPLWS; }
<T1>\n		{ current_lineno++; BEGIN(T0); return T_TEMPLWS; }
<T0>\:{ws}+	{ BEGIN(T2); return T_COLON; }
<T1>\:		{ yyless(0); BEGIN(T0); return T_TEMPLWS; }
<T0,T1>[01]+	{ BEGIN(T1); yylval.str=yytext; return T_LITBITS; }
<T0,T1>{var}+	{ char *p,v=yytext[0]; for(p=yytext; p; p++) if(*p!=v) break; if(*p)
		    yyless((p-yytext)); yylval.str=yytext; BEGIN(T1); return T_VARBITS; }
<T0,T1>{class}{var}?	{ BEGIN(T1); yylval.str=yytext; return T_CLASSBITS; }
<T2>\{{class}{var_}\}	{ yytext[3]='\0'; yylval.str=yytext; return T_TEMPCLASS; }
<T2>\{		{ errormsg("Illegal class expression"); }
<T2>{ws}+	;
<T2>\n		{ current_lineno++; BEGIN(T0); }
<T2>{number}	{ yylval.num=strtol(yytext, NULL, 0); return T_TEMPNUMTOK; }
<T2>{class}{var_}	{ yylval.str=yytext; return T_TEMPCLASS; }
<T2>(([^ \t\n{\\A-Z0-9]|\\.|[A-Z]+[^a-z_{ \t\n])+([A-Z]+[^a-z_{])?|[A-Z]+[^a-z_{])	{ if(yytext[yyleng-1]=='\n') { current_lineno++; BEGIN(T0); } yylval.str=strdup(fixup(yytext)); return T_TEMPLITTOK; }

{ws}+		;
<*>\n		{ current_lineno++; BEGIN(INITIAL); }
\"		{ errormsg("Unterminated string literal"); BEGIN(IGN); }
<IGN>.		;
<*>.		{ errormsg("Unknown character \"%c\"", yytext[0]);BEGIN(IGN); }

%%

char *fixup(char *p)
{
  char *src=p, *dst=p;
  for(;;)
    switch(*dst++=*src++) {
    case '\0':
      return p;
    case '\\':
      dst[-1]=*src++;
      break;
    case ' ': case '\t': case '\n':
      --dst;
    }
}

void yyerror()
{
  errormsg("Parse error");
}

int yywrap()
{
  return file_yywrap();
}
