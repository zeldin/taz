/*
 * $Id: old_asmgen.c,v 1.1.1.1 1997-08-28 23:26:03 marcus Exp $
 *
 * $Log: old_asmgen.c,v $
 * Revision 1.1.1.1  1997-08-28 23:26:03  marcus
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

int maxrecurse = 20;

#define CL_ENUM 0
#define CL_NUMERIC 1
#define CL_MODE 15
#define CL_PARAM 16
#define CL_INLINE 32

struct class {
  int flags;
  void *item;
} *class[256];

struct mapping {
  void **map;
  int max, def;
};

struct numeric {
  int bits, signedness;
};

struct opcode {
  char *name;
  int flags;
} *opcode;
int numop, maxop;

char **xtoken;
int num_xtok, max_xtok;

struct stdtoken { char *str, *token; } stdtok[] = {
  { "<<", "T_SHL" },
  { ">>", "T_SHR" },
  { "<=", "T_LE" },
  { ">=", "T_GE" },
  { "==", "T_EQ" },
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

struct modifier {
  int idx;
  int lsb;
};

struct production {
  char *syn;
  int bits;
  long opcode;
  int nummod;
  struct modifier *mods;
} *prods;
int numprods, maxprods;


#define M_DEF 1
#define M_OPERRE 2
#define M_OPERTOK 4
#define M_OPCODERE 8
#define M_OPCODETOK 16
#define M_STDRE 32
#define M_STDTOK 64
#define M_OPCODEPROD 128
#define M_NUMPROD 256

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

void gen_oper(FILE *f, struct class *c, char *prefix, int mask)
{
  char buf[256];
  struct mapping *m;
  struct numeric *n;
  int i;

  if(c->flags&CL_INLINE)
    return;
  if(c->flags&CL_PARAM) {
    m=c->item;
    for(i=0; i<m->max; i++)
      if(m->map[i]) {
	sprintf(buf, "%s_%d", prefix, i);
	gen_oper(f, m->map[i], buf, mask);
      }
  } else switch(c->flags) {
  case CL_ENUM:
    m=c->item;
    if(mask&M_OPERTOK)
      fprintf(f, "%%token <num> %s\n", prefix);
    for(i=0; i<m->max; i++)
      if(m->map[i]) {
	if(mask&M_OPERRE) {
	  fprintf(f, "<OPERAND>");
	  printre(f, m->map[i]);
	  fprintf(f, "\t{ yylval.num=%d; return %s; }\n", i, prefix);
	}
      }
    break;
  case CL_NUMERIC:
    n=c->item;
    if(mask&M_OPERTOK)
      fprintf(f, "%%type <num> %s\n", prefix);
    if(mask&M_NUMPROD)
      fprintf(f, "%s : expr { $$=checknum%c($1,%d); }\n",
	      prefix, n->signedness, n->bits);
    break;
  }
}

void gen_output(FILE *f, int mask)
{
  char buf[256];
  int i;

  for(i=0; i<NUMVARS; i++)
    if(var[i].mask&mask)
      fprintf(f, "%s %s\n", var[i].name, var[i].value);

  for(i=0; i<256; i++)
    if(class[i]) {
      sprintf(buf, "T_C_%d", i);
      gen_oper(f, class[i], buf, mask);
    }

  for(i=0; i<num_xtok; i++) {
    if(mask&M_OPERRE) {
      fprintf(f, "<OPERAND>");
      printre(f, xtoken[i]);
      fprintf(f, "\treturn T_X_%d;\n", i);
    }
    if(mask&M_OPERTOK)
      fprintf(f, "%%token T_X_%d\n", i);
  }

  for(i=0; i<numop; i++)
    if(opcode[i].flags&mask) {
      if(mask&M_OPCODERE) {
	fprintf(f, "<OPCODE>");
	printre(f, opcode[i].name);
	fprintf(f, "\t{ BEGIN(OPERAND); return T_O_%d; }\n", i);
      }
      if(mask&M_OPCODETOK)
	fprintf(f, "%%token T_O_%d\n", i);
    }

  for(i=0; i<NUM_STDTOK; i++) {
    if(mask&M_STDRE) {
      printre(f, stdtok[i].str);
      fprintf(f, "\treturn %s;\n", stdtok[i].token);
    }
    if(mask&M_STDTOK)
      fprintf(f, "%%token %s\n", stdtok[i].token);
  }

  for(i=0; i<numprods; i++)
    if(mask&M_OPCODEPROD) {
      int j;
      if(i) fprintf(f, " |");
      fprintf(f, "\t%s\n", prods[i].syn);
      fprintf(f, "   { emit_%d", prods[i].bits);
      fprintf(f, "(%ldl", prods[i].opcode);
      for(j=0; j<prods[i].nummod; j++)
	fprintf(f, "|($%d<<%d)", prods[i].mods[j].idx+2, prods[i].mods[j].lsb);
      fprintf(f, "); }\n");
    }
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
  { "numericproductions", M_NUMPROD }
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

int store_opcode(char *o, int flags)
{
  int i;

  for(i=0; i<numop; i++)
    if(!strcmp(opcode[i].name, o))
      if(opcode[i].flags==flags)
	return i;
      else
	return -1;
  if(numop==maxop) {
    maxop<<=1;
    opcode=realloc(opcode, sizeof(struct opcode)*maxop);
  }
  opcode[numop].name=strdup(o);
  opcode[numop].flags=flags;
  return numop++;
}

struct class *find_item(struct mapping *m, int n)
{
  if(n>=0 && n<m->max)
    return m->map[n];
  else
    return NULL;
}

void set_item(struct mapping *m, int n, void *v)
{
  if(n<0)
    return;
  if(n>=m->max) {
    int om=m->max;
    m->max<<=1;
    if(n>=m->max) m->max=n;
    m->map=realloc(m->map, m->max*sizeof(void *));
    while(om<m->max)
      m->map[om++]=NULL;
  }
  m->map[n]=v;
}

struct class *make_item(char flags)
{
  struct class *c=calloc(1, sizeof(struct class));
  switch((c->flags=flags)) {
  case CL_ENUM:
  case CL_PARAM|CL_ENUM:
  case CL_PARAM|CL_NUMERIC:
    c->item=calloc(1, sizeof(struct mapping));
    ((struct mapping *)(c->item))->map=calloc(10,sizeof(void *));
    ((struct mapping *)(c->item))->max=10;
    ((struct mapping *)(c->item))->def=-1;
    break;
  case CL_NUMERIC:
    c->item=calloc(1, sizeof(struct numeric));
    ((struct numeric *)(c->item))->bits=0;
    break;
  default:
    free(c);
    return NULL;
  }
  return c;
}

struct class *find_class(unsigned char name)
{
  return class[name];
}

struct class *make_class(unsigned char name, char flags)
{
  struct class *c;

  c=make_item(flags);
  class[name]=c;
  return c;
}

int add_class(char *s)
{
  int flags=0;
  char *a, name, id[256];
  long val,p=-1;
  struct class *z, *z2;
  struct numeric *n;

  if(*s=='@') { flags|=CL_PARAM; s++; }
  if(*s=='+') { flags|=CL_NUMERIC; s++; }
  name=*s++;
  if(isspace(name)||!isspace(*s++))
    return 1;
  if(flags&CL_PARAM)
    p=strtol(s, &s, 0);
  if(!(z=find_class(name)))
    if(!(z=make_class(name, flags)))
      return 1;
  if(z->flags != flags)
    return 1;
  if(flags&CL_PARAM) {
    flags&=~CL_PARAM;
    if(!(z2=find_item(z->item, p)))
      if(!(z2=make_item(flags)))
	return 1;
    set_item(z->item, p, z2);
    z=z2;
    if(z->flags != flags)
      return 1;
  }
  switch(flags) {
  case CL_ENUM:
    if(2!=sscanf(s, "%s %ld", id, &val))
      return 1;
    if(find_item(z->item, val))
      return 1;
    set_item(z->item, val, strdup(id));
    if((a=strrchr(s, '*')) && strspn(a+1, " \t\n")==strlen(a+1))
      if(((struct mapping *)z->item)->def>=0)
	return 1;
      else
	((struct mapping *)z->item)->def=val;
    break;
  case CL_NUMERIC:
    n=z->item;
    if(n->bits)
      return 1;
    if(2!=sscanf(s, "%ld %c", &val, id))
      return 1;
    if(!strchr("SUXW", id[0]))
      return 1;
    n->bits=val; n->signedness=id[0];
    break;
  default:
    return 1;
  }
  return 0;
}

struct param { int bits, lsb, flags, value; };

struct arg { int c; char *tok; } arg[100];

int add_production(int oc, struct param *p, struct arg *a, int n,
		   int bits, long opcode)
{
  char buf[4096];
  int i, j;

  if(numprods>=maxprods) {
    maxprods<<=1;
    prods=realloc(prods, maxprods*sizeof(struct production));
  }
  prods[numprods].bits=bits;
  prods[numprods].opcode=opcode;
  sprintf(buf, "T_O_%d", oc);
  for(i=0; i<n; i++)
    if(a[i].tok)
      sprintf(buf+strlen(buf), " %s", a[i].tok);
    else
      sprintf(buf+strlen(buf), " T_C_%d", a[i].c);
  prods[numprods].syn=strdup(buf);
  for(i=j=0; i<256; i++)
    if(p[i].flags&2)
      j++;
  prods[numprods].nummod=j;
  prods[numprods].mods=calloc(j, sizeof(struct modifier));
  for(i=j=0; i<256; i++)
    if(p[i].flags&2) {
      prods[numprods].mods[j].idx=p[i].value;
      prods[numprods].mods[j].lsb=p[i].lsb;
      j++;
    }
  numprods++;
  return 0;
}

struct stdtoken *stdtoken(char *n, int l)
{
  int i;

  for(i=0; i<NUM_STDTOK; i++)
    if(strlen(stdtok[i].str)<=l && !strncmp(stdtok[i].str, n,
					    strlen(stdtok[i].str)))
      return &stdtok[i];
  return NULL;
}

char *create_xtoken(char *n, int l)
{
  char buf[256];
  int i;

  for(i=0; i<num_xtok; i++)
    if(strlen(xtoken[i])==l && !strncmp(xtoken[i], n, l)) {
      sprintf(buf, "T_X_%d", i);
      return strdup(buf);
    }
  if(num_xtok>=max_xtok) {
    max_xtok<<=1;
    xtoken=realloc(xtoken, max_xtok*sizeof(char *));
  }
  xtoken[num_xtok]=strdup(n);
  xtoken[num_xtok][l]='\0';
  sprintf(buf, "T_X_%d", num_xtok++);
  return strdup(buf);
}

int tokenize(char *a, int l, int n)
{
  struct stdtoken *s;
  int i;

  arg[n].c=0;
  arg[n].tok=NULL;
  if(l<=0) return n;
  if((s=stdtoken(a,l))) {
    arg[n].tok=s->token;
    return tokenize(a+strlen(s->str), l-strlen(s->str), n+1);
  }
  for(i=1; i<l; i++)
    if(stdtoken(a+i, l-i))
      break;
  arg[n].tok=create_xtoken(a, i);
  return tokenize(a+i, l-i, n+1);
}

int parse_arg(char *a, struct param *p)
{
  int i, n=0;

  for(;;) {
    arg[n].c=0;
    arg[n].tok=NULL;
    for(i=0; a[i]; i++)
      if(class[(unsigned char)a[i]] || isdigit(a[i]))
	break;
    if(i) {
      n=tokenize(a, i, n);
      a+=i;
    }
    if(!*a)
      return n;
    if(isdigit(*a)) {
      a++;
    } else {
      unsigned char cl=*a++;
      unsigned char var=*a++;
      if(!var) return -1;
      if(!p[var].bits || p[var].flags)
	return -1;
      p[var].value=n;
      p[var].flags=2;
      arg[n++].c=cl;
    }    
  }
}

int store_op(char *o, int bits, long op, struct param *p, int n)
{
  char *p1, *p2, b1[256], b2[256], oo[256];
  unsigned char class, var;
  int i, rc;

  if((p1=strchr(o, '{')) && (p2=strchr(p1, '}')) && (p2==p1+3)) {
    struct mapping *m;
    struct class *c;
    strcpy(b1, o); b1[p1-o]='\0';
    strcpy(b2, p2+1);
    class=p1[1]; var=p1[2];
    c=find_class(class);
    if(!c || (c->flags&CL_MODE)!=CL_ENUM) return 1;
    c->flags|=CL_INLINE;
    m=c->item;
    if(!p[var].bits || (p[var].flags&1))
      return 1;
    p[var].flags|=1;
    for(i=0; i<m->max; i++)
      if(m->map[i]) {
	p[var].value=i;
	sprintf(oo, "%s%s%s", b1, (char *)m->map[i], b2);
	if((rc=store_op(oo, bits, op|((i&((1<<p[var].bits)-1))<<p[var].lsb),
			p, n)))
	  return rc;
	if(m->def==i) {
	  sprintf(oo, "%s%s", b1, b2);
	  if((rc=store_op(oo, bits, op|((i&((1<<p[var].bits)-1))<<p[var].lsb),
			  p, n)))
	    return rc;
	}
      }
    p[var].flags&=~1;
    return 0;
  } else {
    int oc=store_opcode(o, FLAG_OPCODE);

    for(i=0; i<256; i++)
      if(p[i].bits && !p[i].flags)
	return 1;

    return add_production(oc, p, arg, n, bits, op);
  }
}

int add_op(char *s)
{
  struct param v[256];
  unsigned char c;
  long opcode=0;
  int i, bits=0;
  char op[256], args[256];

  for(i=0; i<256; i++) {
    v[i].bits=0;
    v[i].flags=0;
  }
  while(!isspace((c=*s++))) {
    opcode<<=1;
    bits++;
    for(i=0; i<256; i++)
      if(v[i].bits)
	v[i].lsb++;
    if(isdigit(c))
      opcode|=c&1;
    else if(v[c].bits) {
      v[c].bits++;
      v[c].lsb--;
    } else {
      v[c].bits=1;
      v[c].lsb=0;
    }
  }
  while(isspace(*s)) s++;
  while(*s!=':') {
    if(!*s)
      return 1;
    s++;
  }

  if(2!=sscanf(s, ": %s %s", op, args))
    return 1;

  i=parse_arg(args, v);
  if(i<0) return 1;

  return store_op(op, bits, opcode, v, i);
}



int main(int argc, char *argv[])
{
  char buf[128], *outfile=NULL;
  int i, c, rc=0;
  extern int yyparse();

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

  for(i=0; i<256; i++) class[i]=NULL;
  numop=0; maxop=10;
  opcode=calloc(maxop, sizeof(struct opcode));
  num_xtok=0; max_xtok=10;
  xtoken=calloc(max_xtok, sizeof(char *));
  numprods=0; maxprods=10;
  prods=calloc(maxprods, sizeof(struct production));

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
