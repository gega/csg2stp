%{
#include <stdio.h>

#define STACKMAX 1000
char *stack[STACKMAX];
int stackp=0;
int mode=0;
int mc=0;
int deep=0;

#define PUSH(a) stack[stackp++]=(a)
#define POP() stack[--stackp]
#define PEEK() stack[stackp]

%}

%%


[a-z_]*[(]								{
                          if(NULL!=yytext) {
                            yytext[strlen(yytext)-1]='\0';
                            printf("<%s",yytext);
                          }
                          PUSH(strdup(yytext));
                          mode=mc=0;
                        }
[a-zA-Z$\"_][-a-zA-Z_0-9\.\"]*		{
                          if(mode==0) {
                            if(yytext[0]!='$') {
                              printf(" %s",yytext);
                              mode=1;
                            } else mode=0;
                          } else if(mode==2) {
                            printf("=\"%s%s",(yytext[0]=='"'?&yytext[1]:yytext),(yytext[0]=='"'?"":"\"")); 
                            mode=0;
                          }
                        }

\=											{ if(mode==1) mode=2;
                        }
[-0-9][-\.0-9e]*					{
                           double v;
                           sscanf(yytext,"%lf",&v);
                           if(mode==2) { printf("=\"%.16g\"",v);
                           mode=0; }
                           else if(mode==3) {
                             printf(" t%d=\"%.16g\"",++mc,v);
                           }
                           else if(mode==4) {
                             printf("%s%.16g",(mc++==0?"":","),v);
                           }
                        }

\"                      ;
\[											{ 
                          if(mode==0) mode=3;
                          else if(mode==2) { printf("=\""); mode=4; deep=1; }
                          else if(mode==4) deep++;
                        }
\]											{ 
                          if(mode==4) {
                            if(--deep==0) { printf("\""); mode=0; }
                          } else mode=0; 
                        }
\,											;
\)											;
\{                      { printf(">\n"); }
\}                      { char *s=POP(); printf("</%s>\n",s); free(s); }
;                       { printf("/>\n"); free(POP()); }
[ \t\n]+                ;

%%

int main(void) {
  printf("<csg>\n");
  yylex();
  printf("</csg>\n");
  return 0;
}
