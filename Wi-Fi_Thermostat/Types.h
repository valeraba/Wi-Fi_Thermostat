#ifndef _Types_H_
#define _Types_H_

#ifdef ARDUINO
#ifdef ESP8266
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif
#define __flash PROGMEM
#endif

#ifdef __ICCAVR__
#define ARDUINO
#endif

#ifdef _WIN32 // Windows
	#include <string.h>
	#include <stddef.h>
	#include <stdlib.h>
	#include <stdio.h>
	#include <stdarg.h>

	#define ROTL32(v, n) _lrotl(v, n)

	typedef unsigned __int8	 __uint8;
	typedef unsigned __int16 __uint16;
	typedef unsigned __int32 __uint32;
	typedef unsigned __int64 __uint64;

#elif ARDUINO // Arduino
	#include <Arduino.h>
	#include <string.h>
	#include <stddef.h>
	#include <stdlib.h>
	#define ROTL32(v, n) ((v) << (n)) | ((v) >> (32 - (n)))
        
	typedef char				__int8;
	typedef signed short		__int16;
	typedef signed long			__int32;
	typedef signed long long	__int64;

	typedef unsigned char		__uint8;
	typedef unsigned short		__uint16;
	typedef unsigned long		__uint32;
	typedef unsigned long long	__uint64;
#else // FreeBSD	
	#include <string.h>
	#include <stddef.h>
	#include <stdlib.h>
	#include <stdint.h>
	#define ROTL32(v, n) ((v) << (n)) | ((v) >> (32 - (n)))

	typedef char		__int8;
	typedef short int	__int16;
	typedef int		__int32;
	typedef long long	__int64;
	typedef unsigned char		__uint8;
	typedef unsigned short		__uint16;
	typedef unsigned int		__uint32;
	typedef unsigned long long	__uint64;
#endif

#ifndef bool
	#define bool __uint8
	#define false 0
	#define true 1
#endif


typedef __int64 TimeStamp;

#ifdef ARDUINO
typedef void (*DebugLogFunc)(const __FlashStringHelper* aFormat, ...);
#else
typedef void (*DebugLogFunc)(const char* aFormat, ...);
#define F
#endif


#endif //_Types_H_
