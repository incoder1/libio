CC=cl
CXX=cl
AS=masm
LD=link

TARGET_ROOT=target
TARGET = $(TARGET_ROOT)\release-win-msvc-dll-x64
OBJ_ROOT = obj
OBJ = $(OBJ_ROOT)\release-win-msv-dll-x64

# dependencies
DEPS_ROOT=deps\msvc
DEPS_INCLUDES=$(DEPS_ROOT)\include
DEPS_LIBS=$(DEPS_ROOT)\lib\x64

LIB_NAME=io
SHARED_EXT=dll

LIBS=msvcrt.lib kernel32.lib user32.lib Ws2_32.lib iconv.dll.lib gnutls.lib 
INCLUEDS=/Iinclude /Iinclude\win /Iinclude\net /Isrc /I$(DEPS_INCLUDES)

OPTIMIZE= /Ox /Oi /Zc:wchar_t
SHARED_DEFINES= /DIO_SHARED_LIB /DIO_BUILD
DEFINES = $(SHARED_DEFINES) /DNDEBUG /DUNICODE
CPPFLAGS = /c /nologo /GL /Zl /std:c++latest $(DEFINES) $(OPTIMIZE) $(INCLUEDS) 
LDFLAGS = /DLL /LTCG /LIBPATH:$(DEPS_LIBS)

PCH = /Yustdafx.hpp /Fp$(OBJ)\stdafx.pch

OBJECTS = stdafx.pch errorcheck.obj memory_traits.obj sockets.obj files.obj synch_socket_channel.obj secure_channel.obj console.obj shared_library.obj buffer.obj channels.obj hashing.obj stringpool.obj charsets.obj charsetdetector.obj unicode_bom.obj charsetcvt.obj text.obj uri.obj http_client.obj xml_error.obj xml_event.obj xml_source.obj xml_parse.obj
LINK_OBJECTS = $(OBJ)\stdafx.obj $(OBJ)\errorcheck.obj $(OBJ)\memory_traits.obj $(OBJ)\sockets.obj $(OBJ)\files.obj $(OBJ)\synch_socket_channel.obj $(OBJ)\secure_channel.obj $(OBJ)\console.obj $(OBJ)\shared_library.obj $(OBJ)\buffer.obj $(OBJ)\channels.obj $(OBJ)\hashing.obj $(OBJ)\stringpool.obj $(OBJ)\charsets.obj $(OBJ)\charsetdetector.obj $(OBJ)\unicode_bom.obj $(OBJ)\charsetcvt.obj $(OBJ)\text.obj $(OBJ)\uri.obj $(OBJ)\http_client.obj $(OBJ)\xml_error.obj $(OBJ)\xml_event.obj $(OBJ)\xml_source.obj $(OBJ)\xml_parse.obj

all: clean link
	copy $(DEPS_LIBS)\iconv-2.dll $(TARGET)\iconv-2.dll
	copy $(DEPS_LIBS)\gnutls.dll $(TARGET)\gnutls.dll

clean:
#	del $(OBJ) > nul
#	del $(TARGET) > nul
	mkdir $(OBJ)
	mkdir $(TARGET)
	
link: compile
	$(LD) $(LDFLAGS) /OUT:$(TARGET)\io.dll /IMPLIB:$(TARGET)\io.lib $(LIBS) $(LINK_OBJECTS)

compile: $(OBJECTS)

stdafx.pch:
	$(CXX) $(CPPFLAGS) /Yc /Fp$(OBJ)\stdafx.pch /Fo$(OBJ)\stdafx.obj src\stdafx.cpp

# win only 
memory_traits.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\win\memory_traits.cpp /Fo$(OBJ)\memory_traits.obj
errorcheck.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\win\errorcheck.cpp /Fo$(OBJ)\errorcheck.obj
files.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\win\files.cpp /Fo$(OBJ)\files.obj
sockets.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\win\sockets.cpp /Fo$(OBJ)\sockets.obj
synch_socket_channel.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\win\synch_socket_channel.cpp /Fo$(OBJ)\synch_socket_channel.obj
sspi_channel.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\win\sspi_channel.cpp /Fo$(OBJ)\sspi_channel.obj
console.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\win\console.cpp /Fo$(OBJ)\console.obj
shared_library.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\shared_library.cpp /Fo$(OBJ)\shared_library.obj

# generic	
buffer.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\buffer.cpp /Fo$(OBJ)\buffer.obj
channels.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\channels.cpp /Fo$(OBJ)\channels.obj
hashing.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\hashing.cpp /Fo$(OBJ)\hashing.obj
stringpool.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\stringpool.cpp /Fo$(OBJ)\stringpool.obj
	
# charsets
charsets.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\charsets.cpp /Fo$(OBJ)\charsets.obj
unicode_bom.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\unicode_bom.cpp /Fo$(OBJ)\unicode_bom.obj
charsetdetector.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\charsetdetector.cpp /Fo$(OBJ)\charsetdetector.obj
charsetcvt.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\charsetcvt.cpp /Fo$(OBJ)\charsetcvt.obj
text.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\text.cpp /Fo$(OBJ)\text.obj
	
# network
uri.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\net\uri.cpp /Fo$(OBJ)\uri.obj
http_client.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\net\http_client.cpp /Fo$(OBJ)\http_client.obj
secure_channel.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\net\secure_channel.cpp /Fo$(OBJ)\secure_channel.obj
	
# XML
xml_error.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\xml_error.cpp /Fo$(OBJ)\xml_error.obj
xml_event.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\xml_event.cpp /Fo$(OBJ)\xml_event.obj
xml_source.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\xml_source.cpp /Fo$(OBJ)\xml_source.obj
xml_parse.obj:
	$(CXX) $(CPPFLAGS) $(PCH) src\xml_parse.cpp /Fo$(OBJ)\xml_parse.obj
