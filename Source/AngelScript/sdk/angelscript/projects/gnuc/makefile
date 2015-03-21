# AngelScript makefile for Linux.
# Type 'make' then 'make install' to complete
# the installation of the library. You no
# longer have to specify SHARED=1 VERSION=x.y.z
# (the Makefile automatically determines it
# and builds it and the static library).
# See README for how to use the shared library
# instead of the static. The README also
# contains other information and in particular
# specifies on how to override the install
# location should you desire this (you don't
# have to - nor should you - edit this
# file).
#
# One note: I don't have a way to test
# the phone builds. I am an old-timer
# and I _still_ miss customer-owned
# coin-operated telephones. In fact
# I still _miss_ the rotary telephone!

LIBNAME=libangelscript
SHLIB=$(LIBNAME)_s.so
ARLIB=$(LIBNAME).a

ifeq ($(PREFIX),)
PREFIX=/usr/local
endif
INCLUDEDEST=$(PREFIX)/include/
LIBRARYDEST=$(PREFIX)/lib/

HEADER = angelscript.h
SRCDIR = ../../source
INCDIR = ../../include

ifeq ($(TARGETPLATFORM), iphone)
	IPHONEBIN =  /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin
	OBJDIR = obj-iphone
	LIBDIR = ../../lib-iphone
	CXX ?= $(IPHONEBIN)/clang++
	CXXFLAGS += -Wall -fPIC -fno-strict-aliasing -arch armv7 -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk -miphoneos-version-min=3.0
else ifeq ($(TARGETPLATFORM), iphonesimulator)
	IPHONEBIN =  /Developer/Platforms/iPhoneSimulator.platform/Developer/usr/bin
	OBJDIR = obj-iphone
	LIBDIR = ../../lib-iphone
	CXX ?= $(IPHONEBIN)/clang++
	CXXFLAGS += -Wall -fPIC -fno-strict-aliasing -arch i386 -isysroot /Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator5.0.sdk -miphoneos-version-min=3.0
else ifeq ($(TARGETPLATFORM), android)
	ANDROIDNDKROOT = /cygdrive/c/android/android-ndk-1.6_r1
	ANDROIDBIN = $(ANDROIDNDKROOT)/build/prebuilt/windows/arm-eabi-4.2.1/bin
	SYSROOT = $(ANDROIDNDKROOT)/build/platforms/android-4/arch-arm
	OBJDIR = obj-android
	LIBDIR = ../../lib-android
	CXX ?= $(ANDROIDBIN)/arm-eabi-gcc
	CXXFLAGS += -I$(SYSROOT)/usr/include \
	-Wall \
	-DANDROID \
	-fno-exceptions \
	-march=armv6 -mthumb-interwork \
	-mfloat-abi=softfp -fno-rtti
else
	OBJDIR = obj
	LIBDIR = ../../lib
	CXX ?= g++
	# On i686 architecture you may need to add -march=i686 if you get 
	# an undefined symbol for __sync_sub_and_fetch_4 in as_atomic.cpp.
	CXXFLAGS += -Wall -fPIC -fno-strict-aliasing
endif


AR ?= ar
RANLIB ?= ranlib

SRCNAMES = \
  as_atomic.cpp \
  as_builder.cpp  \
  as_bytecode.cpp \
  as_callfunc.cpp \
  as_callfunc_arm.cpp \
  as_callfunc_mips.cpp \
  as_callfunc_ppc.cpp \
  as_callfunc_ppc_64.cpp \
  as_callfunc_sh4.cpp \
  as_callfunc_x86.cpp \
  as_callfunc_x64_gcc.cpp \
  as_callfunc_x64_mingw.cpp \
  as_compiler.cpp \
  as_context.cpp \
  as_configgroup.cpp \
  as_datatype.cpp \
  as_generic.cpp \
  as_gc.cpp \
  as_globalproperty.cpp \
  as_memory.cpp \
  as_module.cpp \
  as_objecttype.cpp \
  as_outputbuffer.cpp \
  as_parser.cpp \
  as_restore.cpp \
  as_scriptcode.cpp \
  as_scriptengine.cpp \
  as_scriptfunction.cpp \
  as_scriptnode.cpp \
  as_scriptobject.cpp \
  as_string.cpp \
  as_string_util.cpp \
  as_thread.cpp \
  as_tokenizer.cpp \
  as_typeinfo.cpp \
  as_variablescope.cpp \

OBJ = $(addprefix $(OBJDIR)/, $(notdir $(SRCNAMES:.cpp=.o)))

ifeq ($(TARGETPLATFORM), iphone)
	OBJ += $(OBJDIR)/as_callfunc_arm_xcode.o
else 
	OBJ += $(OBJDIR)/as_callfunc_arm_gcc.o
endif

default: all

all: $(OBJDIR) $(LIBDIR) $(OBJ)
	$(MAKE) $(OBJDIR) $(LIBDIR) $(OBJ)
	$(AR) r $(LIBDIR)/$(ARLIB) $(OBJ)
	$(RANLIB) $(LIBDIR)/$(ARLIB)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared -Wl,-soname,$(LIBRARYDEST)$(LIBNAME)_s.so -o $(LIBDIR)/$(SHLIB) $(OBJ)
	@echo -------------------------------------------------------------------
	@echo Done. As root, type 'make install' to install the library.

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(LIBDIR):
	mkdir -p $(LIBDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

$(OBJDIR)/%.o: $(SRCDIR)/%.S
	$(CXX) $(CXXFLAGS) -o $@ -c $<

$(OBJDIR)/%.o: $(SRCDIR)/%.s
	$(CXX) $(CXXFLAGS) -o $@ -c $<

install_shared:
	@ver=`awk -F\" '/#define ANGELSCRIPT_VERSION_STRING/{print $$2}' ../../include/angelscript.h | cut -d" " -f1`; \
	if [ "$$ver" = "" ]; then \
		echo "Skipping creation of symbolic links..."; \
	else \
		( install -Dm 755 $(LIBDIR)/$(SHLIB) $(LIBRARYDEST)$(LIBNAME)_s-$$ver.so && \
		ln -fs $(LIBRARYDEST)$(LIBNAME)_s-$$ver.so $(LIBRARYDEST)$(SHLIB) && \
		ln -fs $(LIBRARYDEST)$(LIBNAME)_s-$$ver.so $(LIBRARYDEST)$(SHLIB).$$ver ) \
	fi ;


clean:
	rm -f $(OBJ) $(LIBDIR)/$(ARLIB) $(LIBDIR)/$(SHLIB)

install: $(LIBDIR)/$(ARLIB) $(LIBDIR)/$(SHLIB)
	@echo Installing to: $(LIBRARYDEST) and $(INCLUDEDEST)...
	@echo -------------------------------------------------------------------
	install -Dm 644 $(LIBDIR)/$(ARLIB) $(LIBRARYDEST)$(ARLIB)
	install -Dm 644 $(INCDIR)/$(HEADER) $(INCLUDEDEST)$(HEADER)
	$(MAKE) install_shared
	@echo -------------------------------------------------------------------
	@echo Angelscript library installed. Enjoy!

uninstall:
	rm -f $(INCLUDEDEST)$(HEADER) $(LIBRARYDEST)$(LIBNAME)*
	@echo -------------------------------------------------------------------
	@echo Angelscript library uninstalled.

.PHONY: all clean install uninstall
