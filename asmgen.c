/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "taz.h"
#include "asmgen.h"

#define INLINE_CLASSES

VT std_tokens, auto_tokens, opcode_tokens, xforms;
#ifdef INLINE_CLASSES
VT *nslots = NULL;
#endif

int maxrecurse = 20;

struct stdtoken { char *str, *token; } stdtok[] = {
  { "<<", "T_SHL" },
  { ">>", "T_SHR" },
  { "<=", "T_LE" },
  { ">=", "T_GE" },
  { "==", "T_EQL" },
  { "!=", "T_NE" },
  { "~",  "T_COMPL" },
  { "!",  "T_NOT" },
  { "+",  "T_ADD" },
  { "-",  "T_SUB" },
  { "*",  "T_MUL" },
  { "/",  "T_DIV" },
  { "%",  "T_MOD" },
  { "&",  "T_AND" },
  { "|",  "T_OR" },
  { "^",  "T_XOR" },
  { "<",  "T_LT" },
  { ">",  "T_GT" },
  { "=",  "T_EQ" },
  { "(",  "T_LPAR" },
  { ")",  "T_RPAR" },
  { ",",  "T_COMMA" }
};
#define NUM_STDTOK (sizeof(stdtok)/sizeof(stdtok[0]))

#define M_DEF 1
#define M_OPERRE 2
#define M_OPERTOK 4
#define M_OPCODERE 8
#define M_OPCODETOK 16
#define M_STDRE 32
#define M_STDTOK 64
#define M_OPCODEPROD 128
#define M_NUMPROD 256
#define M_ENUMPROD 512
#define M_TEMPLATEPROD 1024
#define M_CLASSTYPES 2048
#define M_EMITX 4096
#define M_X1 16384
#define M_X2 32768
#define M_X3 65536
#define M_X4 131072
#define M_BISON_OPTIONS 262144
#define M_NOKEY 524288
#define M_XFORMS 1048576

#define FLAG_OPCODE (M_OPCODERE|M_OPCODETOK)

struct { char *name, *value; int mask; } var[] = {
  { "trailcomment", ";", M_DEF },
  { "linecomment", "[#*]", M_DEF },
  { "label", "[a-zA-Z_@.][a-zA-Z$_@.0-9]*", M_DEF },
  { "bison-options", "", M_BISON_OPTIONS|M_NOKEY },
};
#define NUMVARS (sizeof(var)/sizeof(var[0]))

void printre(FILE *f, char *re)
{
  while(*re)
    if(isalpha(*re)) {
      putc('[', f);
      putc(toupper(*re), f);
      putc(tolower(*re++), f);
      putc(']', f);
    } else if(isdigit(*re))
      putc(*re++,f );
    else {
      putc('\\', f);
      putc(*re++, f);
    }
}

char *printxform(VT cls, int mode)
{
  static char buf[64];
  if(cls->xnumeric.xform < 0)
    return "(";
  if(mode)
    sprintf(buf, "XFORM%d(", cls->xnumeric.xform);
  else
    sprintf(buf, "MKXFORM(%d,", cls->xnumeric.xform);
  return buf;
}

int enum_checkbits(VT e, int b)
{
  VT i, d, p;
  if(e->xenum.def!=NIL) {
    long n;
    xassert(e->xenum.def, XNUMBER);
    n=e->xenum.def->xnumber.num;
    if(n<0 || (n&~((1l<<b)-1)))
      return 0;
  }
  MAPITER(e->xenum.map, i, d, p) {
    long n;
    xassert(p, XNUMBER);
    n=p->xnumber.num;
    if(n<0 || (n&~((1l<<b)-1)))
      return 0;    
  }
  return 1;
}

#ifdef INLINE_CLASSES
void print_slot(FILE *f, int s)
{
  VT cls;
  if(nslots == NULL || (cls = nslots[s]) == NIL) {
      fprintf(f, "$%d", s);
      return;
  }

  if(cls->xnumeric.relative != -1)
    fprintf(f, "checknum%c(%sMKSUB($%d,MKADD(mksymbref(currloc_sym),MKICON(%d)))),%d)",
	    cls->xnumeric.signedness, printxform(cls, 0), s,
	    cls->xnumeric.relative, cls->xnumeric.bits);	  
  else
    fprintf(f, "checknum%c(%s$%d),%d)",
	    cls->xnumeric.signedness, printxform(cls, 0), s, cls->xnumeric.bits);
}
#endif

void print_shifted(FILE *f, VT bs, int b)
{
  if(!b) {
#ifdef INLINE_CLASSES
    print_slot(f, bs->xbitslice.slot);
#else
    fprintf(f, "$%d", bs->xbitslice.slot);
#endif
  } else if(b<=16) {
    fprintf(f, "MKLS1(");
    print_shifted(f, bs, 0);
    fprintf(f, ",%d)",b);
  } else if(b<=128) {
    fprintf(f, "MKLS2(");
    print_shifted(f, bs, b&15);
    fprintf(f, ",%d)",b&~15);  
  } else {
    fprintf(f, "MKLS2(");
    print_shifted(f, bs, b-128);
    fprintf(f, ",128)");
  }
}

int print_slicedexp(FILE *f, VT bss)
{
  VT i, bs;
  intmax_t s=0;
  int totbits=0, ne=0, no=0, cc=0, nn;

  LISTITER(bss, i, bs) {
    xassert(bs, XBITSLICE);
    if(bs->xbitslice.offs<0) xwrong();
    if(bs->xbitslice.bits<0) xwrong();
    totbits+=bs->xbitslice.bits;
    if(bs->xbitslice.classname==NIL)
      s|=((intmax_t)bs->xbitslice.value)<<bs->xbitslice.offs;
    else if(bs->xbitslice.classname->type==XENUM)
      ne++;
    else
      no++;
  }
  if(s || ne) no++;
  for(nn=1; nn<no; nn++)
    fprintf(f, "MKOR(");
  if(s || ne) {
    fprintf(f, "MKICON(");
    if(s) { fprintf(f, "%jdlu", s); cc++; }
    LISTITER(bss, i, bs) {
      if(bs->xbitslice.classname!=NIL &&
	 bs->xbitslice.classname->type==XENUM) {
	if(cc++) fprintf(f, "|");
	fprintf(f, "($%d", bs->xbitslice.slot);
	if(!enum_checkbits(bs->xbitslice.classname, bs->xbitslice.bits))
	  fprintf(f, "&%ldl", (1l<<bs->xbitslice.bits)-1);
	if(bs->xbitslice.offs)
	  fprintf(f, ")<<%d", bs->xbitslice.offs);
	else
	  fprintf(f, ")");
      }
    }
    fprintf(f, ")");
  }
  if(no) {
    cc=0;
    if(s || ne) cc++;
    LISTITER(bss, i, bs) {
      if(bs->xbitslice.classname!=NIL &&
	 bs->xbitslice.classname->type!=XENUM) {
	if(cc) fprintf(f, ",");
	print_shifted(f, bs, bs->xbitslice.offs);
	if(cc) fprintf(f, ")");
	cc++;
      }
    }   
  } else fprintf(f, "MKICON(0)");
  return totbits;
}

int print_slicedn(FILE *f, VT bss, int *parammap)
{
  VT i, bs;
  intmax_t s=0;
  int totbits=0, ne=0, no=0, cc=0, nn;

  LISTITER(bss, i, bs) {
    xassert(bs, XBITSLICE);
    if(bs->xbitslice.offs<0) xwrong();
    if(bs->xbitslice.bits<0) xwrong();
    totbits+=bs->xbitslice.bits;
    if(bs->xbitslice.classname==NIL)
      s|=((intmax_t)bs->xbitslice.value)<<bs->xbitslice.offs;
    else if(bs->xbitslice.classname->type==XENUM)
      ne++;
    else
      no++;
  }
  if(s || ne) no++;
  for(nn=1; nn<no; nn++)
    fprintf(f, "(");
  if(s || ne) {
    fprintf(f, "(");
    if(s) { fprintf(f, "%jdlu", s); cc++; }
    LISTITER(bss, i, bs) {
      if(bs->xbitslice.classname!=NIL &&
	 bs->xbitslice.classname->type==XENUM) {
	if(cc++) fprintf(f, "|");
	fprintf(f, "(PARAM%d", parammap[bs->xbitslice.slot]);
	if(!enum_checkbits(bs->xbitslice.classname, bs->xbitslice.bits))
	  fprintf(f, "&%ldl", (1l<<bs->xbitslice.bits)-1);
	if(bs->xbitslice.offs)
	  fprintf(f, ")<<%d", bs->xbitslice.offs);
	else
	  fprintf(f, ")");
      }
    }
    fprintf(f, ")");
  }
  if(no) {
    cc=0;
    if(s || ne) cc++;
    LISTITER(bss, i, bs) {
      if(bs->xbitslice.classname!=NIL &&
	 bs->xbitslice.classname->type!=XENUM) {
	VT cls = bs->xbitslice.classname;
	if(cc) fprintf(f, "|");
	if(cls->xnumeric.relative != -1)
	  fprintf(f, "(((%sPARAM%d-(*current_loc+%d)))&%ldl)",
		  printxform(cls, 'S'), parammap[bs->xbitslice.slot],
		  cls->xnumeric.relative, (1l<<bs->xbitslice.bits)-1);
	else
	  fprintf(f, "((%sPARAM%d)&%ldl)", printxform(cls, 'S'),
		  parammap[bs->xbitslice.slot], (1l<<bs->xbitslice.bits)-1);
	if(bs->xbitslice.offs)
	  fprintf(f, "<<%d", bs->xbitslice.offs);
	fprintf(f, ")");
	if(cc) fprintf(f, ")");
	cc++;
      }
    }   
  } else fprintf(f, "0");
  return totbits;
}

void print_slicechecknum(FILE *f, VT bss, int *parammap, char mode)
{
  VT i, bs;
  int cc=0;

  LISTITER(bss, i, bs) {
    xassert(bs, XBITSLICE);
    if(bs->xbitslice.offs<0) xwrong();
    if(bs->xbitslice.bits<0) xwrong();
    if(bs->xbitslice.classname==NIL)
      ;
    else if(bs->xbitslice.classname->type==XNUMERIC) {
      VT cls = bs->xbitslice.classname;
      if(cc && mode == 'S')
	fprintf(f, "&&");
      if(cls->xnumeric.relative != -1)
	fprintf(f, "checknum%c%c(%sPARAM%d-(*current_loc+%d)),%d)",
		cls->xnumeric.signedness, mode, printxform(cls, mode),
		parammap[bs->xbitslice.slot],
		cls->xnumeric.relative, cls->xnumeric.bits);	  
      else
	fprintf(f, "checknum%c%c(%sPARAM%d),%d)",
		cls->xnumeric.signedness, mode, printxform(cls, mode),
		parammap[bs->xbitslice.slot], cls->xnumeric.bits);
      if(mode != 'S')
	fprintf(f, ";");
      cc++;
    }
  }
}

void print_deferredgen(FILE *f, VT bss, int *n)
{
  VT i, bs;
  int totbits=0, nn=0, cc=0;

  LISTITER(bss, i, bs) {
    xassert(bs, XBITSLICE);
    if(bs->xbitslice.offs<0) xwrong();
    if(bs->xbitslice.bits<0) xwrong();
    totbits+=bs->xbitslice.bits;
    if(bs->xbitslice.classname==NIL)
      ;
    else
      nn++;
  }
  if(nn>4) {
    fprintf(stderr, "More than 4 operands in magnitude sensitive op!\n");
    exit(1);
  }
  fprintf(f, "XGEN%d(%d, ", nn, n[nn]++);
  LISTITER(bss, i, bs) {
    if(bs->xbitslice.classname!=NIL) {
      if(cc) fprintf(f, ",");
      if (bs->xbitslice.classname->type==XENUM) {
	fprintf(f, "MKICON($%d)", bs->xbitslice.slot);
      } else {
	fprintf(f, "$%d", bs->xbitslice.slot);
      }
      cc++;
    }
  }
  fprintf(f, ");");
}

static char equiv_signedness(VT cls)
{
  if(cls->xnumeric.signedness == 'S' &&
     cls->xnumeric.relative != -1)
    return 'U';
  else
    return cls->xnumeric.signedness;
}

int mergable(VT t1, VT t2)
{
  int mm, mergemode = 0;
  VT i1, i2;

  /* Fixme... */
  if(t1->xtemplate.extras->xlist.head != NIL ||
     t2->xtemplate.extras->xlist.head != NIL)
    return 0;

  for(i1=t1->xtemplate.tokens->xlist.head,
	i2=t2->xtemplate.tokens->xlist.head;
      i1!=NIL && i2!=NIL;
      i1=i1->xpair.right, i2=i2->xpair.right) {
    VT tok1=i1->xpair.left, tok2=i2->xpair.left;
    if(tok1->type!=tok2->type)
      return 0;
    switch(tok1->type) {
    case XSTRING:
      if(strcmp(tok1->xstring.str, tok2->xstring.str))
	return 0;
      break;
    case XCLASS:
      if(strcmp(tok1->xclass.str->xstring.str, tok2->xclass.str->xstring.str)) {
	VT cls1=mapget(classes,tok1->xclass.str);
	VT cls2=mapget(classes,tok2->xclass.str);
	if(cls1->type != cls2->type || cls1->type != XNUMERIC)
	  return 0;
#if 0
	if(cls1->xnumeric.signedness != cls2->xnumeric.signedness
	   /*|| cls1->xnumeric.relative != cls2->xnumeric.relative*/)
	  return 0;
#else
	if(equiv_signedness(cls1) != equiv_signedness(cls2))
	  return 0;
#endif
	if(cls1->xnumeric.bits == cls2->xnumeric.bits) {
	  if(cls1->xnumeric.xform<0 || !cls2->xnumeric.xform<0 ||
	     cls1->xnumeric.xform == cls2->xnumeric.xform )
	    break;
	  mm = (cls1->xnumeric.xform < cls2->xnumeric.xform? -1 : 1);
	} else
	  mm = (cls1->xnumeric.bits < cls2->xnumeric.bits? -1 : 1);
	if(mergemode) {
	  if((mergemode > 0 && mm < 0) ||
	     (mergemode < 0 && mm > 0))
	    return 0;
	} else {
	  mergemode = mm;
	}
      }
      break;
    default:
      return 0;
    }
  }

  if(i1!=NIL || i2!=NIL)
    return 0;

  return mergemode;
}

void gen_productions(FILE *f, int mask, VT tl, char *name)
{
  VT i, i2, t, tok, bss;
  int n=0, defcnt[5];

  memset(defcnt, 0, sizeof(defcnt));
  fprintf(f, "\n%s\n", (name==NULL? "opcode":name));
  LISTITER(tl, i, t) {
    int nt=0;
    xassert(t, XTEMPLATE);
    if(t->xtemplate.parent != NIL)
      continue;
#ifdef INLINE_CLASSES
    nslots = calloc(t->xtemplate.tokens->xlist.num+1, sizeof(VT));
#endif
    fprintf(f, " %c", (n++? '|':':'));
    LISTITER(t->xtemplate.tokens, i2, tok) {
      ++nt;
      if(tok->type==XCLASS) {
#ifdef INLINE_CLASSES
	VT cls1=mapget(classes,tok->xclass.str);
	if(cls1->type == XNUMERIC) {
	  nslots[nt] = cls1;
	  fprintf(f, " expr");
	} else
#endif
	fprintf(f, " c_%s", tok->xclass.str->xstring.str);
      } else if(tok->type==XNUMBER) {
	fprintf(f, " number%d", tok->xnumber.num);
      } else {
	xassert(tok, XSTRING);
	fprintf(f, " %s", tok->xstring.str);
      }
    }
    fprintf(f, "\n\t{ ");
    LISTITER(t->xtemplate.extras, i2, bss) {
      fprintf(f, "HOLD(");
      fprintf(f, ",%d); ", print_slicedexp(f, bss));
    }
    if(name==NULL) {
      if(t->xtemplate.next != NIL) {
	print_deferredgen(f, t->xtemplate.primary, defcnt);
      } else if(t->xtemplate.primary->xlist.num>0) {
	fprintf(f, "GEN(");
	fprintf(f, ",%d);", print_slicedexp(f, t->xtemplate.primary));
      }
    } else {
      if(t->xtemplate.next != NIL) {
	fprintf(stderr, "Magnitude sensitive operands in template classes not supported.\n");
	exit(1);
      }
      fprintf(f, "$$ = ");
      print_slicedexp(f, t->xtemplate.primary);
      fprintf(f, ";");
    }
    fprintf(f, " }\n");
#ifdef INLINE_CLASSES
    free(nslots);
    nslots = NULL;
#endif
  }
}

void gen_xops(FILE *f, int mask, VT tl)
{
  VT i, i2, t, t2, tok, j, bs;
  int defcnt=0;
  int *parammap;

  LISTITER(tl, i, t) {
    int nn=0;
    xassert(t, XTEMPLATE);
    if(t->xtemplate.parent != NIL)
      continue;
    if(t->xtemplate.next == NIL)
      continue;
    parammap = calloc(t->xtemplate.tokens->xlist.num+1, sizeof(int));
    
    LISTITER(t->xtemplate.primary, j, bs) {
      if(bs->xbitslice.classname!=NIL &&
	 (bs->xbitslice.classname->type == XNUMERIC ||
	  bs->xbitslice.classname->type == XENUM)) {
	parammap[bs->xbitslice.slot] = ++nn;
      }
    }
    switch (nn) {
    case 1: if(mask & M_X1) break; else continue;
    case 2: if(mask & M_X2) break; else continue;
    case 3: if(mask & M_X3) break; else continue;
    case 4: if(mask & M_X4) break; else continue;
    default: xwrong(); continue;
    }
    fprintf(f, "  case %d:\n    ", defcnt);
    for (t2 = t; t2->xtemplate.next != NIL; t2 = t2->xtemplate.next) {
      fprintf(f, "if (");
      print_slicechecknum(f, t2->xtemplate.primary, parammap, 'S');
      fprintf(f, ")\n      ");
      fprintf(f, "EMITNX(");
      fprintf(f, ",%d);", print_slicedn(f, t2->xtemplate.primary, parammap));
      fprintf(f, "\n    else\n    ");
    }
    fprintf(f, "{\n      ");
    print_slicechecknum(f, t2->xtemplate.primary, parammap, 'W');
    fprintf(f, "\n      EMITNX(");
    fprintf(f, ",%d);", print_slicedn(f, t2->xtemplate.primary, parammap));
    fprintf(f, "\n    }\n    break;\n");
    defcnt++;
    free(parammap);
  }
}

void gen_xforms(FILE *f, int mask, VT xforms)
{
  VT iter, nam, id;

  MAPITER(xforms, iter, nam, id) {
    fprintf(f, "extern numtype xform_%s(numtype);\n", nam->xstring.str);
    fprintf(f, "#define XFORM%d xform_%s\n", id->xnumber.num, nam->xstring.str);
  }

  fprintf(f, "\nnumtype be_xform(int n, numtype v)\n{\n  switch(n) {\n");
  MAPITER(xforms, iter, nam, id) {
    int i = id->xnumber.num;
    fprintf(f, "    case %d: return XFORM%d(v);\n", i, i);
  }
  fprintf(f, "    default: fprintf(stderr, \"Internal error: bex:be_xform(%%d)\\n\", n); exit(3);\n  }\n}\n\n");
}

void gen_output(FILE *f, int mask)
{
  char buf[256];
  int i;
  VT iter, tok, nam, cls;

  for(i=0; i<NUMVARS; i++)
    if(var[i].mask&mask) {
      if(var[i].mask&M_NOKEY)
	fprintf(f, "%s\n", var[i].value);
      else
	fprintf(f, "%s %s\n", var[i].name, var[i].value);
    }

  MAPITER(classes, iter, nam, cls) {
    xassert(nam, XSTRING);
    sprintf(buf, "c_%s", nam->xstring.str);
    if(cls->type==XLIST) {
      if(mask&M_TEMPLATEPROD)
	gen_productions(f, mask, cls, buf);
      if(mask&M_CLASSTYPES)
	fprintf(f, "%%type <exp> %s\n", buf);
    } else if(cls->type==XENUM) {
      if(mask&M_ENUMPROD) {
	VT i, key, val;
	int n=0;
	fprintf(f, "\n%s\n", buf);
	if(cls->xenum.def!=NIL) {
	  xassert(cls->xenum.def, XNUMBER);
	  fprintf(f, " :\n\t{ $$ = %d; }\n", cls->xenum.def->xnumber.num);
	  n++;
	}
	MAPITER(cls->xenum.map, i, key, val) {
	  xassert(key, XSTRING);
	  xassert(val, XNUMBER);
	  fprintf(f, " %c %s\n \t{ $$ = %d; }\n", (n++? '|':':'),
		  key->xstring.str, val->xnumber.num);
	}
      }
      if(mask&M_CLASSTYPES)
	fprintf(f, "%%type <num> %s\n", buf);
    } else {
      xassert(cls, XNUMERIC);
#ifndef INLINE_CLASSES
      if(mask&M_NUMPROD) {
	fprintf(f, "\n%s : expr\n", buf);
	if(cls->xnumeric.relative != -1)
	  fprintf(f, "  { $$ = checknum%c(%sMKSUB($1, MKADD(mksymbref(currloc_sym), MKICON(%d)))), %d); }\n",
		  cls->xnumeric.signedness, printxform(cls, 0),
		  cls->xnumeric.relative, cls->xnumeric.bits);	  
	else
	  fprintf(f, "  { $$ = checknum%c(%s$1), %d); }\n",
		  cls->xnumeric.signedness, printxform(cls, 0), cls->xnumeric.bits);
      }
      if(mask&M_CLASSTYPES)
	fprintf(f, "%%type <exp> %s\n", buf);      
#endif
    }
  }

  MAPITER(auto_tokens, iter, tok, nam) {
    if(mask&M_OPERRE) {
      fprintf(f, "<OPERAND>");
      printre(f, tok->xstring.str);
      fprintf(f, "\treturn %s;\n", nam->xstring.str);
    }
    if(mask&M_OPERTOK)
      fprintf(f, "%%token %s\n", nam->xstring.str);
  }

  MAPITER(opcode_tokens, iter, tok, nam) {
    if(mask&M_OPCODERE) {
      fprintf(f, "<OPCODE>");
      printre(f, tok->xstring.str);
      fprintf(f, "\t{ BEGIN(OPERAND); return %s; }\n", nam->xstring.str);
    }
    if(mask&M_OPCODETOK)
      fprintf(f, "%%token %s\n", nam->xstring.str);
  }

  MAPITER(std_tokens, iter, tok, nam) {
    if(mask&M_STDRE) {
      printre(f, tok->xstring.str);
      fprintf(f, "\treturn %s;\n", nam->xstring.str);
    }
    if(mask&M_STDTOK)
      fprintf(f, "%%token %s\n", nam->xstring.str);
  }

  if(mask&M_OPCODEPROD)
    gen_productions(f, mask, opcode_template, NULL);

  if(mask&M_EMITX)
    gen_xops(f, mask, opcode_template);

  if(mask&M_XFORMS)
    gen_xforms(f, mask, xforms);
}

struct { char *name; int mask; } tag[] = {
  { "definitions", M_DEF },
  { "operregexps", M_OPERRE },
  { "opertokens", M_OPERTOK },
  { "opcoderegexps", M_OPCODERE },
  { "opcodetokens", M_OPCODETOK },
  { "stdregexps", M_STDRE },
  { "stdtokens", M_STDTOK },
  { "opcodeproductions", M_OPCODEPROD },
  { "numericproductions", M_NUMPROD },
  { "enumproductions", M_ENUMPROD },
  { "templateproductions", M_TEMPLATEPROD },
  { "classtypes", M_CLASSTYPES },
  { "emitx1", M_EMITX|M_X1 },
  { "emitx2", M_EMITX|M_X2 },
  { "emitx3", M_EMITX|M_X3 },
  { "emitx4", M_EMITX|M_X4 },
  { "bison-options", M_BISON_OPTIONS },
  { "xforms", M_XFORMS },
};
#define NUMTAGS (sizeof(tag)/sizeof(tag[0]))

int process(char *inname, char *outname)
{
  char buf[256],name[256];
  FILE *fin, *fout;
  int rc=1;

  if((fin=fopen(inname, "r"))) {
    if((fout=fopen(outname, "w"))) {
      rc=0;
      while(fgets(buf, sizeof(buf), fin))
	if(1==sscanf(buf, " <- %s", name)) {
	  int i;
	  for(i=0; i<NUMTAGS; i++)
	    if(!strcmp(tag[i].name, name))
	      break;
	  if(i<NUMTAGS)
	    gen_output(fout, tag[i].mask);
	  else {
	    fprintf(stderr, "%s: unknown tag \"%s\".\n", inname, name);
	    rc=1;
	  }
	} else fputs(buf, fout);
      fclose(fout);
    } 
    else perror(outname);
    fclose(fin);
  }
  else perror(inname);
  return rc;
}

VT mktok(VT map, VT n)
{
  static int serial=0;
  char buf[64];
  VT name;

  xassert(n, XSTRING);
  if((name=mapget(map, n))!=NIL)
    return name;
  sprintf(buf, "T_X_%d", serial++);
  name=mkstring(poolstring(&textpool, buf));
  mapset(map, mkstring(poolstring(&textpool, n->xstring.str)), name);
  return name;
}

VT findstdtok(char *t, char **p)
{
  VT tok;
  int n=strlen(t);
  while(n) {
    if((tok=mapget(std_tokens, mktmpstr(t, n)))!=NIL) {
      if(p) *p=t+n;
      return tok;
    }
    --n;
  }
  return NIL;
}

void gentmpltokens(VT, VT, int);

VT insertvalue(VT l, char var, VT num)
{
  VT i, e, nl=mklist();
  xassert(num, XNUMBER);
  LISTITER(l, i, e) {
    xassert(e, XBITSLICE);
    if(e->xbitslice.classname!=NIL && e->xbitslice.classname->type==XSTRING &&
       e->xbitslice.classname->xstring.str[0]==var) {
      listadd(nl, mkbitslice(e->xbitslice.bits,
			     num->xnumber.num&((1l<<e->xbitslice.bits)-1),
			     NIL));
    } else if(e->xbitslice.classname!=NIL &&
	      e->xbitslice.classname->type==XCLASS &&
	      e->xbitslice.classname->xclass.str->xstring.str[strlen(e->xbitslice.classname->xclass.str->xstring.str)-1]==var) {
      char *ts = malloc(strlen(e->xbitslice.classname->xclass.str->xstring.str)+32);
      if(!ts) {
	fprintf(stderr, "Fatal: Out of memory!\n");
	exit(2);
      }
      sprintf(ts, "%.*s%d",
	      (int)(strlen(e->xbitslice.classname->xclass.str->xstring.str)-1),
	      e->xbitslice.classname->xclass.str->xstring.str,
	      num->xnumber.num);
      listadd(nl, mkbitslice(-1, 0,
			     mkclass(mkstring(poolstring(&textpool, ts)))));
      free(ts);
    } else
      listadd(nl, mkbitslice(e->xbitslice.bits, e->xbitslice.value,
			     e->xbitslice.classname));
  }
  return nl;
}

void expandglueclassvalue(VT l, VT t, char var, char var2,
			  VT num, VT num2, int level)
{
  VT e=mklist(), tk=mklist(), elt, i;
  LISTITER(t->xtemplate.extras, i, elt) {
    if(num2 != NIL)
      elt = insertvalue(elt, var2, num2);
    listadd(e, insertvalue(elt, var, num));
  }
  LISTITER(t->xtemplate.tokens, i, elt) {
    listadd(tk, elt);
  }
  elt = t->xtemplate.primary;
  if(num2 != NIL)
    elt = insertvalue(elt, var2, num2);
  gentmpltokens(l, mktemplate(insertvalue(elt, var, num), e, tk), level);
}

void expandglueclass(VT l, VT t, VT gcl, int level)
{
  VT i, e, ip=NIL, gp=NIL, pe=NIL, sp;
  VT ekey, eval;
  VT ecl;
  char var, var2;

  xassert(gcl, XENUM);
  LISTITER(t->xtemplate.tokens, i, e) {
    if(e->type==XCLASS && e->xclass.str->xstring.str[0]=='{') {
      gp=i;
      break;
    } else ip=i;
  }
  xassert(ip, XPAIR);
  xassert(gp, XPAIR);
  var=e->xclass.str->xstring.str[strlen(e->xclass.str->xstring.str)-1];

  if(ip->xpair.left->type==XCLASS) {
    VT cn;

    pe=ip->xpair.left->xclass.str;
    xassert(pe, XSTRING);
    ecl=mapget(classes, (cn=mktmpstr(pe->xstring.str,
				     strlen(pe->xstring.str)-1)));
    if(ecl==NIL) {
      errormsg("Class %s undefined!", cn->xstring.str);
      return;
    }
    if(ecl->type!=XENUM) {
      errormsg("Item before glue class must be string or enum.");
      return;
    }
    var2 = pe->xstring.str[strlen(pe->xstring.str)-1];
    /* ... */
  } else if(ip->xpair.left->type!=XSTRING) {
    errormsg("Item before glue class must be string or enum.");
    return;
  } else sp=ip->xpair.left;

  ip->xpair.right=gp->xpair.right;
  t->xtemplate.tokens->xlist.num--;
  if(t->xtemplate.tokens->xlist.tail==gp)
    t->xtemplate.tokens->xlist.tail=ip;

  if(gcl->xenum.def!=NIL)
    expandglueclassvalue(l, t, var, 0, gcl->xenum.def, NIL, level);

  MAPITER(gcl->xenum.map, i, ekey, eval) {
    xassert(ekey, XSTRING);
    if(pe!=NIL) {
      VT j, e2key, e2val;
      MAPITER(ecl->xenum.map, j, e2key, e2val) {
	char *ns=malloc(strlen(ekey->xstring.str)+
			strlen(e2key->xstring.str)+2);
	if(ns==NULL) xwrong();
	strcpy(ns, e2key->xstring.str);
	strcat(ns, ekey->xstring.str);
	ip->xpair.left=mkstring(poolstring(&textpool, ns));
	free(ns);
	expandglueclassvalue(l, t, var, var2, eval, e2val, level);
      }
    } else {
      char *ns=malloc(strlen(ekey->xstring.str)+strlen(sp->xstring.str)+2);
      if(ns==NULL) xwrong();
      strcpy(ns, sp->xstring.str);
      strcat(ns, ekey->xstring.str);
      ip->xpair.left=mkstring(poolstring(&textpool, ns));
      free(ns);
      expandglueclassvalue(l, t, var, 0, eval, NIL, level);
    }
  }
}

VT findglueclass(VT t)
{
  VT i, e, cn;

  xassert(t, XTEMPLATE);
  LISTITER(t->xtemplate.tokens, i, e) {
    if(e->type==XCLASS) {
      xassert(e->xclass.str, XSTRING);
      if(e->xclass.str->xstring.str[0]=='{') {
	VT cls=mapget(classes,
		      (cn=mktmpstr(e->xclass.str->xstring.str+1,
				   strlen(e->xclass.str->xstring.str)-2)));
	if(cls==NIL)
	  errormsg("Glue class %s undefined!", cn->xstring.str);
	return cls;
      }
    }
  }
  return NIL;
}

void gentmpltokens(VT nl, VT t, int level)
{
  VT i2, l, tok, gcl;
  int ll=level;

  if((gcl=findglueclass(t))) {
    xassert(gcl, XENUM);
    expandglueclass(nl, t, gcl, level);
    return;
  }
  xassert(t, XTEMPLATE);
  l=mklist();
  LISTITER(t->xtemplate.tokens, i2, tok) {
    if(tok->type==XNUMBER || tok->type==XCLASS)
      listadd(l, tok);
    else {
      char *p;
      int le;
      xassert(tok, XSTRING);
      p=tok->xstring.str;
      while((le=strlen(p))) {
	int pos;
	if((tok=findstdtok(p, &p))==NIL) {
	  for(pos=1; pos<le; pos++)
	    if(findstdtok(p+pos, NULL)) {
	      le=pos; break;
	    }
	  tok=mktok((ll? auto_tokens: opcode_tokens), mktmpstr(p, le));
	  p+=le;
	}
	listadd(l, tok);
      }
    }
    ll++;
  }
  t->xtemplate.tokens=l;
  listadd(nl, t);
}

VT gentokens(VT tl, int level)
{
  VT i1, t, l;

  l=mklist();
  LISTITER(tl,i1,t) {
    gentmpltokens(l, t, level);
  }
  return l;
}

int numclassbits(VT c)
{
  if(c->type==XNUMERIC) {
    return (c->xnumeric.padbits>c->xnumeric.bits?
	    c->xnumeric.padbits:c->xnumeric.bits);
  } else if(c->type==XENUM || c->type==XLIST) {
    return -1;
  } else
    xwrong();
}

int crunchbits(VT bs, int pos, int cnt, int *bpos, VT vars, char *name)
{
  int c;
  VT b, v, nn;
  if(bs==NIL)
    return cnt;
  xassert(bs, XPAIR);
  c=crunchbits(bs->xpair.right, pos, cnt, bpos, vars, name);
  b=bs->xpair.left;
  b->xbitslice.offs=*bpos;
  if(b->xbitslice.classname!=NIL && b->xbitslice.classname->type==XCLASS) {
    --c;
    v=mapget(vars, nn=mknumber(pos+c));
    if(v==NIL) {
      fprintf(stderr, "no source expression for %d in %s\n", pos+c, name);
      exit(1);
    }
    xassert(v, XPAIR);
    b->xbitslice.bits=numclassbits(v->xpair.left);
    xassert(v->xpair.right, XNUMBER);
    b->xbitslice.slot=v->xpair.right->xnumber.num;
    mapset(vars, nn, NIL);
  } else if(b->xbitslice.classname!=NIL) {
    xassert(b->xbitslice.classname, XSTRING);
    v=mapget(vars, b->xbitslice.classname);
    if(v==NIL) {
      fprintf(stderr, "no source expression for %s in %s\n",
	      b->xbitslice.classname->xstring.str, name);
      exit(1);
    }
    xassert(v, XPAIR);
    xassert(v->xpair.right, XNUMBER);
    b->xbitslice.slot=v->xpair.right->xnumber.num;
    mapset(vars, b->xbitslice.classname, NIL);
  } else {
    ;
  }
  if(b->xbitslice.classname!=NIL)
    b->xbitslice.classname=v->xpair.left;
  if(b->xbitslice.bits<0)
    *bpos=-1;
  else
    *bpos+=b->xbitslice.bits;
  return c;
}

void crunchbitslice(VT bs, int *pos, VT vars, char *name)
{
  VT i, b;
  int nc=0;
  int bpos=0;

  LISTITER(bs, i, b) {
    xassert(b, XBITSLICE);
    if(b->xbitslice.classname!=NIL && b->xbitslice.classname->type==XCLASS)
      nc++;
  }
  if(crunchbits(bs->xlist.head, *pos, nc, &bpos, vars, name)) xwrong();
  *pos+=nc;
}

VT getsliceclass(VT p, VT el, int n)
{
  VT i1, i2, e, b;

  LISTITER(p, i2, b) {
    xassert(b, XBITSLICE);
    if(b->xbitslice.classname!=NIL && b->xbitslice.classname->type==XCLASS)
      if(!--n)
	return b->xbitslice.classname;
  }
  LISTITER(el, i1, e) {
    LISTITER(e, i2, b) {
      xassert(b, XBITSLICE);
      if(b->xbitslice.classname!=NIL && b->xbitslice.classname->type==XCLASS)
	if(!--n)
	  return b->xbitslice.classname;
    }
  }
  return NIL;
}

static void xwaymerge(VT t, VT t2)
{
  int n;

  while (t->xtemplate.parent!=NIL)
    t = t->xtemplate.parent;

  if ((n = mergable(t, t2)) > 0) {
    /* t has the wider type, make it the child */
    t->xtemplate.parent = t2;
    t2->xtemplate.next = t;
    return;
  }

  while (n < 0) {
    if (!t->xtemplate.next) {
      /* t2 is the most wide type, make it the leaf */
      t->xtemplate.next = t2;
      t2->xtemplate.parent = t;
      return;
    }
    t = t->xtemplate.next;
    n = mergable(t, t2);
  }

  if (!n) {
    fprintf(stderr, "X-way merge failed\n");
    exit(1);
  }

  /* insert t2 before t */
  t2->xtemplate.parent = t->xtemplate.parent;
  t2->xtemplate.next = t;
  t->xtemplate.parent = t2;
  t2->xtemplate.parent->xtemplate.next = t2;
}

void crunchtemplate(VT tl, char *name)
{
  VT i1, t, i2, tok, bs, nam, v, ts;
  
  LISTITER(tl,i1,t) {
    VT vars=mkmapping();
    int pos=1;
    LISTITER(t->xtemplate.tokens, i2, tok) { 
      if(tok->type==XCLASS) {
	char *clsname=tok->xclass.str->xstring.str;
	int clsl=strlen(clsname), anon=0;
	VT cl;
	anon = (clsname[clsl-1]=='_');
	if(!anon && mapget(vars, mktmpstr(clsname+clsl-1, 1))!=NIL) {
	  fprintf(stderr, "nonlinear pattern variable %s in %s\n",
		  clsname+clsl-1, name);
	  exit(1);
	}
	if(!anon) ts=mkstring(poolstring(&textpool, clsname+clsl-1));
	clsname=poolstring(&textpool, clsname);
	clsname[clsl-1]='\0';
	i2->xpair.left=tok=mkclass(mkstring(clsname));
	if((cl=mapget(classes, tok->xclass.str))==NIL) {
	  fprintf(stderr, "undefined class %s used in %s\n", clsname, name);
	  exit(1);
	}
	if(!anon) mapset(vars, ts, mkpair(cl, mknumber(pos)));
      } else if(tok->type==XNUMBER) {
	VT cl, cl2;
	if(mapget(vars, tok)) {
	  fprintf(stderr, "nonlinear pattern reference %d in %s\n",
		  tok->xnumber.num, name);
	  exit(1);
	}
	if((cl=getsliceclass(t->xtemplate.primary, t->xtemplate.extras,
			     tok->xnumber.num))==NIL) {
	  fprintf(stderr, "unable to resolve pattern reference %d in %s\n",
		  tok->xnumber.num, name);
	  exit(1);	  
	}
	if((cl2=mapget(classes, cl->xclass.str))==NIL) {
	  fprintf(stderr, "undefined class %s used in %s\n",
		  cl->xclass.str->xstring.str, name);
	  exit(1);
	}
	mapset(vars, tok, mkpair(cl2, mknumber(pos)));
	i2->xpair.left=cl;
      }
      pos++;
    }
    pos=1;
    crunchbitslice(t->xtemplate.primary, &pos, vars, name);
    LISTITER(t->xtemplate.extras, i2, bs) {
      crunchbitslice(bs, &pos, vars, name);
    }
    MAPITER(vars, i2, nam, v) {
      if(v!=NIL) {
	fprintf(stderr, "Warning: ");
	xprint(stderr, nam);
	fprintf(stderr, " defined but not used in %s\n", name);
      }
    }
  }

  /* Group magnitude sensitive operands */
  LISTITER(tl,i1,t) {
    if(t->xtemplate.parent==NIL && t->xtemplate.next==NIL)
      for(i2=i1->xpair.right; i2!=NIL; i2=i2->xpair.right) {
	VT t2 = i2->xpair.left;
	int n;
	if(t2->xtemplate.parent==NIL && t2->xtemplate.next==NIL &&
	   (n=mergable(t, t2))) {
	  if(t->xtemplate.parent!=NIL || t->xtemplate.next!=NIL) {
	    xwaymerge(t, t2);
	  } else if(n>0) {
	    /* t has the wider type, make it the child */
	    t->xtemplate.parent = t2;
	    t2->xtemplate.next = t;
	  } else {
	    /* t has the narrower type, make it the parent */
	    t->xtemplate.next = t2;
	    t2->xtemplate.parent = t;
	  }
	}
      }
  }
}

void crunchenum(VT e)
{
  VT i, key, val, map=mkmapping();
  MAPITER(e->xenum.map, i, key, val) {
    mapset(map, mktok(auto_tokens, key), val);
  }
  e->xenum.map=map;
}

int varlookup(const char *name)
{
  int i;
  for(i=0; i<NUMVARS; i++)
    if(!strcmp(var[i].name, name))
      return i;
  return -1;
}

void varset(int v, const char *value)
{
  var[v].value = strdup(value);
}

int registerxform(const char *name)
{
  VT v;
  static int xfn=0;
  
  if ((v = mapget(xforms, mktmpstr(name, -1))) == NIL) {
    v = mknumber(xfn++);
    mapset(xforms, mkstring(strdup(name)), v);
  }
  return v->xnumber.num;
}

int main(int argc, char *argv[])
{
  char buf[128], *outfile=NULL;
  int i, c, rc=0;
  extern int yyparse();
  VT iter, key, val;

#ifdef DEBUG_MALLOC
  verbose_debug_exit = 0;
#endif

  ags_init();
  file_init();
  add_incdir("");

  while((c=getopt(argc, argv, "o:I:"))!=EOF)
    switch(c) {
    case 'o':
      outfile=optarg;
      break;
    case 'I':
      add_incdir(optarg);
      break;
    default:
      rc=1;
    }
  if(optind!=argc-1) rc=1;
  if(rc) {
    fprintf(stderr, "usage: asmgen [-I <incdir>] [-o <filename>] <filename>\n");
    return rc;
  }

  std_tokens=mkmapping();
  auto_tokens=mkmapping();
  opcode_tokens=mkmapping();
  for(i=0; i<NUM_STDTOK; i++)
    mapset(std_tokens, mkstring(stdtok[i].str), mkstring(stdtok[i].token));
  xforms=mkmapping();

  numerrors=0;
  if(process_file(argv[optind])) {
    yyparse();
    if(opcode_template==NULL)
      errormsg("No opcode template!\n");
  } else {
    perror(argv[i]);
    return 1;
  }
  file_end();
  if(numerrors)
    return 1;

  opcode_template = gentokens(opcode_template, 0);
  MAPITER(classes,iter,key,val) {
    if(val->type==XLIST)
      mapset(classes, key, val=gentokens(val, 1));
  }
  MAPITER(classes,iter,key,val) {
    if(val->type==XENUM)
      crunchenum(val);
  }
  crunchtemplate(opcode_template, "opcode template");
  MAPITER(classes,iter,key,val) {
    if(val->type==XLIST)
      crunchtemplate(val, key->xstring.str);
  }

  if(outfile) {
    sprintf(buf, "%s.in", outfile);
    if((rc=process(buf, outfile))) {
      if(!unlink(outfile))
	fprintf(stderr, "Destination file \"%s\" removed.\n", outfile);
      return rc;
    }
  } else {
    printf("classes=");
    xprint(stdout, classes);
    printf("\n");
    printf("template=");
    xprint(stdout, opcode_template);
    printf("\n");
  }
  return 0;
}
