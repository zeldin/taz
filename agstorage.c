#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "taz.h"
#include "asmgen.h"

static struct mempool numpool, strpool, pairpool, mappingpool, listpool;
static struct mempool enumpool, numericpool, bitslicepool, templatepool;
static struct mempool classpool;
struct mempool textpool;

VT T;

VT classes;

void xprint(FILE *f, VT a)
{
  if(a)
    switch(a->type) {
    case XNUMBER: fprintf(f, "%d", a->xnumber.num); break;
    case XSTRING: fprintf(f, "\"%s\"", a->xstring.str); break;
    case XPAIR: fprintf(f, "( "); xprint(f, a->xpair.left);
      fprintf(f, " . "); xprint(f, a->xpair.right); fprintf(f, " )"); break;
    case XMAPPING: fprintf(f, "M{ "); a=a->xmapping.list; if(0)
      case XLIST: fprintf(f, "{ ");
	for(a=a->xlist.head; a;) {
	  xprint(f, a->xpair.left); if((a=a->xpair.right)) fprintf(f, ", ");
	} fprintf(f, " }"); break;
    case XENUM: fprintf(f, "ENUM( "); xprint(f, a->xenum.map);
      fprintf(f, ", default="); xprint(f, a->xenum.def); fprintf(f, " )");
      break;
    case XNUMERIC:
      fprintf(f, "NUMERIC( "); switch(a->xnumeric.signedness) {
      case 'X': fprintf(f, "<>"); break;
      case 'S': fprintf(f, "signed"); break;
      case 'U': fprintf(f, "unsigned"); break;
      case 'W': fprintf(f, "wraparound"); break;
      } fprintf(f, ", bits=%d, padbits=%d )", a->xnumeric.bits,
		a->xnumeric.padbits); break;
    case XBITSLICE:
      fprintf(f, "BITSLICE( %d bits @%d, value = ", a->xbitslice.bits,
	      a->xbitslice.offs);
      if(a->xbitslice.classname == NIL)
	fprintf(f, "%ld", a->xbitslice.value);
      else
	xprint(f, a->xbitslice.classname);
      fprintf(f, " )"); break;
    case XTEMPLATE:
      fprintf(f, "TEMPLATE( primary = "); xprint(f, a->xtemplate.primary);
      fprintf(f, ", extras = "); xprint(f, a->xtemplate.extras);
      fprintf(f, ", tokens = "); xprint(f, a->xtemplate.tokens);
      fprintf(f, " )"); break;
    case XCLASS:
      fprintf(f, "CLASS( "); xprint(f, a->xclass.str); fprintf(f, " )"); break;
    }
  else
    fprintf(f, "NIL");
}

int xeq(VT a, VT b)
{
  if(a==b)
    return 1;
  if((!a)||(!b)||(a->type != b->type))
    return 0;
  switch(a->type) {
  case XNUMBER: return a->xnumber.num==b->xnumber.num;
  case XSTRING: return !strcmp(a->xstring.str, b->xstring.str);
  case XPAIR: return xeq(a->xpair.left, b->xpair.left)&&
		xeq(a->xpair.right, b->xpair.right);
  case XMAPPING: return xeq(a->xmapping.list, b->xmapping.list);
  case XLIST: return a->xlist.num==b->xlist.num&&xeq(a->xlist.head, b->xlist.head); 
  default: return 0;
  }
}

VT mknumber(int n)
{
  VT num=poolalloc(&numpool);
  num->type=XNUMBER;
  num->xnumber.num=n;
  return num;
}

VT mkstring(char *s)
{
  VT str=poolalloc(&strpool);
  str->type=XSTRING;
  str->xstring.str=s;
  return str;
}

VT mkpair(VT a, VT b)
{
  VT pair=poolalloc(&pairpool);
  pair->type=XPAIR;
  pair->xpair.left=a;
  pair->xpair.right=b;
  return pair;
}

VT mkmapping()
{
  VT map=poolalloc(&mappingpool);
  map->type=XMAPPING;
  map->xmapping.list=mklist();
  return map;
}

VT mklist()
{
  VT list=poolalloc(&listpool);
  list->type=XLIST;
  list->xlist.num=0;
  list->xlist.head=NIL;
  list->xlist.tail=NIL;
  return list;
}

VT mkenum()
{
  VT e=poolalloc(&enumpool);
  e->type=XENUM;
  e->xenum.map=mkmapping();
  e->xenum.def=NIL;
  return e;
}

VT mknumeric(char s, int b, int p)
{
  VT n=poolalloc(&numericpool);
  n->type=XNUMERIC;
  n->xnumeric.signedness=s;
  n->xnumeric.bits=b;
  n->xnumeric.padbits=p;
  return n;
}

VT mkbitslice(int n, long v, VT c)
{
  VT b=poolalloc(&bitslicepool);
  b->type=XBITSLICE;
  b->xbitslice.value=v;
  b->xbitslice.bits=n;
  b->xbitslice.offs=0;
  b->xbitslice.slot=0;
  if((b->xbitslice.classname=c)!=NIL)
    if(c->type!=XSTRING && c->type!=XCLASS)
      xwrong();
  return b;
}

VT mktemplate(VT p, VT e, VT tk)
{
  VT t=poolalloc(&templatepool);
  xassert(p, XLIST);
  if(e!=NIL) xassert(e, XLIST);
  xassert(tk, XLIST);
  t->type=XTEMPLATE;
  t->xtemplate.primary=p;
  t->xtemplate.extras=e;
  t->xtemplate.tokens=tk;
  return t;
}

VT mkclass(VT s)
{
  VT c=poolalloc(&classpool);
  xassert(s, XSTRING);
  c->type=XCLASS;
  c->xclass.str=s;
  return c;
}

VT mktmpstr(char *s, int n)
{
  static char tmpbuf[256];
  static struct xstring tmpstr = { XSTRING, tmpbuf };
  if(n<0) n=strlen(s);
  if(n>(sizeof(tmpbuf)-1)) {
    s=poolstring(&textpool, s);
    s[n]='\0';
    return mkstring(s);
  }
  strncpy(tmpbuf, s, n);
  tmpbuf[n]='\0';
  return (VT)&tmpstr;
}

VT listadd(VT l, VT e)
{
  VT n;
  xassert(l, XLIST);
  l->xlist.num++;
  n=mkpair(e, NIL);
  if(l->xlist.tail)
    l->xlist.tail->xpair.right=n;
  else
    l->xlist.head=n;
  l->xlist.tail=n;
  return l;
}

void mapset(VT m, VT k, VT e)
{
  VT p;
  xassert(m, XMAPPING);
  for(p=m->xmapping.list->xlist.head; p!=NIL; p=p->xpair.right)
    if(xeq(p->xpair.left->xpair.left, k)) {
      p->xpair.left->xpair.right=e;
      return;
    }
  listadd(m->xmapping.list, mkpair(k, e));
}

VT mapget(VT m, VT k)
{
  VT p;
  xassert(m, XMAPPING);
  for(p=m->xmapping.list->xlist.head; p!=NIL; p=p->xpair.right)
    if(xeq(p->xpair.left->xpair.left, k))
      return p->xpair.left->xpair.right;
  return NIL;
}

void ags_init()
{
  initpool(&numpool, sizeof(struct xnumber), 100);
  initpool(&strpool, sizeof(struct xstring), 100);
  initpool(&pairpool, sizeof(struct xpair), 100);
  initpool(&mappingpool, sizeof(struct xmapping), 100);
  initpool(&listpool, sizeof(struct xlist), 100);
  initpool(&enumpool, sizeof(struct xenum), 100);
  initpool(&numericpool, sizeof(struct xnumeric), 100);
  initpool(&bitslicepool, sizeof(struct xbitslice), 100);
  initpool(&templatepool, sizeof(struct xtemplate), 100);
  initpool(&classpool, sizeof(struct xclass), 100);
  initpool(&textpool, 1, 8000);
  T=mkstring("T");
  classes=mkmapping();
}
