#include <stdio.h>
#include <gc.h>

#ifndef GC_ALPHA_VERSION
# define GC_ALPHA_VERSION GC_TMP_ALPHA_VERSION
#endif

int main()
{
    if (GC_ALPHA_VERSION == GC_NOT_ALPHA) {
	printf("gc%d.%d", GC_VERSION_MAJOR, GC_VERSION_MINOR);
    } else {
	printf("gc%d.%dalpha%d", GC_VERSION_MAJOR,
				 GC_VERSION_MINOR, GC_ALPHA_VERSION);
    }
    return 0;
}
