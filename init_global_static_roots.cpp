
// INCLUDE FILES
#include <e32def.h>

#include <gc.h>
#include <gcconfig.h>

#include "init_global_static_roots.h"

#ifdef __cplusplus
extern "C" {
#endif


void init_global_static_roots()
{
	ptr_t dataStart = NULL;
	ptr_t dataEnd = NULL;	
	#if defined (__WINS__)
		extern int winscw_data_start, winscw_data_end;
		dataStart = ((ptr_t)&winscw_data_start);
		dataEnd   = ((ptr_t)&winscw_data_end);		
	#else
	    extern int Image$$RW$$Limit[], Image$$RW$$Base[];
	    dataStart = ((ptr_t)Image$$RW$$Base);
	    dataEnd   = ((ptr_t)Image$$RW$$Limit);	    			
	#endif //__WINS__	
	
	GC_add_roots(dataStart, dataEnd); 

}


#ifdef __cplusplus
	}
#endif
