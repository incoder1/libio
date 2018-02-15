#include "stdafx.hpp"
#include "msmlang_conv_engine.hpp"

#ifndef __IID_DEFINED__
#define __IID_DEFINED__
typedef struct _IID {
	unsigned long x;
	unsigned short s1;
	unsigned short s2;
	unsigned char  c[8];
} IID;
#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

#ifndef _MIDL_USE_GUIDDEF_
#	define MIDL_DEFINE_GUID(type,name,l,w1,w2,b1,b2,b3,b4,b5,b6,b7,b8) \
        const type name = {l,w1,w2,{b1,b2,b3,b4,b5,b6,b7,b8}}
#endif // !_MIDL_USE_GUIDDEF_

extern "C" {
	MIDL_DEFINE_GUID(IID, IID_IMLangConvertCharset,0xd66d6f98,0xcdaa,0x11d0,0xb8,0x22,0x00,0xc0,0x4f,0xc9,0xb3,0x1f);
	MIDL_DEFINE_GUID(CLSID, CLSID_CMLangConvertCharset,0xd66d6f99,0xcdaa,0x11d0,0xb8,0x22,0x00,0xc0,0x4f,0xc9,0xb3,0x1f);
}

namespace io {

namespace detail {

class mlang_factory {
private:

	static void do_release() noexcept {
		mlang_factory* tmp = _instance.load(std::memory_order_acquire);
		if(nullptr != tmp)
			delete tmp;
		_instance.store(nullptr, std::memory_order_release );
	}


	mlang_factory() noexcept:
		co_initialized_(false),
		lib_mlang_(nullptr) {
		co_initialized_ = (S_OK == ::CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE) );
		if(co_initialized_)
			lib_mlang_ = ::LoadLibraryW(L"mlang.dll");
	}

public:

	static const mlang_factory* instance() noexcept {
		mlang_factory* ret = _instance.load(std::memory_order_consume);
		if(nullptr == ret) {
			lock_guard lock(_init_mtx);
			ret = _instance.load(std::memory_order_consume);
			if(nullptr == ret) {
				std::atexit(&mlang_factory::do_release);
				ret = new (std::nothrow) mlang_factory();
			}
			_instance.store(ret, std::memory_order_release );
		}
		return ret;
	}

	::IMLangConvertCharset* create(::UINT from, ::UINT to, ::DWORD flags) const noexcept {
		::IMLangConvertCharset *ret = nullptr;
		::HRESULT hres = ::CoCreateInstance(
							CLSID_CMLangConvertCharset,
							NULL,
							CLSCTX_INPROC_SERVER,
							IID_IMLangConvertCharset,
							reinterpret_cast<void**>(&ret));
		if( SUCCEEDED( hres ) ) {
			hres = ret->Initialize(from, to, flags);
			if( SUCCEEDED(hres) )
				return ret;
			else
				ret->Release();
		}
		return nullptr;
	}


	~mlang_factory() noexcept {
		if(nullptr != lib_mlang_)
			::FreeLibrary(lib_mlang_);
		if(co_initialized_)
			::CoUninitialize();
	}

private:
	static critical_section _init_mtx;
	static std::atomic<mlang_factory*> _instance;
	bool co_initialized_;
	::HMODULE lib_mlang_;
};

critical_section mlang_factory::_init_mtx;
std::atomic<mlang_factory*> mlang_factory::_instance(nullptr);

engine::engine(::DWORD from, ::DWORD to, cnvrt_control flags) noexcept:
	ml_cc_(nullptr)
{
	ml_cc_ = mlang_factory::instance()->create(from, to, static_cast<::DWORD>(flags) );
}

converrc engine::convert(uint8_t** src,std::size_t& size, uint8_t** dst, std::size_t& avail) const noexcept
{
	::UINT read = static_cast<::UINT>( size );
	::UINT conv = static_cast<::UINT>( avail );
	::HRESULT ret = ml_cc_->DoConversion( *src, &read, *dst, &conv );
	if( S_OK != ret) {
  		switch(ret) {
		case E_FAIL:
			return no_buffer_space;
		case S_FALSE:
			return not_supported;
  		}
	}
	*src = *src + read;
	size -= read;
	*dst = *dst + conv;
	avail -= conv;
	return converrc::success;
}

engine::~engine() noexcept
{
	if(nullptr != ml_cc_)
		ml_cc_->Release();
}

} // namespace detail

} // namespace io
