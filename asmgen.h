
union xany;

struct xnumber {
  int type;
  int num;
};
#define XNUMBER 0

struct xstring {
  int type;
  char *str;
};
#define XSTRING 1

struct xpair {
  int type;
  union xany *left, *right;
};
#define XPAIR 2

struct xmapping {
  int type;
  union xany *list;
};
#define XMAPPING 3

struct xlist {
  int type;
  int num;
  union xany *head, *tail;
};
#define XLIST 4

struct xenum {
  int type;
  union xany *map, *def;
};
#define XENUM 5

struct xnumeric {
  int type;
  char signedness;
  int bits, padbits, relative;
};
#define XNUMERIC 6

struct xbitslice {
  int type;
  long value;
  int bits, offs, slot;
  union xany *classname;
};
#define XBITSLICE 7

struct xtemplate {
  int type;
  union xany *primary, *extras, *tokens, *parent, *next;
};
#define XTEMPLATE 8

struct xclass {
  int type;
  union xany *str;
};
#define XCLASS 9

union xany {
  int type;
  struct xnumber xnumber;
  struct xstring xstring;
  struct xpair xpair;
  struct xmapping xmapping;
  struct xlist xlist;
  struct xenum xenum;
  struct xnumeric xnumeric;
  struct xbitslice xbitslice;
  struct xtemplate xtemplate;
  struct xclass xclass;
};

typedef union xany *VT;

#define NIL ((VT)0)
extern VT T;

extern void xprint(FILE *, VT);
extern int xeq(VT, VT);
extern VT mknumber(int);
extern VT mkstring(char *);
extern VT mkpair(VT, VT);
extern VT mkmapping();
extern VT mklist();
extern VT mkenum();
extern VT mknumeric(char, int, int, int);
extern VT mkbitslice(int, long, VT);
extern VT mktemplate(VT, VT, VT);
extern VT mkclass(VT);
extern VT mktmpstr(char *, int n);
extern VT listadd(VT l, VT e);
extern void mapset(VT, VT, VT);
extern VT mapget(VT, VT);
extern void ags_init();

extern VT classes, opcode_template;
extern struct mempool textpool;

#define xwrong() {fprintf(stderr,"Internal error "__FILE__" line %d\n",__LINE__);exit(1);}
#define xassert(x,y) if((!(x))||(x)->type!=(y))xwrong()else;

#define LISTITER(l,i,e) xassert((l),XLIST);for((i)=(l)->xlist.head; ((i)!=NIL)&&(((e)=(i)->xpair.left),1); (i)=(i)->xpair.right)
#define MAPITER(m,i,k,e) xassert((m),XMAPPING);for((i)=(m)->xmapping.list->xlist.head; ((i)!=NIL)&&(((k)=(i)->xpair.left->xpair.left),((e)=(i)->xpair.left->xpair.right),1); (i)=(i)->xpair.right)
