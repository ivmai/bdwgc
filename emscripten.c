#ifdef __EMSCRIPTEN__
#include <emscripten.h>

#include "private/gc_priv.h"

static void* stack_base;
static void* stack_bottom;

static void scan_fun(void *begin, void *end)
{
	stack_base = end;
	stack_bottom = begin;
}

void *GC_get_main_emscripten_stack_base()
{
	emscripten_scan_stack(scan_fun);
	return stack_base;
}

static void scan_regs(void *begin, void *end)
{
	GC_push_all_stack((ptr_t)begin, (ptr_t)end);
}

void GC_CALLBACK GC_default_emscripten_push_other_roots(void)
{
	/*
	 * This neess -s ASYNCIFY -s ASYNCIFY_STACK_SIZE=128000
	 *
	 * but hopefully the latter is only required for gctest
	 */
	emscripten_scan_registers(scan_regs);
}
#endif
