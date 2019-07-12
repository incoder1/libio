#include "stdafx.hpp"
#include "parser.hpp"


#include <cstring>
#include <sstream>

namespace collada {


// xml parse helper free functions
static bool is_start_element(io::xml::event_type et) noexcept
{
	return io::xml::event_type::start_element == et;
}


static float next_float(const char* str,char** endp)
{
	if( io_likely( '\0' != *str) ) {
		return std::strtof(str, endp);
	}
	*endp = nullptr;
	return 0.0F;
}


static unsigned int next_uint(const char* str,char** endp)
{
	if( io_likely( '\0' != *str) )
		return std::strtoul(str, endp, 10);
	*endp = nullptr;
	return 0;
}

static std::size_t parse_sizet(const io::const_string& str)
{
	char *s = const_cast<char*>( str.data() );
#ifdef IO_CPU_BITS_64
	return std::strtoull(s, &s, 10);
#else
	return std::strtoul(s, &s, 10);
#endif
}


//static bool parse_bool(const io::const_string& val) noexcept
//{
//	typedef io::xml::lexical_cast_traits<bool> bool_cast;
//	const char* str =  val.data();
//	while( std::isspace(*str) )
//		++str;
//	switch(*str) {
//	case '\0':
//	case '0':
//		return false;
//	case '1':
//		return true;
//	default:
//		return bool_cast::from_string(str, &str);
//	}
//}

static std::size_t parse_string_array(const io::const_string& val,const std::size_t size, float* const data)
{
	std::size_t ret = 0;
	if( ! val.blank() ) {
		// parse space separated list of floats
		char *s = const_cast<char*>( val.data() );
		do {
			data[ret] = next_float(s, &s);
			++ret;
		}
		while( nullptr != s && '\0' != *s && ret < size );
	}
	return ret;
}

static unsigned_int_array parse_string_array(const io::const_string& val)
{
	//std::vector<unsigned int> tmp;
	std::size_t size = 0;
	char* s = const_cast<char*>( val.data() );
	std::size_t len = 0;
	// count numbers
	while('\0' != *s) {
		len = std::strspn(s,"\t\n\v\f\r ");
		s += len;
		len = std::strspn( s, "1234567890");
		if(0 != len) {
			++size;
			s += len;
		}
	}
	if( io_likely(0 != size) ) {
		unsigned_int_array ret( size );
		s = const_cast<char*>( val.data() );
		for(std::size_t i=0; i < size; i++) {
			ret[i] = next_uint(s, &s);
		}
		return ret;
	}
	return unsigned_int_array();
}

static float parse_float(const io::const_string& val)
{
	char* s = const_cast<char*>( val.data() );
	return next_float(s, &s);
}

static void parse_vec4(const io::const_string& val, float * ret)
{
	char* s = const_cast<char*>( val.data() );
	for( std::size_t i = 0; i < 4; i++) {
		ret[i] = next_float(s, &s);
	}
}

static io::const_string get_attr(const io::xml::start_element_event& sev, const char* name)
{
	auto  attr = sev.get_attribute("",name);
	if( !attr.second  ) {
		// TODO: add error row/col
		std::string msg = sev.name().local_name().stdstr();
		msg.push_back(' ');
		msg.append( name );
		msg.append(" attribute is mandatory");
		throw std::runtime_error( msg );
	}
	return sev.get_attribute("",name).first;
}

static io::xml::s_event_stream_parser open_parser(io::s_read_channel&& src)
{
	std::error_code ec;
	io::xml::s_event_stream_parser ret = io::xml::event_stream_parser::open(ec, std::move(src) );
	io::check_error_code( ec );
	return ret;
}

// parser

bool parser::is_element(const io::xml::start_element_event& e,const io::cached_string& ln) noexcept
{
	return e.name().local_name() == ln;
}

bool parser::is_element(const io::xml::end_element_event& e,const io::cached_string& ln) noexcept
{
	return e.name().local_name() == ln;
}

parser::parser(io::s_read_channel&& src) noexcept:
	xp_( open_parser( std::forward<io::s_read_channel>(src) ) )
{
	library_materials_ = xp_->precache("library_materials");
	library_effects_ = xp_->precache("library_effects");
	library_geometries_ = xp_->precache("library_geometries");
	library_visual_scenes_ = xp_->precache("library_visual_scenes");
	library_images_ = xp_->precache("library_images");
}

// XML parsing functions

io::xml::state_type parser::to_next_state() noexcept
{
	io::xml::state_type ret = io::xml::state_type::initial;
	// Scan for next XML state
	for(;;) {
		ret = xp_->scan_next();
		switch(ret) {
		case io::xml::state_type::dtd:
			xp_->skip_dtd();
			continue;
		case io::xml::state_type::comment:
			xp_->skip_comment();
			continue;
		default:
			break;
		}
		break;
	}
	return ret;
}

inline void parser::check_eod( io::xml::state_type state,const char* msg)
{
	if(io::xml::state_type::eod == state) {
		if( xp_->is_error() )
			throw_parse_error();
		else
			throw std::logic_error( msg );
	}
}


bool parser::is_end_element(io::xml::event_type et,const io::cached_string& local_name)
{
	using io::xml::event_type;
	return (event_type::end_element == et)
		   ? is_element( xp_->parse_end_element(), local_name)
		   : false;
}

inline bool parser::is_end_element(io::xml::event_type et,const io::xml::qname& tagname)
{
	return is_end_element(et, tagname.local_name() );
}


io::xml::event_type parser::to_next_event(io::xml::state_type& state)
{
	io::xml::event_type ret = io::xml::event_type::start_document;
	bool parsing = true;
	do {
		state = to_next_state();
		switch(state) {
		case io::xml::state_type::eod:
		case io::xml::state_type::event:
			parsing = false;
			break;
		case io::xml::state_type::cdata:
			xp_->read_cdata();
			break;
		case io::xml::state_type::characters:
			xp_->skip_chars();
			break;
		case io::xml::state_type::comment:
			xp_->skip_comment();
			break;
		case io::xml::state_type::dtd:
			xp_->skip_dtd();
			break;
		case io::xml::state_type::initial:
			break;
		}
	}
	while( parsing );
	return ret;
}

io::xml::event_type parser::to_next_tag_event(io::xml::state_type& state)
{
	io::xml::event_type ret = io::xml::event_type::start_document;
	bool parsing = true;
	do {
		ret = to_next_event(state);
		if(io::xml::state_type::eod == state)
			break;
		// got event
		ret = xp_->current_event();
		// skip start doc and dtd
		switch(ret) {
		case io::xml::event_type::start_document:
			xp_->parse_start_doc();
			break;
		case io::xml::event_type::processing_instruction:
			xp_->parse_processing_instruction();
			break;
		default:
			parsing = false;
		}
	}
	while( parsing );
	return ret;
}

void parser::skip_element(const io::xml::start_element_event& e)
{
	if( e.empty_element() )
		return;

	io::xml::qname name = e.name();
	std::string errmsg = name.local_name().stdstr();
	errmsg.append(" is unbalanced");

	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	io::xml::end_element_event eev;
	std::size_t nestin_level = 1;
	do {
		et = to_next_tag_event(state);
		check_eod(state, errmsg.data() );
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			// handle a situation when a tag embed
			// a nesting tag with the same name
			// more likely never happens
			if( io_unlikely( sev.name() == name ) )
				++nestin_level;
		}
		else {
			eev = xp_->parse_end_element();
			if( eev.name() == name)
				--nestin_level;
		}
	}
	while( 0 != nestin_level );
}

io::xml::start_element_event parser::to_next_tag_start(io::xml::state_type& state)
{
	io::xml::event_type et;
	do {
		et = to_next_tag_event(state);
		if(io::xml::state_type::eod == state)
			return io::xml::start_element_event();
		if( io::xml::event_type::end_element == et)
			xp_->parse_end_element();
	}
	while(io::xml::event_type::start_element != et );
	return xp_->parse_start_element();
}

io::const_string parser::get_tag_value()
{
	switch( xp_->scan_next() ) {
	case io::xml::state_type::characters:
		return xp_->read_chars();
	case io::xml::state_type::cdata:
		return xp_->read_cdata();
	default:
		throw std::logic_error("Characters expected");
	}
}

// effect library

void parser::parse_effect(io::const_string&& id, s_effect_library& efl)
{
	static constexpr const char* ERR_MSG = "effect is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	effect ef;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);

		if( is_start_element(et ) )  {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev,"profile_COMMON") ) {
				continue;
			}
			else if( is_element(sev,"technique") ) {
				continue;
			} else if( is_element(sev,"newparam") ) {
				parse_new_param( get_attr(sev,"sid"), efl );
			}
			else if( is_element(sev,"constant") ) {
				ef.shade = shade_type::constant;
			}
			else if( is_element(sev,"blinn") ) {
				ef.shade = shade_type::blinn_phong;
			}
			else if( is_element(sev,"phong") ) {
				ef.shade = shade_type::phong;
			}
			else if( is_element(sev,"lambert") ) {
				ef.shade = shade_type::lambert;
			}
			else if( is_element(sev,"ambient") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				parse_vec4( get_tag_value(), ef.value.pong.adse.vec.ambient );
			}
			else if( is_element(sev,"diffuse") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				// this is diffuse texture and not a material
				if( is_element(sev, "texture") ) {
					ef.shade = shade_type::diffuse_texture;
					io::const_string name = get_attr(sev,"texture");
					io::const_string texcoord;
					auto attr = sev.get_attribute("","texcoord");
					if( attr.second )
						texcoord = attr.first;
					ef.diffuse_tex = s_texture(
									new texture(std::move(name), std::move(texcoord))
								);
				}
				else {
					// this is material values
					parse_vec4( get_tag_value(), ef.value.pong.adse.vec.diffuse );
				}
			}
			else if( is_element(sev,"emission") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				parse_vec4( get_tag_value(), ef.value.pong.adse.vec.emission );
			}
			else if( is_element(sev,"specular") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				parse_vec4( get_tag_value(), ef.value.pong.adse.vec.specular );
			}
			else if( is_element(sev,"shininess") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				ef.value.pong.shininess = parse_float( get_tag_value() );
			}
			else if( is_element(sev,"index_of_refraction") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				ef.value.pong.refraction_index = parse_float( get_tag_value() );
			}
			else if( is_element(sev,"reflective") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				parse_vec4( get_tag_value(), ef.value.reflect.color );
			}
			else if( is_element(sev,"reflectivity") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				ef.value.reflect.value = parse_float( get_tag_value() );
			}
			else if( is_element(sev,"transparent") ) {
				ef.value.transparent.used = true;
				io::const_string opaque = get_attr(sev,"opaque");
				ef.value.transparent.rbg = opaque.equal("RGB_ZERO") || opaque.equal("RGB_ONE");
				ef.value.transparent.invert = opaque.equal("RGB_ZERO") || opaque.equal("A_ZERO");
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				parse_vec4( get_tag_value(), ef.value.transparent.color);
			}
			else if( is_element(sev,"bump") ) {
				sev = to_next_tag_start(state);
				check_eod(state,ERR_MSG);
				// this is bump (normal) mapping effect
				if( is_element(sev, "texture") ) {
					ef.shade = shade_type::bump_mapping;
					io::const_string name = get_attr(sev,"texture");
					io::const_string texcoord;
					auto attr = sev.get_attribute("","texcoord");
					if( attr.second )
						texcoord = attr.first;
					ef.bumpmap_tex = s_texture(
									new texture(std::move(name), std::move(texcoord))
								);
				}
			}
		}
	}
	while( !is_end_element(et,"effect") );
	efl->add_effect( std::forward<io::const_string>(id), std::move(ef) );
}


static surface_type sampler_by_name(const io::const_string& type) noexcept {
	const char* val = type.data();
	if( val[1] == 'D' && ('0' < val[0]) && ('4' > val[0] ) )
		return static_cast<surface_type>( uint8_t(val[0] - '0') );
	switch( val[0] ) {
	case 'C':
        if( type.equal("CUBE") )
        	return surface_type::cube;
		else
			break;
	case 'D':
        if( type.equal("DEPTH") )
        	return surface_type::depth;
		else
			break;
	case 'R':
		if( type.equal("RECT") )
			return surface_type::rect;
		else
			break;
	}
	return surface_type::untyped;
}

void parser::parse_new_param(io::const_string&& sid,s_effect_library& efl)
{
	static constexpr const char* ERR_MSG = "newparam is unbalanced";

	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev,"surface") ) {
				surface sf;
				sf.type = sampler_by_name( get_attr(sev,"type") );
				et = to_next_tag_event(state);
				check_eod(state, ERR_MSG);
				sev = xp_->parse_start_element();
				check_eod(state, ERR_MSG);
				if( is_element(sev,"init_from") )
					sf.init_from = get_tag_value();
				efl->add_surface( std::forward<io::const_string>(sid), std::move(sf) );
			}
			else if( is_sampler_ref(sev) ) {
				et = to_next_tag_event(state);
				check_eod(state, ERR_MSG);
				sev = xp_->parse_start_element();
				check_eod(state, ERR_MSG);
				if( is_element(sev,"source") ) {
					efl->add_sampler_ref(
						std::forward<io::const_string>(sid),
						get_tag_value() );
				}
			}
		}
	}
	while( !is_end_element(et,"newparam") );
}

void parser::parse_effect_library(s_model& md)
{
	static constexpr const char* ERR_MSG = "library_effects is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev,"effect") ) {
				s_effect_library elf = md->effects();
				parse_effect(  get_attr(sev,"id"), elf  );
			}
		}
	}
	while( !is_end_element(et,library_effects_) );
}

// Materials library

void parser::parse_library_materials(s_model& md)
{
	static constexpr const char* ERR_MSG = "library_materials is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	io::const_string material_id;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev,"material") ) {
				material_id = get_attr(sev,"id");
			}
			else if( is_element(sev,"instance_effect") ) {
				io::const_string urlref = get_attr(sev,"url");
				io::const_string url = io::const_string(urlref.data()+1, urlref.size()-1);
				md->add_material_effect_link( std::move(material_id), std::move(url) );
			}
		}
	}
	while( !is_end_element(et,library_materials_) );
}

// Geometry

static input parse_input(const io::xml::start_element_event& e)
{
	input ret;
	io::const_string sematic = get_attr(e,"semantic");
	constexpr const char* NAMES[7] = {
		"VERTEX",
		"POSITION",
		"NORMAL",
		"TEXCOORD",
		"COLOR",
		"TANGENT",
		"BITANGENT"
    };
    for(uint8_t i=0; i < 7; i++) {
		if( sematic.equal( NAMES[i] ) ) {
			ret.type = static_cast<semantic_type>(i);
			break;
		}
    }
	// remove # at the begin
	io::const_string src = get_attr(e,"source");
	ret.accessor_id = io::const_string( src.data()+1, src.size()-1 );

	// optional attributes
	ret.offset = parse_sizet( get_attr(e,"offset") );
	auto attr = e.get_attribute("","set");
	if(attr.second)
		ret.set = parse_sizet( attr.first );

	return ret;
}

void parser::parse_accessor(s_accessor& acsr)
{
	static constexpr const char* ERR_MSG = "accessor is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev,"param") ) {
				parameter p;
				auto attr = sev.get_attribute("","name");
				if(attr.second)
					p.name = attr.first;
				attr = sev.get_attribute("","type");
				if(!attr.second)
					throw std::runtime_error("type attribute expected");
				io::const_string type = attr.first;
				p.presision = type.equal("float") ? presision::float32_t : presision::double64_t;
				acsr->add_parameter( std::move(p) );
			}
		}
	}
	while( !is_end_element(et,"accessor") );
}


void parser::parse_source(const s_source& src)
{

	static constexpr const char* ERR_MSG = "source is unbalanced";

	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev, "technique_common") ) {
				continue;
			}
			else if( is_element(sev,"accessor") ) {
				auto attr = sev.get_attribute("","source");
				if(!attr.second)
					throw std::runtime_error("accessor source attribute is mandatory");
				io::const_string id = io::const_string( attr.first.data()+1, attr.first.size() - 1 );
				attr =  sev.get_attribute("","count");
				std::size_t count = attr.second ? parse_sizet( attr.first ) : 0;
				attr = sev.get_attribute("","stride");
				std::size_t stride = attr.second ? parse_sizet( attr.first ) : 0;
				s_accessor acsr( new accessor( std::move(id), count, stride ) );
				parse_accessor(acsr);
				src->add_accessor( std::move(acsr) );
			}
			else if( is_element(sev, "float_array") ) {
				std::size_t data_size = 0;
				auto attr = sev.get_attribute("","id");
				if(!attr.second)
					throw std::runtime_error("float_array id attribute is mandatory");
				io::const_string id = attr.first;
				attr = sev.get_attribute("","count");
				if(!attr.second)
					throw std::runtime_error("float_array count attribute is mandatory");
				data_size = parse_sizet( attr.first );

				io::const_string data_str = get_tag_value();
				float_array data(data_size);
				std::size_t actual_size = parse_string_array( data_str, data_size, const_cast<float*>( data.get() ) );
				if( io_likely(actual_size == data_size ) ) {
					src->add_float_array( std::move(id), std::move(data) );
				}
				else {
					std::string msg("float_array ");
					msg.append( std::to_string(data_size) );
					msg.append(" elements expected by got ");
					msg.append( std::to_string(actual_size) );
					msg.append(" mind be not a number value in the string");
					throw std::runtime_error( msg );
				}
			}
		}
	}
	while( !is_end_element(et, "source") );
}

//void parser::parse_vertex_data(sub_mesh::input_library_t& layout)
//{
//	io::xml::state_type state;
//	io::xml::event_type et;
//	io::xml::start_element_event sev;
//	do {
//		et = to_next_tag_event(state);
//		check_eod(state, "vertices is unbalanced");
//		if( is_start_element(et) ) {
//			sev = xp_->parse_start_element();
//			if( is_element(sev,"input") )
//				layout.emplace_back( parse_input(sev) );
//		}
//	}
//	while( !is_end_element(et,"vertices") );
//}


static constexpr const char* PRIMITIVES[7] = {
        "lines",
        "linestrips",
        "polygons",
        "polylist",
        "triangles",
        "trifans",
		"tristrips"
};

bool parser::is_sub_mesh(const io::xml::start_element_event& sev) noexcept
{
	for(uint8_t i=0; i < 7; i++) {
		if( is_element(sev,PRIMITIVES[i]) )
			return true;
	}
	return false;
}

bool parser::is_sampler_ref(const io::xml::start_element_event& sev) noexcept
{
	const char * ln = sev.name().local_name().data();
	// ignore 1D, 2D, 3D, CUBE, DEPTH, RECT
	// should be taken from argument
	return 0 == io_memcmp(ln, "sampler", 6);
}

s_sub_mesh parser::parse_sub_mesh(const io::const_string& type, io::const_string&& mat, std::size_t count)
{
	primitive_type pt = primitive_type::triangles;
	for(uint8_t i=0; i < 7; i++) {
		if( type.equal( PRIMITIVES[i] ) ) {
			pt = static_cast<primitive_type>(i);
			break;
		}
	}
	std::string err_msg( type.data() );
	err_msg.append(" is unbalanced");
	const char* ERR_MSG = err_msg.data();

	sub_mesh::input_library_t layout;
	unsigned_int_array idx;

	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev,"input") ) {
				layout.emplace_back( parse_input(sev) );
			}
			else if( is_element(sev,"p") ) {
				idx = parse_string_array( get_tag_value() );
			}
		}
	} while( !is_end_element(et, type) );

	layout.shrink_to_fit();

	return s_sub_mesh(
						new sub_mesh(
							pt,
							std::forward<io::const_string>(mat),
							count,
							std::move(layout),
							std::move(idx)
							)
					);
}

void parser::parse_mesh(const s_mesh& m)
{
	static constexpr const char* ERR_MSG = "mesh is unbalanced";

	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;

	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev, "source") ) {
				io::const_string id = get_attr(sev,"id");
				s_source src( new source() );
				parse_source( src );
				m->add_source( std::move(id), std::move(src) );
			}
			else if( is_element(sev, "vertices") ) {
				//m->set_vertex_id( get_attr(sev,"id") );
				et = to_next_tag_event(state);
				check_eod(state, ERR_MSG);
				sev = xp_->parse_start_element();
				check_eod(state, ERR_MSG);
				if( is_element(sev, "input") ) {
					io::const_string ref = get_attr(sev,"source");
					m->set_pos_src_id( io::const_string( ref.data()+1, ref.size()-1 ) );
				}
			}
			else if( is_sub_mesh(sev) ) {
				auto attr = sev.get_attribute("","material");
				s_sub_mesh sub_mesh = parse_sub_mesh(
							sev.name().local_name(),
							attr.second ? io::const_string(attr.first) : io::const_string(),
							parse_sizet( get_attr(sev,"count") )
							);
				// parse sub_mesh
				m->add_sub_mesh( std::move(sub_mesh) );
			}
		}
	}
	while( !is_end_element(et, "mesh") );
}

void parser::pase_geometry_library(s_model& md)
{
	static const char* ERR_MSG = "library_geometries is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	io::const_string geometry_id;
	io::const_string geometry_name;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( sev.empty_element() )
				continue;
			else if( is_element(sev,"geometry") ) {
				geometry_id = get_attr(sev,"id");
				auto attr = sev.get_attribute("","name");
				geometry_name = attr.second ? attr.first : io::const_string(geometry_id);
			}
			else if( is_element(sev,"mesh") ) {
				s_mesh m( new mesh(std::move(geometry_name) ) );
				parse_mesh(m);
				md->add_geometry( std::move(geometry_id), std::move(m) );
			}
		}
	}
	while( !is_end_element(et, library_geometries_) );
}

void parser::parse_node(node& nd)
{
	static const char* ERR_MSG = "node is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev,"instance_geometry") ) {
				io::const_string url = get_attr(sev,"url");
				nd.geo_ref.url = io::const_string( url.data()+1, url.size() - 1);
				auto attr = sev.get_attribute("","name");
				if( attr.second )
					nd.geo_ref.name.swap( attr.first );
			}
			else if(is_element(sev,"instance_material") ) {
				io::const_string tref = get_attr(sev,"target");
				nd.geo_ref.mat_ref.target = io::const_string( tref.data()+1, tref.size() -1);
				nd.geo_ref.mat_ref.symbol = get_attr(sev,"symbol");
			}
		}
	}
	while( !is_end_element(et, "node") );
}

void parser::parse_visual_scene(s_scene& scn)
{
	static const char* ERR_MSG = "visual_scene is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev, "node") ) {
				node nd;
				nd.id = get_attr(sev, "id");
				auto attr = sev.get_attribute("","name");
				if(attr.second)
					nd.name = std::move(attr.first);
				attr = sev.get_attribute("","type");
				if(attr.second && !attr.first.equal("type") )
					nd.type = node_type::joint;
				else
					nd.type = node_type::node;
				parse_node(nd);
				scn->add_node( std::move(nd) );
			}
		}
	}
	while( !is_end_element(et, "visual_scene") );
}

void parser::parse_library_images(s_model& md)
{
	static const char* ERR_MSG = "library_image is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev, "image") ) {
				io::const_string id = get_attr(sev, "id");
				sev = to_next_tag_start(state);
				check_eod(state, ERR_MSG);
				if( !is_element(sev,"init_from") )
					throw std::runtime_error("init_from element expected");
				md->add_image( std::move(id), get_tag_value() );
			}
		}
	}
	while( !is_end_element(et, library_images_) );
}

void parser::library_visual_scenes(s_model& md)
{
	static const char* ERR_MSG = "library_visual_scenes is unbalanced";
	io::xml::state_type state;
	io::xml::event_type et;
	io::xml::start_element_event sev;
	do {
		et = to_next_tag_event(state);
		check_eod(state, ERR_MSG);
		if( is_start_element(et) ) {
			sev = xp_->parse_start_element();
			check_eod(state, ERR_MSG);
			if( is_element(sev,"visual_scene") ) {
				s_scene scn( new scene( get_attr(sev,"id"), sev.get_attribute("","name").first ) );
				parse_visual_scene(scn);
				md->set_scene( std::move(scn) );
			}
		}
	}
	while( !is_end_element(et,library_visual_scenes_) );
}


s_model parser::load()
{
	s_model ret( new model() );
	io::xml::state_type state;
	io::xml::start_element_event e = to_next_tag_start(state);
	check_eod(state, "Expecting COLLADA model file");
	if( !e.name().local_name().equal("COLLADA") )
		throw std::runtime_error("Expecting COLLADA model file");
	do {
		e = to_next_tag_start(state);
		// nothing to do
		if( e.empty_element() )
			continue;
		else if( is_element(e,library_images_) ) {
			parse_library_images(ret);
		}
		else if( is_element(e,library_materials_) ) {
			parse_library_materials(ret);
		}
		else if( is_element(e,library_effects_) ) {
			parse_effect_library(ret);
		}
		else if( is_element(e,library_geometries_) ) {
			pase_geometry_library(ret);
		}
		else if( is_element(e,library_visual_scenes_) ) {
			library_visual_scenes(ret);
		}
	}
	while( state != io::xml::state_type::eod );

	if( xp_->is_error() )
		throw_parse_error();

	return ret;
}

void parser::throw_parse_error()
{
	std::error_code ec;
	xp_->get_last_error(ec);
	std::string msg("XML error [");
	msg.append( std::to_string( xp_->row() ) );
	msg.push_back(',');
	msg.append( std::to_string( xp_->col() ) );
	msg.append("] ");
	msg.append( ec.message() );
	throw std::runtime_error( msg );
}

parser::~parser() noexcept
{
}


} // namespace collada
