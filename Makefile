#
# $Id: Makefile,v 1.1.1.1 1997-08-28 23:26:03 marcus Exp $
#
# $Log: Makefile,v $
# Revision 1.1.1.1  1997-08-28 23:26:03  marcus
# Imported sources
#

CC = gcc
LEX = flex
YACC = bison -y
RM = rm -f
CFLAGS = -g -Wall -pedantic -DDEBUG -DDEBUG_MALLOC # -O4711

TARGET = as68k
ASMGENFILE = m68k.tab
HASHSIZE = 12000

OBJS = main.o lexer.o gram.o file.o symbol.o storage.o macro.o smach.o \
		backend.o dmalloc.o

AGOBJS = asmgen.o aglexer.o aggram.o agstorage.o file.o storage.o dmalloc.o

all : $(TARGET)

clean :
	-$(RM) *.o *~ core
	-$(RM) $(TARGET) asmgen sieve
	-$(RM) lexer.c lexer.l
	-$(RM) gram.c gram.h gram.y y.tab.c y.tab.h
	-$(RM) aglexer.c aggram.c aggram.h

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
	$(YACC) -d $^ 
	mv y.tab.c aggram.c
	mv y.tab.h aggram.h

sieve : sieve.o

hashsize.h : sieve
	echo "#define HASHSIZE `./sieve $(HASHSIZE)`" > hashsize.h

lexer.l : asmgen lexer.l.in $(ASMGENFILE)
	./asmgen -o lexer.l $(ASMGENFILE)

gram.y : asmgen gram.y.in $(ASMGENFILE)
	./asmgen -o gram.y $(ASMGENFILE)

lexer.c : lexer.l
	$(LEX) -t $^ > $@

gram.c gram.h : gram.y
	$(YACC) -d -v $^ 
	mv y.tab.c gram.c
	mv y.tab.h gram.h

lexer.o : lexer.c gram.h taz.h smach.h

gram.o : gram.c gram.h taz.h smach.h backend.h

main.o : main.c taz.h smach.h backend.h

file.o : file.c taz.h

symbol.o : symbol.c taz.h hashsize.h

storage.o : storage.c taz.h

macro.o : macro.c taz.h

smach.o : smach.c taz.h smach.h backend.h

backend.o : backend.c taz.h smach.h backend.h

dmalloc.o : dmalloc.c

