
/*---------------------------------------------------------------*/
/*---                                                         ---*/
/*--- This file (guest-x86/ghelpers.c) is                     ---*/
/*--- Copyright (c) 2004 OpenWorks LLP.  All rights reserved. ---*/
/*---                                                         ---*/
/*---------------------------------------------------------------*/

#include "libvex_basictypes.h"
#include "libvex_guest_x86.h"
#include "libvex_ir.h"

#include "main/vex_util.h"
#include "guest-x86/gdefs.h"

/* This file contains helper functions for x86 guest code.
   Calls to these functions are generated by the back end.
   These calls are of course in the host machine code and 
   this file will be compiled to host machine code, so that
   all makes sense.  

   x86guest_findhelper() is the only exported function. 

   Only change the signatures of these helper functions very
   carefully.  If you change the signature here, you'll have to change
   the parameters passed to it in the IR calls constructed by
   x86toIR.c.

   Some of this code/logic is derived from QEMU, which is copyright
   Fabrice Bellard, licensed under the LGPL.  It is used with
   permission.  
*/

/* Set to 1 to get detailed profiling info about use of the flag
   machinery. */
#define PROFILE_EFLAGS 0


typedef UChar uint8_t;
typedef UInt  uint32_t;

#define CC_O CC_MASK_O
#define CC_P CC_MASK_P
#define CC_C CC_MASK_C


static const uint8_t parity_table[256] = {
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    CC_P, 0, 0, CC_P, 0, CC_P, CC_P, 0,
    0, CC_P, CC_P, 0, CC_P, 0, 0, CC_P,
};

/* n must be a constant to be efficient */
static inline int lshift(int x, int n)
{
    if (n >= 0)
        return x << n;
    else
        return x >> (-n);
}


#define PREAMBLE(__data_bits)					\
   const UInt DATA_MASK 					\
      = __data_bits==8 ? 0xFF 					\
                       : (__data_bits==16 ? 0xFFFF 		\
                                          : 0xFFFFFFFF); 	\
   const UInt SIGN_MASK = 1 << (__data_bits - 1);		\
   const UInt CC_DST = cc_dst;					\
   const UInt CC_SRC = cc_src

/*-------------------------------------------------------------*/

#define ACTIONS_ADD(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   int src1, src2, dst;						\
   src1 = CC_SRC;						\
   src2 = CC_DST;						\
   dst  = src1 + src2;						\
   cf = (DATA_TYPE)dst < (DATA_TYPE)src1;			\
   pf = parity_table[(uint8_t)dst];				\
   af = (dst ^ src1 ^ src2) & 0x10;				\
   zf = ((DATA_TYPE)dst == 0) << 6;				\
   sf = lshift(dst, 8 - DATA_BITS) & 0x80;			\
   of = lshift((src1 ^ src2 ^ -1) & (src1 ^ dst), 		\
               12 - DATA_BITS) & CC_O;				\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_ADC(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   int src1, src2, dst;						\
   src1 = CC_SRC;						\
   src2 = CC_DST;						\
   dst = src1 + src2 + 1;					\
   cf = (DATA_TYPE)dst <= (DATA_TYPE)src1;			\
   pf = parity_table[(uint8_t)dst];				\
   af = (dst ^ src1 ^ src2) & 0x10;				\
   zf = ((DATA_TYPE)dst == 0) << 6;				\
   sf = lshift(dst, 8 - DATA_BITS) & 0x80;			\
   of = lshift((src1 ^ src2 ^ -1) & (src1 ^ dst), 		\
                12 - DATA_BITS) & CC_O;				\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_SUB(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   int src1, src2, dst;						\
   src1 = CC_DST;						\
   src2 = CC_SRC;						\
   dst = src1 - src2;						\
   cf = (DATA_TYPE)src1 < (DATA_TYPE)src2;			\
   pf = parity_table[(uint8_t)dst];				\
   af = (dst ^ src1 ^ src2) & 0x10;				\
   zf = ((DATA_TYPE)dst == 0) << 6;				\
   sf = lshift(dst, 8 - DATA_BITS) & 0x80;			\
   of = lshift((src1 ^ src2) & (src1 ^ dst),	 		\
               12 - DATA_BITS) & CC_O; 				\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_SBB(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   int src1, src2, dst;						\
   src1 = CC_DST;						\
   src2 = CC_SRC;						\
   dst = (src1 - src2) - 1;					\
   cf = (DATA_TYPE)src1 <= (DATA_TYPE)src2;			\
   pf = parity_table[(uint8_t)dst];				\
   af = (dst ^ src1 ^ src2) & 0x10;				\
   zf = ((DATA_TYPE)dst == 0) << 6;				\
   sf = lshift(dst, 8 - DATA_BITS) & 0x80;			\
   of = lshift((src1 ^ src2) & (src1 ^ dst), 			\
               12 - DATA_BITS) & CC_O;				\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_LOGIC(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   cf = 0;							\
   pf = parity_table[(uint8_t)CC_DST];				\
   af = 0;							\
   zf = ((DATA_TYPE)CC_DST == 0) << 6;				\
   sf = lshift(CC_DST, 8 - DATA_BITS) & 0x80;			\
   of = 0;							\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_INC(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   int src1, src2;						\
   src1 = CC_DST - 1;						\
   src2 = 1;							\
   cf = CC_SRC;							\
   pf = parity_table[(uint8_t)CC_DST];				\
   af = (CC_DST ^ src1 ^ src2) & 0x10;				\
   zf = ((DATA_TYPE)CC_DST == 0) << 6;				\
   sf = lshift(CC_DST, 8 - DATA_BITS) & 0x80;			\
   of = ((CC_DST & DATA_MASK) == SIGN_MASK) << 11;		\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_DEC(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   int src1, src2;						\
   src1 = CC_DST + 1;						\
   src2 = 1;							\
   cf = CC_SRC;							\
   pf = parity_table[(uint8_t)CC_DST];				\
   af = (CC_DST ^ src1 ^ src2) & 0x10;				\
   zf = ((DATA_TYPE)CC_DST == 0) << 6;				\
   sf = lshift(CC_DST, 8 - DATA_BITS) & 0x80;			\
   of = ((CC_DST & DATA_MASK) 					\
        == ((uint32_t)SIGN_MASK - 1)) << 11;			\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_SHL(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   cf = (CC_SRC >> (DATA_BITS - 1)) & CC_C;			\
   pf = parity_table[(uint8_t)CC_DST];				\
   af = 0; /* undefined */					\
   zf = ((DATA_TYPE)CC_DST == 0) << 6;				\
   sf = lshift(CC_DST, 8 - DATA_BITS) & 0x80;			\
   /* of is defined if shift count == 1 */			\
   of = lshift(CC_SRC ^ CC_DST, 12 - DATA_BITS) & CC_O;		\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_SAR(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);  					\
   int cf, pf, af, zf, sf, of;					\
   cf = CC_SRC & 1;						\
   pf = parity_table[(uint8_t)CC_DST];				\
   af = 0; /* undefined */					\
   zf = ((DATA_TYPE)CC_DST == 0) << 6;				\
   sf = lshift(CC_DST, 8 - DATA_BITS) & 0x80;			\
   /* of is defined if shift count == 1 */			\
   of = lshift(CC_SRC ^ CC_DST, 12 - DATA_BITS) & CC_O;	 	\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

/* ROL: cf' = lsb(result).  of' = msb(result) ^ lsb(result). */
/* DST = result, SRC = old flags */
#define ACTIONS_ROL(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int fl 							\
      = (CC_SRC & ~(CC_O | CC_C))				\
        | (CC_C & CC_DST)					\
        | (CC_O & (lshift(CC_DST, 11-(DATA_BITS-1)) 		\
                   ^ lshift(CC_DST, 11)));			\
   return fl;							\
}

/*-------------------------------------------------------------*/

/* ROR: cf' = msb(result).  of' = msb(result) ^ msb-1(result). */
/* DST = result, SRC = old flags */
#define ACTIONS_ROR(DATA_BITS,DATA_TYPE)			\
{								\
   PREAMBLE(DATA_BITS);						\
   int fl 							\
      = (CC_SRC & ~(CC_O | CC_C))				\
        | (CC_C & (CC_DST >> (DATA_BITS-1)))			\
        | (CC_O & (lshift(CC_DST, 11-(DATA_BITS-1)) 		\
                   ^ lshift(CC_DST, 11-(DATA_BITS-1)+1)));	\
   return fl;							\
}

/*-------------------------------------------------------------*/

#define ACTIONS_SMUL(DATA_BITS,DATA_STYPE,DATA_S2TYPE)		\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   DATA_STYPE  hi;						\
   DATA_STYPE  lo = ((DATA_STYPE)CC_SRC) * ((DATA_STYPE)CC_DST);\
   DATA_S2TYPE rr = ((DATA_S2TYPE)((DATA_STYPE)CC_SRC))		\
                    * ((DATA_S2TYPE)((DATA_STYPE)CC_DST));	\
   hi = (DATA_STYPE)(rr >>/*s*/ DATA_BITS);			\
   cf = (hi != (lo >>/*s*/ (DATA_BITS-1)));			\
   pf = parity_table[(uint8_t)lo];				\
   af = 0; /* undefined */					\
   zf = (lo == 0) << 6;						\
   sf = lshift(lo, 8 - DATA_BITS) & 0x80;			\
   of = cf << 11;						\
   return cf | pf | af | zf | sf | of;				\
}

/*-------------------------------------------------------------*/

#define ACTIONS_UMUL(DATA_BITS,DATA_UTYPE,DATA_U2TYPE)		\
{								\
   PREAMBLE(DATA_BITS);						\
   int cf, pf, af, zf, sf, of;					\
   DATA_UTYPE  hi;						\
   DATA_UTYPE  lo = ((DATA_UTYPE)CC_SRC) * ((DATA_UTYPE)CC_DST);\
   DATA_U2TYPE rr = ((DATA_U2TYPE)((DATA_UTYPE)CC_SRC))		\
                    * ((DATA_U2TYPE)((DATA_UTYPE)CC_DST));	\
   hi = (DATA_UTYPE)(rr >>/*u*/ DATA_BITS);			\
   cf = (hi != 0);						\
   pf = parity_table[(uint8_t)lo];				\
   af = 0; /* undefined */					\
   zf = (lo == 0) << 6;				  	     	\
   sf = lshift(lo, 8 - DATA_BITS) & 0x80;			\
   of = cf << 11;						\
   return cf | pf | af | zf | sf | of;				\
}



#define CC_SHIFT_C 0
#define CC_SHIFT_P 2
#define CC_SHIFT_A 4
#define CC_SHIFT_Z 6
#define CC_SHIFT_S 7
#define CC_SHIFT_O 11

#if PROFILE_EFLAGS

static UInt tabc[CC_OP_NUMBER];
static UInt tab[CC_OP_NUMBER][16];
static Bool initted     = False;
static UInt n_calc_cond = 0;
static UInt n_calc_all  = 0;
static UInt n_calc_c    = 0;

static void showCounts ( void )
{
   Int op, co;
   Char ch;
   vex_printf("\nALL=%d  COND=%d   C=%d\n",
              n_calc_all-n_calc_cond-n_calc_c, n_calc_cond, n_calc_c);
   vex_printf("      CARRY    O   NO    B   NB    Z   NZ   BE  NBE"
              "    S   NS    P   NP    L   NL   LE  NLE\n");
   vex_printf("     ----------------------------------------------"
              "----------------------------------------\n");
   for (op = 0; op < CC_OP_NUMBER; op++) {

      ch = ' ';
      if (op > 0 && (op-1) % 3 == 2) 
         ch = 'L';

      vex_printf("%2d%c: ", op, ch);
      vex_printf("%6d ", tabc[op]);
      for (co = 0; co < 16; co++) {
         Int n = tab[op][co];
         if (n >= 1000) {
            vex_printf(" %3dK", n / 1000);
         } else 
         if (n >= 0) {
           vex_printf(" %3d ", n );
         } else {
            vex_printf("     ");
         }
      }
      vex_printf("\n");
   }
   vex_printf("\n");
}

static void initCounts ( void )
{
   Int op, co;
   initted = True;
   for (op = 0; op < CC_OP_NUMBER; op++) {
      tabc[op] = 0;
      for (co = 0; co < 16; co++)
         tab[op][co] = 0;
   }
}

#endif /* PROFILE_EFLAGS */

/* CALLED FROM GENERATED CODE */
/* Calculate all the 6 flags from the supplied thunk parameters. */
/*static*/ UInt calculate_eflags_all ( UInt cc_op, UInt cc_src, UInt cc_dst )
{
#  if PROFILE_EFLAGS
   n_calc_all++;
#  endif
   switch (cc_op) {
      case CC_OP_COPY:
         return cc_src & (CC_MASK_O | CC_MASK_S | CC_MASK_Z 
                          | CC_MASK_A | CC_MASK_C | CC_MASK_P);

      case CC_OP_ADDB:   ACTIONS_ADD( 8,  UChar  );
      case CC_OP_ADDW:   ACTIONS_ADD( 16, UShort );
      case CC_OP_ADDL:   ACTIONS_ADD( 32, UInt   );

      case CC_OP_ADCB:   ACTIONS_ADC( 8,  UChar  );
      case CC_OP_ADCW:   ACTIONS_ADC( 16, UShort );
      case CC_OP_ADCL:   ACTIONS_ADC( 32, UInt   );

      case CC_OP_SUBB:   ACTIONS_SUB(  8, UChar  );
      case CC_OP_SUBW:   ACTIONS_SUB( 16, UShort );
      case CC_OP_SUBL:   ACTIONS_SUB( 32, UInt   );

      case CC_OP_SBBB:   ACTIONS_SBB(  8, UChar  );
      case CC_OP_SBBW:   ACTIONS_SBB( 16, UShort );
      case CC_OP_SBBL:   ACTIONS_SBB( 32, UInt   );

      case CC_OP_LOGICB: ACTIONS_LOGIC(  8, UChar  );
      case CC_OP_LOGICW: ACTIONS_LOGIC( 16, UShort );
      case CC_OP_LOGICL: ACTIONS_LOGIC( 32, UInt   );

      case CC_OP_INCB:   ACTIONS_INC(  8, UChar  );
      case CC_OP_INCW:   ACTIONS_INC( 16, UShort );
      case CC_OP_INCL:   ACTIONS_INC( 32, UInt   );

      case CC_OP_DECB:   ACTIONS_DEC(  8, UChar  );
      case CC_OP_DECW:   ACTIONS_DEC( 16, UShort );
      case CC_OP_DECL:   ACTIONS_DEC( 32, UInt   );

      case CC_OP_SHLB:   ACTIONS_SHL(  8, UChar  );
      case CC_OP_SHLW:   ACTIONS_SHL( 16, UShort );
      case CC_OP_SHLL:   ACTIONS_SHL( 32, UInt   );

      case CC_OP_SARB:   ACTIONS_SAR(  8, UChar  );
      case CC_OP_SARW:   ACTIONS_SAR( 16, UShort );
      case CC_OP_SARL:   ACTIONS_SAR( 32, UInt   );

      case CC_OP_ROLB:   ACTIONS_ROL(  8, UChar  );
      case CC_OP_ROLW:   ACTIONS_ROL( 16, UShort );
      case CC_OP_ROLL:   ACTIONS_ROL( 32, UInt   );

      case CC_OP_RORB:   ACTIONS_ROR(  8, UChar  );
      case CC_OP_RORW:   ACTIONS_ROR( 16, UShort );
      case CC_OP_RORL:   ACTIONS_ROR( 32, UInt   );

      case CC_OP_UMULB:  ACTIONS_UMUL(  8, UChar,  UShort );
      case CC_OP_UMULW:  ACTIONS_UMUL( 16, UShort, UInt   );
      case CC_OP_UMULL:  ACTIONS_UMUL( 32, UInt,   ULong  );

      case CC_OP_SMULB:  ACTIONS_SMUL(  8, Char,  Short );
      case CC_OP_SMULW:  ACTIONS_SMUL( 16, Short, Int   );
      case CC_OP_SMULL:  ACTIONS_SMUL( 32, Int,   Long  );

      default:
         /* shouldn't really make these calls from generated code */
         vex_printf("calculate_eflags_all( %d, 0x%x, 0x%x )\n",
                    cc_op, cc_src, cc_dst );
         vpanic("calculate_eflags_all");
   }
}


/* CALLED FROM GENERATED CODE */
/* Calculate just the carry flag from the supplied thunk parameters. */
static UInt calculate_eflags_c ( UInt cc_op, UInt cc_src, UInt cc_dst )
{
   /* Fast-case some common ones. */
   if (cc_op == CC_OP_LOGICL)
      return 0;
   if (cc_op == CC_OP_DECL)
      return cc_src;

#  if PROFILE_EFLAGS
   if (!initted)
      initCounts();
   tabc[cc_op]++;

   n_calc_c++;
#  endif
   return calculate_eflags_all(cc_op,cc_src,cc_dst) & CC_MASK_C;
}


/* CALLED FROM GENERATED CODE */
/* returns 1 or 0 */
/*static*/ UInt calculate_condition ( UInt/*Condcode*/ cond, 
                                      UInt cc_op, UInt cc_src, UInt cc_dst )
{
   UInt eflags = calculate_eflags_all(cc_op, cc_src, cc_dst);
   UInt of,sf,zf,cf,pf;
   UInt inv = cond & 1;

#  if PROFILE_EFLAGS
   if (!initted)
     initCounts();

   tab[cc_op][cond]++;
   n_calc_cond++;

   if (0 == ((n_calc_all+n_calc_c) & 0x3FFF)) showCounts();
#  endif

   switch (cond) {
      case CondNO:
      case CondO: /* OF == 1 */
         of = eflags >> CC_SHIFT_O;
         return 1 & (inv ^ of);

      case CondNZ:
      case CondZ: /* ZF == 1 */
         zf = eflags >> CC_SHIFT_Z;
         return 1 & (inv ^ zf);

      case CondNB:
      case CondB: /* CF == 1 */
         cf = eflags >> CC_SHIFT_C;
         return 1 & (inv ^ cf);
         break;

      case CondNBE:
      case CondBE: /* (CF or ZF) == 1 */
         cf = eflags >> CC_SHIFT_C;
         zf = eflags >> CC_SHIFT_Z;
         return 1 & (inv ^ (cf | zf));
         break;

      case CondNS:
      case CondS: /* SF == 1 */
         sf = eflags >> CC_SHIFT_S;
         return 1 & (inv ^ sf);

      case CondNP:
      case CondP: /* PF == 1 */
         pf = eflags >> CC_SHIFT_P;
         return 1 & (inv ^ pf);

      case CondNL:
      case CondL: /* (SF xor OF) == 1 */
         sf = eflags >> CC_SHIFT_S;
         of = eflags >> CC_SHIFT_O;
         return 1 & (inv ^ (sf ^ of));
         break;

      case CondNLE:
      case CondLE: /* ((SF xor OF) or ZF)  == 1 */
         sf = eflags >> CC_SHIFT_S;
         of = eflags >> CC_SHIFT_O;
         zf = eflags >> CC_SHIFT_Z;
         return 1 & (inv ^ ((sf ^ of) | zf));
         break;

      default:
         /* shouldn't really make these calls from generated code */
         vex_printf("calculate_condition( %d, %d, 0x%x, 0x%x )\n",
                    cond, cc_op, cc_src, cc_dst );
         vpanic("calculate_condition");
   }
}


/* The only exported function. */

Addr64 x86guest_findhelper ( Char* function_name )
{
   if (vex_streq(function_name, "calculate_eflags_all"))
      return (Addr64)(& calculate_eflags_all);
   if (vex_streq(function_name, "calculate_eflags_c"))
      return (Addr64)(& calculate_eflags_c);
   if (vex_streq(function_name, "calculate_condition"))
      return (Addr64)(& calculate_condition);
   vex_printf("\nx86 guest: can't find helper: %s\n", function_name);
   vpanic("x86guest_findhelper");
}

/* Used by the optimiser to try specialisations.  Returns an
   equivalent expression, or NULL if none. */

static Bool isU32 ( IRExpr* e, UInt n )
{
   return e->tag == Iex_Const
          && e->Iex.Const.con->tag == Ico_U32
          && e->Iex.Const.con->Ico.U32 == n;
}

IRExpr* x86guest_spechelper ( Char* function_name,
                              IRExpr** args )
{
#  define unop(_op,_a1) IRExpr_Unop((_op),(_a1))
#  define binop(_op,_a1,_a2) IRExpr_Binop((_op),(_a1),(_a2))
#  define mkU32(_n) IRExpr_Const(IRConst_U32(_n))

   Int i, arity = 0;
   for (i = 0; args[i]; i++)
      arity++;
#  if 0
   vex_printf("spec request:\n");
   vex_printf("   %s  ", function_name);
   for (i = 0; i < arity; i++) {
      vex_printf("  ");
      ppIRExpr(args[i]);
   }
   vex_printf("\n");
#  endif

   if (vex_streq(function_name, "calculate_eflags_c")) {
      /* specialise calls to above "calculate_eflags_c" function */
      IRExpr *cc_op, *cc_src, *cc_dst;
      vassert(arity == 3);
      cc_op = args[0];
      cc_src = args[1];
      cc_dst = args[2];

      if (isU32(cc_op, CC_OP_LOGICL)) {
         /* cflag after logic is zero */
         return mkU32(0);
      }
      if (isU32(cc_op, CC_OP_DECL) || isU32(cc_op, CC_OP_INCL)) {
         /* If the thunk is dec or inc, the cflag is supplied as CC_SRC. */
         return cc_src;
      }
      if (isU32(cc_op, CC_OP_SUBL)) {
         /* C after sub denotes unsigned less than */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, cc_dst, cc_src));
      }

#     if 0
      if (cc_op->tag == Iex_Const) {
         vex_printf("CFLAG "); ppIRExpr(cc_op); vex_printf("\n");
      }
#     endif

      return NULL;
   }

   if (vex_streq(function_name, "calculate_condition")) {
      /* specialise calls to above "calculate condition" function */
      IRExpr *cond, *cc_op, *cc_src, *cc_dst;
      vassert(arity == 4);
      cond = args[0];
      cc_op = args[1];
      cc_src = args[2];
      cc_dst = args[3];

      /*---------------- SUBL ----------------*/

      if (isU32(cc_op, CC_OP_SUBL) && isU32(cond, CondZ)) {
         /* long sub/cmp, then Z --> test dst==src */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, cc_dst, cc_src));
      }

      if (isU32(cc_op, CC_OP_SUBL) && isU32(cond, CondL)) {
         /* long sub/cmp, then L (signed less than) 
            --> test dst <s src */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32S, cc_dst, cc_src));
      }

      if (isU32(cc_op, CC_OP_SUBL) && isU32(cond, CondLE)) {
         /* long sub/cmp, then LE (signed less than or equal)
            --> test dst <=s src */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32S, cc_dst, cc_src));
      }

      if (isU32(cc_op, CC_OP_SUBL) && isU32(cond, CondBE)) {
         /* long sub/cmp, then BE (unsigned less than or equal)
            --> test dst <=u src */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLE32U, cc_dst, cc_src));
      }

      if (isU32(cc_op, CC_OP_SUBL) && isU32(cond, CondB)) {
         /* long sub/cmp, then B (unsigned less than)
            --> test dst <u src */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, cc_dst, cc_src));
      }

      /*---------------- SUBW ----------------*/

      if (isU32(cc_op, CC_OP_SUBW) && isU32(cond, CondZ)) {
         /* byte sub/cmp, then Z --> test dst==src */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ16, 
                           unop(Iop_32to16,cc_dst), 
                           unop(Iop_32to16,cc_src)));
      }

      /*---------------- SUBB ----------------*/

      if (isU32(cc_op, CC_OP_SUBB) && isU32(cond, CondZ)) {
         /* byte sub/cmp, then Z --> test dst==src */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ8, 
                           unop(Iop_32to8,cc_dst), 
                           unop(Iop_32to8,cc_src)));
      }

      if (isU32(cc_op, CC_OP_SUBB) && isU32(cond, CondNZ)) {
         /* byte sub/cmp, then Z --> test dst!=src */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpNE8, 
                           unop(Iop_32to8,cc_dst), 
                           unop(Iop_32to8,cc_src)));
      }

      if (isU32(cc_op, CC_OP_SUBB) && isU32(cond, CondNBE)) {
         /* long sub/cmp, then NBE (unsigned greater than)
            --> test src <=u dst */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpLT32U, 
                           binop(Iop_And32,cc_src,mkU32(0xFF)),
			   binop(Iop_And32,cc_dst,mkU32(0xFF))));
      }

      /*---------------- LOGICL ----------------*/

      if (isU32(cc_op, CC_OP_LOGICL) && isU32(cond, CondZ)) {
         /* long and/or/xor, then Z --> test dst==0 */
         return unop(Iop_1Uto32,binop(Iop_CmpEQ32, cc_dst, mkU32(0)));
      }

      if (isU32(cc_op, CC_OP_LOGICL) && isU32(cond, CondS)) {
         /* long and/or/xor, then S --> test dst <s 0 */
         return unop(Iop_1Uto32,binop(Iop_CmpLT32S, cc_dst, mkU32(0)));
      }

      if (isU32(cc_op, CC_OP_LOGICL) && isU32(cond, CondLE)) {
         /* long and/or/xor, then LE
            This is pretty subtle.  LOGIC sets SF and ZF according to the
            result and makes OF be zero.  LE computes (SZ ^ OF) | ZF, but
            OF is zero, so this reduces to SZ | ZF -- which will be 1 iff
            the result is <=signed 0.  Hence ...
         */
         return unop(Iop_1Uto32,binop(Iop_CmpLE32S, cc_dst, mkU32(0)));
      }

      /*---------------- LOGICB ----------------*/

      if (isU32(cc_op, CC_OP_LOGICB) && isU32(cond, CondZ)) {
         /* byte and/or/xor, then Z --> test dst==0 */
         return unop(Iop_1Uto32,
                     binop(Iop_CmpEQ32, binop(Iop_And32,cc_dst,mkU32(255)), 
                                        mkU32(0)));
      }

      /*---------------- DECL ----------------*/

      if (isU32(cc_op, CC_OP_DECL) && isU32(cond, CondZ)) {
         /* dec L, then Z --> test dst == 0 */
         return unop(Iop_1Uto32,binop(Iop_CmpEQ32, cc_dst, mkU32(0)));
      }

      if (isU32(cc_op, CC_OP_DECL) && isU32(cond, CondS)) {
         /* dec L, then S --> compare DST <s 0 */
         return unop(Iop_1Uto32,binop(Iop_CmpLT32S, cc_dst, mkU32(0)));
      }

      return NULL;
   }

#  undef unop
#  undef binop
#  undef mkU32

   return NULL;
}


/*-----------------------------------------------------------*/
/*--- Utility functions for x87 FPU conversions.          ---*/
/*-----------------------------------------------------------*/


/* 80 and 64-bit floating point formats:

   80-bit:

    S  0       0-------0      zero
    S  0       0X------X      denormals
    S  1-7FFE  1X------X      normals (all normals have leading 1)
    S  7FFF    10------0      infinity
    S  7FFF    10X-----X      snan
    S  7FFF    11X-----X      qnan

   S is the sign bit.  For runs X----X, at least one of the Xs must be
   nonzero.  Exponent is 15 bits, fractional part is 63 bits, and
   there is an explicitly represented leading 1, and a sign bit,
   giving 80 in total.

   64-bit avoids the confusion of an explicitly represented leading 1
   and so is simpler:

    S  0      0------0   zero
    S  0      X------X   denormals
    S  1-7FE  any        normals
    S  7FF    0------0   infinity
    S  7FF    0X-----X   snan
    S  7FF    1X-----X   qnan

   Exponent is 11 bits, fractional part is 52 bits, and there is a 
   sign bit, giving 64 in total.
*/


/* Convert a IEEE754 double (64-bit) into an x87 extended double
   (80-bit), mimicing the hardware fairly closely.  Both numbers are
   stored little-endian.  Limitations, all of which could be fixed,
   given some level of hassle:

   * Does not handle double precision denormals.  As a result, values
     with magnitudes less than 1e-308 are flushed to zero when they
     need not be.

   * Identity of NaNs is not preserved.

   See comments in the code for more details.  
*/
static void convert_f64le_to_f80le ( /*IN*/UChar* f64, /*OUT*/UChar* f80 )
{
   Bool  isInf;
   Int   bexp;
   UChar sign;

   sign = (f64[7] >> 7) & 1;
   bexp = (f64[7] << 4) | ((f64[6] >> 4) & 0x0F);
   bexp &= 0x7FF;

   /* If the exponent is zero, either we have a zero or a denormal.
      Produce a zero.  This is a hack in that it forces denormals to
      zero.  Could do better. */
   if (bexp == 0) {
      f80[9] = sign << 7;
      f80[8] = f80[7] = f80[6] = f80[5] = f80[4]
             = f80[3] = f80[2] = f80[1] = f80[0] = 0;
      return;
   }
   
   /* If the exponent is 7FF, this is either an Infinity, a SNaN or
      QNaN, as determined by examining bits 51:0, thus:
          0  ... 0    Inf
          0X ... X    SNaN
          1X ... X    QNaN
      where at least one of the Xs is not zero.
   */
   if (bexp == 0x7FF) {
      isInf = (f64[6] & 0x0F) == 0 
              && f64[5] == 0 && f64[4] == 0 && f64[3] == 0 
              && f64[2] == 0 && f64[1] == 0 && f64[0] == 0;
      if (isInf) {
         /* Produce an appropriately signed infinity:
            S 1--1 (15)  1  0--0 (63)
         */
         f80[9] = (sign << 7) | 0x7F;
         f80[8] = 0xFF;
         f80[7] = 0x80;
         f80[6] = f80[5] = f80[4] = f80[3] 
                = f80[2] = f80[1] = f80[0] = 0;
         return;
      }
      /* So it's either a QNaN or SNaN.  Distinguish by considering
         bit 51.  Note, this destroys all the trailing bits
         (identity?) of the NaN.  IEEE754 doesn't require preserving
         these (it only requires that there be one QNaN value and one
         SNaN value), but x87 does seem to have some ability to
         preserve them.  Anyway, here, the NaN's identity is
         destroyed.  Could be improved. */
      if (f64[6] & 8) {
         /* QNaN.  Make a QNaN:
            S 1--1 (15)  1  1--1 (63) 
         */
         f80[9] = (sign << 7) | 0x7F;
         f80[8] = 0xFF;
         f80[7] = 0xFF;
         f80[6] = f80[5] = f80[4] = f80[3] 
                = f80[2] = f80[1] = f80[0] = 0xFF;
      } else {
         /* SNaN.  Make a SNaN:
            S 1--1 (15)  0  1--1 (63) 
         */
         f80[9] = (sign << 7) | 0x7F;
         f80[8] = 0xFF;
         f80[7] = 0x7F;
         f80[6] = f80[5] = f80[4] = f80[3] 
                = f80[2] = f80[1] = f80[0] = 0xFF;
      }
      return;
   }

   /* It's not a zero, denormal, infinity or nan.  So it must be a
      normalised number.  Rebias the exponent and build the new
      number.  */
   bexp += (16383 - 1023);

   f80[9] = (sign << 7) | ((bexp >> 8) & 0xFF);
   f80[8] = bexp & 0xFF;
   f80[7] = (1 << 7) | ((f64[6] << 3) & 0x78) | ((f64[5] >> 5) & 7);
   f80[6] = ((f64[5] << 3) & 0xF8) | ((f64[4] >> 5) & 7);
   f80[5] = ((f64[4] << 3) & 0xF8) | ((f64[3] >> 5) & 7);
   f80[4] = ((f64[3] << 3) & 0xF8) | ((f64[2] >> 5) & 7);
   f80[3] = ((f64[2] << 3) & 0xF8) | ((f64[1] >> 5) & 7);
   f80[2] = ((f64[1] << 3) & 0xF8) | ((f64[0] >> 5) & 7);
   f80[1] = ((f64[0] << 3) & 0xF8);
   f80[0] = 0;
}


/////////////////////////////////////////////////////////////////

/* Convert a x87 extended double (80-bit) into an IEEE 754 double
   (64-bit), mimicing the hardware fairly closely.  Both numbers are
   stored little-endian.  Limitations, all of which could be fixed,
   given some level of hassle:

   * Does not create double precision denormals.  As a result, values
     with magnitudes less than 1e-308 are flushed to zero when they
     need not be.

   * Rounding following truncation could be a bit better.

   * Identity of NaNs is not preserved.

   See comments in the code for more details.  
*/
static void convert_f80le_to_f64le ( /*IN*/UChar* f80, /*OUT*/UChar* f64 )
{
   Bool  isInf;
   Int   bexp;
   UChar sign;

   sign = (f80[9] >> 7) & 1;
   bexp = (((UInt)f80[9]) << 8) | (UInt)f80[8];
   bexp &= 0x7FFF;

   /* If the exponent is zero, either we have a zero or a denormal.
      But an extended precision denormal becomes a double precision
      zero, so in either case, just produce the appropriately signed
      zero. */
   if (bexp == 0) {
      f64[7] = sign << 7;
      f64[6] = f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
      return;
   }
   
   /* If the exponent is 7FFF, this is either an Infinity, a SNaN or
      QNaN, as determined by examining bits 62:0, thus:
          0  ... 0    Inf
          0X ... X    SNaN
          1X ... X    QNaN
      where at least one of the Xs is not zero.
   */
   if (bexp == 0x7FFF) {
      isInf = (f80[7] & 0x7F) == 0 
              && f80[6] == 0 && f80[5] == 0 && f80[4] == 0 
              && f80[3] == 0 && f80[2] == 0 && f80[1] == 0 && f80[0] == 0;
      if (isInf) {
         if (0 == (f80[7] & 0x80))
            goto wierd_NaN;
         /* Produce an appropriately signed infinity:
            S 1--1 (11)  0--0 (52)
         */
         f64[7] = (sign << 7) | 0x7F;
         f64[6] = 0xF0;
         f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
         return;
      }
      /* So it's either a QNaN or SNaN.  Distinguish by considering
         bit 62.  Note, this destroys all the trailing bits
         (identity?) of the NaN.  IEEE754 doesn't require preserving
         these (it only requires that there be one QNaN value and one
         SNaN value), but x87 does seem to have some ability to
         preserve them.  Anyway, here, the NaN's identity is
         destroyed.  Could be improved. */
      if (f80[8] & 0x40) {
         /* QNaN.  Make a QNaN:
            S 1--1 (11)  1  1--1 (51) 
         */
         f64[7] = (sign << 7) | 0x7F;
         f64[6] = 0xFF;
         f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0xFF;
      } else {
         /* SNaN.  Make a SNaN:
            S 1--1 (11)  0  1--1 (51) 
         */
         f64[7] = (sign << 7) | 0x7F;
         f64[6] = 0xF7;
         f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0xFF;
      }
      return;
   }

   /* If it's not a Zero, NaN or Inf, and the integer part (bit 62) is
      zero, the x87 FPU appears to consider the number denormalised
      and converts it to a QNaN. */
   if (0 == (f80[7] & 0x80)) {
      wierd_NaN:
      /* Strange hardware QNaN:
         S 1--1 (11)  1  0--0 (51) 
      */
      /* On a PIII, these QNaNs always appear with sign==1.  I have
         no idea why. */
      f64[7] = (1 /*sign*/ << 7) | 0x7F;
      f64[6] = 0xF8;
      f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
      return;
   }

   /* It's not a zero, denormal, infinity or nan.  So it must be a 
      normalised number.  Rebias the exponent and consider. */
   bexp -= (16383 - 1023);
   if (bexp >= 0x7FF) {
      /* It's too big for a double.  Construct an infinity. */
      f64[7] = (sign << 7) | 0x7F;
      f64[6] = 0xF0;
      f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
      return;
   }

   if (bexp < 0) {
   /* It's too small for a double.  Construct a zero.  Note, this
      is a kludge since we could conceivably create a
      denormalised number for bexp in -1 to -51, but we don't
      bother.  This means the conversion flushes values
      approximately in the range 1e-309 to 1e-324 ish to zero
      when it doesn't actually need to.  This could be
      improved. */
      f64[7] = sign << 7;
      f64[6] = f64[5] = f64[4] = f64[3] = f64[2] = f64[1] = f64[0] = 0;
      return;
   }

   /* Ok, it's a normalised number which is representable as a double.
      Copy the exponent and mantissa into place. */
   /*
   for (i = 0; i < 52; i++)
      write_bit_array ( f64,
                        i,
                        read_bit_array ( f80, i+11 ) );
   */
   f64[0] = (f80[1] >> 3) | (f80[2] << 5);
   f64[1] = (f80[2] >> 3) | (f80[3] << 5);
   f64[2] = (f80[3] >> 3) | (f80[4] << 5);
   f64[3] = (f80[4] >> 3) | (f80[5] << 5);
   f64[4] = (f80[5] >> 3) | (f80[6] << 5);
   f64[5] = (f80[6] >> 3) | (f80[7] << 5);

   f64[6] = ((bexp << 4) & 0xF0) | ((f80[7] >> 3) & 0x0F);

   f64[7] = (sign << 7) | ((bexp >> 4) & 0x7F);

   /* Now consider any rounding that needs to happen as a result of
      truncating the mantissa. */
   if (f80[1] & 4) /* read_bit_array(f80, 10) == 1) */ {
      /* Round upwards.  This is a kludge.  Once in every 64k
         roundings (statistically) the bottom two bytes are both 0xFF
         and so we don't round at all.  Could be improved. */
      if (f64[0] != 0xFF) { 
         f64[0]++; 
      }
      else 
      if (f64[0] == 0xFF && f64[1] != 0xFF) {
         f64[0] = 0;
         f64[1]++;
      }
      /* else we don't round, but we should. */
   }
}


/*----------------------------------------------*/
/*--- The exported fns ..                    ---*/
/*----------------------------------------------*/

/* Layout of the real x87 state. */

typedef
   struct {
      UShort env[14];
      UChar  reg[80];
   }
   Fpu_State;

/* Offsets, in 16-bit ints, into the FPU environment (env) area. */
#define FP_ENV_CTRL   0
#define FP_ENV_STAT   2
#define FP_ENV_TAG    4
#define FP_ENV_IP     6 /* and 7 */
#define FP_ENV_CS     8
#define FP_ENV_OPOFF  10 /* and 11 */
#define FP_ENV_OPSEL  12
#define FP_REG(ii)    (10*(7-(ii)))


/* VISIBLE TO LIBVEX CLIENT */
void x87_to_vex ( /*IN*/UChar* x87_state, /*OUT*/UChar* vex_state )
{
   Int        r;
   UInt       tag;
   Double*    vexRegs = (Double*)(vex_state + OFFB_F0);
   UChar*     vexTags = (UChar*)(vex_state + OFFB_FTAG0);
   Fpu_State* x87     = (Fpu_State*)x87_state;
   UInt       ftop    = (x87->env[FP_ENV_STAT] >> 11) & 7;
   UInt       tagw    = x87->env[FP_ENV_TAG];
   UInt       fpucw   = x87->env[FP_ENV_CTRL];
   UInt       c320    = x87->env[FP_ENV_STAT] & 0x4500;

   /* Copy registers and tags */
   for (r = 0; r < 8; r++) {
      tag = (tagw >> (2*r)) & 3;
      if (tag == 3) {
         /* register is empty */
         vexRegs[r] = 0.0;
         vexTags[r] = 0;
      } else {
         /* register is non-empty */
         convert_f80le_to_f64le( &x87->reg[FP_REG(r)], (UChar*)&vexRegs[r] );
         vexTags[r] = 1;
      }
   }

   /* stack pointer */
   *(UInt*)(vex_state + OFFB_FTOP) = ftop;

   /* control word */
   *(UInt*)(vex_state + OFFB_FPUCW) = fpucw;

   /* status word */
   *(UInt*)(vex_state + OFFB_FC320) = c320;
}


/* VISIBLE TO LIBVEX CLIENT */
void vex_to_x87 ( /*IN*/UChar* vex_state, /*OUT*/UChar* x87_state )
{
   Int        i, r;
   UInt       tagw;
   Double*    vexRegs = (Double*)(vex_state + OFFB_F0);
   UChar*     vexTags = (UChar*)(vex_state + OFFB_FTAG0);
   Fpu_State* x87     = (Fpu_State*)x87_state;
   UInt       ftop    = *(UInt*)(vex_state + OFFB_FTOP);
   UInt       c320    = *(UInt*)(vex_state + OFFB_FC320);

   for (i = 0; i < 14; i++)
      x87->env[i] = 0;

   x87->env[1] = x87->env[3] = x87->env[5] = x87->env[13] = 0xFFFF;
   x87->env[FP_ENV_CTRL] = (UShort)( *(UInt*)(vex_state + OFFB_FPUCW) );
   x87->env[FP_ENV_STAT] = ((ftop & 7) << 11) | (c320 & 0x4500);

   tagw = 0;
   for (r = 0; r < 8; r++) {
      if (vexTags[r] == 0) {
         /* register is empty */
         tagw |= (3 << (2*r));
         convert_f64le_to_f80le( (UChar*)&vexRegs[r], &x87->reg[FP_REG(r)] );
      } else {
         /* register is full. */
         tagw |= (0 << (2*r));
         convert_f64le_to_f80le( (UChar*)&vexRegs[r],  &x87->reg[FP_REG(r)] );
      }
   }
   x87->env[FP_ENV_TAG] = tagw;
}


/*---------------------------------------------------------------*/
/*--- end                                guest-x86/ghelpers.c ---*/
/*---------------------------------------------------------------*/
