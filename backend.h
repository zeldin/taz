extern void backend_init();
extern void backend_end();
extern void begen(SMV v, int b);
extern void behold(SMV v, int b);
extern int be_freehold;
extern int *be_holdbits;
extern SMV *be_holdSMVs;

#define GEN(x,b) begen(x,b)
#define HOLD(x,b) ((--be_freehold)?(((void)(*be_holdbits++=b)),((void)(*be_holdSMVs++=x))):behold(x,b))

extern void be_emitn(int, numtype);
extern void be_emiti(int, unsigned char *);

#define EMITN(bi,n) be_emitn(bi, n)
#define EMITI(by,p) be_emiti(by, p)

#define EMIT1(n) EMITN(1,(n))
#define EMIT2(n) EMITN(2,(n))
#define EMIT3(n) EMITN(3,(n))
#define EMIT4(n) EMITN(4,(n))
#define EMIT5(n) EMITN(5,(n))
#define EMIT6(n) EMITN(6,(n))
#define EMIT7(n) EMITN(7,(n))

#define EMIT8(n) EMITN(8,(n))
#define EMIT16(n) EMITN(16,(n))
#define EMIT24(n) EMITN(24,(n))
#define EMIT32(n) EMITN(32,(n))
#define EMIT40(n) EMITN(40,(n))
#define EMIT48(n) EMITN(48,(n))
#define EMIT56(n) EMITN(56,(n))

