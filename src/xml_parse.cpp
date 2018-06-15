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
static const std::size_t MID_BUFF_SIZE = 64;
static const std::size_t HUGE_BUFF_SIZE = 128;

// unicode constants in digit forms, to handle endians
static constexpr const int LEFTB =  60; // '<';
static constexpr const int RIGHTB =  62; // '>';
static constexpr const int SRIGHTB = 93; // ']'
static constexpr const int QNM = 34; // '"'
static constexpr const int QM = 63; // '?'
static constexpr const int EM = 33;//'!';
static constexpr const int SPACE = 32;//' ';
static constexpr const int SOLIDUS = 47;// '/'
static constexpr const int HYPHEN = 45;// '-'
static constexpr const int COLON = 58; // ':'
static constexpr const int ENDL = 0;
static constexpr const int iEOF = std::char_traits<char32_t>::eof();

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
	return start_with(s, PROLOGUE, 3) && is_whitespace( *(s+3) );
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
	char *s = const_cast<char*>(str);
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

#ifdef IO_IS_LITTLE_ENDIAN

static constexpr char32_t INVALID_CHAR = -1;

namespace u8_decode {

static constexpr const char32_t SH2    = 6;
static constexpr const char32_t SH3    = 12;
static constexpr const char32_t SH4    = 18;

static constexpr const uint8_t TAIL_MASK  = 0x3F;
static constexpr const uint8_t B2_MASK    = 0x1F;
static constexpr const uint8_t B3_MASK    = 0x0F;
static constexpr const uint8_t B4_MASK    = 0x07;

static constexpr char tail(const uint8_t tail) noexcept
{
	return tail & TAIL_MASK;
}

static constexpr char32_t decode2(const uint8_t* mb2) noexcept
{
	return ( (mb2[0] & B2_MASK) << SH2) + tail(mb2[1]);
}

static constexpr char32_t decode3(const uint8_t* mb3) noexcept
{
	return ( (mb3[0] & B3_MASK) << SH3)  +
		   ( tail(mb3[1]) << SH2) +
		   ( tail(mb3[2]) );
}

static constexpr char32_t decode4(const uint8_t* mb4) noexcept
{
	return ( (mb4[0] & B4_MASK) << SH4) +
		   ( tail(mb4[1]) << SH3) +
		   ( tail(mb4[2]) << SH2) +
		   tail(mb4[3]);
}

} // namespace u8_decode

static IO_NO_INLINE error check_xml_name(const char* tn) noexcept
{
	const char* c = tn;
	if( io_unlikely( ('\0' == *c) || is_digit(*c) ) )
		return error::illegal_name;
	do {
		switch( u8_char_size(*c) ) {
		case io_likely(1):
			if( io_unlikely( !is_xml_name_char(static_cast<char32_t>(*c) ) ) )
				return error::illegal_name;
			++c;
			break;
		case 2:
			if(!is_xml_name_char(u8_decode::decode2( reinterpret_cast<const uint8_t*>(c) ) ) )
				return error::illegal_name;
			c += 2;
			break;
		case 3:
			if(!is_xml_name_char(u8_decode::decode3( reinterpret_cast<const uint8_t*>(c) ) ) )
				return error::illegal_name;
			c += 3;
			break;
		case 4:
			if(!is_xml_name_char(u8_decode::decode4( reinterpret_cast<const uint8_t*>(c) ) ) )
				return error::illegal_name;
			c += 4;
			break;
		default:
			return error::illegal_name;
		}
	}
	while( !cheq('\0',*c) );
	return error::ok;
}

#else

static bool utf8_to_utf32(const char* src, std::size_t &slen, char32_t* dst, std::size_t &dlen) noexcept
std::error_code ec;
std::size_t ulen = transcode(ec,
							 reinterpret_cast<const uint8_t*>(src),len,
							 dst,dlen);
return !ec || 0 != ulen;
}

static error check_xml_name(const char* tn) noexcept
{
	if( is_digit(*tn) )
		return error::illegal_name;
	// need to convert into USC-4 in this point
	const std::size_t len = io_strlen(tn);
	const std::size_t u32len = const_string::utf8_length(tn);
	char32_t *ch32 = nullptr;
	if(0 == len)
		return error::illegal_name;
	else if(u32len < MAX_DEPTH) {
		// stack in this point
		ch32 = static_cast<char32_t*>( io_alloca( u32len * sizeof(char32_t) ) );
	}
	else {
		// a huge name, more then 255 bytes
		ch32 =   memory_traits::calloc_temporary<char32_t>(u32len);
		if(nullptr == ch32)
			return error::out_of_memory;
	}
	std::size_t i = len, j = u32len;
	if( !utf8_to_utf32(tn, i, ch32, j) ) {
		// huge name, release memory for UCS-4
		if(len > MAX_DEPTH)
			memory_traits::free_temporary( ch32 );
		return error::illegal_name;
	}
	// name start char
	if( !is_xml_name_start_char(*ch32) )
		return error::illegal_name;
	// rest of name
	for(std::size_t i = 1; i < u32len; i++) {
		if( !is_xml_name_char( ch32[i] ) )
			return error::illegal_name;
	}
	if(u32len > MAX_DEPTH)
		memory_traits::free_temporary( ch32 );
	return error::ok;
}

#endif // IO_IS_LITTLE_ENDIAN



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

static inline error validate_attribute_name(const char* name) noexcept
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
}

event_stream_parser::~event_stream_parser() noexcept
{}

void event_stream_parser::assign_error(error ec) noexcept
{
	state_.current = state_type::eod;
	if(error::ok == state_.ec)
		state_.ec = ec;
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

/*
char event_stream_parser::skip_to_symbol(const char symbol) noexcept
{
	char i = next();
	while( !cheq( symbol, i) && !is_eof(i) )
		i = next();
	return i;
}
*/

byte_buffer event_stream_parser::read_entity() noexcept
{
	std::error_code ec;
	byte_buffer ret = byte_buffer::allocate(ec, MID_BUFF_SIZE);
	if( io_unlikely( ec ) )
		return byte_buffer();
	ret.put( scan_buf_ );
	sb_clear( scan_buf_ );
	for(char c = next(); !is_error(); c = next() ) {
		putch(ret, c );
		switch( char8_traits::to_int_type(c) ) {
		case LEFTB:
		case io_unlikely(EOF):
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

#define check_event_parser_state( _EVENT_TYPE, _ERR_RET_TYPE )\
    if(state_type::event != state_.current || current_ != _EVENT_TYPE ) { \
        assign_error(error::invalid_state); \
        return _ERR_RET_TYPE(); \
    }

document_event event_stream_parser::parse_start_doc() noexcept
{
	static constexpr const char* VERSION  = "version=\"";
	static constexpr const char* ENCODING = "encoding=\"";
	static constexpr const char* STANDALONE = "standalone=\"";
	static const char YES[] = {'y','e','s'};
	static const char NO[] = {'n','o'};
	static const char END_PROLOGUE[] = {'?','>'};

	check_event_parser_state(event_type::start_document, document_event )

	byte_buffer buff( read_entity() );

	if( !check_buffer(buff) )
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

	i += 9; // i + strlen(VERSION)
	char *stop = tstrchr(i, QNM);
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
		i = const_cast<char*>( j + 10 );
		stop  = tstrchr( i, QNM );
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
		i = const_cast<char*> ( j + 12 );
		stop  = tstrchr( i, QNM);
		if(nullptr == stop ) {
			assign_error(error::illegal_prologue);
			return document_event();
		}
		if(str_size(i,stop) > 3) {
			assign_error(error::illegal_prologue);
			return document_event();
		}
		standalone =  ( 0 == io_memcmp( i, YES, 3) );
		if( !standalone ) {
			if( 0 != io_memcmp(i, NO, 2) ) {
				assign_error(error::illegal_prologue);
				return document_event();
			}
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
	if( !check_buffer(buff) )
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
	int i;
	std::size_t brackets = 1;
	do {
		i = next();
		switch(i) {
		case iEOF:
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
	if(state_type::dtd != state_.current) {
		assign_error(error::invalid_state);
		return const_string();
	}
	std::error_code ec;
	byte_buffer dtd = byte_buffer::allocate(ec, MID_BUFF_SIZE);
	if( ! check_buffer(dtd) )
		return const_string();
	dtd.put(scan_buf_);
	sb_clear(scan_buf_);
	std::size_t brackets = 1;
	int i;
	do {
		i = next();
		switch(i) {
		case iEOF:
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
	byte_buffer buff = byte_buffer::allocate(errc, HUGE_BUFF_SIZE);
	if( !check_buffer(buff) )
		return byte_buffer();
	const uint16_t pattern = (separator << 8) | separator;
	char c;
	uint16_t i = 0;
	do {
		c = next();
		if( is_eof(c) )
			break;
		else
			putch(buff, c);
		i = (i << 8) | uint16_t(c);
	}
	while( pattern != i && !is_error() );
	buff.flip();
	if( io_unlikely( !cheq(RIGHTB, next() ) ) ) {
		if(error::ok != state_.ec)
			assign_error(ec);
		return byte_buffer();
	}
	return std::move(buff);
}

const_string event_stream_parser::read_comment() noexcept
{
	if(state_type::comment != state_.current) {
		assign_error(error::invalid_state);
		return const_string();
	}
	byte_buffer tmp( read_until_double_separator(HYPHEN, error::illegal_commentary) );
	if( tmp.empty() || 0 == io_strcmp("--", tmp.position().cdata() ) )
		return  const_string();
	else
		return const_string( tmp.position().cdata(), tmp.last().cdata()-3 );
}

const_string event_stream_parser::read_chars() noexcept
{
	if(state_type::characters != state_.current) {
		assign_error(error::invalid_state);
		return const_string();
	}
	std::error_code ec;
	byte_buffer ret = byte_buffer::allocate(ec,HUGE_BUFF_SIZE);
	if( ec )
		return const_string();
	// just "\s<" in scan stack
	if( cheq(LEFTB,scan_buf_[1]) && cheq('\0',scan_buf_[3]) ) {
		char tmp[2] = { scan_buf_[0], '\0'};
		sb_clear(scan_buf_);
		scan_buf_[0] = '<';
		return const_string(tmp);
	}

	for(char c = next(); !cheq(LEFTB, c) && !is_error(); c = next() ) {
		if( io_unlikely( cheq(RIGHTB,c) || cheq(iEOF,c) ) ) {
			assign_error(error::root_element_is_unbalanced);
			break;
		}
		putch(ret, c);
	}

	if( is_error() )
		return const_string();

	sb_clear( scan_buf_ );
	scan_buf_[0] = '<';
	ret.flip();
	return ret.empty() ? const_string() :  const_string( ret.position().cdata(), ret.last().cdata() );
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
		case iEOF:
			sb_clear( scan_buf_ );
			assign_error(error::root_element_is_unbalanced);
			return;
		}
	}

}

const_string event_stream_parser::read_cdata() noexcept
{
	if(state_type::cdata != state_.current) {
		assign_error(error::invalid_state);
		return const_string();
	}
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
	i = const_cast<char*>( io::strstr2b( start, "=\"") );
	if(nullptr == i)
		return attribute();
	cached_string name = pool_->get( start, str_size(start, i) );
	// 2 symbols
	i += 2;
	start = i;
	i = tstrchrn(i, char8_traits::to_char_type(QNM), MAX_DEPTH );
	if(nullptr == i) {
		assign_error(error::illegal_name);
		return attribute();
	}
	byte_buffer val;
	if( str_size(start,i) > 0) {
		if( !val.extend( str_size(start,i+1) ) ) {
			assign_error(error::out_of_memory);
			return attribute();
		}
		// normalize
		for(char *ch = start; ch != i; ch++) {
			if( between('\t','\r',*ch) )
				putch(val, ' ');
			else
				putch(val,*ch);
			if(is_error())
				return attribute();
		}
		val.flip();
		++i;
	}
	len = str_size(from, i);
	return attribute( name, const_string(val.position().cdata(), val.last().cdata()) );
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
	char result = src_->next();
	if( src_->eof() )
		result = EOF;
	return result;
}

start_element_event event_stream_parser::parse_start_element() noexcept
{
	check_event_parser_state(event_type::start_element, start_element_event);

	byte_buffer buff = read_entity();
	if( !check_buffer(buff) )
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
			if( !validate_xml_name( attr.name(), true ) ) {
				assign_error( error::illegal_chars );
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
	if( !check_buffer(buff) )
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
		if( is_whitespace(second) )
			assign_error( error::illegal_markup );
		else {
			state_.current = state_type::event;
			current_ = event_type::start_element;
		}
	}
}

void event_stream_parser::scan() noexcept
{
	if( 0 == nesting_ ) {
		for(char c = next(); c != '<'; c = next()) {
			if( !is_whitespace(c) ) {
				assign_error(error::illegal_markup);
				return;
			}
		}
		sb_clear(scan_buf_);
		scan_buf_[0] = '<';
	}
	switch( char8_traits::to_int_type(*scan_buf_) ) {
	case LEFTB:
		s_entity();
		break;
	case iEOF:
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
