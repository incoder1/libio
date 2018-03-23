#ifndef __IO_SECURE_CHANNEL_HPP_INCLUDED__
#define __IO_SECURE_CHANNEL_HPP_INCLUDED__

#include "config.hpp"

#ifdef HAS_PRAGMA_ONCE
#pragma once
#endif // HAS_PRAGMA_ONCE

#ifdef IO_TLS_PROVIDER_GNUTSL
#	include "tls/gnutls_secure_channel.hpp"
#endif // IO_TLS_PROVIDER_GNUTSL

#endif // __IO_SECURE_CHANNEL_HPP_INCLUDED__
