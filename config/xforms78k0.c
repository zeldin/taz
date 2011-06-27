#include <stdio.h>
#include <string.h>

#include "taz.h"
#include "smach.h"
#include "backend.h"

numtype xform_saddr(numtype v)
{
  if(v>=0xfe20 && v<=0xff1f)
    return v&0xff;
  else
    return 0xffff;
}

numtype xform_sfr(numtype v)
{
  if(v>=0xff00 && v<=0xffff)
    return v&0xff;
  else
    return 0xffff;
}

numtype xform_swap16u(numtype v)
{
  if(v>=0 && v<=0xffff)
    return ((v&0xff)<<8)|((v&0xff00)>>8);
  else
    return v;
}

numtype xform_swap16x(numtype v)
{
  if(v>=-0x10000 && v<=0xffff)
    return ((v&0xff)<<8)|((v&0xff00)>>8);
  else
    return v;
}

numtype xform_addr11(numtype v)
{
  if(v>=0x800 && v<=0xfff)
    return (v&0xff)|((v&0x700)<<4)|0xc00;
  else
    return 0xffff;
}

numtype xform_addr5(numtype v)
{
  if(v>=0x40 && v<=0x7e && !(v&1))
    return (v>>1)&0x1f;
  else
    return 0xffff;
}
