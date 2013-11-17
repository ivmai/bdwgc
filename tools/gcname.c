#include <stdio.h>
#include <gc.h>

int main(void)
{
    printf("gc-%d.%d.%d",
           GC_VERSION_MAJOR, GC_VERSION_MINOR, GC_VERSION_MICRO);
    return 0;
}
