#include "vbo.hpp"
#include <new>


namespace gl {

//buffer
s_buffer buffer::create(const void* data, std::size_t size, buffer_type bt, data_type dt, stride st, buffer_usage u)
{
    ::GLuint id;
    ::glGenBuffers(1,&id);
	::glBindBuffer( static_cast<GLenum>(bt), id );
	::glBufferData( static_cast<GLenum>(bt), size, data, static_cast<GLenum>(u) );
	if(GL_OUT_OF_MEMORY == ::glGetError() )
		throw std::bad_array_new_length();
	::glBindBuffer(static_cast<GLenum>(bt), id );
	return s_buffer(new buffer(id,bt,dt, size, st,u) );
}


buffer::buffer(::GLuint id,buffer_type bt,data_type dt,std::size_t size, stride st, buffer_usage u) noexcept:
	io::object(),
	id_(id),
	type_(bt),
	data_type_(dt),
	size_(size),
	stride_(st),
	usage_(u),
	bound_(ATOMIC_FLAG_INIT)
{}


bool buffer::bind() noexcept
{
	if( !bound_.test_and_set() ) {
		::glBindBuffer( static_cast<GLenum>(type_), id_ );
	}
	return false;
}

void buffer::unbind() noexcept
{
	::glBindBuffer(static_cast<GLenum>(type_), id_ );
	bound_.clear();
}

buffer::~buffer() noexcept
{
	if( 0 != size_ )
		::glDeleteBuffers( 1, &id_ );
}

} // namespace gl
