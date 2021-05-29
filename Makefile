#

all:		csg2xml xml2mak

csg2xml:	lex.yy.c csg2xml.l
					gcc -o csg2xml lex.yy.c -lfl

xml2mak:	xml2mak.c
					gcc -Wall -O2 -o xml2mak xml2mak.c -lexpat

lex.yy.c:	csg2xml.l
					flex csg2xml.l

clean:
					rm -f csg2xml xml2mak lex.yy.c *.o
