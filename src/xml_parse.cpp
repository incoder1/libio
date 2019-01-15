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
#include "xml_parse.hpp"
#include "strings.hpp"

namespace io {
namespace xml {

static constexpr const char* PROLOGUE = "xml";
static const char* COMMENT = "<!--";
static const char* CDATA    = "<![CDATA[";
static const char* DOCTYPE  = "<!DOCTYPE";

static const std::size_t SMALL_BUFF_SIZE = 32;
static const std::size_t MEDIUM_BUFF_SIZE = 64;
static const std::size_t HUGE_BUFF_SIZE = 128;

// unicode constants in digit forms, to handle endians
static constexpr const int ENDL = 0;
static constexpr const int LEFTB =  60; // '<';
static constexpr const int RIGHTB =  62; // '>';
static constexpr const int SRIGHTB = 93; // ']'
static constexpr const int QNM = 34; // '"'
static constexpr const int APH = 39; // '\''
static constexpr const int SPACE = 32;//' ';
static constexpr const int EM = 33;//'!';
static constexpr const int SOLIDUS = 47;// '/'
static constexpr const int HYPHEN = 45;// '-'
static constexpr const int COLON = 58; // ':'
static constexpr const int ES = 61 ; // '='
static constexpr const int QM = 63; // '?'


#ifdef UCHAR_MAX
static constexpr const std::size_t MAX_DEPTH = UCHAR_MAX;
#else

#ifdef __GNUG__
static constexpr const std::size_t MAX_DEPTH = __UINT8_MAX__;
#else
static constexpr const std::size_t MAX_DEPTH = 255;
#endif // __GNUG__

#endif // UCHAR_MAX

static inline bool is_prologue(const char *s) noexcept
{
	return start_with(s, PROLOGUE, 3) && is_whitespace( s[3] );
}

static inline bool is_comment(const char *s) noexcept
{
	return start_with(s, COMMENT, 4);
}

static inline bool is_cdata(const char* s) noexcept
{
	return start_with(s, CDATA, 9);
}

static inline bool is_doc_type(const char *s) noexcept
{
	return start_with(s, DOCTYPE, 9);
}

static std::size_t prefix_delimit(const char* src) noexcept
{
	static const char* DELIMS = "\t\n\v\f\r :/>";
	return io_strcspn(src, DELIMS);
}

static std::size_t extract_prefix(std::size_t &start, const char* str) noexcept
{
	const char *s = str;
	if( cheq(LEFTB,*s) ) {
		if( cheq(SOLIDUS,*(s+1) ) ) {
			start += 2;
			s += 2;
		}
		else {
			++start;
			++s;
		}
	}
	s += prefix_delimit(s);
	if( !cheq(COLON, *s) ) {
		start = 0;
		return 0;
	}
	return str_size( (str + start), s );
}

static std::size_t extract_local_name(std::size_t& start,const char* str) noexcept
{
	start = 0;
	char *s = const_cast<char*>(str);
	if( !is_one_of(*s, LEFTB,COLON,QM) )
		return 0;
	++start;
	++s;
	if(cheq(SOLIDUS,*s)) {
		++s;
		++start;
	}
	s +=  xmlname_strspn(s);
	if( cheq(ENDL, *s) ) {
		start = 0;
		return 0;
	}
	return memory_traits::distance(str,s-1) - (start-1);
}


// Works only for UCS-4
#ifdef __GNUG__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

static bool is_xml_name_char(uint32_t ch) noexcept
{
	switch( ch ) {
	case 0x5F:
	case 0x3A:
	case 0x2D:
	case 0x2E:
	case 0xB7:
	case 0x30 ... 0x39:
	case 0x41 ... 0x5A:
	case 0x61 ... 0x7A:
	case 0xC0 ... 0xD6:
	case 0xD8 ... 0xF6:
	case 0xF8 ... 0x2FF:
	case 0x370 ... 0x37D:
	case 0x37F ... 0x1FFF:
	case 0x200C ... 0x200D:
	case 0x203F ... 0x2040:
	case 0x2070 ... 0x218F:
	case 0x2C00 ... 0x2FEF:
	case 0x0300 ... 0x036F:
	case 0x3001 ... 0xD7FF:
	case 0xF900 ... 0xFDCF:
	case 0xFDF0 ... 0xFFFD:
	case 0x10000 ... 0xEFFFF:
		return true;
	default:
		return false;
	}
}


#pragma GCC diagnostic pop

#else

static bool is_xml_name_start_char_lo(char32_t ch) noexcept
{
	// _ | :
	return is_one_of(ch,0x5F,0x3A) || is_latin1(ch);
}

static bool is_xml_name_start_char(char32_t ch) noexcept
{
	return is_xml_name_start_char_lo(ch) ||
		   between(0xC0,0xD6, ch)    ||
		   between(0xD8,0xF6, ch)    ||
		   between(0xF8,0x2FF,ch)    ||
		   between(0x370,0x37D,ch)   ||
		   between(0x37F,0x1FFF,ch)  ||
		   between(0x200C,0x200D,ch) ||
		   between(0x2070,0x218F,ch) ||
		   between(0x2C00,0x2FEF,ch) ||
		   between(0x3001,0xD7FF,ch) ||
		   between(0xF900,0xFDCF,ch) ||
		   between(0xFDF0,0xFFFD,ch) ||
		   between(0x10000,0xEFFFF,ch);
}

static bool is_xml_name_char(uint32_t ch) noexcept
{
	return is_digit(ch) ||
		   // - | . | U+00B7
		   is_one_of(ch,0x2D,0x2E,0xB7) ||
		   is_xml_name_start_char(ch) ||
		   between(0x0300,0x036F,ch)  ||
		   between(0x203F,0x2040,ch);
}

#endif // __GNUG__



static error check_xml_name(const char* tn) noexcept
{
	const char* c = tn;
	if( io_unlikely( ('\0' == *c) || is_digit(*c) ) )
		return error::illegal_name;
	uint32_t utf32c;
	while( '\0' != *c ) {
		switch( utf8::char_size(*c) ) {
		case io_likely(1):
			utf32c = static_cast<uint32_t>( char8_traits::to_int_type(*c) );
			++c;
			break;
		case 2:
			utf32c = utf8::decode2( c );
			c += 2;
			break;
		case 3:
			utf32c = utf8::decode3( c );
			c += 3;
			break;
		case 4:
			utf32c = utf8::decode4( c );
			c += 4;
			break;
		default:
			io_unreachable
			return error::illegal_name;
		}
		if( !is_xml_name_char(utf32c) )
			return error::illegal_name;
	}
	return error::ok;
}

static error validate_tag_name(const char* name) noexcept
{
	// check XML,xMl,xml etc
	char first[3];
	for(std::size_t i=0; i < 3; i++)
		first[i] = latin1_to_lower(name[i]);
	if( start_with(first, PROLOGUE, 3) )
		return error::illegal_name;
	return check_xml_name(name);
}

static error validate_attribute_name(const char* name) noexcept
{
	return check_xml_name(name);
}

// event_stream_parser
s_event_stream_parser event_stream_parser::open(std::error_code& ec,s_source&& src) noexcept
{
	if(!src) {
		ec = std::make_error_code( std::errc::bad_address );
		return s_event_stream_parser();
	}
	s_string_pool pool = string_pool::create(ec);
	if(!pool)
		return s_event_stream_parser();
	return s_event_stream_parser( nobadalloc<event_stream_parser>::construct( ec, src, std::move(pool) ) );
}

s_event_stream_parser event_stream_parser::open(std::error_code& ec,s_read_channel&& src) noexcept
{
	s_source xmlsrc = source::create(ec, std::forward<s_read_channel>(src) );
	return !ec ? open(ec, std::move(xmlsrc) ) : s_event_stream_parser();
}

event_stream_parser::event_stream_parser(const s_source& src, s_string_pool&& pool) noexcept:
	object(),
	src_(src),
	state_(),
	current_(event_type::start_document),
	pool_(std::forward<s_string_pool>(pool)),
	validated_(),
	nesting_(0)
{
	validated_.reserve(64);
	for(char c = next(); c != '<'; c = next()) {
		if( io_unlikely(!is_whitespace(c)) ) {
			assign_error(error::illegal_markup);
			return;
		}
	}
	sb_clear(scan_buf_);
	scan_buf_[0] = '<';
}

event_stream_parser::~event_stream_parser() noexcept
{}

inline void event_stream_parser::assign_error(error ec) noexcept
{
	state_.current = state_type::eod;
	if(error::ok == state_.ec)
		state_.ec = ec;
}

__forceinline void event_stream_parser::putch(byte_buffer& buf, char ch) noexcept
{
	if( io_unlikely( !buf.put(ch) && ( !buf.ln_grow() || !buf.put(ch) ) ) )
			assign_error(error::out_of_memory);
}

// extract name and namespace prefix if any
qname event_stream_parser::extract_qname(const char* from, std::size_t& len) noexcept
{
	cached_string prefix;
	cached_string local_name;
	len = 0;
	std::size_t start = 0;
	std::size_t count = extract_prefix(start, from );
	if( count > 0 )
		prefix = pool_->get( from+start, count);
	len += start+count;
	const char* name = from+len;
	count = extract_local_name(start,name);
	if(count > 0)
		local_name = pool_->get(name+start, count);
	else {
		assign_error(error::illegal_name);
		return qname();
	}
	len += start+count;
	char* left = const_cast<char*>( from + len);
	if( cheq(SOLIDUS,*left) ) {
		++len;
		++left;
	}
	if(cheq(RIGHTB,*left))
		++len;
	return qname( std::move(prefix), std::move(local_name) );
}

state_type event_stream_parser::scan_next() noexcept
{
	if(state_type::eod != state_.current)
		scan();
	return state_.current;
}

byte_buffer event_stream_parser::read_entity() noexcept
{
	std::error_code ec;
	byte_buffer ret = byte_buffer::allocate(ec, MEDIUM_BUFF_SIZE);
	if(  io_unlikely(ec) ) {
		assign_error(error::out_of_memory);
		return byte_buffer();
	}
	ret.put( scan_buf_ );
	sb_clear( scan_buf_ );
	for(char c = next(); !is_error(); c = next() ) {
		putch(ret, c );
		switch( char8_traits::to_int_type(c) ) {
		case LEFTB:
		case EOF:
			assign_error(error::illegal_markup);
			break;
		case RIGHTB:
			ret.flip();
			return std::move(ret);
		}
	}
	// error state
	return byte_buffer();
}

#define check_state( _STATE_TYPE, _EMPTY_RET_TYPE)\
	if( state_.current != _STATE_TYPE ) {\
		assign_error(error::invalid_state);\
		return _EMPTY_RET_TYPE();\
	}

#define check_event_parser_state( _EVENT_TYPE, _EMPTY_RET_TYPE )\
    if(state_type::event != state_.current || current_ != _EVENT_TYPE ) { \
        assign_error(error::invalid_state); \
        return _EMPTY_RET_TYPE(); \
    }

document_event event_stream_parser::parse_start_doc() noexcept
{
	static constexpr const char* VERSION  = "version=";
	static constexpr const char* ENCODING = "encoding=";
	static constexpr const char* STANDALONE = "standalone=";
	static constexpr const char* YES = "yes";
	static constexpr const char* NO = "no";
	static constexpr const char* END_PROLOGUE = "?>";

	check_event_parser_state(event_type::start_document, document_event )

	byte_buffer buff( read_entity() );

	if( is_error() )
		return document_event();

	buff.shift(5);

	const_string version, encoding;
	bool standalone = false;

	const char* prologue = buff.position().cdata();
	// extract version
	char* i = io_strstr( const_cast<char*>(prologue), VERSION );

	if(nullptr == i) {
		assign_error(error::illegal_prologue);
		return document_event();
	}
	i += 8; // i + strlen(VERSION)
	int sep = char8_traits::to_int_type( *i );
	if( !is_one_of(sep,QNM,APH) ) {
		assign_error(error::illegal_prologue);
		return document_event();
	} else
		++i;
	char *stop = tstrchr(i, sep);
	if(nullptr == stop )  {
		assign_error(error::illegal_prologue);
		return document_event();
	}
	version = const_string( i, stop);

	if( version.empty() ) {
		assign_error(error::out_of_memory);
		return document_event();
	}

	// extract optional
	i = const_cast<char*>( stop + 1 );

	// extract encoding if exist
	const char* j = io_strstr(i, ENCODING);
	if(nullptr != j) {
		i = const_cast<char*>( j + 9 );
		sep = char8_traits::to_int_type( *i );
		if( !is_one_of(sep,QNM,APH) ) {
			assign_error(error::illegal_prologue);
			return document_event();
		} else
			++i;
		stop  = tstrchr( i, sep );
		if(nullptr == stop ) {
			assign_error(error::illegal_prologue);
			return document_event();
		}
		encoding = const_string(i,stop);
		if( encoding.empty() ) {
			assign_error(error::out_of_memory);
			return document_event();
		}
		i = const_cast<char*> ( stop + 1 );
	}
	// extract standalone if exist
	j = io_strstr(i, STANDALONE);
	if(nullptr != j) {
		// j + strlen(STANDALONE)
		i = const_cast<char*> ( j + 11 );
		sep = char8_traits::to_int_type( *i );
		if( !is_one_of(sep,QNM,APH) ) {
			assign_error(error::illegal_prologue);
			return document_event();
		} else
			++i;
		stop  = tstrchr( i, sep );
		if(nullptr == stop || (str_size(i,stop) > 3) ) {
			assign_error(error::illegal_prologue);
			return document_event();
		}
		standalone =  ( 0 == io_memcmp( i, YES, 3) );
		if( !standalone &&  ( 0 != io_memcmp(i, NO, 2) ) ) {
			assign_error(error::illegal_prologue);
			return document_event();
		}
		i = const_cast<char*> ( stop + 1 );
	}
	// check error in this point
	if( 0 != io_memcmp( find_first_symbol(i), END_PROLOGUE, 2) ) {
		assign_error(error::illegal_prologue);
		return document_event();
	}
	return document_event( std::move(version), std::move(encoding), standalone);
}

instruction_event event_stream_parser::parse_processing_instruction() noexcept
{
	check_event_parser_state(event_type::processing_instruction, instruction_event)
	byte_buffer buff = read_entity();
	if( is_error() )
		return instruction_event();
	buff.move(1);
	char *i = const_cast<char*>( buff.position().cdata() );
	std::size_t start = 0;
	std::size_t len = extract_local_name(start,i);
	i += start;
	const_string target(i,len);
	if( target.empty() ) {
		assign_error(error::out_of_memory);
		return instruction_event();
	}
	i += len;
	len = io_strlen(i);
	const_string data( i, len-2);
	if( target.empty() ) {
		assign_error(error::out_of_memory);
		return instruction_event();
	}
	return instruction_event( std::move(target), std::move(data) );
}

void event_stream_parser::skip_dtd() noexcept
{
	if(state_type::dtd != state_.current) {
		assign_error(error::invalid_state);
		return;
	}
	std::size_t brackets = 1;
	do {
		switch( char8_traits::to_int_type( next() ) ) {
		case EOF:
			assign_error(error::illegal_dtd);
			break;
		case LEFTB:
			++brackets;
			break;
		case RIGHTB:
			--brackets;
			break;
		default:
			break;
		}
	}
	while( brackets > 0);
}

const_string event_stream_parser::read_dtd() noexcept
{
	check_state(state_type::dtd, const_string)
	std::error_code ec;
	byte_buffer dtd = byte_buffer::allocate(ec, MEDIUM_BUFF_SIZE);
	if( ec ) {
		assign_error(error::out_of_memory);
		return const_string();
	}
	dtd.put(scan_buf_);
	sb_clear(scan_buf_);
	std::size_t brackets = 1;
	int i;
	do {
		i = next();
		switch( char8_traits::to_int_type(i) ) {
		case EOF:
			assign_error(error::illegal_dtd);
			return const_string();
		case LEFTB:
			++brackets;
			break;
		case RIGHTB:
			--brackets;
			break;
		default:
			break;
		}
		putch(dtd, static_cast<char>(i) );
	}
	while( brackets > 0 && !is_error() );
	dtd.flip();
	return const_string( dtd.position().cdata(), dtd.last().cdata() );
}

void event_stream_parser::skip_comment() noexcept
{
	if(state_type::comment != state_.current) {
		assign_error(error::invalid_state);
		return;
	}
	if( !sb_check(scan_buf_) ) {
		assign_error(error::illegal_commentary);
		return;
	}
	sb_clear(scan_buf_);
	static constexpr uint16_t _ptrn = (HYPHEN << 8) | HYPHEN;
	uint16_t i = 0;
	char c;
	do {
		c = next();
		if( is_eof(c) )
			break;
		i = (i << 8) | c;
	}
	while( !is_error() && _ptrn != i );
	c = next();
	if( !cheq(RIGHTB,c) )
		assign_error(error::illegal_commentary);
}

byte_buffer event_stream_parser::read_until_double_separator(int separator,error ec) noexcept
{
	if( !sb_check(scan_buf_) ) {
		assign_error(ec);
		return byte_buffer();
	}
	sb_clear(scan_buf_);
	std::error_code errc;
	byte_buffer ret = byte_buffer::allocate(errc, HUGE_BUFF_SIZE);
	if( errc ) {
		assign_error(error::out_of_memory);
		return byte_buffer();
	}
	const uint16_t pattern = (separator << 8) | separator;
	char c;
	uint16_t i = 0;
	do {
		c = next();
		if( io_unlikely( !ret.put(c) &&  (!ret.exp_grow() || !ret.put(c) ) ) ) {
			assign_error(error::out_of_memory);
			return byte_buffer();
		}
		i = (i << 8) | uint16_t(c);
	}
	while( (pattern != i || is_eof(i) ) && io_likely(!is_error()) );
	ret.flip();
	if( is_eof(c) || !cheq(RIGHTB, next() ) ) {
		if(error::ok != state_.ec)
			assign_error(ec);
		return byte_buffer();
	}
	return std::move(ret);
}

const_string event_stream_parser::read_comment() noexcept
{
    check_state(state_type::comment, const_string)
	byte_buffer tmp( read_until_double_separator(HYPHEN, error::illegal_commentary) );
	if( tmp.empty() || 0 == io_strcmp("--", tmp.position().cdata() ) )
		return  const_string();
	else
		return const_string( tmp.position().cdata(), tmp.last().cdata()-3 );
}

const_string event_stream_parser::read_chars() noexcept
{
	check_state(state_type::characters, const_string)
	std::error_code ec;
	byte_buffer ret = byte_buffer::allocate(ec,HUGE_BUFF_SIZE);
	if( ec )
		return const_string();
	// just "\s<" in scan stack
	if( 0 == io_memcmp( (scan_buf_+1),"<", 2)  ) {
		char tmp[2] = { scan_buf_[0], '\0'};
		sb_clear(scan_buf_);
		scan_buf_[0] = '<';
		return const_string(tmp);
	}
	char c;
	bool reading = true;
	do {
		c = next();
		switch( char8_traits::to_int_type(c) ) {
		case LEFTB:
			reading = false;
			break;
		case RIGHTB:
		case EOF:
			reading = false;
			assign_error(error::root_element_is_unbalanced);
			break;
		default:
			if( !ret.put(c) ) {
				if( !ret.exp_grow() ) {
					reading = false;
					assign_error(error::out_of_memory);
				}  else {
					ret.put(c);
				}
				break;
			}
		}
	}
	while( reading );

	if( is_error() )
		return const_string();

	sb_clear( scan_buf_ );
	scan_buf_[0] = '<';
	ret.flip();
	return !ret.empty() ?  const_string( ret.position().cdata(), ret.length() ) : const_string();
}

void event_stream_parser::skip_chars() noexcept
{
	if(state_type::characters != state_.current) {
		assign_error(error::invalid_state);
		return;
	}
	for(int i = char8_traits::to_int_type( next() ); !is_error() ; i = char8_traits::to_int_type( next() ) ) {
		switch(i) {
		case LEFTB:
			sb_clear( scan_buf_ );
			sb_append( scan_buf_, static_cast<char>(LEFTB) );
			return;
		case RIGHTB:
			sb_clear( scan_buf_ );
			assign_error(error::illegal_chars);
			return;
		case io_unlikely(EOF):
			sb_clear( scan_buf_ );
			assign_error(error::root_element_is_unbalanced);
			return;
		}
	}

}

const_string event_stream_parser::read_cdata() noexcept
{
	check_state(state_type::cdata, const_string)
	byte_buffer tmp( read_until_double_separator(SRIGHTB, error::illegal_cdata_section) );
	tmp.flip();
	return const_string( tmp.position().cdata(), tmp.last().cdata()-3 );
}

attribute event_stream_parser::extract_attribute(const char* from, std::size_t& len) noexcept
{
	len = 0;
	// skip lead spaces, don't copy them into name
	char *i = find_first_symbol(from);
	if( nullptr == i || is_one_of(*i, SOLIDUS,RIGHTB) )
		return attribute();

	char* start = i;
	i = tstrchr(start, ES);
	int val_sep = char8_traits::to_int_type( *(++i) );
	if( nullptr == i || !is_one_of( val_sep, QNM, APH) )
		return attribute();

	cached_string np;
	cached_string ln;
	char *tmp = tstrchrn( start, COLON, str_size(start,i) );
	if(nullptr != tmp) {
        np = pool_->get( start,  str_size(start, tmp) );
        start = tmp + 1;
	}
	ln = pool_->get(start, str_size(start, i-1) );

	// extract attribute value
	i += 1; // skip ( "|' )
	start = i;
	// find closing value separator
	i = io_strchr(i, val_sep );
	if(nullptr == i) {
		assign_error(error::illegal_attribute);
		return attribute();
	}
	const std::size_t val_size = str_size(start,i);
    const_string value;
	if( val_size > 0) {
		char* val = nullptr;
        if( io_likely(val_size <= HUGE_BUFF_SIZE) ) {
			val = static_cast<char*>( io_alloca(val_size+1) );
			val[val_size] = '\0';
		} else {
            val = memory_traits::calloc_temporary<char>( val_size+1 );
            if( io_unlikely(nullptr == val) ) {
				assign_error(error::out_of_memory);
				return attribute();
            }
		}
		// normalize attribute value
		char *v = val;
		for(char *ch = start; ch != i; ch++) {
			if( between('\t','\r',*ch) )
				*v = ' ';
			else
				*v = *ch;
			++v;
		}
		value = const_string(val, val_size);
		if( io_unlikely(val_size > HUGE_BUFF_SIZE) )
			memory_traits::free_temporary(val);
		++i;
	}
	len = str_size(from, i);

	return attribute( qname( std::move(np), std::move(ln) ), std::move(value) );
}

bool event_stream_parser::validate_xml_name(const cached_string& str, bool attr) noexcept
{
	if( validated_.end() == validated_.find( str.hash() ) ) {
		error err;
		if(attr)
			err = validate_attribute_name( str.data() );
		else
			err = validate_tag_name( str.data() );
		if(error::ok != err ) {
			assign_error( err );
			return false;
		}
		validated_.insert( str.hash() );
		return true;
	}
	return true;
}

inline char event_stream_parser::next() noexcept
{
	return src_->next();
}

start_element_event event_stream_parser::parse_start_element() noexcept
{
	check_event_parser_state(event_type::start_element, start_element_event);

	byte_buffer buff = read_entity();
	if( is_error() )
		return start_element_event();
	bool empty_element = cheq(SOLIDUS, *(buff.last().cdata()-3));
	if( !empty_element )
		++nesting_;
	std::size_t len = 0;
	qname name = extract_qname( buff.position().cdata(), len );
	if(is_error())
		return start_element_event();
	// check name validity
	if( name.has_prefix() && !validate_xml_name( name.prefix(), false ) )
		return start_element_event();
	if( !validate_xml_name( name.local_name(), false ) )
		return start_element_event();
	start_element_event result( std::move(name), empty_element );
	if( is_error() )
		return result;
	char *left = const_cast<char*>( buff.position().cdata() + len );
	if( is_whitespace(*left) ) {
		std::size_t offset = 0;
		attribute attr( extract_attribute(left,offset) );
		while(offset != 0) {
			qname attr_name = attr.name();
			if( ( attr_name.has_prefix() && !validate_xml_name(attr_name.prefix(), true) )
				||
				  !validate_xml_name( attr.name().local_name(), true )
			  ) {
				assign_error( error::illegal_attribute );
				return start_element_event();
			}
			if( ! result.add_attribute( std::move(attr) ) ) {
				assign_error( error::illegal_attribute );
				return start_element_event();
			}
			left += offset;
			attr = extract_attribute(left,offset);
		}
	}
	return  result;
}

end_element_event event_stream_parser::parse_end_element() noexcept
{
	check_event_parser_state(event_type::end_element, end_element_event )

	if(nesting_ == 0) {
		assign_error(error::root_element_is_unbalanced);
		return end_element_event();
	}
	--nesting_;
	if(0 == nesting_)
		state_.current =  state_type::eod;
	byte_buffer buff = read_entity();
	if( is_error() )
		return end_element_event();
	std::size_t len = 0;
	qname name = extract_qname( buff.position().cdata(), len );
	if( is_error() )
		return end_element_event();
	return end_element_event( std::move(name) );
}

void event_stream_parser::s_instruction_or_prologue() noexcept
{
	if(0 != nesting_) {
		assign_error(error::illegal_markup);
		return;
	}
	for(uint8_t i=2; i < 7; i++)
		scan_buf_[i] = next();
	if( !sb_check(scan_buf_) ) {
		assign_error(error::illegal_markup);
		return;
	}
	else if( is_prologue(scan_buf_+2) ) {
		if(state_type::initial != state_.current) {
			assign_error(error::illegal_prologue);
			return;
		}
		current_ = event_type::start_document;
		state_.current = state_type::event;
	}
	else {
		current_ = event_type::processing_instruction;
		state_.current = state_type::event;
	}
}

void event_stream_parser::s_comment_cdata_or_dtd() noexcept
{
	scan_buf_[2] = next();
	scan_buf_[3] = next();
	if( !sb_check(scan_buf_)  ) {
		assign_error(error::root_element_is_unbalanced);
		return;
	}
	if( is_comment(scan_buf_) ) {
		state_.current =  state_type::comment;
		return;
	}
	for(uint8_t i=4; i < 9; i++)
		scan_buf_[i] = next();

	if( !sb_check(scan_buf_) ) {
		assign_error(error::root_element_is_unbalanced);
		return;
	}

	if( is_cdata(scan_buf_) )
		state_.current = state_type::cdata;
	else if( is_doc_type(scan_buf_) )
		state_.current = state_type::dtd;
	else
		assign_error(error::illegal_markup);
}


void event_stream_parser::s_entity() noexcept
{
	scan_buf_[1] = next();
	int second = char8_traits::to_int_type( scan_buf_[1] );
	switch( second ) {
	case QM:
		s_instruction_or_prologue();
		break;
	case EM:
		s_comment_cdata_or_dtd();
		break;
	case SOLIDUS:
		state_.current = state_type::event;
		current_ = event_type::end_element;
		break;
	default:
		if( io_unlikely( is_whitespace(second) ) )
			assign_error( error::illegal_markup );
		else {
			state_.current = state_type::event;
			current_ = event_type::start_element;
		}
	}
}

void event_stream_parser::scan() noexcept
{
	switch( char8_traits::to_int_type(*scan_buf_) ) {
	case LEFTB:
		s_entity();
		break;
	case EOF:
		state_.current = state_type::eod;
		if( 0 != nesting_)
			assign_error(error::root_element_is_unbalanced);
		break;
	default:
		state_.current = state_type::characters;
	}
}

} // namesapce xml

} // namesapce io
