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
#ifndef _IO_WIN_CONSOLE_HPP_INCLUDED__
#define _IO_WIN_CONSOLE_HPP_INCLUDED__

#include <config.hpp>

#ifdef HAS_PRAGMA_ONCE
#pragma once
#endif // HAS_PRAGMA_ONCE

#include <atomic>
#include <memory>
#include <iostream>

#include <text.hpp>
#include <stream.hpp>
#include <ostream>

#include <wincon.h>
#include <tchar.h>

#include "criticalsection.hpp"
#include "errorcheck.hpp"

#ifndef _CU
#	define _CU(quote) L##quote
#endif // __LOCALE_TEXT

namespace io {

class console;

namespace win {

class IO_PUBLIC_SYMBOL console_channel final: public read_write_channel {
	console_channel(const console_channel&) = delete;
	console_channel& operator=(const console_channel&) = delete;
	console_channel(console_channel&&) = delete;
	console_channel& operator=(console_channel&&) = delete;
public:
	console_channel(::HANDLE hcons, ::WORD orig, ::WORD attr) noexcept;
	virtual ~console_channel() noexcept override;
	virtual std::size_t read(std::error_code& err,uint8_t* const buff, std::size_t bytes) const noexcept override;
	virtual std::size_t write(std::error_code& err, const uint8_t* buff,std::size_t size) const noexcept override;
private:
	friend class io::console;

	void change_color(::DWORD attr) noexcept;

	::HANDLE hcons_;
	::WORD orig_attr_;
	::WORD attr_;
};

} // namesapce win

enum class text_color: uint8_t {
	navy_blue    = 0x01,
	navy_green   = 0x02,
	navy_aqua    = 0x03,
	navy_red     = 0x04,
	magenta      = 0x05,
	brown        = 0x06,
	white        = 0x07,
	gray         = 0x08,
	light_blue   = 0x09,
	light_green  = 0x0A,
	light_aqua   = 0x0B,
	light_red    = 0x0C,
	light_purple = 0x0D,
	yellow       = 0x0E,
	bright_white = 0x0F
};



/// \brief System console (terminal window) low level access API.
/// Windows based implementation
/// Console is not a replacement for C++ iostreams, it is for compatibility
/// with channels API. Unlike iostreams this is low level access interface to
/// the console OS system calls without locales and formating support.
/// Console is usefull to implement a software like POSIX 'cat','more' etc. when you
/// only need to output some raw character data into console screen, and don't care
/// about content.
/// Console is UNICODE (UTF-16LE) only! I.e. supports multi-languages text at time
class IO_PUBLIC_SYMBOL console {
	console(const console&) = delete;
	console operator=(const console&) = delete;
public:
	typedef ::HANDLE native_id;
	typedef channel_ostream<char> cons_ostream;
private:
	console();

	static const console* get();
	static void release_console() noexcept;

	static s_write_channel conv_channel(const s_write_channel& ch);

	/// Releases allocated console, and returns original console codepage
	~console() noexcept;

public:

	/// Reset default console output colors
	/// \param in color for input stream
	/// \param out color for output stream
	/// \param err color for error stream
	static void reset_colors(const text_color in,const text_color out,const text_color err) noexcept;

	/// Returns console input channel
	/// \return console input channel
	/// \throw can throw std::bad_alloc, when out of memory
	static inline s_read_channel in()
	{
		return s_read_channel( get()->cin_ );
	}

	/// Returns console output channel
	/// \throw can throw std::bad_alloc, when out of memory
	static inline s_write_channel out()
	{
		return s_write_channel( get()->cout_ );
	}

	static inline s_write_channel err()
	{
		return s_write_channel( get()->cerr_ );
	}

	/// Returns std::basic_stream<char> with ato-reconverting
	/// UTF-8 multibyte characters into console default UTF-16LE
	static std::ostream& out_stream();

	/// Returns std::basic_stream<char> with ato-reconverting
	/// UTF-8 multibyte characters into console default UTF-16LE
	static std::ostream& error_stream();

	static inline std::wostream& out_wstream()
	{
		return std::wcout;
	}

	static inline std::wostream& out_errstream()
	{
		return std::wcerr;
	}

	/// Returns console character set, always UTF-16LE for Windows
	/// \return current console character set
	/// \throw never throws
	static inline const charset& charset() noexcept
	{
		return code_pages::UTF_16LE;
	}
private:
	static std::atomic<console*> _instance;
	static io::critical_section _cs;
	bool need_release_;
	::UINT prev_charset_;
	win::console_channel *cin_;
	win::console_channel *cout_;
	win::console_channel *cerr_;
};

} // namesapce io

#endif // _IO_WIN_CONSOLE_HPP_INCLUDED__
