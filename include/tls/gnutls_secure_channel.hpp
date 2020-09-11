/*
 *
 * Copyright (c) 2016-2020
 * Viktor Gubin
 *
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 *
 */
/**
    Transport Layer Security channels implementations on top of GNU TLS library http://www.gnutls.org/

    WARN! According to the LGPL V2.1 you must link GNU TLS dynamically.
    Commerce usage in non open source software is allowed.
    See: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html

    Boost Software License can be used with LGPL

    See: https://www.gnu.org/licenses/license-list.en.html#GPLCompatibleLicenses

*/
#ifndef __IO_GNUTLS_SECURE_CHANNEL_HPP_INCLUDED__
#define __IO_GNUTLS_SECURE_CHANNEL_HPP_INCLUDED__

#include "config.hpp"

#ifdef HAS_PRAGMA_ONCE
#pragma once
#endif // HAS_PRAGMA_ONCE

#include <atomic>
#include <memory>

// GNU TLS reference
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>

#include "channels.hpp"
#include "threading.hpp"

namespace io {

namespace net {

namespace secure {

namespace detail {

class transport:public io::object
{
	transport(const transport&) = delete;
	transport& operator=(const transport&) = delete;
protected:
	constexpr transport() noexcept:
		object()
	{}
public:
	virtual ssize_t pull(std::error_code& ec, void* dst,std::size_t len) noexcept = 0;

	virtual ssize_t push(std::error_code& ec, const void* src,std::size_t len) noexcept = 0;

	virtual ~transport() noexcept = default;
};

DECLARE_IPTR(transport);

class synch_transport final: public transport {
public:
	synch_transport(io::s_read_write_channel&& raw) noexcept;
	virtual ssize_t pull(std::error_code& ec, void* dst,std::size_t len) noexcept override;
	virtual ssize_t push(std::error_code& ec, const void* src,std::size_t len) noexcept override;
private:
	io::s_read_write_channel raw_;
};

} // namesapce detail

class credentials
{
    credentials(const credentials& other) = delete;
    credentials& operator=(const credentials& rhs) = delete;
public:

    credentials(credentials&& other) noexcept:
        creds_( other.creds_ )
    {
        other.creds_ = nullptr;
    }

    credentials& operator=(credentials&& rhs) noexcept
    {
        credentials( static_cast<credentials&&>(rhs) ).swap( *this );
        return *this;
    }

    ::gnutls_certificate_credentials_t get() const noexcept
    {
        return creds_;
    }

    inline void swap(credentials& other) noexcept {
        std::swap(creds_, other.creds_ );
    }

    ~credentials() noexcept
    {
        if(nullptr != creds_)
            ::gnutls_certificate_free_credentials(creds_);
    }

    static credentials system_trust_creds(std::error_code& ec) noexcept;
private:
    constexpr credentials() noexcept:
        creds_(nullptr)
    {}
private:
    ::gnutls_certificate_credentials_t creds_;
};


class session;
DECLARE_IPTR(session);

class session final:public object {

    session(const session&) = delete;
    session& operator=(const session&) = delete;

public:

    static s_session client_session(std::error_code &ec, ::gnutls_certificate_credentials_t crd, s_read_write_channel&& raw) noexcept;

    ~session() noexcept;

    inline void swap(session& other) noexcept
    {
        std::swap(peer_, other.peer_);
    }

    std::size_t read(std::error_code& ec, uint8_t * const data, std::size_t max_size) const noexcept;

    std::size_t write(std::error_code& ec, const uint8_t *data, std::size_t data_size) const noexcept;

private:

	session(gnutls_session_t peer, detail::s_transport&& connection) noexcept;

	detail::s_transport connection() const noexcept {
		return connection_;
	}

    static int client_handshake(::gnutls_session_t peer) noexcept;

    friend int session_timeout(::gnutls_transport_ptr_t, unsigned int ms) noexcept;

    friend int get_session_error_no(::gnutls_transport_ptr_t) noexcept;

 	friend ssize_t session_push(::gnutls_transport_ptr_t tr, const ::giovec_t* buffers, int bcount) noexcept;

    friend ssize_t session_pull(::gnutls_transport_ptr_t tr, void * data, std::size_t max_size) noexcept;

private:
    ::gnutls_session_t peer_;
    detail::s_transport connection_;
    std::error_code ec_;
};

class IO_PUBLIC_SYMBOL tls_channel final: public read_write_channel
{
public:
	virtual ~tls_channel() noexcept override;
	virtual std::size_t read(std::error_code& ec,uint8_t* const buff, std::size_t bytes) const noexcept override;
	virtual std::size_t write(std::error_code& ec, const uint8_t* buff,std::size_t size) const noexcept override;
private:
    friend class nobadalloc<tls_channel>;
    tls_channel(s_session&& session) noexcept;
private:
    s_session session_;
};


class IO_PUBLIC_SYMBOL service {
	service(const service&) = delete;
	service& operator=(const service&) = delete;
public:
    static const service* instance(std::error_code& ec) noexcept;
    ~service() noexcept;

    s_read_write_channel new_client_blocking_connection(std::error_code& ec,s_read_write_channel&& raw) const noexcept;

    s_asynch_channel new_client_connection(std::error_code& ec, s_asynch_channel&& raw) const noexcept;

private:

    static void destroy_gnu_tls_atexit() noexcept;
    service(credentials&& creds) noexcept:
    	creds_( std::forward<credentials>(creds) )
	{}
private:
    static std::atomic<service*> _instance;
    static critical_section _mtx;
    credentials creds_;
};


} // namespace secure

} // namespace net

} // namespace io

#endif // __IO_GNUTLS_SECURE_CHANNEL_HPP_INCLUDED__
