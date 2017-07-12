#ifndef __IO_WIN_HMALLOC_HPP_INCLUDED__
#define __IO_WIN_HMALLOC_HPP_INCLUDED__

#include "winconf.hpp"
#include <assert.h>
#include <limits>
#include <memory>
#include <cstdlib>

namespace io {

#define IO_PREVENT_MACRO

struct memory_traits {

/*
		return static_cast<std::size_t>( sysconf(_SC_PAGESIZE) );
*/

	static inline std::size_t page_size()
	{
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		return static_cast<std::size_t>( si.dwPageSize );
	}

	static inline void* malloc IO_PREVENT_MACRO (std::size_t count) noexcept
	{
		return std::malloc(count);
	}

	static inline void free IO_PREVENT_MACRO (void * const ptr) noexcept
	{
		std::free(ptr);
	}

	template<typename T>
	static inline std::size_t distance(const T* less_address,const T* lager_address) noexcept
	{
		std::ptrdiff_t diff = lager_address - less_address;
		return diff > 0 ? static_cast<std::size_t>(diff) : 0;
	}

	template<typename T>
	static inline std::size_t raw_distance(const T* less_address,const T* lager_address) noexcept
	{
		return distance<T>(less_address,lager_address) * sizeof(T);
	}

	template<typename T>
	static inline T* calloc_temporary(std::size_t count) noexcept
	{
		return std::get_temporary_buffer<T>(count).first;
	}

	template<typename T>
	static inline void free_temporary(T* const ptr) noexcept
	{
		std::return_temporary_buffer<T>(ptr);
	}

};

} // namesapace io

#endif // __IO_WIN_HMALLOC_HPP_INCLUDED__
