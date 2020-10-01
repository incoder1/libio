#include "stdafx.hpp"
#include "posix/io_context.hpp"

namespace io {

namespace net {

static constexpr int SOCKET_ERROR = -1;
static constexpr int INVALID_SOCKET = -1;

static int new_socket(int af, transport prot) noexcept
{
	int type = 0;
	switch(prot) {
	case transport::tcp:
		type = SOCK_STREAM;
		break;
	case transport::udp:
		type = SOCK_DGRAM;
		break;
	case transport::icmp:
	case transport::icmp6:
		type = SOCK_RAW;
		break;
	}
	int ret = ::socket(af,type,static_cast<int>(prot));
	if(INVALID_SOCKET != ret) {
		if(AF_INET6 == af) {
			int off = 0;
			::setsockopt(
			    ret,
			    IPPROTO_IPV6,
			    IPV6_V6ONLY,
			    reinterpret_cast<const char*>(&off),
			    sizeof(off)
			);
		}
	}
	return ret;
}

// io_context
s_io_context io_context::create(std::error_code& ec) noexcept
{
    io_context *ret = nobadalloc<io_context>::construct(ec);
    return ec ? s_io_context() : s_io_context(ret);
}

io_context::io_context() noexcept:
    io::object()
{}

io_context::~io_context() noexcept
{}

s_read_write_channel io_context::client_blocking_connect(std::error_code& ec, net::socket&& socket) const noexcept
{
    int s = new_scoket(ec, socket.get_endpoint().family(), socket.transport_protocol(), false );
    if(ec)
        return s_read_write_channel();
    const ::addrinfo *ai = static_cast<const ::addrinfo *>(socket.get_endpoint().native());
    if( SOCKET_ERROR == :connect(s, ai->ai_addr, ai->ai_addrlen) ) {
        ec = std::error_code( ::errc,  std::system_category() );
        return s_read_write_channel();
    }
    return s_read_write_channel( nobadalloc<net::synch_socket_channel>::construct(ec, s ) );
}

s_read_write_channel io_context::client_blocking_connect(std::error_code& ec, const char* host, uint16_t port) const noexcept
{
	const io::net::socket_factory *sf = io::net::socket_factory::instance(ec);
	if(!ec) {
		return client_blocking_connect(ec, sf->client_tcp_socket(ec,host,port) );
	}
	return s_read_write_channel();
}

} // namespace io

} // namespace net