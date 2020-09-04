/*
 * Copyright (c) 2016-2019
 * Viktor Gubin
 *
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */
#ifndef __CHANNELS_HPP_INCLUDED__
#define __CHANNELS_HPP_INCLUDED__

#include "config.hpp"

#ifdef HAS_PRAGMA_ONCE
#pragma once
#endif // HAS_PRAGMA_ONCE

#include <functional>
#include <system_error>
#include <type_traits>

#include "object.hpp"
#include "buffer.hpp"
#include "scoped_array.hpp"
#include "errorcheck.hpp"

namespace io {

/// \brief General input/output channel interface
/// \details Adds ability for put implementor into instrusive_ptr
class IO_PUBLIC_SYMBOL channel:public object {
public:
	channel(const channel&) = delete;
	channel& operator=(const channel&) = delete;
protected:
	constexpr channel() noexcept:
		object()
	{}
	virtual ~channel() override = default;
};

template <class C>
class unsafe {
};

/**
  General interface to input operations on an resource like a: file, socket, std in device, named pipe, shared memory blocks etc.
 **/
class IO_PUBLIC_SYMBOL read_channel:public virtual channel {
protected:
	read_channel() noexcept;
public:
	/// Reads certain number of bytes from underlying resource
	/// \param ec
	///		operation error code
	/// \param buff
	///		memory buffer to store read data, must be continues and at least size bytes length
	/// \param bytes
	///		requested bytes to read
	/// \return number of bytes read or 0 if nothing read or EOF riched
	/// \throw never throws
	virtual std::size_t read(std::error_code& ec,uint8_t* const buff, std::size_t bytes) const noexcept = 0;
};

DECLARE_IPTR(read_channel);

template <>
class unsafe<read_channel> {
public:
	explicit unsafe(s_read_channel&& ch) noexcept:
		ec_(),
		ch_( std::forward<s_read_channel>(ch) )
	{}
	explicit unsafe(const s_read_channel& ch) noexcept:
		ec_(),
		ch_(ch)
	{}
	std::size_t read(uint8_t* const buff, std::size_t bytes) const
	{
		std::size_t ret = ch_->read( ec_, buff, bytes);
		io::check_error_code( ec_ );
		return ret;
	}
private:
	mutable std::error_code ec_;
	s_read_channel ch_;
};

/**
 * General interface to output operations on an resource
 * like a: file, socket, std out device, named pipe, shared memory blocks etc.
 **/
class IO_PUBLIC_SYMBOL write_channel:public virtual channel {
protected:
	write_channel() noexcept;
public:
	/// Writes certain number of bytes to underlying resource
	/// \param ec
	///		operation error code
	/// \param buff
	///		memory buffer to storing data to write, must be continues and at least size bytes length
	/// \param size
	///		requested bytes to write
	/// \return number of bytes written or 0 if nothing written
	/// \throw never throws
	virtual std::size_t write(std::error_code& ec, const uint8_t* buff,std::size_t size) const noexcept = 0;
};

DECLARE_IPTR(write_channel);

template <>
class unsafe<write_channel> {
public:
	explicit unsafe(s_write_channel&& ch) noexcept:
		ec_(),
		ch_( std::forward<s_write_channel>(ch) )
	{}
	explicit unsafe(const s_write_channel& ch) noexcept:
		ec_(),
		ch_(ch)
	{}
	std::size_t write(const uint8_t* buff,const std::size_t size) const
	{
		std::size_t ret = ch_->write(ec_, buff, size);
		io::check_error_code( ec_ );
		return ret;
	}
private:
	mutable std::error_code ec_;
	s_write_channel ch_;
};


/**
* General interface to input and output operations on an resource
* like a: file, socket, std in/out device, named pipe, shared memory blocks etc.
**/
class IO_PUBLIC_SYMBOL read_write_channel:public virtual channel, public read_channel, public write_channel {
protected:
	read_write_channel() noexcept;
public:
	virtual ~read_write_channel() noexcept = 0;
	virtual std::size_t read(std::error_code& ec,uint8_t* const buff, std::size_t bytes) const noexcept override = 0;
	virtual std::size_t write(std::error_code& ec, const uint8_t* buff,std::size_t size) const noexcept override = 0;
};

DECLARE_IPTR(read_write_channel);

template <>
class unsafe<read_write_channel> {
public:
	explicit unsafe(s_read_write_channel&& ch) noexcept:
		ec_(),
		ch_( std::forward<s_read_write_channel>(ch) )
	{}
	explicit unsafe(const s_read_write_channel& ch) noexcept:
		ec_(),
		ch_(ch)
	{}
	std::size_t read(uint8_t* const buff,const std::size_t bytes) const
	{
		std::size_t ret = ch_->read( ec_, buff, bytes);
		io::check_error_code( ec_ );
		return ret;
	}
	std::size_t write(const uint8_t* buff,const std::size_t size) const
	{
		std::size_t ret = ch_->write( ec_, buff, size);
		io::check_error_code( ec_ );
		return ret;
	}
private:
	mutable std::error_code ec_;
	s_read_write_channel ch_;
};

/**
 * General interface to input, output and position moving operations on an resource
 * like a: file, socket, shared memory blocks etc.
 **/
class IO_PUBLIC_SYMBOL random_access_channel:public read_write_channel {
protected:
	random_access_channel() noexcept;
public:
	virtual ~random_access_channel() noexcept = 0;
	/// Moves current device position forward
	/// \param ec
	///		operation error code
	/// \param size
	///			moving offset
	virtual std::size_t forward(std::error_code& err,std::size_t size) noexcept = 0;
	/// Moves current device position backward
	/// \param ec
	///		operation error code
	/// \param size
	///			moving offset
	virtual std::size_t backward(std::error_code& err, std::size_t size) noexcept = 0;
	/// Moves current device position forward by offset from device begin
	/// \param ec
	///		operation error code
	/// \param size
	///			moving offset
	virtual std::size_t from_begin(std::error_code& err, std::size_t size) noexcept = 0;
	/// Moves current device backward from device end
	/// \param ec
	///		operation error code
	/// \param size
	///			moving offset
	virtual std::size_t from_end(std::error_code& err, std::size_t size) noexcept = 0;
	/// Gets current device position as an offset from the starting device position
	/// \param ec
	///		operation error code
	virtual std::size_t position(std::error_code& err) noexcept = 0;
};

DECLARE_IPTR(random_access_channel);

template <>
class unsafe<random_access_channel> {
public:
	explicit unsafe(s_random_access_channel&& ch) noexcept:
		ec_(),
		ch_( std::forward<s_random_access_channel>(ch) )
	{}
	explicit unsafe(const s_random_access_channel& ch) noexcept:
		ec_(),
		ch_( ch )
	{}
	std::size_t read(uint8_t* const buff, std::size_t bytes) const
	{
		std::size_t ret = ch_->read( ec_, buff, bytes);
		io::check_error_code( ec_ );
		return ret;
	}
	std::size_t write(const uint8_t* buff,std::size_t size) const
	{
		std::size_t ret = ch_->write( ec_, buff, size);
		io::check_error_code( ec_ );
		return ret;
	}
	std::size_t forward(std::size_t size)
	{
		std::size_t ret = ch_->forward(ec_, size);
		io::check_error_code( ec_ );
		return ret;
	}
	std::size_t backward(std::size_t size)
	{
		std::size_t ret = ch_->forward(ec_, size);
		io::check_error_code( ec_ );
		return ret;
	}
	std::size_t from_begin(std::size_t size)
	{
		std::size_t ret = ch_->from_begin(ec_, size);
		io::check_error_code( ec_ );
		return ret;
	}
	std::size_t from_end(std::size_t size)
	{
		std::size_t ret = ch_->from_end(ec_, size);
		io::check_error_code( ec_ );
		return ret;
	}
	std::size_t position()
	{
		std::size_t ret = ch_->position(ec_);
		io::check_error_code( ec_ );
		return ret;
	}
private:
	mutable std::error_code ec_;
	s_random_access_channel ch_;
};

// predefinition of io_context
class io_context;
DECLARE_IPTR(io_context);

typedef std::function<void(std::error_code&,uint8_t*,std::size_t)> asynch_read_completition_routine;
typedef std::function<void(std::error_code&,const uint8_t*,std::size_t)> asynch_write_completition_routine;

class IO_PUBLIC_SYMBOL asynch_channel:public channel {
	asynch_channel(const asynch_channel&) = delete;
	asynch_channel& operator=(const asynch_channel&) = delete;
protected:
	 asynch_channel(os_descriptor_t hnd, const asynch_read_completition_routine& rc, const asynch_write_completition_routine& wc) noexcept;
	 void on_read_finished(std::error_code& ec,uint8_t* bytes,std::size_t read) const;
	 void on_write_finished(std::error_code& ec, const uint8_t* last, std::size_t written) const;
	 friend class io_context;
private:
	os_descriptor_t hnd_;
	asynch_read_completition_routine read_callback_;
	asynch_write_completition_routine write_callback_;
public:
	virtual ~asynch_channel() noexcept = 0;
	/// Returns underlying operating system file handle or socket descriptor
	os_descriptor_t handle() const noexcept {
		return hnd_;
	}
	virtual void read(uint8_t* into, std::size_t limit, std::size_t start_from) const noexcept = 0;
	virtual void write(const uint8_t* what, std::size_t bytes, std::size_t start_from) const noexcept = 0;
	virtual bool cancel_pending() const noexcept = 0;
	virtual bool cancel_all() const noexcept = 0;
};

DECLARE_IPTR(asynch_channel);

/// Transmits a buffer data into a write channel
/// function will re-attempt to write unless
/// size bytes from buffer will be written to the destination
/// channel, or an io_error
/// \param ec operation error code, contains error when io error
/// \param buff memory buffer, must not be nullptr
/// \param size count of bytes to transmit,
///				must be greater then 0 and less or equals to memory buffer size
/// \param dst destination write channel
std::size_t IO_PUBLIC_SYMBOL transmit_buffer(
    std::error_code& ec,
    const s_write_channel& dst,
    const uint8_t* buffer,
    std::size_t size) noexcept;


/// Transmits all read channels data to destination write channel
/// \param ec operation error code, contains error
///				when io error or not enough memory for allocating buffer
/// \param src source read channel
/// \param dst destination write channel
/// \param buff memory buffer size, OS page size will be used if 0
/// \return count of bytes transfered
/// \throw never throws
std::size_t IO_PUBLIC_SYMBOL transmit(std::error_code& ec,const s_read_channel& src, const s_write_channel& dst, unsigned long buff_size) noexcept;


} // namespace io

#endif // __CHANNELS_HPP_INCLUDED__
