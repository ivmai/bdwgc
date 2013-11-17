# include <stdio.h>
# include <gc.h>

int main(int argc, char ** argv)
{
    int i;

    for (i = 1; i < argc; i++) {
      printf("gc-%d.%d.%d/%s ",
             GC_VERSION_MAJOR, GC_VERSION_MINOR, GC_VERSION_MICRO, argv[i]);
    }
    return(0);
}
