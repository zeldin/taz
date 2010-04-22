/*
 * $Id: asmgen.c,v 1.2 1997-09-27 01:52:56 marcus Exp $
 *
 * $Log: asmgen.c,v $
 * Revision 1.2  1997-09-27 01:52:56  marcus
 * An empty primary is now accepted.
 *
 * Revision 1.1.1.1  1997/08/28 23:26:03  marcus
 * Imported sources
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "taz.h"
#include "asmgen.h"

VT std_tokens, auto_tokens, opcode_tokens;

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

#define FLAG_OPCODE (M_OPCODERE|M_OPCODETOK)

struct { char *name, *value; int mask; } var[] = {
  { "trailcomment", ";", M_DEF },
  { "linecomment", "[#*]", M_DEF },
  { "label", "[a-zA-Z_@.][a-zA-Z$_@.0-9]*", M_DEF }
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

void print_shifted(FILE *f, VT bs, int b)
{
  if(!b) {
    fprintf(f, "$%d", bs->xbitslice.slot);
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
  long s=0;
  int totbits=0, ne=0, no=0, cc=0, nn;

  LISTITER(bss, i, bs) {
    xassert(bs, XBITSLICE);
    if(bs->xbitslice.offs<0) xwrong();
    if(bs->xbitslice.bits<0) xwrong();
    totbits+=bs->xbitslice.bits;
    if(bs->xbitslice.classname==NIL)
      s|=bs->xbitslice.value<<bs->xbitslice.offs;
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
    if(s) { fprintf(f, "%ldl", s); cc++; }
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

void gen_productions(FILE *f, int mask, VT tl, char *name)
{
  VT i, i2, t, tok, bss;
  int n=0;

  fprintf(f, "\n%s\n", (name==NULL? "opcode":name));
  LISTITER(tl, i, t) {
    xassert(t, XTEMPLATE);
    fprintf(f, " %c", (n++? '|':':'));
    LISTITER(t->xtemplate.tokens, i2, tok) {
      if(tok->type==XCLASS) {
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
      if(t->xtemplate.primary->xlist.num>0) {
	fprintf(f, "GEN(");
	fprintf(f, ",%d);", print_slicedexp(f, t->xtemplate.primary));
      }
    } else {
      fprintf(f, "$$ = ");
      print_slicedexp(f, t->xtemplate.primary);
      fprintf(f, ";");
    }
    fprintf(f, " }\n");
  }
}

void gen_output(FILE *f, int mask)
{
  char buf[256];
  int i;
  VT iter, tok, nam, cls;

  for(i=0; i<NUMVARS; i++)
    if(var[i].mask&mask)
      fprintf(f, "%s %s\n", var[i].name, var[i].value);

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
      if(mask&M_NUMPROD) {
	fprintf(f, "\n%s : expr\n", buf);
	if(cls->xnumeric.relative != -1)
	  fprintf(f, "  { $$ = checknum%c(MKSUB($1, MKADD(mksymbref(currloc_sym), MKICON(%d))), %d); }\n",
		  cls->xnumeric.signedness, cls->xnumeric.relative,
		  cls->xnumeric.bits);	  
	else
	  fprintf(f, "  { $$ = checknum%c($1, %d); }\n",
		  cls->xnumeric.signedness, cls->xnumeric.bits);
      }
      if(mask&M_CLASSTYPES)
	fprintf(f, "%%type <exp> %s\n", buf);      
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
  { "classtypes", M_CLASSTYPES }
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

void crunchtemplate(VT tl, char *name)
{
  VT i1, t, i2, tok, bs, nam, v, ts;
  
  LISTITER(tl,i1,t) {
    VT vars=mkmapping();
    int pos=1;
    LISTITER(t->xtemplate.tokens, i2, tok) { 
      if(tok->type==XCLASS) {
	char *clsname=tok->xclass.str->xstring.str;
	int clsl=strlen(clsname);
	VT cl;
	if(mapget(vars, mktmpstr(clsname+clsl-1, 1))!=NIL) {
	  fprintf(stderr, "nonlinear pattern variable %s in %s\n",
		  clsname+clsl-1, name);
	  exit(1);
	}
	ts=mkstring(poolstring(&textpool, clsname+clsl-1));
	clsname=poolstring(&textpool, clsname);
	clsname[clsl-1]='\0';
	i2->xpair.left=tok=mkclass(mkstring(clsname));
	if((cl=mapget(classes, tok->xclass.str))==NIL) {
	  fprintf(stderr, "undefined class %s used in %s\n", clsname, name);
	  exit(1);
	}
	mapset(vars, ts, mkpair(cl, mknumber(pos)));
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
}

void crunchenum(VT e)
{
  VT i, key, val, map=mkmapping();
  MAPITER(e->xenum.map, i, key, val) {
    mapset(map, mktok(auto_tokens, key), val);
  }
  e->xenum.map=map;
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
