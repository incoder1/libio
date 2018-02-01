﻿/*
 *
 * Copyright (c) 2016
 * Viktor Gubin
 *
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#ifndef __COMPILLER_CONFIG_MSVC_HPP_INCLUDED__
#define __COMPILLER_CONFIG_MSVC_HPP_INCLUDED__

#pragma once

#if _MSVC_LANG < 201103L
#	error "This library needs at least MS VC++ 15 with /std:c++latest compiller option"
#endif // CPP11 detection

#include <intrin.h>
#include <stdint.h>

#define HAS_PRAGMA_ONCE

#ifndef _CPPRTTI
#	ifndef IO_NO_RTTI
#		define IO_NO_RTTI
#	endif
#endif

#ifndef IO_NO_INLINE
#	define IO_NO_INLINE __declspec(noinline)
#endif // IO_NO_INLINE

#ifndef  _CPPUNWIND
#	define IO_NO_EXCEPTIONS
// use static STL and stdlib C++ when exeptions off
// to avoid std::unxpected, when exeptions off
#	ifdef _HAS_EXCEPTIONS
#		undef _HAS_EXCEPTIONS
#	endif
#	define _HAS_EXCEPTIONS 0
#	define _STATIC_CPPLIB
#endif // exception

// MS VC doesn't generate big endian code
#define IO_IS_LITTLE_ENDIAN 1

#if defined(_M_IX86) || defined(_M_AMD64)
#	define IO_CPU_INTEL
#endif

#if defined(_M_X64) || defined(_M_AMD64) || defined(_M_ARM64) || defined(_WIN64)
#	define IO_CPU_BITS_64
#endif

#ifdef IO_CPU_BITS_64
#	define io_size_t_abs(__x) llabs( (__x) )
#else
#	define io_size_t_abs(__x) labs( (__x) )
#endif


#define io_bswap16 _byteswap_ushort
#define io_bswap32 _byteswap_ulong
#define io_bswap64 _byteswap_uint64

#define io_alloca(x) _alloca((x))

#define io_memmove(__dst, __src, __bytes) memmove( (__dst), (__src), (__bytes) )

#define io_memchr(__s,__c,__n) memchr( (__s), (__c), (__n) )

#define io_memset(__p,__v,__bytes) memset( (__p), (__v), (__bytes) )

#define io_memcmp(__p1,__p2,__bytes) memcmp( (__p1), (__p2),(__bytes) )

#define io_zerro_mem(__p,__bytes) memset( (__p), 0, (__bytes) )

#define io_strstr(__s1,__s2) strstr( (__s1), (__s2) )

#define io_strchr(__s,__c) strchr( (__s), (__c) )

#define io_strlen(__s) strlen( (__s) )

#define io_strcmp(__lsh,__rsh) strcmp( (__lsh), (__rhs) )

#define io_strcpy(__s1,__s2 ) strcpy( (__s1), (__s2) )

#define io_strcspn(__s, __p) strcspn( (__s), (__p) )

#define io_strpbrk(__s, __p) strpbrk( (__s), (__p) )

#define io_strchr(__s,__c) strchr( (__s), (__c) )

#define io_isalpha(__ch) isalpha((__ch))
#define io_isspace(__ch) isspace((__ch))
#define io_islower(__ch) islower((__ch))
#define io_isupper(__ch) isupper((__ch))
#define io_isdigit(__ch) isdigit((__ch))
#define io_tolower(__ch) tolower((__ch))
#define io_toupper(__ch) toupper((__ch))


#ifndef IO_PUSH_IGNORE_UNUSED_PARAM

#	define IO_PUSH_IGNORE_UNUSED_PARAM\
			_Pragma("warning( push )")\
			_Pragma("warning( push )")\
			_Pragma("warning( disable : CA1801)")

#	define IO_POP_IGNORE_UNUSED_PARAM _Pragma("warning( pop )")

#endif // ignore unused parameters warning, for stub templates and plased new operators

inline int io_clz(unsigned long x )
{
   unsigned long ret = 0;
   _BitScanForward(&ret, x);
   return  static_cast<int>(ret);
}


#endif // __COMPILLER_CONFIG_MSVC_HPP_INCLUDED__