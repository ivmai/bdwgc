/*
 * Copyright (c) 1993 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to copy this code for any purpose,
 * provided the above notices are retained on all copies.
 *
 * Author: Hans-J. Boehm (boehm@parc.xerox.com)
 */
/*
 * These are functions on cords that do not need to understand their
 * implementation.  They serve also serve as example client code for
 * cord_basics.
 */
# include <stdio.h>
# include <string.h>
# include "cord.h"
# include "ec.h"
# define I_HIDE_POINTERS	/* So we get access to allocation lock.	*/
				/* We use this for lazy file reading, 	*/
				/* so that we remain independent 	*/
				/* of the threads primitives.		*/
# include "../gc.h"

/* The standard says these are in stdio.h, but they aren't always: */
# ifndef SEEK_SET
#   define SEEK_SET 0
# endif
# ifndef SEEK_END
#   define SEEK_END 2
# endif

# define BUFSZ 2048	/* Size of stack allocated buffers when		*/
			/* we want large buffers.			*/

typedef void (* oom_fn)(void);

# define OUT_OF_MEMORY {  if (CORD_oom_fn != (oom_fn) 0) (*CORD_oom_fn)(); \
			  abort("Out of memory\n"); }

typedef struct {
	size_t min;
	size_t max;
	size_t count;
	char * buf;
} CORD_fill_data;

int CORD_fill_proc(char c, void * client_data)
{
    register CORD_fill_data * d = (CORD_fill_data *)client_data;
    register size_t count = d -> count;
    
    (d -> buf)[count] = c;
    d -> count = ++count;
    if (count >= d -> min) {
    	return(1);
    } else {
    	return(0);
    }
}

int CORD_batched_fill_proc(const char * s, void * client_data)
{
    register CORD_fill_data * d = (CORD_fill_data *)client_data;
    register size_t count = d -> count;
    register size_t max = d -> max;
    register char * buf = d -> buf;
    register const char * t = s;
    
    while(((d -> buf)[count] = *t++) != '\0') {
        count++;
        if (count >= max) break;
    }
    d -> count = count;
    if (count >= d -> min) {
    	return(1);
    } else {
    	return(0);
    }
}

/* Fill buf with between min and max characters starting at i.  Returns */
/* the number of characters actually put in buf. Assumes min characters	*/
/* are available.							*/ 
size_t CORD_fill_buf(CORD x, size_t i, size_t min,
			     size_t max, char * buf)
{
    CORD_fill_data fd;
    
    fd.min = min;
    fd.max = max;
    fd.buf = buf;
    fd.count = 0;
    (void)CORD_iter5(x, i, CORD_fill_proc, CORD_batched_fill_proc, &fd);
    return(fd.count);
}


/* Compare two nonempty strings the hard way. */
int CORD_cmp_general_case(CORD x, size_t xlen, CORD y, size_t ylen)
{
    char xbuf [BUFSZ];
    char ybuf [BUFSZ];
    register size_t pos = 0;	/* First position not yet transfered to xbuf */
    register size_t n_to_get;
    register int result;  
    for (;;) {
        n_to_get = BUFSZ;
        if (xlen < BUFSZ) n_to_get = xlen;
        if (ylen < n_to_get) n_to_get = ylen;
        (void) CORD_fill_buf(x, pos, n_to_get, n_to_get, xbuf);
        (void) CORD_fill_buf(y, pos, n_to_get, n_to_get, ybuf);
        result = strncmp(xbuf,ybuf,n_to_get);
        if (result != 0) return(result);
        pos += n_to_get; xlen -= n_to_get; ylen -= n_to_get;
        if (xlen == 0) {
            if (ylen == 0) {
            	return(0);
            } else {
            	return(-1);
            }
        }
        if (ylen == 0) {
            return(1);
        }
    }
}


int CORD_cmp(CORD x, CORD y)
{
    if (x == 0) {
        if (y == 0) {
            return (0);
        } else {
            return(-1);
        }
    }
    if (y == 0) return(1);
    if(IS_STRING(x) && IS_STRING(y)) {
        return(strcmp(x, y));
    }
    {
#	define SBUFLEN 30
#	define MINCMPLEN 5
        char xbuf[SBUFLEN];
        char ybuf[SBUFLEN];
        register size_t xlen = CORD_len(x);
        register size_t ylen = CORD_len(y);
        register size_t req_len = 0;
        register int result;
        
        if (xlen <= SBUFLEN) req_len = xlen;
        if (ylen <= SBUFLEN && ylen < xlen) req_len = ylen;
        if (req_len != 0) {
            (void) CORD_fill_buf(x, 0, req_len, req_len, xbuf);
            (void) CORD_fill_buf(x, 0, req_len, req_len, ybuf);
            result = strncmp(xbuf, ybuf, req_len);
            if (result != 0) return(result);
            return(xlen-ylen);
        } else {
            /* Both have length > SBUFLEN */
            register size_t xchars;
            register size_t ychars;
            register int result;
            
            xchars = CORD_fill_buf(x, 0, MINCMPLEN, SBUFLEN, xbuf);
            ychars = CORD_fill_buf(y, 0, MINCMPLEN, SBUFLEN, ybuf);
            result = strncmp(xbuf, ybuf, xchars < ychars? xchars : ychars);
            if (result != 0) return(result);
            return(CORD_cmp_general_case(x, xlen, y, ylen));
        }
    }
}

char * CORD_to_char_star(CORD x)
{
    register size_t len;
    char * result;
    
    if (x == 0) return("");
    len = CORD_len(x);
    result = (char *)GC_MALLOC_ATOMIC(len + 1);
    if (result == 0) OUT_OF_MEMORY;
    if (CORD_fill_buf(x, 0, len, len, result) != len) abort("Goofed");
    result[len] = '\0';
    return(result);
}

typedef struct FetchDataRep {
    struct FetchCacheRep * new_cache;
    char character;
} * fetch_data;

int CORD_fetch_proc(char c, void * client_data)
{
    register fetch_data d = (fetch_data)client_data;
    
    d -> character = c;
    return(1);
}

char CORD_fetch(CORD x, size_t i)
{
    struct FetchDataRep result;
    
    if (!CORD_iter5(x, i, CORD_fetch_proc, CORD_NO_FN, &result)) {
    	abort("bad index?");
    }
    return (result.character);
}


int CORD_put_proc(char c, void * client_data)
{
    register FILE * f = (FILE *)client_data;
    
    return(putc(c, f) == EOF);
}

int CORD_batched_put_proc(const char * s, void * client_data)
{
    register FILE * f = (FILE *)client_data;
    
    return(fputs(s, f) == EOF);
}
    

int CORD_put(CORD x, FILE * f)
{
    if (CORD_iter5(x, 0, CORD_put_proc, CORD_batched_put_proc, f)) {
        return(EOF);
    } else {
    	return(1);
    }
}

typedef struct {
    size_t pos;		/* Current position in the cord */
    char target;	/* Character we're looking for	*/
} chr_data;

int CORD_chr_proc(char c, void * client_data)
{
    register chr_data * d = (chr_data *)client_data;
    
    if (c == d -> target) return(1);
    (d -> pos) ++;
    return(0);
}

int CORD_rchr_proc(char c, void * client_data)
{
    register chr_data * d = (chr_data *)client_data;
    
    if (c == d -> target) return(1);
    (d -> pos) --;
    return(0);
}

int CORD_batched_chr_proc(const char *s, void * client_data)
{
    register chr_data * d = (chr_data *)client_data;
    register char * occ = strchr(s, d -> target);
    
    if (occ == 0) {
      	d -> pos += strlen(s);
      	return(0);
    } else {
    	d -> pos += occ - s;
    	return(1);
    }
}

size_t CORD_chr(CORD x, size_t i, int c)
{
    chr_data d;
    
    d.pos = i;
    d.target = c;
    if (CORD_iter5(x, i, CORD_chr_proc, CORD_batched_chr_proc, &d)) {
        return(d.pos);
    } else {
    	return(CORD_NOT_FOUND);
    }
}

size_t CORD_rchr(CORD x, size_t i, int c)
{
    chr_data d;
    
    d.pos = i;
    d.target = c;
    if (CORD_riter4(x, i, CORD_rchr_proc, &d)) {
        return(d.pos);
    } else {
    	return(CORD_NOT_FOUND);
    }
}

void CORD_ec_flush_buf(CORD_ec x)
{
    register size_t len = x[0].ec_bufptr - x[0].ec_buf;
    char * s;

    if (len == 0) return;
    s = GC_MALLOC_ATOMIC(len+1);
    memcpy(s, x[0].ec_buf, len);
    s[len] = '\0';
    x[0].ec_cord = CORD_cat_char_star(x[0].ec_cord, s, len);
    x[0].ec_bufptr = x[0].ec_buf;
}

/*ARGSUSED*/
char CORD_nul_func(size_t i, void * client_data)
{
    return('\0');
}


CORD CORD_nul(size_t i)
{
    return(CORD_from_fn(CORD_nul_func, 0, i));
}

CORD CORD_from_file_eager(FILE * f)
{
    register int c;
    CORD_ec ecord;
    
    CORD_ec_init(ecord);
    for(;;) {
        c = getc(f);
        if (c == 0) {
          /* Append the right number of NULs	*/
          /* Note that any string of NULs is rpresented in 4 words,	*/
          /* independent of its length.					*/
            register size_t count = 1;
            
            CORD_ec_flush_buf(ecord);
            while ((c = getc(f)) == 0) count++;
            ecord[0].ec_cord = CORD_cat(ecord[0].ec_cord, CORD_nul(count));
        }
        if (c == EOF) break;
        CORD_ec_append(ecord, c);
    }
    (void) fclose(f);
    return(CORD_balance(CORD_ec_to_cord(ecord)));
}

/* The state maintained for a lazily read file consists primarily	*/
/* of a large direct-mapped cache of previously read values.		*/
/* We could rely more on stdio buffering.  That would have 2		*/
/* disadvantages:							*/
/*  	1) Empirically, not all fseek implementations preserve the	*/
/*	   buffer whenever they could.					*/
/*	2) It would fail if 2 different sections of a long cord		*/
/*	   were being read alternately.					*/
/* We do use the stdio buffer for read ahead.				*/
/* To guarantee thread safety in the presence of atomic pointer		*/
/* writes, cache lines are always replaced, and never modified in	*/
/* place.								*/

# define LOG_CACHE_SZ 14
# define CACHE_SZ (1 << LOG_CACHE_SZ)
# define LOG_LINE_SZ 7
# define LINE_SZ (1 << LOG_LINE_SZ)

typedef struct {
    size_t tag;
    char data[LINE_SZ];
    	/* data[i%LINE_SZ] = ith char in file if tag = i/LINE_SZ	*/
} cache_line;

typedef struct {
    FILE * lf_file;
    size_t lf_current;	/* Current file pointer value */
    cache_line * volatile lf_cache[CACHE_SZ/LINE_SZ];
} lf_state;

# define MOD_CACHE_SZ(n) ((n) & (CACHE_SZ - 1))
# define DIV_CACHE_SZ(n) ((n) >> LOG_CACHE_SZ)
# define MOD_LINE_SZ(n) ((n) & (LINE_SZ - 1))
# define DIV_LINE_SZ(n) ((n) >> LOG_LINE_SZ)
# define LINE_START(n) ((n) & ~(LINE_SZ - 1))

typedef struct {
    lf_state * state;
    size_t file_pos;	/* Position of needed character. */
    cache_line * new_cache;
} refill_data;

/* Executed with allocation lock. */
static char refill_cache(client_data)
refill_data * client_data;
{
    register lf_state * state = client_data -> state;
    register size_t file_pos = client_data -> file_pos;
    FILE *f = state -> lf_file;
    size_t line_start = LINE_START(file_pos);
    size_t line_no = DIV_LINE_SZ(MOD_CACHE_SZ(file_pos));
    cache_line * new_cache = client_data -> new_cache;
    
    if (line_start != state -> lf_current
        && fseek(f, line_start, SEEK_SET) != 0) {
    	    abort("fseek failed");
    }
    if (fread(new_cache -> data, sizeof(char), LINE_SZ, f)
    	<= file_pos - line_start) {
    	abort("fread failed");
    }
    new_cache -> tag = DIV_LINE_SZ(file_pos);
    /* Store barrier goes here. */
    state -> lf_cache[line_no] = new_cache;
    state -> lf_current = line_start + LINE_SZ;
    return(new_cache->data[MOD_LINE_SZ(file_pos)]);
}

char CORD_lf_func(size_t i, void * client_data)
{
    register lf_state * state = (lf_state *)client_data;
    register cache_line * cl = state -> lf_cache[DIV_LINE_SZ(MOD_CACHE_SZ(i))];
    
    if (cl == 0 || cl -> tag != DIV_LINE_SZ(i)) {
    	/* Cache miss */
    	refill_data rd;
    	
        rd.state = state;
        rd.file_pos =  i;
        rd.new_cache = GC_NEW_ATOMIC(cache_line);
        if (rd.new_cache == 0) OUT_OF_MEMORY;
        return((char)(GC_word)
        	  GC_call_with_alloc_lock((GC_fn_type) refill_cache, &rd));
    }
    return(cl -> data[MOD_LINE_SZ(i)]);
}    

/*ARGSUSED*/
void CORD_lf_close_proc(void * obj, void * client_data)  
{
    if (fclose(((lf_state *)obj) -> lf_file) != 0) {
    	abort("CORD_lf_close_proc: fclose failed");
    }
}			

CORD CORD_from_file_lazy_inner(FILE * f, size_t len)
{
    register lf_state * state = GC_NEW(lf_state);
    register int i;
    register int c;
    
    if (state == 0) OUT_OF_MEMORY;
    state -> lf_file = f;
    for (i = 0; i < CACHE_SZ/LINE_SZ; i++) {
        state -> lf_cache[i] = 0;
    }
    state -> lf_current = 0;
    GC_register_finalizer(state, CORD_lf_close_proc, 0, 0, 0);
    return(CORD_from_fn(CORD_lf_func, state, len));
}

CORD CORD_from_file_lazy(FILE * f)
{
    register size_t len;
    
    if (fseek(f, 0l, SEEK_END) != 0) {
        abort("Bad fd argument - fseek failed");
    }
    if ((len = ftell(f)) < 0) {
        abort("Bad fd argument - ftell failed");
    }
    rewind(f);
    return(CORD_from_file_lazy_inner(f, len));
}

# define LAZY_THRESHOLD (16*1024 + 1)

CORD CORD_from_file(FILE * f)
{
    register size_t len;
    
    if (fseek(f, 0l, SEEK_END) != 0) {
        abort("Bad fd argument - fseek failed");
    }
    if ((len = ftell(f)) < 0) {
        abort("Bad fd argument - ftell failed");
    }
    rewind(f);
    if (len < LAZY_THRESHOLD) {
        return(CORD_from_file_eager(f));
    } else {
        return(CORD_from_file_lazy_inner(f, len));
    }
}
