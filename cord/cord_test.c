# include "cord.h"
# include <stdio.h>
/* This is a very incomplete test of the cord package.  It knows about	*/
/* a few internals of the package (e.g. when C strings are returned)	*/
/* that real clients shouldn't rely on.					*/

# define ABORT(string) \
{ int x = 0; fprintf(stderr, "FAILED: %s\n", string); x = 1 / x; }

int count;

int test_fn(char c, void * client_data)
{
    if (client_data != (void *)13) ABORT("bad client data");
    if (count < 64*1024+1) {
        if ((count & 1) == 0) {
            if (c != 'b') ABORT("bad char");
        } else {
            if (c != 'a') ABORT("bad char");
        }
        count++;
        return(0);
    } else {
        if (c != 'c') ABORT("bad char");
        count++;
        return(1);
    }
}


test_basics()
{
    CORD x = "ab";
    register int i;
    CORD y;
    CORD_pos p;
    
    x = CORD_cat(x,x);
    if (!IS_STRING(x)) ABORT("short cord should usually be a string");
    if (strcmp(x, "abab") != 0) ABORT("bad CORD_cat result");
    
    for (i = 1; i < 16; i++) {
        x = CORD_cat(x,x);
    }
    x = CORD_cat(x,"c");
    if (CORD_len(x) != 128*1024+1) ABORT("bad length");
    
    count = 0;
    if (CORD_iter5(x, 64*1024-1, test_fn, CORD_NO_FN, (void *)13) == 0) {
        ABORT("CORD_iter5 failed");
    }
    if (count != 64*1024 + 2) ABORT("CORD_iter5 failed");
    
    count = 0;
    CORD_set_pos(p, x, 64*1024-1);
    while(CORD_pos_valid(p)) {
       	(void) test_fn(CORD_pos_fetch(p), (void *)13);
	CORD_next(p);
    }
    if (count != 64*1024 + 2) ABORT("Position based iteration failed");
    
    y = CORD_substr(x, 1023, 5);
    if (!IS_STRING(y)) ABORT("short cord should usually be a string");
    if (strcmp(y, "babab") != 0) ABORT("bad CORD_substr result");
    
    y = CORD_substr(x, 1024, 8);
    if (!IS_STRING(y)) ABORT("short cord should usually be a string");
    if (strcmp(y, "abababab") != 0) ABORT("bad CORD_substr result");
    
    y = CORD_substr(x, 128*1024-1, 8);
    if (!IS_STRING(y)) ABORT("short cord should usually be a string");
    if (strcmp(y, "bc") != 0) ABORT("bad CORD_substr result");
    
    x = CORD_balance(x);
    if (CORD_len(x) != 128*1024+1) ABORT("bad length");
    
    count = 0;
    if (CORD_iter5(x, 64*1024-1, test_fn, CORD_NO_FN, (void *)13) == 0) {
        ABORT("CORD_iter5 failed");
    }
    if (count != 64*1024 + 2) ABORT("CORD_iter5 failed");
    
    y = CORD_substr(x, 1023, 5);
    if (!IS_STRING(y)) ABORT("short cord should usually be a string");
    if (strcmp(y, "babab") != 0) ABORT("bad CORD_substr result");
}

test_extras()
{
    register int i;
    CORD y = "abcdefghijklmnopqrstuvwxyz0123456789";
    CORD x = "{}";
    CORD w, z;
    FILE *f;
    
    for (i = 1; i < 100; i++) {
        x = CORD_cat(x, y);
    }
    z = CORD_balance(x);
    if (CORD_cmp(x,z) != 0) ABORT("balanced string comparison wrong");
    if (CORD_cmp(x,CORD_cat(z, CORD_nul(13))) >= 0) ABORT("comparison 2");
    if (CORD_cmp(CORD_cat(x, CORD_nul(13)), z) <= 0) ABORT("comparison 3");
    if (CORD_cmp(x,CORD_cat(z, "13")) >= 0) ABORT("comparison 4");
    if ((f = fopen("/tmp/cord_test", "w")) == 0) ABORT("open failed");
    if (CORD_put(z,f) == EOF) ABORT("CORD_put failed");
    if (fclose(f) == EOF) ABORT("fclose failed");
    w = CORD_from_file(fopen("/tmp/cord_test", "r"));
    if (CORD_len(w) != CORD_len(z)) ABORT("file length wrong");
    if (CORD_cmp(w,z) != 0) ABORT("file comparison wrong");
    if (CORD_cmp(CORD_substr(w, 50*36+2, 36), y) != 0)
    	ABORT("file substr wrong");
    z = CORD_from_file_lazy(fopen("/tmp/cord_test", "r"));
    if (CORD_cmp(w,z) != 0) ABORT("File conversions differ");
    if (CORD_chr(w, 0, '9') != 37) ABORT("CORD_chr failed 1");
    if (CORD_chr(w, 3, 'a') != 38) ABORT("CORD_chr failed 2");
    if (CORD_rchr(w, CORD_len(w) - 1, '}') != 1) ABORT("CORD_rchr failed");
    x = y;
    for (i = 1; i < 14; i++) {
        x = CORD_cat(x,x);
    }
    if ((f = fopen("/tmp/cord_test", "w")) == 0) ABORT("2nd open failed");
    if (CORD_put(x,f) == EOF) ABORT("CORD_put failed");
    if (fclose(f) == EOF) ABORT("fclose failed");
    w = CORD_from_file(fopen("/tmp/cord_test", "r"));
    if (CORD_len(w) != CORD_len(x)) ABORT("file length wrong");
    if (CORD_cmp(w,x) != 0) ABORT("file comparison wrong");
    if (CORD_cmp(CORD_substr(w, 1000*36, 36), y) != 0)
    	ABORT("file substr wrong");
    if (strcmp(CORD_to_char_star(CORD_substr(w, 1000*36, 36)), y) != 0)
    	ABORT("char * file substr wrong");
    if (strcmp(CORD_substr(w, 1000*36, 2), "ab") != 0)
    	ABORT("short file substr wrong");
    if (remove("/tmp/cord_test") != 0) ABORT("remove failed");
}

main()
{
    test_basics();
    test_extras();
    fprintf(stderr, "SUCCEEDED\n");
    return(0);
}
