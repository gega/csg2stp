/* gcc -Wall -O2 -o xml2mak xml2mak.c -lexpat
 *
 * csg xml to occ-csg makefile generator
 *
    This file is part of csg2stp.

    csg2stp is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as 
    published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.

    Foobar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General
    Public License along with Foobar.
    If not, see <https://www.gnu.org/licenses/>.
 *
 */
#define _GNU_SOURCE

#include <expat.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <inttypes.h>
#include <unistd.h>
#include <dirent.h>


struct trav;
struct tag;


int ac_circle(int mode, struct trav *trv, const char *fres, const char **at);
int ac_cube(int mode, struct trav *trv, const char *fres, const char **at);
int ac_cylinder(int mode, struct trav *trv, const char *fres, const char **at);
int ac_difference(int mode, struct trav *trv, const char *fres, const char **at);
int ac_union(int mode, struct trav *trv, const char *fres, const char **at);
int ac_intersection(int mode, struct trav *trv, const char *fres, const char **at);
int ac_linear_extrude(int mode, struct trav *trv, const char *fres, const char **at);
int ac_polygon(int mode, struct trav *trv, const char *fres, const char **at);
int ac_polyhedron(int mode, struct trav *trv, const char *fres, const char **at);
int ac_projection(int mode, struct trav *trv, const char *fres, const char **at);
int ac_rotate_extrude(int mode, struct trav *trv, const char *fres, const char **at);
int ac_sphere(int mode, struct trav *trv, const char *fres, const char **at);
int ac_square(int mode, struct trav *trv, const char *fres, const char **at);


#define OCCEXELONG "occ-csg"
#define OCCSWPEXELONG "occ-csg"
#define OCCEXE "occ-csg"
#define OCCSWPEXE "occ-csg"
#define MV "mv"
#define RM "rm"
#define STACKSIZE 100

#define FL_SHORT  (1L<<0)
#define FL_IGNORE (1L<<1)
#define FL_ERROR  (1L<<2)
#define FL_START  (1L<<3)
#define FL_END    (1L<<4)

#define EPSILON 0.00001
#define FEQ(a,b) (fabs((a)-(b))<EPSILON)

#define PUSH(t,a,f,i) do { \
                     if(trv->stkp>=STACKSIZE) { fprintf(stderr,"stack overflow!\n"); exit(0); } \
                     trv->stk[1+trv->stkp].tag=t; \
                     trv->stk[1+trv->stkp].num=0; \
                     trv->stk[1+trv->stkp].att=strdup(a); \
                     trv->stk[1+trv->stkp].frs=f; \
                     trv->stk[1+trv->stkp].id=i; \
                     trv->stkp++; \
                   } while(0)

#define POP(t) do { \
                     if(trv->stkp<=0) { fprintf(stderr,"stack underflow!\n"); exit(0); } \
                     if(NULL!=trv->stk[trv->stkp].att) free(trv->stk[trv->stkp].att); \
                     if(NULL!=trv->stk[trv->stkp].frs) free(trv->stk[trv->stkp].frs); \
                     --trv->stkp; \
                   } while(0)

#define PEEK(n) (&trv->stk[trv->stkp+(n)])


struct stack
{
  int num;
  const struct tag *tag;
  char *att;
  char *frs;
  int id;
};

struct trav
{
  int depth;
  int tagc;
  struct stack stk[STACKSIZE];
  int stkp;
  const char *tmp;
  const char *out;
};

struct tag
{
  char *name;
  int (*action)(int,struct trav *,const char *,const char **);
  int flags;
};


static const struct tag tags[]=
{
  { "circle", ac_circle, FL_SHORT },
  { "color", NULL, FL_IGNORE },
  { "cube", ac_cube, FL_SHORT },
  { "cylinder", ac_cylinder, FL_SHORT },
  { "difference", ac_difference, 0 },
  { "group", NULL, FL_IGNORE },
  { "hull", NULL, FL_ERROR },
  { "import", NULL, FL_ERROR },
  { "intersection", ac_intersection, 0 },
  { "linear_extrude", ac_linear_extrude, 0 },
  { "multmatrix", ac_union, 0 },
  { "polygon", ac_polygon, FL_SHORT },
  { "polyhedron", ac_polyhedron, FL_SHORT },
  { "projection", NULL, FL_ERROR },
  { "render", NULL, FL_IGNORE },
  { "rotate_extrude", ac_rotate_extrude, 0 },
  { "sphere", ac_sphere, FL_SHORT },
  { "union", ac_union, 0 },
  { "csg", NULL, 0 },
  { "square", ac_square, FL_SHORT },
  {NULL}
};


static char *fresult(struct trav *trv, int m, int num)
{
  int ln,n=0;
  char *ret=NULL;
  
  if(NULL!=trv&&trv->stkp>0)
  {
    if(num<0) num=PEEK(n)->num;
    ln=strlen(trv->tmp)+100;
    if(NULL!=(ret=malloc(ln)))
    {
      if(m==0) snprintf(ret,ln,"%d_%s%d_%s_%d.stp",       PEEK(n)->id,trv->tmp,trv->depth,PEEK(n)->tag->name,num);
      else     snprintf(ret,ln,"%d_%s%d_%s_%d_TMP_%d.stp",PEEK(n)->id,trv->tmp,trv->depth,PEEK(n)->tag->name,num,m);
    }
  }
  
  return(ret);
}

static const char *getattr(const char **at, char *nd)
{
  const char *ret=NULL;
  int i;

  if(at!=NULL)
  {
    for(i=0;at[i]!=NULL;i+=2)
    {
      if(strcmp(at[i],nd)==0) ret=at[i+1];
    }
  }
  return(ret);
}

int chain(struct trav *trv, const char *fres, const char *op)
{
  int i,n;
  char *ftmpres,*fsrc;
  
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  n=PEEK(0)->num;
  if(n>0)
  {
    fsrc=fresult(trv,1,0);
    for(i=1;i<=n;i++)
    {
      ftmpres=fresult(trv,i+1,-1);
      printf("# %d %s\n",__LINE__,__FUNCTION__);
      if(i==1) printf("$(T)/%s:\t$(T)/%s\n\t\t%s $(T)/%s $(T)/%s\n",ftmpres,fresult(trv,0,i),MV,fresult(trv,0,i),ftmpres);
      else printf("$(T)/%s:\t$(T)/%s $(T)/%s\n\t\t%s %s $(T)/%s $(T)/%s $(T)/%s 0.5 0.03\n",(i<n?ftmpres:fres),fsrc,fresult(trv,0,i),OCCEXE,op,fsrc,fresult(trv,0,i),(i<n?ftmpres:fres));
      free(fsrc);
      fsrc=ftmpres;
    }
    
    if(n==1) 
    {
      ftmpres=fresult(trv,n+1,-1);
      printf("\n$(T)/%s:\t$(T)/%s\n\t\t%s $(T)/%s $(T)/%s\n",fres,ftmpres,MV,ftmpres,fres);
      free(ftmpres);
    }
    free(fsrc);
  }

  return(0);
}

// <circle r="1.015"/>
// --create 2d:circle x,y,r
int ac_circle(int mode, struct trav *trv, const char *fres, const char **at)
{ 
  double r;

  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  r=atof(getattr(at,"r"));
  printf("$(T)/%s:\n\t\t%s --create 2d:circle 0,0,%g $(T)/%s\n",fres,OCCEXE,r,fres);

  return(0); 
}

// <cube size="0.370199,0.370199,2.5" center="true"/>
// --create box x1,y1,z1,x2,y2,z2
// --transform scale     sx,sy,sz
int ac_cube(int mode, struct trav *trv, const char *fres, const char **at)
{ 
  char *f1;

  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  
  // 1x1x1 box
  f1=fresult(trv,1,-1);
  printf("$(T)/%s:\n",fres);
  if(strcmp(getattr(at,"center"),"true")==0) printf("\t\t%s --create box -0.5,-0.5,-0.5,0.5,0.5,0.5 $(T)/%s\n",OCCEXE,f1);
  else printf("\t\t%s --create box 0,0,0,1,1,1 $(T)/%s\n",OCCEXE,f1);
  // scale to size
  printf("\t\t%s --transform scale %s $(T)/%s $(T)/%s\n",OCCEXE,getattr(at,"size"),f1,fres);
  printf("\t\t%s -f $(T)/%s\n",RM,f1);
  free(f1);

  return(0);
}

// <cylinder h="0.5" r1="0.55" r2="0.55" center="false"/>
// cylinder (1xd)  --create cyl x1,y1,z1,r,h
// cylinder (2xd)  --create cone x1,y1,z1,r1,r2,h
int ac_cylinder(int mode, struct trav *trv, const char *fres, const char **at)
{ 
  double h,r1,r2;
  char *f1;

  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  h=atof(getattr(at,"h"));
  r1=atof(getattr(at,"r1"));
  r2=atof(getattr(at,"r2"));
  f1=fresult(trv,1,-1);
  printf("$(T)/%s:\n",fres);
  if(FEQ(r1,r2)) printf("\t\t%s --create cyl 0,0,0,%g,%g $(T)/%s\n",OCCEXE,r1,h,f1);
  else printf("\t\t%s --create cone 0,0,0,%g,%g,%g $(T)/%s\n",OCCEXE,r1,r2,h,f1);
  if(strcmp(getattr(at,"center"),"true")==0) printf("\t\t%s --transform translate 0,0,%g $(T)/%s $(T)/%s\n\t\t%s $(T)/%s\n",OCCEXE,-(h/2.0),f1,fres,RM,f1);
  else printf("\t\t%s $(T)/%s $(T)/%s\n",MV,f1,fres);
  free(f1);

  return(0); 
}

//  occ-csg --csg difference file1.stp file2.stp file-out.stp
int ac_difference(int mode, struct trav *trv, const char *fres, const char **at)
{
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  if(mode==FL_END) chain(trv,fres,"--csg difference");
  return(0);
}

// convert 4x4 matrix to 4x3 matrix
static char *transmatrix(char *m)
{
  int i,n;
  if(NULL!=m) for(i=n=0;m[i]!='\0';i++) if(m[i]==','&&++n>=12) m[i]='\0';
  return(m);
}

int ac_union(int mode, struct trav *trv, const char *fres, const char **at)
{
  char *f1;
  
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  if(mode==FL_END&&PEEK(0)->num>0)
  {
    f1=fresult(trv,1,-1);
    chain(trv,f1,"--csg union");
    printf("$(T)/%s:\t$(T)/%s\n",fres,f1);
    if(NULL==PEEK(0)->att||PEEK(0)->att[0]=='\0') printf("\t\t%s $(T)/%s $(T)/%s\n",MV,f1,fres);
    else printf("\t\t%s --transform matrix %s $(T)/%s $(T)/%s\n\t\t%s $(T)/%s\n",OCCEXE,transmatrix(PEEK(0)->att),f1,fres,RM,f1);
    free(f1);
  }
  return(0);
}

int ac_intersection(int mode, struct trav *trv, const char *fres, const char **at)
{
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  if(mode==FL_END) chain(trv,fres,"--csg intersection");
  return(0);
}

// <linear_extrude height="1" center="false" convexity="1" scale="1,1">
// --create extrusion:file ex,ey,ez f.stp
int ac_linear_extrude(int mode, struct trav *trv, const char *fres, const char **at)
{
  int n;
  static int center=0;
  static double height=1;
  char *f1,*f2;
  
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  if(mode==FL_START) height=atof(getattr(at,"height"));
  if(mode==FL_START) center=(strcmp("true",getattr(at,"center"))==0?1:0);
  if(mode==FL_END&&PEEK(0)->num>0)
  {
    n=PEEK(0)->num;
    if(n>0)
    {
      f1=fresult(trv,1,-1);
      f2=fresult(trv,2,-1);
      chain(trv,f1,"--csg union");
      printf("$(T)/%s:\t$(T)/%s\n",fres,f1);
      printf("\t\t%s --create extrusion:file %g,%g,%g $(T)/%s $(T)/%s\n",OCCEXE,0.0,0.0,height,f1,f2);
      if(center)
      {
        printf("\t\t%s --transform translate 0,0,%g $(T)/%s $(T)/%s\n",OCCEXE,-height/2,f2,fres);
      }
      else
      {
        printf("\t\t%s $(T)/%s $(T)/%s\n",MV,f2,fres);
      }
      printf("\t\t%s -f $(T)/%s $(T)/%s\n",RM,f1,f2);
      free(f1);
      free(f2);
    }
  }
  return(0);
}

// <polygon points="9.417009999999999,-2.98536,8.473520000000001,-2.15644,7.97895,-2.71936,..." />
// --create 2d:polygon x1,y1,x2,y2,...                       2dpolygon.stp
int ac_polygon(int mode, struct trav *trv, const char *fres, const char **at)
{
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  printf("$(T)/%s:\n\t\t%s --create 2d:polygon %s $(T)/%s\n",fres,OCCEXE,getattr(at,"points"),fres);
  return(0);
}

// <square size="6.34,2.96" center="true"/>
// --create 2d:polygon x1,y1,x2,y2,x3,y3,x4,y4
int ac_square(int mode, struct trav *trv, const char *fres, const char **at)
{
  int center=0;
  double w,h;
  char *a;
  double x1,y1,x4,y4;
  
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  center=(strcmp("true",getattr(at,"center"))==0?1:0);
  a=strdup(getattr(at,"size"));
  w=atof(a);
  h=atof(strchr(a,',')+1);
  if(center)
  {
    x1=-w/2.0;
    y1=h/2.0;
    x4=w/2.0;
    y4=-h/2.0;
  }
  else
  {
    x1=0;
    y1=h;
    x4=w;
    y4=0;
  }
  free(a);
  printf("$(T)/%s:\n\t\t%s --create 2d:polygon %g,%g,%g,%g,%g,%g,%g,%g,%g,%g $(T)/%s\n",fres,OCCEXE,x1,y1,x4,y1,x4,y4,x1,y4,x1,y1,fres);
  return(0);
}

#define GETX(p,i) (p[i*3])
#define GETY(p,i) (p[i*3+1])
#define GETZ(p,i) (p[i*3+2])

// <polyhedron 
//    points="0,0,2.4,18.143,-8.07779,2.4,17.794,-6.83049,2.4,18.888,-6.13708,2.4,17.4122,-7.7524,3,18.5409,-7.11719,3,18.1271,-5.88986,3,0,0,3.6,18.143,-8.07779,3.6,17.794,-6.83049,3.6,18.888,-6.13708,3.6"
//    faces=",0,1,2,2,3,0,1,0,4,4,0,7,7,8,4,8,7,9,10,9,7,10,7,6,6,7,0,3,6,0,2,1,4,3,2,6,10,6,9,8,9,4,4,5,2,2,5,6,6,5,9,9,5,4"
//    convexity="5"/>
//  occ-csg --create polygons x1,y1,z1,x2,y2,z2,... p1v1,p1v2,p1v3,...:p2v1,p2v2,p2v3,... polygons.stp
int ac_polyhedron(int mode, struct trav *trv, const char *fres, const char **at)
{
  double *xyz;
  int n,i,f1,f2,f3;
  char *p,*q;
  
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);

  // read points
  p=strdup(getattr(at,"points"));
  for(n=1,q=p;*q;q++) if(*q==',') ++n;
  printf("$(T)/%s:\n\t\t%s --create polygons",fres,OCCEXE);
  xyz=calloc(n,sizeof(double));
  for(q=p-1,i=0;q!=NULL;q=strchr(q,','),i++) 
  {
    xyz[i]=atof(++q);
    printf("%c%.4g",(i==0?' ':','),xyz[i]);
  }
  
  // go on to faces
  free(p);
  p=strdup(getattr(at,"faces"));
  for(q=p,i=0;q!=NULL;i++)
  {
    q=strchr(q,',');
    if(NULL==q) break;
    f1=atoi(++q);
    q=strchr(q,',');
    if(NULL==q) break;
    f2=atoi(++q);
    q=strchr(q,',');
    if(NULL==q) break;
    f3=atoi(++q);
    printf("%c%d,%d,%d",(i==0?' ':':'),f1,f2,f3);
  }
  printf(" $(T)/%s\n",fres);
    
  free(p);
  free(xyz);
  return(0);
}

#define PI 3.14159265358979323846
#define ANGLE_360 999
// <rotate_extrude angle="360" convexity="10">
// 2d:circle - rotate/translate 2d:polygon - rotate/translate 2d:polygon ==> path.stp
// extrude over path
int ac_rotate_extrude(int mode, struct trav *trv, const char *fres, const char **at)
{
  int n;
  static double angle=0;
  char *f1,*f2;
  
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  if(mode==FL_START) angle=atof(getattr(at,"angle"));
  if(mode==FL_END&&PEEK(0)->num>0)
  {
    n=PEEK(0)->num;
    if(n>0)
    {
      f1=fresult(trv,1,-1);
      f2=fresult(trv,2,-1);
      chain(trv,f1,"--csg union");
      printf("$(T)/%s:\t$(T)/%s\n",fres,f1);
      printf("\t\t%s --transform matrix 1,0,0,0,0,2.22045e-16,-1,0,0,1,2.22045e-16,0 $(T)/%s $(T)/%s\n",OCCEXE,f1,f2);
      printf("\t\t%s --create extrusion:sweep %.2f Z $(T)/%s $(T)/%s\n",OCCSWPEXE,angle,f2,fres);
      printf("\t\t%s $(T)/%s $(T)/%s\n",RM,f1,f2);
      free(f1);
      free(f2);
    }
  }
  return(0);
}

// <sphere r="0.3"/>
//  occ-csg --create sphere x1,y1,z1,r                                sphere.stp
int ac_sphere(int mode, struct trav *trv, const char *fres, const char **at)
{
  double r;
  printf("\n# %s at %d --> %s\n",__FUNCTION__,trv->tagc,fres);
  r=atof(getattr(at,"r"));
  printf("$(T)/%s:\n\t\t%s --create sphere 0,0,0,%g $(T)/%s\n",fres,OCCEXE,r,fres);
  return(0);
}


static char *strattr(const char **at)
{
  char *ret;
  int i,l;

  ret=calloc(1,1);
  for(i=l=0;at[i]!=NULL;i+=2)
  {
    l+=strlen(at[i+1])+1;
    ret=realloc(ret,l+1);
    if(i>0) strcat(ret,",");
    strcat(ret,at[i+1]);
  }
  
  return(ret);
}

static const struct tag *findtag(const struct tag *ts, const char *tg)
{
  if(NULL!=ts) { while(NULL!=ts->name) if(strcmp(ts->name,tg)==0) break; else ts++; }
  
  return(ts);
}

static void start_element(void *dt, const char *el, const char **at)
{
  static int cnt=1000;
  struct trav *trv=(struct trav *)dt;
  const struct tag *t=findtag(tags,el);
  char *a;

  ++cnt;
  if(NULL!=t&&(t->flags&FL_IGNORE)==0)
  {
    trv->tagc++;
    if((t->flags&FL_ERROR)!=0)
    {
      fprintf(stderr,"error: tag '%s' not supported\n",el);
      printf("echo 'error: unsupported operation: %s'\n",el);
      printf("exit\n");
      exit(1);
    }
    PEEK(0)->num++;
    a=strattr(at);
    char *f=fresult(trv,0,-1);
    PUSH(t,a,f,cnt);
    free(a);
    trv->depth++;
    printf("\n");
    if(NULL!=t->action) t->action(FL_START,trv,PEEK(0)->frs,at);
  }
  else if(t==NULL)
  {
    fprintf(stderr,"***UNKNOWN tag: %s\n",el);
    exit(1);
  }
}

static void end_element(void *data, const char *el)
{
  struct trav *trv=(struct trav *)data;
  const struct tag *t=findtag(tags,el);

  if(NULL!=t&&(t->flags&FL_IGNORE)==0)
  {
    if(NULL!=t->action&&(t->flags&FL_SHORT)==0) t->action(FL_END,trv,PEEK(0)->frs,NULL);
    if(trv->depth==2) printf("\n$(T)/%s:\t$(T)/%s\n\t\t%s $(T)/%s $(T)/%s\n",trv->out,PEEK(0)->frs,MV,PEEK(0)->frs,trv->out);
    POP();
    trv->depth--;
  }
}

int parse_xml(char *csg, char *out)
{
    struct trav traversal;
    struct trav *trv=&traversal;
    int ret=0;
    
    memset(trv,0,sizeof(struct trav));
    trv->tmp="tmp_";
    trv->out="brep_result";
    if(NULL!=csg)
    {
      int ln=strlen(csg);
      printf("# makefile for %s\n\nT := $(shell mktemp -d)\n\n$(info WDIR=$(T))\n\nall:\t%s\n\n",out,out);
      XML_Parser parser = XML_ParserCreate(NULL);
      XML_SetElementHandler(parser, start_element, end_element);
      XML_SetUserData(parser, (void *)trv);

      if (XML_Parse(parser, csg, ln, XML_TRUE) == XML_STATUS_ERROR) 
      {
        fprintf(stderr,"error: %s\n", XML_ErrorString(XML_GetErrorCode(parser)));
        ret=-1;
      }
      XML_ParserFree(parser);
      printf("\n%s:\t$(T)/%s\n\t\t%s $(T)/%s %s\n",out,trv->out,MV,trv->out,out);
      printf("\t\t%s -rf $T\n",RM);
    }

    return(ret);
}

int main(int argc, char **argv)
{
  FILE *f;
  struct stat sb;
  char *csg;
  int ret=0,r;

  if(argc<3)
  {
    printf("Usage: %s csg.xml out.mak\n",argv[0]);
    exit(0);
  }
  f=fopen(argv[1],"r");
  if(f!=NULL)
  {
    if(0==fstat(fileno(f),&sb)&&sb.st_size>0)
    {
      csg=malloc(sb.st_size+1);
      if(csg!=NULL)
      {
        csg[0]='\0';
        if(0<(r=fread(csg,sizeof(char),sb.st_size,f)))
        {
          csg[r]='\0';
          parse_xml(csg,argv[2]);
        }
        free(csg);
      }
      else ret=10;
    }
    else
    {
      fprintf(stderr,"fstat failed on '%s'\n",argv[1]);
      ret=3;
    }
    fclose(f);
  }
  else 
  {
    fprintf(stderr,"failed to open csg xml file '%s'\n",argv[1]);
    ret=2;
  }
  
  return(ret);
}
