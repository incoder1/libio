#ifndef __IO_ICONV_CONV_ENGINE_HPP_INCLUDED__
#define __IO_ICONV_CONV_ENGINE_HPP_INCLUDED__

#include <cerrno>
#include <iconv.h>

#ifdef HAS_PRAGMA_ONCE
#pragma once
#endif // HAS_PRAGMA_ONCE

// for non GNU libiconv, gnu libc iconv for example
#ifndef iconvctl

#define ICONV_SET_DISCARD_ILSEQ   4  /* const int *argument */

static int iconvctl (iconv_t cd, int request, void* argument)
{
	return 1;
}

#endif // iconvctl


namespace io {

/// \brief Character set conversation (transcoding) error code
enum converrc: int {
	/// conversation was success
	success = 0,

	/// character code conversion invalid multi-byte sequence
	invalid_multibyte_sequence = 1,

	/// character code conversion incomplete multi-byte sequence
	incomplete_multibyte_sequence = 2,

	/// character code conversion no more room i.e. output buffer overflow
	no_buffer_space = 3,

	/// conversation between character's is not supported
	not_supported,

	/// An unknown error
	unknown = -1
};

enum class cnvrt_control
{
	failure_on_failing_chars,
	discard_on_failing_chars
};


namespace detail {

#define INVALID_ICONV_DSPTR reinterpret_cast<::iconv_t>(-1)
#define ICONV_ERROR static_cast<std::size_t>(-1)

class engine {
	engine(const engine&) = delete;
	engine& operator=(const engine&) = delete;
private:

	static inline converrc iconv_to_conv_errc(int err_no) noexcept {
		switch(err_no) {
		case 0:
			return converrc::success;
		case E2BIG:
			return converrc::no_buffer_space;
		case EILSEQ:
			return converrc::invalid_multibyte_sequence;
		case EINVAL:
			return converrc::incomplete_multibyte_sequence;
		default:
			return converrc::unknown;
		}
	}

public:

	engine(engine&& other) noexcept:
		iconv_(other.iconv_)
	{
		other.iconv_ = INVALID_ICONV_DSPTR;
	}

	engine& operator=(engine&& rhs) noexcept {
		engine( std::forward<engine>(rhs) ).swap( *this );
		return *this;
	}

	explicit operator bool() {
		return is_open();
	}

	constexpr engine() noexcept:
		iconv_(INVALID_ICONV_DSPTR)
	{}

	engine(const char* from,const char* to, cnvrt_control control) noexcept:
		iconv_(INVALID_ICONV_DSPTR)
	{
		iconv_ = ::iconv_open( to, from );
		if(INVALID_ICONV_DSPTR != iconv_) {
			int discard = control == cnvrt_control::discard_on_failing_chars? 1: 0;
			::iconvctl(iconv_, ICONV_SET_DISCARD_ILSEQ, &discard);
		}
	}

	~engine() noexcept {
		if(INVALID_ICONV_DSPTR != iconv_)
			::iconv_close(iconv_);
	}

	inline void swap(engine& other) noexcept {
		std::swap(iconv_, other.iconv_);
	}

	inline bool is_open() noexcept {
		return INVALID_ICONV_DSPTR != iconv_;
	}

	converrc convert(uint8_t** src,std::size_t& size, uint8_t** dst, std::size_t& avail) const noexcept
	{
		char **s = reinterpret_cast<char**>(src);
		char **d = reinterpret_cast<char**>(dst);
		if( ICONV_ERROR == ::iconv(iconv_, s, std::addressof(size), d, std::addressof(avail) ) )
			return iconv_to_conv_errc(errno);
		return converrc::success;
	}

private:
	iconv_t iconv_;
};

#undef INVALID_ICONV_DSPTR
#undef ICONV_ERROR

} // namespace detail


std::error_code make_error_code(io::converrc ec) noexcept;
std::error_condition make_error_condition(io::converrc err) noexcept;

class chconv_error_category final: public std::error_category {
private:

	friend std::error_code make_error_code(io::converrc ec) noexcept {
		return std::error_code( ec, *(chconv_error_category::instance()) );
	}

	friend std::error_condition make_error_condition(io::converrc err) noexcept {
		return std::error_condition(static_cast<int>(err), *(chconv_error_category::instance()) );
	}

	static inline const chconv_error_category* instance() {
		static chconv_error_category _instance;
		return &_instance;
	}

	static const char* cstr_message(int err_code) {
		converrc ec = static_cast<converrc>(err_code);
		switch( ec ) {
		case converrc::success:
			return "No error";
		case converrc::no_buffer_space:
			return "Destination buffer is to small to trascode all characters";
		case converrc::invalid_multibyte_sequence:
			return "Invalid multi-byte sequence";
		case converrc::incomplete_multibyte_sequence:
			return "Incomplete multi-byte sequence";
		case converrc::not_supported:
			return "Conversion between provided code-pages is not supported";
		case converrc::unknown:
			break;
		}
		return "Character conversion error";
	}

public:

	constexpr chconv_error_category() noexcept:
		std::error_category()
	{}

	virtual const char* name() const noexcept override {
		return "Character set conversation error";
	}

	virtual std::error_condition default_error_condition (int err) const noexcept override {
		return std::error_condition(err, *instance() );
	}

	virtual bool equivalent (const std::error_code& code, int condition) const noexcept override {
		return static_cast<int>(this->default_error_condition(code.value()).value()) == condition;
	}

	virtual std::string message(int err_code) const override {
		return std::string(  cstr_message(err_code) );
	}

};

} // namespace io

#endif // __IO_ICONV_CONV_ENGINE_HPP_INCLUDED__
