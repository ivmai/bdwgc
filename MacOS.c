/*
	MacOS.c
	
	Some routines for the Macintosh OS port of the Hans-J. Boehm, Alan J. Demers
	garbage collector.
	
	<Revision History>
	
	11/22/94  pcb  StripAddress the temporary memory handle for 24-bit mode.
	11/30/94  pcb  Tracking all memory usage so we can deallocate it all at once.
	
	by Patrick C. Beard.
 */
/* Boehm, November 17, 1995 11:50 am PST */

#include <Resources.h>
#include <Memory.h>
#include <LowMem.h>
#include <stdio.h>
#include <stdlib.h>
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

/* track the use of temporary memory so it can be freed all at once. */

typedef struct TemporaryMemoryBlock TemporaryMemoryBlock, **TemporaryMemoryHandle;

struct TemporaryMemoryBlock {
	TemporaryMemoryHandle nextBlock;
	char data[];
};

static TemporaryMemoryHandle theTemporaryMemory = NULL;
static Boolean firstTime = true;

void GC_MacFreeTemporaryMemory(void);

Ptr GC_MacTemporaryNewPtr(Size size, Boolean clearMemory)
{
	static Boolean firstTime = true;
	OSErr result;
	TemporaryMemoryHandle tempMemBlock;
	Ptr tempPtr = nil;

	tempMemBlock = (TemporaryMemoryHandle)TempNewHandle(size + sizeof(TemporaryMemoryBlock), &result);
	if (tempMemBlock && result == noErr) {
		HLockHi((Handle)tempMemBlock);
		tempPtr = (**tempMemBlock).data;
		if (clearMemory) memset(tempPtr, 0, size);
		tempPtr = StripAddress(tempPtr);

		// keep track of the allocated blocks.
		(**tempMemBlock).nextBlock = theTemporaryMemory;
		theTemporaryMemory = tempMemBlock;
	}
	
	// install an exit routine to clean up the memory used at the end.
	if (firstTime) {
		atexit(&GC_MacFreeTemporaryMemory);
		firstTime = false;
	}
	
	return tempPtr;
}

void GC_MacFreeTemporaryMemory()
{
	long totalMemoryUsed = 0;
	TemporaryMemoryHandle tempMemBlock = theTemporaryMemory;
	while (tempMemBlock != NULL) {
		TemporaryMemoryHandle nextBlock = (**tempMemBlock).nextBlock;
		totalMemoryUsed += GetHandleSize((Handle)tempMemBlock);
		DisposeHandle((Handle)tempMemBlock);
		tempMemBlock = nextBlock;
	}
	theTemporaryMemory = NULL;
	fprintf(stdout, "[total memory used:  %ld bytes.]\n", totalMemoryUsed);
}
