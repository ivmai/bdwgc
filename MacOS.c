/*
	MacOS.c
	
	Some routines for the Macintosh OS port of the Hans-J. Boehm, Alan J. Demers
	garbage collector.
	
	<Revision History>
	
	by Patrick C. Beard.
 */
/* Boehm, July 28, 1994 10:35 am PDT */

#include <Resources.h>
#include <Memory.h>
#include <LowMem.h>
#include <stdio.h>
#include <string.h>

// use 'CODE' resource 0 to get exact location of the beginning of global space.

typedef struct {
	unsigned long aboveA5;
	unsigned long belowA5;
	unsigned long JTSize;
	unsigned long JTOffset;
} *CodeZeroPtr, **CodeZeroHandle;

void* GC_MacGetDataStart()
{
	CodeZeroHandle code0 = (CodeZeroHandle)GetResource('CODE', 0);
	if (code0) {
		long belowA5Size = (**code0).belowA5;
		ReleaseResource((Handle)code0);
		return (LMGetCurrentA5() - belowA5Size);
	}
	fprintf(stderr, "Couldn't load the jump table.");
	exit(-1);
	return 0;
}

Ptr GC_MacTemporaryNewPtr(Size size, Boolean clearMemory)
{
	OSErr result;
	Handle tempHandle;
	Ptr tempPtr;
	
	tempHandle = TempNewHandle(size, &result);
	if (tempHandle && result == noErr) {
		HLockHi(tempHandle);
		tempPtr = *tempHandle;
		if (clearMemory) memset(tempPtr, 0, size);
		return tempPtr;
	}
	return nil;
}
