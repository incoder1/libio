/*
 *
 * Copyright (c) 2016
 * Viktor Gubin
 *
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */
#include "stdafx.hpp"
#include "charsetcvt.hpp"
#include "strings.hpp"


namespace io {

// chconv_error_category


#ifdef IO_IS_LITTLE_ENDIAN
#	define SYSTEM_UTF16 code_pages::UTF_16LE
#	define SYSTEM_UTF32 code_pages::UTF_32LE
#else
#	define SYSTEM_UTF16 code_pages::UTF_16BE
#	define SYSTEM_UTF32 code_pages::UTF_32BE
#endif // IO_IS_LITTLE_ENDIAN


static detail::engine new_engine(const charset& from, const charset& to, cnvrt_control control) noexcept
{
#ifdef IO_CONV_ENGINE_ICONV
	return detail::engine( from.name(), to.name(), control);
#elif defined( IO_CONV_EGNINE_MSMLANG )
	return detail::engine( from.code(), to.code(), control);
#endif // IO_CONV_ENGINE_ICONV
}


s_code_cnvtr code_cnvtr::open(std::error_code& ec,
							  const charset& from,
							  const charset& to,
							  cnvrt_control control) noexcept
{
	if( from == to ) {
		ec = make_error_code(converrc::not_supported);
		return s_code_cnvtr();
	}
	detail::engine eng( new_engine(from, to, control) );
	if(!eng) {
		ec = make_error_code(converrc::not_supported );
		return s_code_cnvtr();
	}
	code_cnvtr* ret = io::nobadalloc<code_cnvtr>::construct( ec, std::move(eng)  );
	return !ec ? s_code_cnvtr( ret ) : s_code_cnvtr();
}

code_cnvtr::code_cnvtr(detail::engine&& eng) noexcept:
	object(),
	eng_( std::forward<detail::engine>(eng) )
{
}

void code_cnvtr::convert(std::error_code& ec, uint8_t** in,std::size_t& in_bytes_left,uint8_t** const out, std::size_t& out_bytes_left) const noexcept
{
	converrc result = eng_.convert(in, in_bytes_left, out, out_bytes_left);
	if( converrc::success != result ) {
		ec = make_error_code(result);
		return;
	}
}

void code_cnvtr::convert(std::error_code& ec, const uint8_t* src,const std::size_t size, byte_buffer& dst) const noexcept
{
	dst.clear();
	std::size_t left = size;
	std::size_t available = dst.capacity();
	uint8_t** s = const_cast<uint8_t**>( std::addressof(src) );
	const uint8_t* d = dst.position().get();
	while(!ec && left > 0)
		convert(ec, s, left, const_cast<uint8_t**>( std::addressof(d) ), available);
	dst.move(dst.capacity() - available);
	dst.flip();
}

// free functions
std::size_t IO_PUBLIC_SYMBOL transcode(std::error_code& ec, const uint8_t* u8_src, std::size_t src_bytes, char16_t* const dst, std::size_t dst_size) noexcept
{
	assert(nullptr != u8_src && src_bytes > 0);
	assert(nullptr != dst && dst_size > 0);
	static detail::engine eng( new_engine(
								   code_pages::UTF_8,
								   SYSTEM_UTF16,
								   cnvrt_control::failure_on_failing_chars)
							 );
	uint8_t* s = const_cast<uint8_t*>( reinterpret_cast<const uint8_t*>(u8_src) );
	uint8_t* d = reinterpret_cast<uint8_t*>(dst);
	std::size_t left = src_bytes;
	std::size_t avail = dst_size * sizeof(char16_t);
	converrc result = eng.convert(&s,left,&d,avail);
	if( converrc::success != result ) {
		ec = make_error_code(result);
		return 0;
	}
	return dst_size - (avail / sizeof(char16_t) );
}

std::size_t IO_PUBLIC_SYMBOL transcode(std::error_code& ec,const uint8_t* u8_src, std::size_t src_bytes, char32_t* const dst, std::size_t dst_size) noexcept
{
	assert(nullptr != u8_src && src_bytes > 0);
	assert(nullptr != dst && dst_size > 1);
	static detail::engine eng(new_engine(
								  code_pages::UTF_8,
								  SYSTEM_UTF32,
								  cnvrt_control::failure_on_failing_chars
							  )
							 );
	uint8_t* s = const_cast<uint8_t*>( reinterpret_cast<const uint8_t*>(u8_src) );
	uint8_t* d = reinterpret_cast<uint8_t*>(dst);
	std::size_t left = src_bytes;
	std::size_t avail = dst_size * sizeof(char32_t);
	converrc result = eng.convert( &s,left, &d, avail);
	if( converrc::success != result ) {
		ec = make_error_code(result);
		return 0;
	}
	return dst_size - (avail / sizeof(char32_t) );
}

std::size_t IO_PUBLIC_SYMBOL transcode(std::error_code& ec,const char16_t* u16_src, std::size_t src_width, uint8_t* const u8_dst, std::size_t dst_size) noexcept
{
	assert(nullptr != u16_src && src_width > 0);
	assert(nullptr != u8_dst && dst_size > 1);
	static detail::engine eng(new_engine(
								  SYSTEM_UTF16,
								  code_pages::UTF_8,
								  cnvrt_control::failure_on_failing_chars
							  ));
	uint8_t* s = const_cast<uint8_t*>( reinterpret_cast<const uint8_t*>(u16_src) );
	uint8_t* d = const_cast<uint8_t*>(u8_dst);
	std::size_t left = src_width * sizeof(char16_t);
	std::size_t avail = dst_size;
	converrc result = eng.convert( &s,left, &d, avail);
	if( converrc::success != result ) {
		ec = make_error_code(result);
		return 0;
	}
	return dst_size - avail;
}

std::size_t IO_PUBLIC_SYMBOL transcode(std::error_code& ec,const char32_t* u32_src, std::size_t src_width, uint8_t* const u8_dst, std::size_t dst_size) noexcept
{
	assert(nullptr != u8_dst && dst_size > 0);
	static detail::engine eng(new_engine(
								  SYSTEM_UTF32,
								  code_pages::UTF_8,
								  cnvrt_control::failure_on_failing_chars
							  )
							 );
	uint8_t* s = const_cast<uint8_t*>( reinterpret_cast<const uint8_t*>(u32_src) );
	uint8_t* d = const_cast<uint8_t*>(u8_dst);
	std::size_t left = src_width * sizeof(char32_t);
	std::size_t avail = dst_size;
	converrc result = eng.convert( &s,left, &d,avail);
	if( converrc::success != result ) {
		ec = make_error_code(result);
		return 0;
	}
	return dst_size - avail;
}

} // namespace io
