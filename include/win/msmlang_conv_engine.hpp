#ifndef __IO_MSMLANG_CONV_ENGINE_HPP_INCLUDED__
#define __IO_MSMLANG_CONV_ENGINE_HPP_INCLUDED__

#include <config.hpp>

#ifdef HAS_PRAGMA_ONCE
#pragma once
#endif // HAS_PRAGMA_ONCE

#include <atomic>
#include "criticalsection.hpp"

#include <mlang.h>

namespace io {

enum converrc {
	/// conversation was success
	success,
	/// conversation between character's is not supported
	not_supported,
	/// character code conversion no more room i.e. output buffer overflow
	no_buffer_space
};

enum class cnvrt_control: uint32_t
{
	failure_on_failing_chars = 0,
	discard_on_failing_chars = 8
};

namespace detail {

class engine {
	engine(const engine&) = delete;
	engine& operator=(const engine&) = delete;
public:

	engine(engine&& other) noexcept:
		ml_cc_(other.ml_cc_)
	{
		other.ml_cc_ = nullptr;
	}

	engine& operator=(engine&& rhs) noexcept {
		engine( std::forward<engine>(rhs) ).swap( *this );
		return *this;
	}

	engine(::DWORD from, ::DWORD to, cnvrt_control flags) noexcept;

	~engine() noexcept;

	explicit operator bool() noexcept {
		return is_open();
	}

	inline bool is_open() const {
		return nullptr != ml_cc_;
	}

	inline void swap(engine& other) noexcept {
		std::swap(ml_cc_, other.ml_cc_);
	}

	converrc convert(uint8_t** src,std::size_t& size, uint8_t** dst, std::size_t& avail) const noexcept;

private:
	::IMLangConvertCharset* ml_cc_;
};

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
		switch( static_cast<converrc>(err_code) ) {
		case converrc::success:
			return "No error";
		case converrc::no_buffer_space:
			return "Destination buffer is to small to trascode all characters";
		case converrc::not_supported:
			return "Conversion between provided code-pages is not supported";
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


#endif // __IO_MSMLANG_CONV_ENGINE_HPP_INCLUDED__
