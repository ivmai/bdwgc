/* 
 * Copyright 1988, 1989 Hans-J. Boehm, Alan J. Demers
 * Copyright (c) 1991 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this compiler for any purpose,
 * provided the above notices are retained on all copies.
 */
 
/* Machine specific parts contributed by various people.  See README file. */

/*********************************/
/*                               */
/* Definitions for conservative  */
/* collector                     */
/*                               */
/*********************************/

/*********************************/
/*                               */
/* Easily changeable parameters  */
/*                               */
/*********************************/

# if defined(sun) && defined(mc68000)
#    define M68K_SUN
#    define mach_type_known
# endif
# if defined(hp9000s300)
#    define M68K_HP
#    define mach_type_known
# endif
# if defined(vax)
#    define VAX
#    ifdef ultrix
#	define ULTRIX
#    else
#	define BSD
#    endif
#    define mach_type_known
# endif
# if defined(mips)
#    define MIPS
#    ifdef ultrix
#	define ULTRIX
#    else
#	define RISCOS
#    endif
#    define mach_type_known
# endif
# if defined(sequent) && defined(i386)
#    define I386
#    define mach_type_known
# endif
# if defined(ibm032)
#   define RT
#   define mach_type_known
# endif
# if defined(sun) && defined(sparc)
#   define SPARC
#   define mach_type_known
# endif
# if defined(_IBMR2)
#   define IBMRS6000
#   define mach_type_known
# endif


/* Feel free to add more clauses here */

/* Or manually define the machine type here.  A machine type is 	*/
/* characterized by the architecture and assembler syntax.  Some	*/
/* machine types are further subdivided by OS.  In that case, we use	*/
/* the macros ULTRIX, RISCOS, and BSD to distinguish.			*/
/* The distinction in these cases is usually the stack starting address */
# ifndef mach_type_known
#   define M68K_SUN /* Guess "Sun" */
		    /* Mapping is: M68K_SUN   ==> Sun3 assembler,      */
		    /*             M68K_HP    ==> HP9000/300,          */
		    /*             I386       ==> Sequent Symmetry,    */
                    /*             NS32K      ==> Encore Multimax,     */
                    /*             MIPS       ==> R2000 or R3000       */
                    /*			(RISCOS, ULTRIX variants)      */
                    /*		   VAX	      ==> DEC VAX	       */
                    /*			(BSD, ULTRIX variants)	       */
# endif

#define PRINTSTATS  /* Print garbage collection statistics                  */
		    /* For less verbose output, undefine in reclaim.c      */

#define PRINTTIMES  /* Print the amount of time consumed by each garbage   */
		    /* collection.                                         */

#define PRINTBLOCKS /* Print object sizes associated with heap blocks,     */
		    /* whether the objects are atomic or composite, and    */
		    /* whether or not the block was found to be empty      */
		    /* duing the reclaim phase.  Typically generates       */
		    /* about one screenful per garbage collection.         */
#undef PRINTBLOCKS

#ifdef SILENT
#  ifdef PRINTSTATS
#    undef PRINTSTATS
#  endif
#  ifdef PRINTTIMES
#    undef PRINTTIMES
#  endif
#  ifdef PRINTNBLOCKS
#    undef PRINTNBLOCKS
#  endif
#endif

#define HBLK_MAP    /* Maintain a map of all potential heap blocks        */
		    /* starting at heapstart.                             */
		    /* Normally, this performs about as well as the       */
		    /* standard stack of chunk pointers that is used      */
		    /* otherwise.  It loses if a small section of the     */
		    /* heap consists of garbage collected objects.        */
		    /* It is ESSENTIAL if pointers to object interiors    */
		    /* are considered valid, i.e. if INTERIOR_POINTERS    */
		    /* is defined.                                        */
#undef HBLK_MAP

#define MAP_SIZE 8192  /* total data size < MAP_SIZE * HBLKSIZE = 32 Meg  */
#define MAXHBLKS 4096  /* Maximum number of chunks which can be           */
		       /* allocated                                       */
#define INTERIOR_POINTERS
		    /* Follow pointers to the interior of an object.      */
		    /* Substantially increases the probability of         */
		    /* unnnecessary space retention.  May be necessary    */
		    /* with gcc -O or other C compilers that may clobber  */
		    /* values of dead variables prematurely.  Pcc         */
		    /* derived compilers appear to pose no such problems. */
		    /* Empirical evidence suggests that this is probably  */
		    /* still OK for most purposes, so long as pointers    */
		    /* are known to be 32 bit aligned.  The combination   */
		    /* of INTERIOR_POINTERS and UNALIGNED (e.g. on a      */
		    /* Sun 3 with the standard compiler) causes easily    */
		    /* observable spurious retention and performance      */
		    /* degradation.                                       */
#undef INTERIOR_POINTERS

#ifdef SPARC
#   define ALIGN_DOUBLE  /* Align objects of size > 1 word on 2 word   */
			 /* boundaries.  Wasteful of memory, but       */
			 /* apparently required by SPARC architecture. */

#endif

#if defined(INTERIOR_POINTERS) && !defined(HBLK_MAP)
    --> check for interior pointers requires a heap block map
#endif

#define MERGE_SIZES /* Round up some object sizes, so that fewer distinct */
		    /* free lists are actually maintained.  This applies  */
		    /* only to the top level routines in misc.c, not to   */
		    /* user generated code that calls allocobj and        */
		    /* allocaobj directly.                                */
		    /* Slows down average programs slightly.  May however */
		    /* substantially reduce fragmentation if allocation   */
		    /* request sizes are widely scattered.                */
#undef MERGE_SIZES

/* ALIGN_DOUBLE requires MERGE_SIZES at present. */
# if defined(ALIGN_DOUBLE) && !defined(MERGE_SIZES)
#   define MERGE_SIZES
# endif


/* For PRINTTIMES to tell the truth, we need to know the value of HZ for
   this system. */

#if defined(M68K_HP) || defined(M68K_SUN) || defined(SPARC)
#  include <sys/param.h>
#  define FLOAT_HZ (double)HZ
#else
#  define FLOAT_HZ 60.0    /* Guess that we're in the U.S. */
#endif

#ifdef M68K_SUN
#  define UNALIGNED       /* Pointers are not longword aligned         */
#  define ALIGNMENT   2   /* Pointers are aligned on 2 byte boundaries */
			  /* by the Sun C compiler.                    */
#else
#  ifdef VAX
#    undef UNALIGNED      /* Pointers are longword aligned by 4.2 C compiler */
#    define ALIGNMENT 4
#  else
#    ifdef RT
#      undef UNALIGNED
#      define ALIGNMENT 4
#    else
#      ifdef SPARC
#        undef UNALIGNED
#        define ALIGNMENT 4
#      else
#        ifdef I386
#           undef UNALIGNED         /* Sequent compiler aligns pointers */
#           define ALIGNMENT 4
#        else
#          ifdef NS32K
#            undef UNALIGNED        /* Pointers are aligned on NS32K */
#            define ALIGNMENT 4
#          else
#            ifdef MIPS
#              undef UNALIGNED      /* MIPS hardware requires pointer */
				    /* alignment                      */
#              define ALIGNMENT 4
#            else
#              ifdef M68K_HP
#                define UNALIGNED
#                define ALIGNMENT 2 /* 2 byte alignment inside struct/union, */
				    /* 4 bytes elsewhere */
#              else
#		 ifdef IBMRS6000
#		   undef UNALIGNED
#		   define ALIGNMENT 4
#		 else
		    --> specify alignment <--
#		 endif
#              endif
#            endif
#          endif
#        endif
#      endif
#    endif
#  endif
# endif

# ifdef RT
#   define STACKTOP ((word *) 0x1fffd800)
# else
#   ifdef I386
#     define STACKTOP ((word *) 0x3ffff000)  /* For Sequent */
#   else
#     ifdef NS32K
#       define STACKTOP ((word *) 0xfffff000) /* for Encore */
#     else
#       ifdef MIPS
#	  ifdef ULTRIX
#           define STACKTOP ((word *) 0x7fffc000)
#	  else
#	    ifdef RISCOS
#             define STACKTOP ((word *) 0x7ffff000)
			      /* Could probably be slightly lower since  */
			      /* startup code allocates lots of junk     */
#	    else
		--> fix it
#	    endif
#	  endif
#       else
#         ifdef M68K_HP
#           define STACKTOP ((word *) 0xffeffffc)
			      /* empirically determined.  seems to work. */
#	  else
#	    ifdef IBMRS6000
#	      define STACKTOP ((word *) 0x2ff80000)
#           else
#	      if defined(VAX) && defined(ULTRIX)
#		define STACKTOP ((word *) 0x7fffc800)
#	      else
	 /* other VAXes, SPARC, and various flavors of Sun 2s and Sun 3s use */
	 /* the default heuristic, which is to take the address of a local   */
	 /* variable in gc_init, and round it up to the next multiple        */
	 /* of 16 Meg.  This is crucial on Suns, since various models        */
	 /* that are supposed to be able to share executables, do not        */
	 /* use the same stack base.  In particular, Sun 3/80s are           */
	 /* different from other Sun 3s.                                     */
	 /* This probably works on some more of the above machines.          */
#	      endif
#	    endif
#         endif
#       endif
#     endif
#   endif
# endif

/* Start of data segment for each of the above systems.  Note that the */
/* default case works only for contiguous text and data, such as on a  */
/* Vax.                                                                */
# ifdef M68K_SUN
#   define DATASTART ((char *)((((long) (&etext)) + 0x1ffff) & ~0x1ffff))
# else
#   ifdef RT
#     define DATASTART ((char *) 0x10000000)
#   else
#     ifdef I386
#       define DATASTART ((char *)((((long) (&etext)) + 0xfff) & ~0xfff))
#     else
#       ifdef NS32K
	  extern char **environ;
#         define DATASTART ((char *)(&environ))
			      /* hideous kludge: environ is the first   */
			      /* word in crt0.o, and delimits the start */
			      /* of the data segment, no matter which   */
			      /* ld options were passed through.        */
#       else
#         ifdef MIPS
#           define DATASTART 0x10000000
			      /* Could probably be slightly higher since */
			      /* startup code allocates lots of junk     */
#         else
#           ifdef M68K_HP
#             define DATASTART ((char *)((((long) (&etext)) + 0xfff) & ~0xfff))
#	    else
#             ifdef IBMRS6000
#		define DATASTART ((char *)0x20000000)
#             else
#               define DATASTART (&etext)
#	      endif
#           endif
#         endif
#       endif
#     endif
#   endif
# endif

# define HINCR 16          /* Initial heap increment, in blocks of 4K        */
# define MAXHINCR 512      /* Maximum heap increment, in blocks              */
# define HINCR_MULT 3      /* After each new allocation, hincr is multiplied */
# define HINCR_DIV 2       /* by HINCR_MULT/HINCR_DIV                        */
# define GC_MULT 3         /* Don't collect if the fraction of   */
			   /* non-collectable memory in the heap */
			   /* exceeds GC_MUL/GC_DIV              */
# define GC_DIV  4

# define NON_GC_HINCR 8    /* Heap increment if most of heap if collection */
			   /* was suppressed because most of heap is not   */
			   /* collectable                                  */

/*  heap address bounds.  These are extreme bounds used for sanity checks. */
/*  HEAPLIM may have to be increased for machines with incredibly large    */
/*  amounts of memory.                                                     */

#ifdef RT
#   define HEAPSTART 0x10000000
#   define HEAPLIM   0x1fff0000
#else
# if defined(M68K_SUN) || defined(M68K_HP)
#   define HEAPSTART 0x00010000
#   define HEAPLIM   0x04000000
# else
#   ifdef SPARC
#       define HEAPSTART 0x00010000
#       define HEAPLIM   0x10000000
#   else
#     ifdef VAX
#       define HEAPSTART 0x400
#       define HEAPLIM   0x10000000
#     else
#       ifdef I386
#         define HEAPSTART 0x1000
#         define HEAPLIM 0x10000000
#       else
#         ifdef NS32K
#           define HEAPSTART 0x2000
#           define HEAPLIM   0x10000000
#         else
#           ifdef MIPS
#             define HEAPSTART 0x10000000
#             define HEAPLIM 0x20000000
#           else
#	      ifdef IBMRS6000
#		define HEAPSTART 0x20000000
#		define HEAPLIM 0x2ff70000
#	      else
	         --> values unknown <--
#	      endif
#           endif
#         endif
#       endif
#     endif
#   endif
# endif
#endif

/*********************************/
/*                               */
/* Machine-dependent defines     */
/*                               */
/*********************************/

#define WORDS_TO_BYTES(x)   ((x)<<2)
#define BYTES_TO_WORDS(x)   ((x)>>2)

#define WORDSZ              32
#define LOGWL               5    /* log[2] of above */
#define BYTES_PER_WORD      (sizeof (word))
#define ONES                0xffffffff
#define MSBYTE              0xff000000
#define SIGNB               0x80000000
#define MAXSHORT            0x7fff
#define modHALFWORDSZ(n) ((n) & 0xf)    /* mod n by size of half word    */
#define divHALFWORDSZ(n) ((n) >> 4)	/* divide n by size of half word */
#define modWORDSZ(n) ((n) & 0x1f)       /* mod n by size of word         */
#define divWORDSZ(n) ((n) >> 5)         /* divide n by size of word      */
#define twice(n) ((n) << 1)             /* double n                      */

typedef unsigned long word;

#define TRUE  1
#define FALSE 0

/*********************/
/*                   */
/*  Size Parameters  */
/*                   */
/*********************/

/*  heap block size, bytes */
/* for RT see comment below */

#define HBLKSIZE   0x1000


/*  max size objects supported by freelist (larger objects may be   */
/*  allocated, but less efficiently)                                */
/*      asm(".set MAXOBJSZ,0x200")      if HBLKSIZE/2 == 0x200      */

#define MAXOBJSZ    (HBLKSIZE/8)
		/* Should be BYTES_TO_WORDS(HBLKSIZE/2), but a cpp */
		/* misfeature prevents that.                       */
#define MAXAOBJSZ   (HBLKSIZE/8)

# define divHBLKSZ(n) ((n) >> 12)
 
# define modHBLKSZ(n) ((n) & 0xfff)
 
# define HBLKPTR(objptr) ((struct hblk *)(((long) (objptr)) & ~0xfff))



/********************************************/
/*                                          */
/*    H e a p   B l o c k s                 */
/*                                          */
/********************************************/

/*  heap block header */
#define HBLKMASK   (HBLKSIZE-1)

#define BITS_PER_HBLK (HBLKSIZE * 8)

#define MARK_BITS_PER_HBLK (BITS_PER_HBLK/WORDSZ)
	   /* upper bound                                    */
	   /* We allocate 1 bit/word.  Only the first word   */
	   /* in each object is actually marked.             */

# ifdef ALIGN_DOUBLE
#   define MARK_BITS_SZ (((MARK_BITS_PER_HBLK + 2*WORDSZ - 1)/(2*WORDSZ))*2)
# else
#   define MARK_BITS_SZ ((MARK_BITS_PER_HBLK + WORDSZ - 1)/WORDSZ)
# endif
	   /* Upper bound on number of mark words per heap block  */

struct hblkhdr {
    long hbh_sz;    /* sz > 0 ==> objects are sz-tuples of poss. pointers */
		    /* sz < 0 ==> objects are sz-tuples not pointers      */
		    /* if free, the size in bytes of the whole block      */
		    /* Misc.c knows that hbh_sz comes first.              */
# ifndef HBLK_MAP
    struct hblk ** hbh_index;   /* Pointer to heap block list entry   */
				/* for this block                     */
# else
#   ifdef ALIGN_DOUBLE
      /* Add another 1 word field to make the total even.  Gross, but ... */
	long hbh_dummy;
#   endif
# endif
    struct hblk * hbh_next; /* Link field for hblk free list */
    long hbh_mask;      /* If hbh_mask >= 0 then:                          */
			/*   x % (4 * hbh_sz) == x & hbh_mask              */
			/*   sz is a power of 2 and < the size of a heap   */
			/*     block.                                      */
			/* A hack to speed up pointer validity check on    */
			/* machines with slow division.                    */
    long hbh_marks[MARK_BITS_SZ];
			    /* Bit i in the array refers to the             */
			    /* object starting at the ith word (header      */
			    /* INCLUDED) in the heap block.                 */
			    /* For free blocks, hbh_marks[0] = 1, indicates */
			    /* block is uninitialized.                      */
};

/*  heap block body */

# define BODY_SZ ((HBLKSIZE-sizeof(struct hblkhdr))/sizeof(word))

struct hblk {
    struct hblkhdr hb_hdr;
    word hb_body[BODY_SZ];
};

# define hb_sz hb_hdr.hbh_sz
# ifndef HBLK_MAP
#   define hb_index hb_hdr.hbh_index
# endif
# define hb_marks hb_hdr.hbh_marks
# define hb_next hb_hdr.hbh_next
# define hb_uninit hb_hdr.hbh_marks[0]
# define hb_mask hb_hdr.hbh_mask

/*  lists of all heap blocks and free lists  */
/* These are grouped together in a struct    */
/* so that they can be easily skipped by the */
/* mark routine.                             */
/* Mach_dep.c knows about the internals      */
/* of this structure.                        */

struct __gc_arrays {
  struct obj * _aobjfreelist[MAXAOBJSZ+1];
			  /* free list for atomic objs*/
  struct obj * _objfreelist[MAXOBJSZ+1];
			  /* free list for objects */
# ifdef HBLK_MAP
    char _hblkmap[MAP_SIZE];
#   define HBLK_INVALID 0    /* Not administered by collector   */
#   define HBLK_VALID 0x7f   /* Beginning of a valid heap block */
    /* A value n, 0 < n < 0x7f denotes the continuation of a valid heap    */
    /* block which starts at the current address - n * HBLKSIZE or earlier */
# else
    struct hblk * _hblklist[MAXHBLKS];
# endif
};

extern struct __gc_arrays _gc_arrays; 

# define objfreelist _gc_arrays._objfreelist
# define aobjfreelist _gc_arrays._aobjfreelist
# ifdef HBLK_MAP
#   define hblkmap _gc_arrays._hblkmap
# else
#   define hblklist _gc_arrays._hblklist
# endif

# define begin_gc_arrays ((char *)(&_gc_arrays))
# define end_gc_arrays (((char *)(&_gc_arrays)) + (sizeof _gc_arrays))

struct hblk ** last_hblk;  /* Pointer to one past the real end of hblklist */

struct hblk * hblkfreelist;

extern long heapsize;       /* Heap size in bytes */

long hincr;                /* current heap increment, in blocks              */

/* Operations */
# define update_hincr  hincr = (hincr * HINCR_MULT)/HINCR_DIV; \
		       if (hincr > MAXHINCR) {hincr = MAXHINCR;}
# define HB_SIZE(p) abs((p) -> hb_sz)
# define abs(x)  ((x) < 0? (-(x)) : (x))

/*  procedures */

extern void
freehblk();

extern struct hblk *
allochblk();

/****************************/
/*                          */
/*   Objects                */
/*                          */
/****************************/

/*  object structure */

struct obj {
    union {
	struct obj *oun_link;   /* --> next object in freelist */
#         define obj_link       obj_un.oun_link
	word oun_component[1];  /* treats obj as list of words */
#         define obj_component  obj_un.oun_component
    } obj_un;
};

/*  Test whether something points to a legitimate heap object */


extern char end;

# ifdef HBLK_MAP
  char * heapstart; /* A lower bound on all heap addresses */
		    /* Known to be HBLKSIZE aligned.       */
# endif

char * heaplim;   /* 1 + last address in heap */

word * stacktop;  /* 1 + highest address in stack.  Set by gc_init. */

/* Check whether the given HBLKSIZE aligned hblk pointer refers to the   */
/* beginning of a legitimate chunk.                                      */
/* Assumes that *p is addressable                                        */
# ifdef HBLK_MAP
#   define is_hblk(p)  (hblkmap[divHBLKSZ(((long)p) - ((long)heapstart))] \
			== HBLK_VALID)
# else
#   define is_hblk(p) ( (p) -> hb_index >= hblklist \
			&& (p) -> hb_index < last_hblk \
			&& *((p)->hb_index) == (p))
# endif
# ifdef INTERIOR_POINTERS
    /* Return the hblk_map entry for the pointer p */
#     define get_map(p)  (hblkmap[divHBLKSZ(((long)p) - ((long)heapstart))])
# endif

# ifdef INTERIOR_POINTERS
  /* Return the word displacement of the beginning of the object to       */
  /* which q points.  q is an address inside hblk p for objects of size s */
  /* with mask m corresponding to s.                                      */
#  define get_word_no(q,p,s,m) \
	    (((long)(m)) >= 0 ? \
		(((((long)q) - ((long)p) - (sizeof (struct hblkhdr))) & ~(m)) \
		 + (sizeof (struct hblkhdr)) >> 2) \
		: ((((long)q) - ((long)p) - (sizeof (struct hblkhdr)) >> 2) \
		   / (s)) * (s) \
		   + ((sizeof (struct hblkhdr)) >> 2))
# else
  /* Check whether q points to an object inside hblk p for objects of size s */
  /* with mask m corresponding to s.                                         */
#  define is_proper_obj(q,p,s,m) \
	    (((long)(m)) >= 0 ? \
		(((((long)(q)) - (sizeof (struct hblkhdr))) & (m)) == 0) \
		: (((long) (q)) - ((long)(p)) - (sizeof (struct hblkhdr))) \
		   % ((s) << 2) == 0)
#  endif

/* The following is a quick test whether something is an object pointer */
/* It may err in the direction of identifying bogus pointers            */
/* Assumes heap + text + data + bss < 64 Meg.                           */
#ifdef M68K_SUN
#   define TMP_POINTER_MASK 0xfc000003  /* pointer & POINTER_MASK should be 0 */
#else
# ifdef RT
#   define TMP_POINTER_MASK 0xc0000003
# else
#   ifdef VAX
#     define TMP_POINTER_MASK 0xfc000003
#   else
#     ifdef SPARC
#       define TMP_POINTER_MASK 0xfc000003
#     else
#       ifdef I386
#         define TMP_POINTER_MASK 0xfc000003
#       else
#         ifdef NS32K
#           define TMP_POINTER_MASK 0xfc000003
#         else
#           ifdef MIPS
#             define TMP_POINTER_MASK 0xc0000003
#           else
#             ifdef M68K_HP
#               define TMP_POINTER_MASK 0xfc000003
#             else
#		ifdef IBMRS6000
#		  define TMP_POINTER_MASK 0xd0000003
#		else
	          --> dont know <--
#		endif
#             endif
#           endif
#         endif
#       endif
#     endif
#   endif
# endif
#endif

#ifdef INTERIOR_POINTERS
#   define POINTER_MASK (TMP_POINTER_MASK & 0xfffffff8)
	/* Don't pay attention to whether address is properly aligned */
#else
#   define POINTER_MASK TMP_POINTER_MASK
#endif

#ifdef HBLK_MAP
#  define quicktest(p) (((long)(p)) > ((long)(heapstart)) \
			&& !(((unsigned long)(p)) & POINTER_MASK))
#else
# ifdef UNALIGNED
#  define quicktest(p) (((long)(p)) > ((long)(&end)) \
                        && !(((unsigned long)(p)) & POINTER_MASK) \
                        && (((long)(p)) & HBLKMASK))
	/* The last test throws out pointers to the beginning of heap */
        /* blocks.  Small integers shifted by 16 bits tend to look    */
        /* like these.                                                */
# else
#  define quicktest(p) (((long)(p)) > ((long)(&end)) \
			&& !(((unsigned long)(p)) & POINTER_MASK))
# endif
#endif


/*  Marks are in a reserved area in                          */
/*  each heap block.  Each word has one mark bits associated */
/*  with it. Only those corresponding to the beginning of an */
/*  object are used.                                         */


/* Operations */

/*
 * Retrieve, set, clear the mark bit corresponding
 * to the nth word in a given heap block.
 * Note that retrieval will work, so long as *hblk is addressable.
 * In particular, the check whether hblk is a legitimate heap block
 * can be postponed until after the mark bit is examined.
 *
 * (Recall that bit n corresponds to object beginning at word n)
 */

# define mark_bit(hblk,n) (((hblk)->hb_marks[divWORDSZ(n)] \
			    >> (modWORDSZ(n))) & 1)

/* The following assume the mark bit in question is either initially */
/* cleared or it already has its final value                         */
# define set_mark_bit(hblk,n) (hblk)->hb_marks[divWORDSZ(n)] \
				|= 1 << modWORDSZ(n)

# define clear_mark_bit(hblk,n) (hblk)->hb_marks[divWORDSZ(n)] \
				&= ~(1 << modWORDSZ(n))

/*  procedures */

/* Small object allocation routines */
extern struct obj * allocobj();
extern struct obj * allocaobj();

/* Small object allocation routines that mark only from registers */
/* expected to be preserved by C.                                 */
extern struct obj * _allocobj();
extern struct obj * _allocaobj();

/* general purpose allocation routines */
extern struct obj * gc_malloc();
extern struct obj * gc_malloc_atomic();

