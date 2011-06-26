#
# $Id$
#

-include config.mk

CC = gcc
LEX = flex
YACC = bison -y
RM = rm -f
CFLAGS = -g -Wall -pedantic -DDEBUG -DDEBUG_MALLOC -DTARGET=\"$(TARGET)\" # -O4711

HASHSIZE = 12000

OBJS = main.o lexer.o gram.o file.o symbol.o storage.o macro.o smach.o \
		backend.o bex.o dmalloc.o $(XFORMOBJS)

AGOBJS = asmgen.o aglexer.o aggram.o agstorage.o fileag.o storage.o dmalloc.o

all : check_target $(TARGET)

clean :
	-$(RM) *.o *~ core
	-$(RM) $(TARGET) asmgen sieve
	-$(RM) lexer.c lexer.l
	-$(RM) gram.c gram.h gram.y y.tab.c y.tab.h y.output gram.output
	-$(RM) aglexer.c aggram.c aggram.h
	-$(RM) bex.c

$(TARGET) : $(OBJS)
	$(CC) $^ -o $@

asmgen : $(AGOBJS)

asmgen.o : asmgen.c asmgen.h taz.h

agstorage.o : agstorage.c asmgen.h taz.h

aglexer.o : aglexer.c aggram.h taz.h

aggram.o : aggram.c aggram.h asmgen.h taz.h

aglexer.c : aglexer.l
	$(LEX) -t $^ > $@

aggram.c aggram.h : aggram.y
	$(YACC) -d -o aggram.c $^ 

sieve : sieve.o

hashsize.h : sieve
	echo "#define HASHSIZE `./sieve $(HASHSIZE)`" > hashsize.h

lexer.l : asmgen lexer.l.in $(ASMGENFILE)
	./asmgen -o lexer.l $(ASMGENFILE)

gram.y : asmgen gram.y.in $(ASMGENFILE)
	./asmgen -o gram.y $(ASMGENFILE)

bex.c : asmgen bex.c.in $(ASMGENFILE)
	./asmgen -o bex.c $(ASMGENFILE)

lexer.c : lexer.l
	$(LEX) -t $^ > $@

gram.c gram.h : gram.y
	$(YACC) -d -v -o gram.c $^ 

lexer.o : lexer.c gram.h taz.h smach.h

gram.o : gram.c gram.h taz.h smach.h backend.h

main.o : main.c taz.h smach.h backend.h

file.o : file.c taz.h smach.h

fileag.o : file.c taz.h
	$(CC) $(CFLAGS) -DNO_SMACH -c -o $@ $<

symbol.o : symbol.c taz.h hashsize.h

storage.o : storage.c taz.h

macro.o : macro.c taz.h

smach.o : smach.c taz.h smach.h backend.h

backend.o : backend.c taz.h smach.h backend.h

bex.o : bex.c taz.h smach.h backend.h

dmalloc.o : dmalloc.c

xforms%.o : xforms%.c taz.h smach.h backend.h

#
# Configurartion section
#

config: check_target
	@echo "Configuring for building $(TARGET)"
	@echo "TARGET=$(TARGET)" > config.mk
	@echo "ASMGENFILE=$(ASMGENFILE)" >> config.mk

check_target:
	@test -n "$(TARGET)" || (echo "No configuration selected, see README" && exit 1)

config_m68k :
	$(MAKE) TARGET=as68k ASMGENFILE=config/m68k.tab config

