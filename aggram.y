%{

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "taz.h"
#include "asmgen.h"

VT curr_cl, opcode_template;
int en;

static long decodebits(const char *);

%}

%union {
  int num;
  char *str;
  union xany *vt;
}

%token <str> T_STRING T_CLASSDEFNAME T_ENUMNAME
%token <str> T_LITBITS T_VARBITS T_CLASSBITS T_TEMPLITTOK T_TEMPCLASS
%token <num> T_NUMBER T_TEMPNUMTOK

%token T_INCLUDE T_INCDIR
%token T_ENUM T_NUMERIC T_TEMPLATE T_DEFAULT
%token T_BITS T_ZPADTO T_SPADTO T_SIGNED T_UNSIGNED T_WRAPAROUND
%token T_ENUMSTART T_ENUMEND
%token T_TEMPLSTART T_TEMPLEND T_TEMPLWS
%token T_EQ T_COLON

%type<vt> classdefname templateblock
%type<vt> templatedefs templatedef
%type<vt> bitfield bitfieldcomps bitfieldcomp
%type<vt> extrafields extrafield tpattern ttoken
%type<num> signedness numbits numpadbits

%%

specification : { opcode_template=NIL; } definitions

definitions : definitions definition |

definition : meta
	   | classdef
	   | maintemplate

meta	: T_INCDIR T_STRING	{ add_incdir($2); }
	| T_INCLUDE T_STRING	{ if(!process_include($2))
				  errormsg("Can't find include file %s", $2); }


classdef : T_ENUM enumdefname T_ENUMSTART {en=-1;} enumdefs T_ENUMEND edefault
	 | T_NUMERIC classdefname signedness numbits numpadbits
		{ mapset(classes, $2, mknumeric($3, $4, $5)); }
	 | T_TEMPLATE classdefname T_TEMPLSTART templateblock T_TEMPLEND
		{ mapset(classes, $2, $4); }

enumdefs : enumdefs enumdef
	 |

enumdef : T_ENUMNAME
		{ mapset(curr_cl->xenum.map, mkstring($1), mknumber(++en)); }
	| T_ENUMNAME T_EQ T_NUMBER
		{ mapset(curr_cl->xenum.map, mkstring($1), mknumber(en=$3)); }

edefault : T_DEFAULT T_EQ T_NUMBER
		{ curr_cl->xenum.def=mknumber($3); }
	|

signedness : T_SIGNED { $$ = 'S'; }
	   | T_UNSIGNED { $$ = 'U'; }
	   | T_WRAPAROUND { $$ = 'W'; }
	   | { $$ = 'X'; }

numbits : T_BITS T_EQ T_NUMBER { $$ = $3; }

numpadbits : T_ZPADTO T_EQ T_NUMBER { $$ = $3; }
	   | T_SPADTO T_EQ T_NUMBER { $$ = -$3; }
	   | { $$ = 0; }

maintemplate : T_TEMPLATE T_TEMPLSTART templateblock T_TEMPLEND
		{ if(opcode_template!=NIL) {
		  errormsg("More than one main template definition"); YYABORT;
		} else opcode_template=$3; }

templateblock : templatedefs

templatedefs : templatedefs templatedef { $$ = listadd($1, $2); }
	     | { $$ = mklist(); }

templatedef : bitfield T_TEMPLWS extrafields T_COLON tpattern
		{ $$ = mktemplate($1, $3, $5); }

bitfield : bitfieldcomps

bitfieldcomps : bitfieldcomp { $$=listadd(mklist(), $1); }
	      | bitfieldcomps bitfieldcomp
		{ $$=listadd($1, $2); }

bitfieldcomp : T_LITBITS
		{ $$=mkbitslice(strlen($1), decodebits($1), NIL); }
	     | T_VARBITS
		{ $$=mkbitslice(strlen($1), 0,
				mkstring(poolstring(&textpool,
						    $1+strlen($1)-1))); }
	     | T_CLASSBITS
		{ $$=mkbitslice(-1, 0, mkclass(mkstring(poolstring(&textpool,
								   $1)))); }

extrafields : extrafields extrafield { $$ = listadd($1, $2); }
	    | { $$ = mklist(); }

extrafield : bitfield T_TEMPLWS

tpattern : tpattern ttoken { $$ = listadd($1, $2); }
	 | { $$ = mklist(); }

ttoken : T_TEMPLITTOK { $$ = mkstring(poolstring(&textpool, $1)); }
       | T_TEMPCLASS  { $$ = mkclass(mkstring(poolstring(&textpool, $1))); }
       | T_TEMPNUMTOK { $$ = mknumber($1); }

enumdefname : classdefname
	{ mapset(classes, $1, curr_cl = mkenum()); }

classdefname : T_CLASSDEFNAME
	{
	  VT s=mkstring(poolstring(&textpool, $1));
	  if(mapget(classes, s))
	    { errormsg("Duplicate declaration of class %s", $1); YYABORT; }
	  $$=s;
	}

%%

static long decodebits(const char *s)
{
  long n=0;

  while(*s) {
    n<<=1;
    n|=(*s++)&1;
  }
  return n;
}
