#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "taz.h"
#include "hashsize.h"

static struct symbol *hashtable[HASHSIZE];

struct mempool symbolpool, symbolnamepool;

static int hash(const char *s)
{
  unsigned int h=0;
  while(*s) h=h*17+*s++;
  return h%HASHSIZE;
}

struct symbol *lookup_symbol(const char *name)
{
  struct symbol *s=hashtable[hash(name)];
  while(s)
    if(!strcmp(s->name, name))
      return s;
    else
      s=s->chain;
  return NULL;
}

struct symbol *create_symbol(char *name, int type)
{
  struct symbol *s;
  if((s=lookup_symbol(name))) {
    if((type==SYMB_SET && s->type==SYMB_SET) ||
       (s->type==SYMB_UNDEF && (type==SYMB_EQU || type==SYMB_RELOC))) {
      if(s->type==SYMB_UNDEF) {
	s->type=type;
	s->defined_file=current_filename;
	s->defined_line=current_lineno;
      }
      poolfreestr(&symbolnamepool, name);
      return s;
    }
    errormsg("label %s already defined in %s line %d",
	     name, s->defined_file, s->defined_line);
    return NULL;
  } else {
    int h=hash(name);
    s=poolalloc(&symbolpool);
    s->name=name;
    s->type=type;
    s->defined_file=current_filename;
    s->defined_line=current_lineno;
    s->chain=hashtable[h];
    hashtable[h]=s;
    return s;
  }
}

void symbol_init()
{
  struct symbol *s;
  int i;

  initpool(&symbolpool, sizeof(struct symbol), 1000);
  initpool(&symbolnamepool, 1, 8000);
  for(i=0; i<HASHSIZE; i++)
    hashtable[i]=NULL;
  if((s=create_symbol(poolstring(&symbolnamepool, "__TAZ__"), SYMB_EQU))) {
    s->value.num=1;
    s->defined_file="<Init>";
    s->defined_line=0;
  }
}

void symbol_end()
{
  emptypool(&symbolpool);
  emptypool(&symbolnamepool);
}
