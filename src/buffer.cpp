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
#include "buffer.hpp"

namespace io {

byte_buffer::byte_buffer(byte_buffer&& other) noexcept:
	arr_( std::forward<detail::mem_block>(other.arr_) ),
	capacity_(other.capacity_),
	position_(other.position_),
	last_(other.last_)
{
	other.capacity_ = 0;
	other.position_ = nullptr;
	other.last_ = nullptr;
}

std::size_t byte_buffer::put(const uint8_t* begin,const uint8_t* const end) noexcept
{
	if(end <= begin)
		return 0;
	std::size_t ret = memory_traits::distance(begin,end);
	if( ret > available())
		return 0;
    std::memmove( position_, begin, ret );
    position_ += ret;
    last_ = position_ + 1;
	return ret;
}

void byte_buffer::move(std::size_t offset) noexcept
{
	if( offset > available() ) {
		position_ = arr_.get() + capacity_-1;
		last_ = arr_.get() + capacity_;
		return;
	}
	position_ += offset;
	last_ = position_ + 1;
}

bool byte_buffer::extend(std::size_t extend_size) noexcept
{
	bool has_data = arr_ && nullptr != last_;
	capacity_ += extend_size;
	uint8_t *new_data = static_cast<uint8_t*>( memory_traits::realloc(arr_.get(), capacity_) );
	// out of memory
	if(nullptr == new_data)
		return false;

	if( has_data ) {
		position_ = new_data + memory_traits::distance( arr_.get(), position_ );
		last_ = new_data + memory_traits::distance(arr_.get(), last_);
		const std::size_t tail =  memory_traits::distance(last_, (new_data + capacity_) );
		if(tail > 0)
			io_zerro_mem(last_, tail );
	} else {
		position_ = new_data;
		last_ = position_;
		io_zerro_mem(position_, capacity_);
	}

	arr_ = detail::mem_block( new_data );
	return true;
}

bool byte_buffer::exp_grow() noexcept
{
	return extend ( (capacity_ << 1) );
}

bool byte_buffer::ln_grow() noexcept
{
	return extend ( (capacity_ >> 1) );
}

byte_buffer byte_buffer::allocate(std::error_code& ec, std::size_t capacity) noexcept
{
	detail::mem_block block( detail::mem_block::allocate(capacity) );
	if( !block ) {
		ec = std::make_error_code(std::errc::not_enough_memory);
		return byte_buffer();
	}
	return byte_buffer( std::move(block), capacity );
}


} // namespace io
