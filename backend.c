#include <stdio.h>

#include "taz.h"
#include "smach.h"
#include "backend.h"

int *be_holdbits;
SMV *be_holdSMVs;
int be_freehold;
static int allochold;
static int *bitbase;
static SMV *SMVbase;

void begen(SMV v, int b)
{
  int used = allochold-be_freehold;
  for(;;) {
    smach_emit(v, b);
    if(used<0)
      break;
    b = *--be_holdbits;
    v = *--be_holdSMVs;
    --used;
  }
  be_freehold = allochold+1;
  smach_flush();
}

void behold(SMV v, int b)
{
  if(allochold != 0) {
    be_freehold = allochold;
    allochold <<= 1;
    be_holdbits = (bitbase = realloc(bitbase, sizeof(*bitbase)*allochold)) +
      be_freehold;
    be_holdSMVs = (SMVbase = realloc(SMVbase, sizeof(*SMVbase)*allochold)) +
      be_freehold;
  } else {
    be_freehold = allochold = 10;
    be_holdbits = bitbase = malloc(sizeof(*bitbase)*allochold);
    be_holdSMVs = SMVbase = malloc(sizeof(*SMVbase)*allochold);
  }
  if(bitbase == NULL || SMVbase == NULL) {
    fprintf(stderr, "Fatal: Out of memory!\n");
    exit(2);
  }
  *be_holdbits++=b;
  *be_holdSMVs++=v;
}

void be_emitn(int bi, numtype n)
{
  printf("%0*lx\n", (bi+3)>>2, n);
}

void be_emiti(int by, unsigned char *p)
{
  while(by--)
    printf("%02x", *p++);
  printf("\n");
}

void backend_init()
{
  be_holdbits = bitbase = NULL;
  be_holdSMVs = SMVbase = NULL;
  allochold = 0;
  be_freehold = 1;
}

void backend_end()
{
  if(bitbase) free(bitbase);
  if(SMVbase) free(SMVbase);
}
