#define M_ICON 1
#define M_SYMB 2
#define M_ADD  3
#define M_SUB  4
#define M_AND  5
#define M_OR   6
#define M_XOR  7
#define M_SHL  8
#define M_SHR  9
#define M_MUL  10
#define M_DIV  11
#define M_MOD  12
#define M_NEG  13
#define M_CPL  14
#define M_NOT  15
#define M_LS1  16
#define M_LS2  17
#define M_CHECKU 18
#define M_CHECKS 19
#define M_CHECKW 20
#define M_CHECKX 21

#define M_MAX_BINARY  M_MOD
#define M_MAX_UNARY   M_NOT

typedef int SMV;

extern SMV mkicon(numtype);
extern SMV mksymbref(struct symbol *);
extern SMV mkunary(int, SMV);
extern SMV mkbinary(int, SMV, SMV);
extern SMV mkls(int, SMV, int);
extern SMV mkchecknum(int, SMV, int);
extern int evalconst(SMV, numtype *);
extern void smach_flush();
extern void smach_emit(SMV, int);
extern void smach_xemit(int, int, SMV, int, SMV, int);
extern void smach_xemit1(int, SMV);
extern void smach_xemit2(int, SMV, SMV);
extern void smach_xemit3(int, SMV, SMV, SMV);
extern void smach_xemit4(int, SMV, SMV, SMV, SMV);
extern void smach_setsym(struct symbol *, SMV);
extern void smach_cnop(numtype, numtype);
extern void smach_term();
extern struct symbol *smach_run();
extern void smach_init();
extern void smach_end();
extern void smach_pushfile(int);
extern void smach_popfile();
