/* 



  ********************

   8080 model hacked to 68HC11

  *********************

  The 8080 model is register based.  For HC11 converting that to data stack based.
    data stack is 16 locations for each function indexed by X.
    push / pop is one location 0,X variable type char
    secval is 1,X      a = b - c;    b is primary, c is secal
    secval also can be an address using 0,Y
    2,X to 15,X is space for function args and local variables
    mypush() mypop() check() reduces push and pop of tempval, otherwise would need an optimization output like 
      some of the other code output modules

    missing is using the hc11 direct memory addressing modes as 8080 uses HL for memory access.  This model
    uses register Y as if it were like HL.

   * suggested setup to provide stack space to main()

#asm
  ORG $1000     * internal registers
#endasm

char REG[];     * REG[PORTE] access the PORTE register
#define PORTE 0xa


#asm
  ORG $2000
  LDX $1ff0    * locals,  temp, secval space for main(). f0 to ff.
  JMP main
#endasm

 *******************************************************************
   HL   arg pointer, primary register for ints and pointers
   A    primary register for char's
   BC   takes place of stack args
   DE   takes place of stack args
*/


#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "mcsym.h"
#include "mcout.h"
#include "mcc.h"
#include "mcvar.h"
#include "mcexp.h"

#define DBG 0

int c26= 0;  /* conditional executable for C26 code */
int progdata= 0; /* only for c26 */
int pic= 0;

int rtotal;   /* just needed for link */
int shortfun, shortcall;

/* output code dependant routines */

#define DSECT 1
#define CSECT 2

void loadhlvia();


int jumpval;

int  flags;              /* valid status flags? */
static section;
extern char hash[];
int loadoffset;      /* for call stack offset on loads */
int struct_def;      /* avoid comment on label line for structure init, add a nl() to label */
int pushed;          /* maybe avoid some push pop during function calls without using an optimizer */
int from_secval;     /* load int secval in Y reg instead of D */

/* stack optimization */
/* 
   routines must call check(HL) or check(A) 
   before using the HL or A registers 
*/
void check( int what );
void mypush( int size );
void mypop( struct EVAL *e1);


#define A 4
#define HL 2
#define BC 1       /* match offsets of fargs and auto for bc and de */
#define DE 3
#define SA 5
#define SHL 6
#define STACKSIZE 15


static int stack[STACKSIZE+1];

static int arg; /* function call offset */



void pushadd(){       /* for c26 compat */
}

void popadd(){
}


void preg( int off ){

  if( off > 14 )error("Too many locals, use globals");
  printf("%d,X",off+2+loadoffset);         /* !!! added loadoffset here */

/******
 switch( off ){
   case 0: printf("c"); break;
   case 1: printf("b"); break;
   case 2: printf("e"); break;
   case 3: printf("d"); break;
   default: error("Too many locals - use static"); 
   }   ****** */
}

void prefun(){

 check(A);
 check(HL);
 if( pushed == 0 ){
    printf("\tPSHX\t  *;_CALL\n");
    printf("\tXGDX\n");
    printf("\tSUBD\t#16\n");
    printf("\tXGDX\n");
 }
/* printf("\tpush\td\n");*/
 arg= 0;  loadoffset = 16;

}

void postfun(){

 /* printf("\tPULX\n"); */  /* delay, maybe next code is another call */
  flags= 1;                /* functions always gen status */
  pushed = 1;

}

void startasm(){

  if( pushed ) postfun2();
}

void postfun2(){

  printf("\tPULX\n");
  pushed = 0;
}

void callfun( struct EVAL *e1 ){
char *p;

 
 if( e1->type == VALUE ){
    printf("\thlpc\n");
    }
 else{
    if( e1->type == FOUND ) p= findname( &e1->var );
    else{ 
       p= &e1->var.name;
       e1->var.type= CHAR;  /* change default from int */
       }

    printf("\tJSR\t%s\n", p );
    }

 loadoffset = 0;
}


void storearg( struct EVAL *e1 ){

if( DBG ) printf("* In storearg\n" );

 /* avoid moves to self */
/* debug code only ----------- */
/* printf("scope %d  offset %d  type %d  ident %d\n",
    e1->var.scope,e1->var.offset,e1->var.type,e1->var.ident); */
/*----------------------------- */
/* this was causing problems, fixed the constants
but still have problem with expressions with a suppressed var
ie call( creg + 2 );   T80 fixes most of extra loads
 if( ( e1->var.scope == FARG || e1->var.scope == AUTO ) &&
      e1->var.offset == arg && e1->type != CONSTANT ){
    if( e1->var.type == CHAR && e1->var.ident == VAR ) ++arg;
    else arg+= 2;
    return;
    }
 */


 loadval( e1 );

 if( arg + e1->datasize  > 15 ){
    error("Exceeded local storage framesize");
    }


 if( e1->datasize == 1 ){

    printf("\tSTAB\t%d,X\n",arg+2);
 /*****
    switch( arg ){
       case 0:  printf("\tmov\tc,a\n"); break;
       case 1:  printf("\tmov\tb,a\n"); break;
       case 2:  printf("\tmov\te,a\n"); break;
       case 3:  printf("\tmov\td,a\n"); break;
       }
 ******/
    ++arg;
    }
 else{
  if( DBG ) printf("* ident %d, reg %d\n", e1->var.ident, e1->reg );

  /*  if( e1->var.ident == POINTER || e1->var.ident == ARRAY ) */
      if( e1->reg == HL )
         printf("\tSTY\t%d,X\n",arg+2);
    else printf("\tSTD\t%d,X\n",arg+2);

  /****
    switch( arg ){
       case 0: printf("\tmov\tc,l\n\tmov\tb,h\n");  break;
       case 2: printf("\tmov\te,l\n\tmov\td,h\n");  break;
       default: error("Not integer boundary");  break;
       }
  *****/
    arg+= 2;
    }
}


void pinport( struct EVAL *e1 ){

  if( e1->type != CONSTANT ) error("Expected constant");
  printf("\tin\t%d\n",e1->val);
}


void poutport( struct EVAL *e1 ){
  
  if( e1->type != CONSTANT ) error("Expected constant");
  printf("\tout\t%d\n",e1->val);
}


int loadlit( char *p ){   /* load val for string literal */

   check(HL);
   printf("\tLDY\t#%s\n",p);     /* */
   return(HL);
}


void preturn(){
extern int last_ret;
 
  if( pushed ) postfun2();
  if( last_ret == 0 ){
     if( shortfun ){
       printf("\tPULY\n");
       printf("\tSTY\t_tempY\n");
     /*  printf("\tPULX\n"); is restored by RTI sequence */
       printf("\tRTI\n");
     }
     else printf("\tRTS\n");
  }

}


void check(int reg){
int i,j;


  if( reg != A && reg != HL ) return;  /* skip push pop of HL DE BC, maybe not needed, or maybe push pop Y reg */

   for( i= 0; i < STACKSIZE; i++ ) if( stack[i] == reg ) break;
   if( i == STACKSIZE ) return;

   /* really push the value and all above it */
   if( pushed ) postfun2();

   for( j= STACKSIZE; j >= i;  j--){
      if( stack[j] == A ){
         printf("\tSTAB\t0,X\n");
         stack[j]= SA;
         }
      if( stack[j] == HL ){
         printf("\tpshy\n");
         stack[j]= SHL;
         }
      if( stack[j] == BC ){
         printf("\tpush\tb\n");
         stack[j]= SHL;         /* reg vars always popped to HL */
         }
      if( stack[j] == DE ){
         printf("\tpush\td\n");
         stack[j]= SHL;
         }
      }

}   


void mypush( int size ){
int i;

  if( stack[STACKSIZE] != 0 ) error("Mystack overflow");

  for( i= STACKSIZE ; i > 0 ; i-- ) stack[i]= stack[i-1];
  stack[0]= size;

}


void mypop(struct EVAL *e1){
int i;

   if( stack[0] == SA ) printf("\tLDAB\t0,X\n");
   if( stack[0] == SHL ){ 
      printf("\tpuly\n");
      e1->reg= HL;
      }
   /* all other regs are on stack as SHL */
   for( i= 0; i < STACKSIZE; i++) stack[i]= stack[i+1];

}


void cast( struct EVAL *e1, size ){

 if( e1->datasize == size ) return;  /* not needed */

 if( e1->type == VALUE ) {    /* only need to move loaded values */
    if( size == 2 && e1->datasize == 1 ){
       check(HL);
       printf("\tCLRA\t\t*cast\n");
     /*  printf("\tXGDY\n"); */
       }
    else if( size == 1 && e1->datasize == 2 ){
       check(A);
     /*  printf("\tXGDY\t*cast\n"); */
       printf("\tCLRA\t\t*cast\n");
       }
    /* else it may be some other size not supported */
    }

 if( e1->type == SECVAL ){     /* clear high byte */
    if( size == 2 && e1->datasize == 1 ) printf("\tmvi\td,0\n");
    }

 e1->datasize= size;   

}


void doubleval( struct EVAL *e1 ){
/* this version supports only 8 and 16 bit values */

  if( e1->datasize == 1 ) printf("\tadd\ta\n");
  else if( e1->datasize == 2 ) printf("\tdad\th\n");
  else error("Unsupported size");


}



void incdecm( int op ){

if( DBG ) printf("*In incdecm\n");

   if( op == '+' ) printf("\tINC\t0,Y\n");
   else printf("\tDEC\t0,Y\n");
}

void incdechl( int op ){

if( DBG ) printf("*In incdechl\n");
   
   check(HL);
   if( op == '+' ) printf("\tINY\n");
   else printf("\tDEY\n");
}

void incdecreg( int op, int off, size ){
  
if( DBG ) printf("*In incdecreg\n");

   if( pushed ) postfun2();
   switch ( size ){
      case 1:
      if( op == '+' ) printf("\tINC\t");
      else printf("\tDEC\t");
      break;

      case 2:
      check(off);  /* check if BC or DE need a push */
      if( op == '+' ) printf("\tINC\t");
      else printf("\tDEC\t");
      break;
      }
  preg( off );
  nl();
}


void preincdec( struct EVAL *e1, int op ){

if( DBG ) printf("*In preincdec\n");

   if( pushed ) postfun2();
switch( e1->var.ident ){
   case VAR:
   switch( e1->var.type ){
      case CHAR:
      flags= 1;
      if( e1->var.scope == FARG || e1->var.scope == AUTO ){
         incdecreg( op, e1->var.offset, 1 );
         }
      else{
         address( e1 );
         incdecm( op );
         }
      break;
                     
      case INT:
      default:    /* assume int var size  for any others */
      if( ( e1->var.scope == FARG || e1->var.scope == AUTO ) && 0 ){   /* !!! defeated, use else */
         incdecreg( op, e1->var.offset, 2 );    /* only inc's the high byte instead of int */
         }
      else{
         loadval( e1 );
         incdechl( op );
         store( e1,0 );
         }
      break;
            
      }  /* end var.type */
      break;

   case POINTER:
   /* get size of whats pointed to */
   /* assume just simple vars for now */
   if( ( e1->var.scope == FARG || e1->var.scope == AUTO ) && 0 ){ /* !!! skipped code, use else */
      incdecreg( op, e1->var.offset, 2 );
      if( e1->var.type != CHAR ) incdecreg( op, e1->var.offset, 2 );
      }
   else{
      loadval( e1 );
      incdechl( op );
      if( e1->var.type != CHAR ) incdechl( op );
      store( e1,0 );
      }
   break;

   default:  error("Can't inc-dec this type"); break;

   }   /* end var.ident */
 

}





void postincdec( struct EVAL *e1, int op ){

if( DBG ) printf("*In postincdec\n");

    flags = 1;   /* !!! try this for while( var-- ) */
   if( pushed ) postfun2();

switch( e1->var.ident ){
   case VAR:
   switch( e1->var.type ){
      case CHAR:
      if( e1->var.scope == FARG || e1->var.scope == AUTO ){
         loadval( e1 );
         incdecreg( op, e1->var.offset, 1 );
         printf("\tTSTB\n");                 /* post inc/dec changes the flags */
         }
      else{
         address( e1 );
         loadval( e1 );
         incdecm( op );
         printf("\tTSTB\n");                 /* post inc/dec changes the flags */
         }
      break;
                     
      case INT:
      default:    /* assume int var size */
      if( e1->var.scope == FARG || e1->var.scope == AUTO ){
         loadval( e1 );
         push( e1 );
         incdecreg( op, e1->var.offset, 2 );
    /*     pop( e1 );   */
         }
      else{
         loadval( e1 );
         push(e1);
         incdechl( op );
         store( e1,0 );
         pop(e1);
         }
      break;
            
      }  /* end var.type */
      break;

   case POINTER:
   /* get size of whats pointed to */
   /* assume just simple vars for now */

   if( ( e1->var.scope == FARG || e1->var.scope == AUTO ) && 0){  /* !!! defeated */
      loadadd( e1 );
      push( e1 );
      incdecreg( op, e1->var.offset, 2 );
      if( e1->var.type != CHAR ) incdecreg( op, e1->var.offset, 2 );
  /*    pop( e1 );   */
      }
   else{
      loadadd( e1 );
      e1->type= ADDRESS;     /* fake out push */
      push( e1 );
      e1->type= VALUE;       /* call value for store */
      incdechl( op );
      if( e1->var.type != CHAR ) incdechl( op );
      store( e1,0 );
     /* pop( e1 );   */          /* want it to remain value for now */
      e1->type = STACKVAL;       /* this may be better, fix out of order load and push *p1++ = *p2++ */
      }
   break;

   default:  error("Can't inc-dec this type"); break;

   }   /* end var.ident */
 
}


void negate(){
   compliment();
   printf("\tADDD\t#1\n");
}

void compliment(){
   printf("\tCOMA\n");
   printf("\tCOMB\n");
}


void cpi( long val ){   /* compare immediate */
   printf("\tCMPB\t#%d\n",(int)val);
   flags= 1;
}


void  status(struct EVAL * e1){        /* generate flag status */
   
   if( flags == 0 ){ 
      loadval(e1);
    /*  printf("\tora\ta\n");  */
      }
   flags= 1;
}


void storei( struct EVAL *e1 ){
char *p;

 /* assume int type */
  switch( e1->var.scope ){
     case FARG:
     case AUTO:
       if( pushed && loadoffset == 0 ) postfun2();

     /**************
        if( e1->var.offset == 0 ){
           printf("\tmov\tc,l\n");
           printf("\tmov\tb,h\n");
           }
        else{
           printf("\tmov\te,l\n");
           printf("\tmov\td,h\n");
           }
    ******************/
        if( e1->var.ident == POINTER || e1->var.ident == ARRAY )
             printf("\tSTY\t%d,X\n",e1->var.offset+2);
        else printf("\tSTD\t%d,X\n",e1->var.offset+2);  
        break;

     case STATIC:  error("Use globals");
     case GLOBAL:
     case EXTERN:
        p= findname(&e1->var);
       if( e1->var.ident == POINTER || e1->var.ident == ARRAY ) printf("\tSTY\t");
       else printf("\tSTD\t");
        printf("%s",p);
               if( e1->val ) printf("+%d",e1->val);
               nl();
        break;
     }
     e1->type= VALUE;
}


void store( struct EVAL * e1, int reg ){
char *p;

   if( e1->var.type != CHAR || 
      ( e1->var.type == CHAR &&  e1->var.ident == POINTER &&
        e1->var.indirect != 0 ) ) {
        storei( e1 );
        return;
        }

   /* rest is for chars */

   switch( e1->type ){
      case VALUE:   
      case CONSTANT: error("can't store to that"); break;
      case TEMPVAL:
      case STACKVAL:
      case STACKADD:
         pop( e1 );         /* nobreak */
      case ADDRESS: 
         switch( e1->reg ){
           case HL: printf("\tSTAB\t0,Y\n");  break;
           case BC: printf("\tstax\tb\n"); break;
           case DE: printf("\tstax\td\n"); break;
           }
         break;
      case FOUND:
         switch( e1->var.scope ){
            case STATIC:  error("Use globals");
            case GLOBAL:
            case EXTERN:
               p= findname( &e1->var );
               printf("\tSTAB\t%s",p);
               if( e1->val ) printf("+%d",e1->val);
               nl();
               break;
            case FARG:
            case AUTO:
               if( pushed && loadoffset == 0 ) postfun2();
               if(e1->var.offset > 8) error("Too many locals");
               printf("\tSTAB\t");
               printf("%d",e1->var.offset+2);
               printf(",X\n");
               break;
            }
      /*   break; */
      } /* end switch type */
   /* think want to say its has value now even though its probably address */
   e1->type= VALUE;
}


void tempval( struct EVAL * e1 ){   /* store value on stack */

    if( e1->type == CONSTANT ) return;  /* don't need to store constant */
    loadval( e1 );
    push( e1 );
}


/*static tempcount = 0;*/

void push( struct EVAL * e1){


    if( e1->datasize == 1  ){
       mypush( A );   
       }
    else mypush( e1->reg ); 

    if( e1->type == VALUE )  e1->type= STACKVAL;

 /*    if( tempcount > 1 ) error("Out of temp/stack");
    printf("\tSTAB\t%d,X\n",tempcount); */


 /*   e1->var.offset = tempcount; 
    ++tempcount;

    if( e1->datasize != 1 ) error("push int not implemented");
*/
}

void pop(struct EVAL *e1 ){

  mypop( e1); 

/*
  printf("\tLDAB\t%d,X\n",e1->var.offset);
  if( e1->type == STACKVAL ) e1->type= VALUE; 

  if( tempcount == 0 ) error("Too many pops");
  else --tempcount;
*/
}




void loadval( struct EVAL * e1 ){
char *p;   
int mysize, save;

if( DBG ) printf("* In loadval\n");

  mysize= e1->datasize;
  switch( e1->type ){
     case VALUE:             
     case FUNCALL:
        break;

     case STACKVAL:
        pop(e1);
        break;
     case CONSTANT:
     if( e1->val > 255 || e1->val < -128 ){  /* int size */
         check(HL); mysize= SOINT;
         printf("\tLDD\t#%d\n",e1->val);
         }
     else{
        check(A);  mysize= SOCHAR;
        printf("\tLDAB\t#%d\n",e1->val);
        }
        break;
     case STACKADD:  pop( e1 );  /* nobreak */
     case ADDRESS:
        if( e1->var.ident == VAR && e1->var.type == CHAR ){ 
           check(A);  mysize= SOCHAR;
           switch( e1->reg ){
              case BC:
              case DE:
              case HL: printf("\tLDAB\t0,Y\n"); break;
            /*  case BC: printf("\tldax\tb\n"); break;
              case DE: printf("\tldax\td\n"); break;
            */
              }
           }
        else{ 
           loadhlvia();
           mysize= SOINT;
           }
        break;
     case FOUND:
        switch( e1->var.scope ){
           case STATIC:    error("Use globals");
           case GLOBAL:
           case EXTERN:
              p= findname( &e1->var );
              if( e1->var.type == CHAR && e1->var.ident == VAR ){
                 check(A);  mysize= SOCHAR;
                 printf("\tLDAB\t%s",p);
                 if( e1->val ) printf("+%d",e1->val );
                 nl();
                 }
              else if( e1->var.ident == ARRAY ){ 
                 address( e1 );
                 mysize= SOP;
                 }
              else{ 
                 check(HL); mysize= SOINT;
                 if( e1->var.ident == POINTER || from_secval ) printf("\tLDY\t%s",p) , e1->reg = HL;
                 else printf("\tLDD\t%s",p );
                 if(e1->val) printf("+%d",e1->val);
                 nl();
                 }
              break;
           case FARG:
           case AUTO:
              if( pushed && loadoffset == 0 ) postfun2();
              if( e1->var.ident == VAR && e1->var.type == CHAR ){
                 check(A); mysize= SOCHAR;
                 printf("\tLDAB\t%d,X",e1->var.offset+2+loadoffset);
               /*  preg( e1->var.offset ); */
                 nl();
                 }
              else if( e1->var.ident == ARRAY ) 
                    error("Register can't be array");
              else{ 
                 check(HL);
                 if( e1->var.ident == POINTER || from_secval ){
                      printf("\tLDY\t%d,X",e1->var.offset+2+loadoffset);
                      e1->reg = HL;
                 }
                 else printf("\tLDD\t%d,X",e1->var.offset+2+loadoffset);
               /*  preg( e1->var.offset ); */
                 nl();
               /* !!! assume pointer addition and its backwards */
                /* printf("\tXGDY\n"); */
              /*  **** printf("\tmov\th,");  
                 preg(e1->var.offset);
                 nl();****  */
                 mysize= SOINT;   
                 }
              break;
           } /* end scope */
           break;

     }  /* end switch type */

  e1->type = VALUE;
  flags= 1;            /* !!! was zero */

  if( e1->datasize == 0 ) e1->datasize= mysize;
  else if( mysize != e1->datasize ){  /* gen cast code */
     save= e1->datasize;
     e1->datasize = mysize;      /* set to size actual loaded */
     cast( e1, save );           /* and change it back */
     }

  from_secval = 0;
}

     
/* loadadd - much same as loadval except can use other than primary reg */
void loadadd( struct EVAL * e1 ){
char *p;

 if( DBG )printf("*In loadadd()\n");

 switch(e1->type ) {
   case FOUND:
   switch( e1->var.scope ){
      case AUTO:
      case FARG:
       if( pushed && loadoffset == 0 ) postfun2();
       printf("\tLDY\t%d,X\n",e1->var.offset+2);
      /* e1->reg= e1->var.offset + 1; */ /*just say its in a register */
       e1->reg = HL;
      break;

      default:   /* globals etc.. */
      check(HL); 
      p= findname( &e1->var );
      printf("\tLDY\t%s\n",p);
      e1->reg= HL;
      break;
      }
   break;

   case STACKADD:
   case STACKVAL:
      pop( e1 );      /* no break */
   case ADDRESS:
   case VALUE:
      switch( e1->reg ){
         case HL:  loadhlvia(); break;

         case BC:
         case DE:   
         check(HL);
         printf("\tpush\t");  preg( e1->reg ); nl();
         printf("\tpop\th\n");
         loadhlvia();
         break;
         }
   break;
   }   
 
 e1->type= VALUE;
}


     
     
void loadhlvia() {     /* load HL via HL */

check(HL);      /* in case we want to save the address */
printf("\tpush\td\n");

  printf("\tmov\te,m\n");
  printf("\tinx\th\n");
  printf("\tmov\td,m\n");
  printf("\txchg\n");

printf("\tpop\td\n");     

}

void storesvia(){     /* store stack via HL */
  printf("\txchg\n");
  printf("\txthl\n");
  printf("\txchg\n");
  printf("\tmov\tm,e\n");
  printf("\tinx\th\n");
  printf("\tmov\tm,d\n");
  printf("\tpop\td\n"); 
}



void address( struct EVAL * e1 ){
char *p;

  if( e1->type == ADDRESS ) return;

  check(HL);
  switch( e1->var.scope ){
     case FARG:
     case AUTO:
        /* error("Can't form address of stack variable"); */
        printf("* Suggest using different form of expression\n");
        if( pushed && loadoffset == 0 ) postfun2();
        check(A);
        printf("\txgdx\n");
        printf("\taddd\t#%d\n",e1->var.offset+2);
        printf("\txgdy\n");
        break;

     case STATIC:    error("Use globals");
     case GLOBAL:
     case EXTERN:
        p= findname( &e1->var );
        printf("\tLDY\t#%s",p);
        if( e1->val ) printf("+%d",e1->val);
        nl();
        break;

     }
  e1->type= ADDRESS;
  e1->reg= HL;
}



void secval( struct EVAL * e1 ){


if( DBG ) printf("* In secval size%d\n",e1->datasize );

if( e1->type == SECVAL ) return;
switch (e1->datasize){
   case 0:   /* unknown assume char in this case */
   case 1:

   if( e1->type == FUNCALL ) e1->type = VALUE;

   /* constants and address loaded do not need data loaded */
   if( e1->type == CONSTANT ) return;
   if( e1->type == FOUND ){
      if( e1->var.scope == FARG || e1->var.scope == AUTO ){
         if( pushed ) postfun2();
         e1->type= REGVAL;
         return;
         }
      else { 
         address( e1 );
         return;
         }
      }
   if( e1->type == ADDRESS ){ 
      if( e1->reg == HL )return;
      else{
         check(HL);
         printf("\tmov\tl,");
         preg( e1->reg -1);   nl();
         printf("\tmov\th,");
         preg( e1->reg );   nl();
         e1->reg= HL;
         return;
         }
      }
   if( e1->type == VALUE ) printf("\tSTAB\t1,X\n");
      
   if( e1->type == STACKVAL ){ 
      pop( e1 );
      printf("\tSTAB\t1,X\n");
      }
   break;

   case 2:
/*   printf("\tpush\td\n"); */
   switch( e1->type ){
      case CONSTANT: printf("\tLDY\t#%d\n",( int ) e1->val);   /*  */
                     e1->reg = HL;                            /* !!! new 2/27 */
      break;

      case STACKVAL: printf("\txchg\n");  
      pop( e1 );  
      printf("\txthl\n");  /* no break */
      case VALUE : /* printf("\txchg\n"); */ 
      break;

      case FOUND:   
      switch( e1->var.scope ){
         case STATIC:      error("Use globals");
         case GLOBAL:
         case FARG:
         case AUTO:
         case EXTERN:
              from_secval = 1;
              loadval( e1 );  /*printf("\txchg\n");*/
             /* this loads in Y for pointer add, array[] code */
          /*   if( e1->var.ident == POINTER || e1->var.ident == ARRAY ) loadval( e1 );
             else address( e1 ); this breaks loading the index of the array in reg B */
         break;
         
         default:

     /*    printf("\tLDY\t");     loads char farg as double
         preg(e1->var.offset);
         nl(); */
     /*  ***
         if( e1->var.ident == VAR && e1->var.type == CHAR ){   /* was cast 
            printf("\tmvi\td,0\n");
            }
         else{
            printf("\tmov\td,");
            preg(e1->var.offset);
            nl();
            }
     *** */
         break;
         }
      break;

      case ADDRESS: 
      printf("\tmov\te,m\n"); 
      if( e1->var.ident == VAR && e1->var.type == CHAR ){  /* was cast */
         printf("\tmvi\td,0\n");
         }
      else{
         printf("\tinx\th\n"); 
         printf("\tmov\td,m\n");  
         }
      break;
      }
   break;  /* case size 2 */

   default: error("Type not supported");
   }
e1->type= SECVAL;
  
}


void doopstore( struct EVAL * e1, struct EVAL * e2, int op ){
}



void doop( struct EVAL * e1, struct EVAL * e2, int op ){
int mask;
int count;

if( DBG ) printf("* In doop\n");

   if( e1->datasize > e2->datasize ) cast( e2, e1->datasize );
   else if( e2->datasize > e1->datasize ) cast( e1, e2->datasize);
   
check(HL);                    /* 2/27 */
  secval( e2 );
if( e2->reg == HL && ( op == '+' || op == '-' ) ){          /* 2/27  2/28 */
  printf("\tSTY\t_tempY\n");
}
  loadval( e1 );

if( e1->datasize < 2 ){           /* char code */
   mask= 0;
   if( e2->type == CONSTANT ){
      switch( op ){
         case '+': printf("\tADDB"); break;
         case '-': printf("\tSUBB"); break;
         case '&': printf("\tANDB"); break;
         case '|': printf("\tORAB"); break;
         case '^': printf("\tEORB"); break;
         /* shifts are constant only for 8080 */
         case '>':
            mask= 255;
            if( ( count= (int) e2->val ) > 7 ) error("Too many shifts");   /* zero result */
            else{
                while( count-- ){ 
                   printf("\tLSRB\n");
                   mask >>= 1;
                   }
               /* printf("\tani\t%d\n",mask); */
                }
            break;
         case '<':
            mask= 255;
            if( ( count= (int) e2->val ) > 7 )  error("Too many shifts");
            else{
                while( count-- ){ 
                   printf("\tLSLB\n");
                   mask <<= 1;
                   }
               /* printf("\tani\t%d\n",mask & 255); */
                }
            break;
            
         default: error("Not supported");    /*   * / %   */
         }
      if( mask == 0 ) printf("\t#%d\n",( int )e2->val);
      }

   else{       /* not immediate op */
      switch( op ){
         case '+': printf("\tADDB\t"); break;
         case '-': printf("\tSUBB\t"); break;
         case '&': printf("\tANDB\t"); break;
         case '|': printf("\tORAB\t"); break;
         case '^': printf("\tEORB\t"); break;
         default: error("(* / %) Not supported");    /*   * / %   */
         }
      
      if( e2->type == ADDRESS ) printf("0,Y\n");
      else if( e2->type == REGVAL ){
         preg( e2->var.offset );
         nl();
         }
      else printf("1,X\n");    /* secval assumed */           
      }

   flags= 1;
   }

else if( e1->datasize == 2 ) {  /* int and pointer code 8 + 16 */
   switch(op){
      case '+':
         if( e1->var.ident == POINTER || e1->var.ident == ARRAY ){
            if( e2->reg == HL ) printf("\tLDD\t_tempY\n");
            printf("\tABY\n");
         }
         else{
          /* printf("\tSTY\t_tempY\n");  2/27 */
           printf("\tADDD\t_tempY\n");
         }
      break; /* was dad d */
      case '-':
        /* printf("\tSTY\t_tempY\n"); */
         printf("\tSUBD\t_tempY\n");
      break;
      case '>':
      case '<':
         if( op == '<' ) printf("\tLSLD\n");
         else printf("\tLSRD\n");
         printf("\tDEY\n");
         printf("\tBNE\t*-3\n");
      break;
      default: error("Only add/sub/shift supported");
      }
   flags= 0;
 /*  printf("\tpop\td\n"); */
   e1->reg= HL;
   }  /* end int code */


}







void compare( struct EVAL * e1, struct EVAL * e2 ){
struct EVAL * temp;
int swap;

/* 8080 can not easily do JLE and JG so swap for these */
   swap= 0;
/********
   if( jumpval == JLE || jumpval == JG ){
      swap= 1;
      temp= e1;   e1= e2;   e2= temp;     /* swap pointers and jump type 
      if( jumpval == JG ) jumpval= JL;
      else jumpval= JGE;
      }
**********/

   secval( e2 );
   loadval( e1 );
   switch( e2->type ){
      case CONSTANT:
         printf("\tCMPB\t#%d\n",e2->val);
         break;
      case ADDRESS:
         printf("\tCMPB\t0,Y\n");
         break;
      
      case REGVAL:
         printf("\tcmpB\t");
         preg( e2->var.offset );
         nl();
         break;
      case SECVAL:
         printf("\tcmpB\t1,X\n");
         break;
      default:
         error("Unknown type");
      }

   flags= 1;
/* say e1 is loaded even if it wasn't, if there was a swap */
   if( swap ) e2->type = VALUE;   /* e2 is really e1 */
}



void pad( int count ){   /* output DB to fill in array */
int i;

  if( count <= 0 ) return;
  while( count ){
     printf("\tFCB ");
     for( i= 0; i < 10 ; i++ ){
        printf("0");
        --count;
        if( count && i < 9 ) printf(",");
        if( count == 0 ) break;
        }
     printf("\n");
     }
}

void dw( char *p ){      /* output string as data word */
   printf("\tFCC | %s |\n",p);
}



/* output literal as series of bytes terminated in a zero */
int outlit( char *p ){  
int count;
int i, c;  

  i= strlen( p );     /* get rid of the "" */
  if( i ){ 
     p[i-1]= 0;
     ++p;
     }
  count= 0;
  while( *p ){
     printf("\tFCB ");
     for( i= 0; i < 10 ; i++ ){
        c= *p;
        if( c == '\\' ){
           c= *++p;
           switch( tolower(c) ){
              case 't': c= '\t'; break;
              case 'r': c= '\r'; break;
              case '\\': c= '\\'; break; 
              case '\'': c= '\''; break;
              case 'n': c= '\n'; break;
              case 'a': c= '\a'; break;
              case 'b': c= '\b'; break;
              case 'f': c= '\f'; break;
              case 'v': c= '\v'; break;
              }
         }
         printf("%d",c);
         ++count;
         if( c == 0 ) break;
         if( i < 9 )printf(",");
         ++p;
         }
     printf("\n");
     }
  if( c != 0 ) printf("\tFCB 0\n");
  return count;
}

void dbytes( long val, int size ){ /* output as list of DB's lsb to msb */
long div= 1;   
long i;

  dbytes_big( val, size );
  return; 

   printf("\tFCB ");
   while( size ){
      i= (val/div) % 256;
      printf("%ld",i);
      val-= i;
      div*= 256;
      if( --size ) printf(",");
      }
   nl();
}

void dbytes_big( long val, int size ){   /* output in big endian order for 68hc11 */
long div;
long i;

   i = size;
   div = 1;
   while( --i ) div *= 256;
   printf("\tFCB ");
   while( size ){
      i = val/div;
      val = val % div;
      printf("%ld",i);
      div /= 256;
      if( --size ) printf(",");
   }
   nl();   

}



void comment( char *p ){  /* print as comment */

  printf("* %s\n",p);
  if( strstr( p, "struct" ) ) struct_def = 1;
}


void dsect(){  /* output to data section */

   if( section != DSECT ){ 
      printf("* : DSECT\n");
      section= DSECT;
      }
}

void csect(){ /* output to code section */
   if( section != CSECT ){ 
      printf("* : CSECT\n");
      section= CSECT;
      }
}

void nl(){           /* new output line */
   printf("\n");
}

void plabel( char *p ){    /* print a label */
extern int last_ret;
   last_ret = 0;
   if( pushed ) postfun2();
   printf("%s: ",p);
   if( struct_def ) nl(), struct_def = 0;
   shortfun = ( *p == '_' ) ? 1 : 0;
}

void pilabel( int num ){   /* print a label from number */
extern int last_ret;
   last_ret= 0;
   if( pushed ) postfun2();
   printf("%s%d:\n",hash,num);
}


void ds( int size ){       /* print a Date Space directive */
   printf("\tBSZ %d\n",size);

}

void jump( int lab){             /* jump */

   if( pushed ) postfun2();

   if( jumpval != ALWAYS ){   /* gen status flags for jump */
      if( flags == 0 ){
         printf("\tstaB\t0,X\n");
       /*  printf("\toraB\t0,X\n");  store wll set flags but even that not needed perhaps */
         flags= 1;
         }
      }

   switch( jumpval ){
      case JE:   printf("\tBEQ");   break;
      case JNE:  printf("\tBNE");  break;
      case JL:   printf("\tBLO");   break;
      case JGE:  printf("\tBGE");  break;
      case ALWAYS:  printf("\tJMP");  break;
      /* not used - compare swaps for another */
      case JLE:   printf("\tBLS"); break;   /* jc label, jz label */
      case JG:    printf("\tBHI"); break;   /* jc no, jz no, jmp label, no: */
           error("Internal code error - jump");
      }

   printf("\t%s%d\n",hash,lab);
   flags= 0;
}


void outline( char *p ){  /* just print out the line */

  printf("%s",p);

}

void pjump( char *p ){  /* print jump to label, used by goto */

  printf("\tjmp\t%s\n", p);
}


void pextern( char *p ) {

  printf("\textrn\t%s\n", p);
}

void end(){

   printf("\n*;end\n");
}


void dumpdata(){

   printf("globals:\n*;DSECT\n");

}

void center(char *p){
  plabel( p );
  nl();

  if( shortfun ){
     /* printf("\tPSHX\n");  is saved by interrupt sequence */
      printf("\tXGDX\n");
      printf("\tSUBD  #16\n");
      printf("\tXGDX\n");
      printf("\tLDY\t_tempY\n");
      printf("\tPSHY\n");
  }
     
}

void rel( int d ){

}

void stackspace(){

}

void stackvar( int size ){

}

void pointeradd( struct EVAL *e1, struct EVAL *e2, int op ){
struct SYMBOL one;
struct EVAL e3;
int s;

if( DBG ) printf("*In pointeradd\n");


 /* just regular if both are pointers */
 if( (e2->var.ident == POINTER || e2->var.ident == ARRAY) &&
    ( e2->type != CONSTANT ) ){
    doop( e1, e2, op );
    return;
    }

 /* adjust size of e2 according to e1 parameters */
 copysymbol( &one, &e1->var );
 if( one.ident == ARRAY ){
    if( one.size2 ){       /* double arrary */
       one.size2= 0;
       s= size( &one );
       }
    else s= sizeone( &one );
    }
 else{        /* its a pointer */
    one.ident= VAR;
    s= size( &one );
    }


 if( e2->type == CONSTANT ) e2->val *= (long)s;
 else if( s == 1 ) ;   /* don't need to adjust */
 else{
    loadval( e2 );
    switch( s ){
       case 2:             /* double the value */
       doubleval( e2 );
       break;

       case 4:  doubleval( e2 );  doubleval( e2 ); break;

       default:             /* need to multiply */
       e3.type= CONSTANT;
       e3.val= (long) s;
       doop( e2, & e3, '*' );
       break;
       }
   }

 /* now do the original operation */

 /* optimize constants - no code generated yet */
 if( e1->type == FOUND && e2->type == CONSTANT  &&
       ( e1->var.scope == STATIC || e1->var.scope == GLOBAL ||
       e1->var.scope == EXTERN )  && e1->var.ident == ARRAY )
       e1->val += e2->val;
 else{
    /* set size before calling doop */
    if( e1->datasize == 0 ) e1->datasize = SOP;
    doop( e1, e2, op );
    }

}
