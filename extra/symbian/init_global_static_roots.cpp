// Symbian-specific file.

#include <e32def.h>

#include "gc.h"

extern "C" {

#if defined(__WINS__)
extern int winscw_data_start, winscw_data_end;
#else
extern int Image$$RW$$Limit[], Image$$RW$$Base[];
#endif

GC_API void GC_CALL
GC_init_global_static_roots()
{
  void *dataStart;
  void *dataEnd;

#if defined(__WINS__)
  dataStart = &winscw_data_start;
  dataEnd = &winscw_data_end;
#else
  dataStart = (void *)Image$$RW$$Base;
  dataEnd = (void *)Image$$RW$$Limit;
#endif
  GC_add_roots(dataStart, dataEnd);
}

} /* extern "C" */
