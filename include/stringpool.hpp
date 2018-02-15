/*
 * Copyright (c) 2016
 * Viktor Gubin
 *
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */
#ifndef __IO_STRINGPOOL_HPP_INCLUDED__
#define __IO_STRINGPOOL_HPP_INCLUDED__

#include "config.hpp"

#ifdef HAS_PRAGMA_ONCE
#pragma once
#endif // HAS_PRAGMA_ONCE

#include "text.hpp"

#include <atomic>
#include <ostream>
#include <string>
#include <unordered_map>

#include "hashing.hpp"
#include "object.hpp"

namespace io {

class string_pool;
DECLARE_IPTR(string_pool);

class IO_PUBLIC_SYMBOL cached_string final {
private:
	static inline void intrusive_add_ref(uint8_t* const ptr) noexcept
	{
		std::size_t volatile *p = reinterpret_cast<std::size_t volatile*>(ptr);
		detail::atomic_traits::inc(p);
	}
	static inline bool intrusive_release(uint8_t* const ptr) noexcept
	{
		std::size_t volatile *p = reinterpret_cast<std::size_t volatile*>(ptr);
		return static_cast<size_t>(0) == detail::atomic_traits::dec(p);
	}
	friend class string_pool;
	cached_string(const char* str, std::size_t length) noexcept;
public:

	typedef std::char_traits<char> traits_type;

	/// Creates empty constant string  object
	constexpr cached_string() noexcept:
		data_(nullptr)
	{}

	/// Shallow copy this string (inc reference count)
	cached_string(const cached_string& other) noexcept;

	/// Copy assignment operator, shallow copy this string
	cached_string& operator=(const cached_string& rhs) noexcept {
		cached_string(rhs).swap( *this );
		return *this;
	}

	/// Movement constructor, default movement semantic
	cached_string(cached_string&& other) noexcept:
		data_(other.data_)
	{
		other.data_ = nullptr;
	}

	/// Movement assignment operator, default movement semantic
	cached_string& operator=(cached_string&& other) noexcept {
		cached_string( std::forward<cached_string>(other) ).swap( *this );
		return *this;
	}

	/// Decrement this string reference count, release allocated memory when
	/// reference count bring to 0
	~cached_string() noexcept;

	/// Swaps this object with another one
	/// \param with object to swap with this
	inline void swap(cached_string& with) noexcept {
		std::swap(data_, with.data_);
	}

	/// Returns whether this string is pointing on nullptr
	/// \return whether nullptr string
	inline bool empty() const noexcept {
		return nullptr == data_;
	}

	/// Checks whether this string empty or contains only whitespace characters
	/// \return whether this string is blank
	bool blank() const noexcept;

	/// Returns raw C-style zero ending string
	/// \return C-style string, "" if string is empty
	inline const char* data() const noexcept {
		return nullptr == data_ ? "" : reinterpret_cast<char*>( data_ + sizeof(std::size_t) );
	}

#ifndef NDEBUG
	/// Returns raw C-style zero ending string same as data(), provided for IDE's and debuggers
	/// \return C-style string "" if string is empty
	inline const char* c_str() const noexcept {
		return data();
	}
#endif // NDEBUG

	/// Converts this string to system UCS-2 ( UTF-16 LE or BE)
	inline std::u16string convert_to_u16() const {
		return empty() ? std::u16string() : transcode_to_u16( data(), size() );
	}

	/// Converts this string to system UCS-4 ( UTF-32 LE or BE)
	inline std::u32string convert_to_u32() const {
		return empty() ? std::u32string() : transcode_to_u32( data(), size() );
	}

	/// Converts this string to system whide UNICODE (UTF-16/32 LE/BE OS and CPU byte order depends) representation
	inline std::wstring convert_to_ucs() const {
		return empty() ? std::wstring() : transcode_to_ucs( data(), size() );
	}

	/// Returns string size in bytes
	/// \return string size in bytes
	inline std::size_t size() const noexcept {
		return empty() ? 0 : traits_type::length( data() );
	}

	/// Returns underlying memory array address value
	/// \return  underlying memory array address value
	inline std::size_t hash() const noexcept {
		return reinterpret_cast<std::size_t>(data_) ;
	}

	/// Checks underlying character array is binary same to rhs array
	/// \param rhs array to check with
	/// \param bytes rhs array size in byte
	inline bool equal(const char* rhs, std::size_t bytes) const noexcept {
		const char *tmp = data();
		return tmp == rhs ? true : 0 == traits_type::compare( tmp, rhs, bytes);
	}

	/// Lexicographically compare the string with another
	/// \param rhs a string to compare with
	/// \return whether strings equals
	bool operator==(const cached_string& rhs) const noexcept {
		return data_ == rhs.data_;
	}

	bool operator<(const cached_string& rhs) const noexcept {
		return traits_type::compare( data(), rhs.data(), rhs.size() ) < 0;
	}

private:
	uint8_t* data_;
};

inline std::ostream& operator<<(std::ostream& os, const cached_string& cstr)
{
	return ( os << cstr.data() );
}

inline std::wostream& operator<<(std::wostream& os, const cached_string& cstr)
{
	return ( os << cstr.convert_to_ucs() );
}

inline std::basic_ostream<char16_t>& operator<<(std::basic_iostream<char16_t>& os, const cached_string& cstr)
{
	return ( os << cstr.convert_to_u16() );
}

inline std::basic_ostream<char32_t>& operator<<(std::basic_iostream<char32_t>& os, const cached_string& cstr)
{
	return ( os << cstr.convert_to_u32() );
}



/// \brief A pool of raw character arrays i.e. C style strings,
/*!
* Allocates memory for the binary equal string only once
* This can be used to save memory for storing string similar string objects
*/
class IO_PUBLIC_SYMBOL string_pool:public object {
	string_pool(const string_pool&) = delete;
	string_pool& operator=(const string_pool&) = delete;
private:

	/// Construct new string pool
	explicit string_pool() noexcept;
public:

	static s_string_pool create(std::error_code& ec) noexcept;

	virtual ~string_pool() noexcept override
	{}

	/// Returns a cached_string object for the raw character array.
	/// Allocates memory, if string is not presented in this pool
	/// \param s source character array
	/// \param size size of array in bytes
	/// \return cached_string object or empty cached string if out of memory or size is 0
	/// \throw never throws
	const cached_string get(const char* s, std::size_t count) noexcept;

	/// Returns a cached_string object for the C zero ending string
	/// Allocates memory, if string is not presented in memory.
	/// \param s source zero terminated C string
	/// \return cached_string object or empty cached string if out of memory or s is "" or nullptr
	/// \throw never throws
	inline const cached_string get(const char* s) noexcept {
		return get(s, std::char_traits<char>::length(s) );
	}

	/// Returns count of strings cached by this pool
	/// \return count of strings
	inline std::size_t size() const {
		return pool_.size();
	}
private:
	typedef std::unordered_map<
		std::size_t,
		cached_string,
		std::hash<std::size_t>,
		std::equal_to<std::size_t>,
		io::h_allocator< std::pair<const std::size_t, cached_string> >
		> pool_type;
	pool_type pool_;
};

} // namespace io

#endif // __IO_STRINGPOOL_HPP_INCLUDED__
