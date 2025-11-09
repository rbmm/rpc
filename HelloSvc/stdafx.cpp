
#include "stdafx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file
void* __cdecl operator new[](size_t ByteSize)
{
	return LocalAlloc(0, ByteSize);
}

void* __cdecl operator new(size_t ByteSize)
{
	return LocalAlloc(0, ByteSize);
}

void __cdecl operator delete(void* Buffer)
{
	LocalFree(Buffer);
}

void __cdecl operator delete(void* Buffer,size_t)
{
	LocalFree(Buffer);
}
void __cdecl operator delete[](void* Buffer)
{
	LocalFree(Buffer);
}

void __cdecl operator delete[](void* Buffer, size_t)
{
	LocalFree(Buffer);
}
