#ifndef loader_ext_h
#define loader_ext_h

#include "windef.h"
#include "attributes.h"

extern LPVOID FILE_dommap( int unix_handle, LPVOID start,
			   DWORD size_high, DWORD size_low,
			   DWORD offset_high, DWORD offset_low,
			   int prot, int flags );
extern int FILE_munmap( LPVOID start, DWORD size_high, DWORD size_low );
extern int wcsnicmp(const unsigned short* s1, const unsigned short* s2, int n);
extern int XINE_FORMAT_PRINTF(1, 2) __vprintf( const char *format, ... );

#endif
