#  FLAC - Free Lossless Audio Codec
#  Copyright (C) 2001,2002,2003  Josh Coalson
#
#  This program is part of FLAC; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#
# GNU makefile fragment for building a library
#

include $(topdir)/build/config.mk

ifeq ($(DARWIN_BUILD),yes)
CC          = cc
CCC         = c++
else
CC          = gcc
CCC         = g++
endif
NASM        = nasm
LINK        = ar cru
OBJPATH     = $(topdir)/obj
LIBPATH     = $(OBJPATH)/$(BUILD)/lib
DEBUG_LIBPATH     = $(OBJPATH)/debug/lib
RELEASE_LIBPATH   = $(OBJPATH)/release/lib
ifeq ($(DARWIN_BUILD),yes)
STATIC_LIB_SUFFIX = a
DYNAMIC_LIB_SUFFIX = dylib
else
STATIC_LIB_SUFFIX = a
DYNAMIC_LIB_SUFFIX = so
endif
STATIC_LIB_NAME     = $(LIB_NAME).$(STATIC_LIB_SUFFIX)
DYNAMIC_LIB_NAME    = $(LIB_NAME).$(DYNAMIC_LIB_SUFFIX)
STATIC_LIB          = $(LIBPATH)/$(STATIC_LIB_NAME)
DYNAMIC_LIB         = $(LIBPATH)/$(DYNAMIC_LIB_NAME)
DEBUG_STATIC_LIB    = $(DEBUG_LIBPATH)/$(STATIC_LIB_NAME)
DEBUG_DYNAMIC_LIB   = $(DEBUG_LIBPATH)/$(DYNAMIC_LIB_NAME)
RELEASE_STATIC_LIB  = $(RELEASE_LIBPATH)/$(STATIC_LIB_NAME)
RELEASE_DYNAMIC_LIB = $(RELEASE_LIBPATH)/$(DYNAMIC_LIB_NAME)
ifeq ($(DARWIN_BUILD),yes)
LINKD       = $(CC) -dynamiclib -flat_namespace -undefined suppress -install_name $(DYNAMIC_LIB)
else
LINKD       = $(CC) -shared
endif

debug   : CFLAGS = -g -O0 -DDEBUG $(DEBUG_CFLAGS) -Wall -W -DVERSION=$(VERSION) $(DEFINES) $(INCLUDES)
valgrind: CFLAGS = -g -O0 -DDEBUG $(DEBUG_CFLAGS) -DFLAC__VALGRIND_TESTING -Wall -W -DVERSION=$(VERSION) $(DEFINES) $(INCLUDES)
release : CFLAGS = -O3 -fomit-frame-pointer -funroll-loops -finline-functions -DNDEBUG $(RELEASE_CFLAGS) -Wall -W -Winline -DFLaC__INLINE=__inline__ -DVERSION=$(VERSION) $(DEFINES) $(INCLUDES)

LFLAGS  = -L$(LIBPATH)

DEBUG_OBJS = $(SRCS_C:%.c=%.debug.o) $(SRCS_CC:%.cc=%.debug.o) $(SRCS_CPP:%.cpp=%.debug.o) $(SRCS_NASM:%.nasm=%.debug.o)
RELEASE_OBJS = $(SRCS_C:%.c=%.release.o) $(SRCS_CC:%.cc=%.release.o) $(SRCS_CPP:%.cpp=%.release.o) $(SRCS_NASM:%.nasm=%.release.o)

debug   : $(ORDINALS_H) $(DEBUG_STATIC_LIB) $(DEBUG_DYNAMIC_LIB)
valgrind: $(ORDINALS_H) $(DEBUG_STATIC_LIB) $(DEBUG_DYNAMIC_LIB)
release : $(ORDINALS_H) $(RELEASE_STATIC_LIB) $(RELEASE_DYNAMIC_LIB)

$(DEBUG_STATIC_LIB): $(DEBUG_OBJS)
	$(LINK) $@ $(DEBUG_OBJS) && ranlib $@

$(RELEASE_STATIC_LIB): $(RELEASE_OBJS)
	$(LINK) $@ $(RELEASE_OBJS) && ranlib $@

$(DEBUG_DYNAMIC_LIB) : $(DEBUG_OBJS)
ifeq ($(DARWIN_BUILD),yes)
	$(LINKD) -o $@ $(DEBUG_OBJS) $(LFLAGS) $(LIBS) -lc
else
	$(LINKD) -o $@ $(DEBUG_OBJS) $(LFLAGS) $(LIBS)
endif

$(RELEASE_DYNAMIC_LIB) : $(RELEASE_OBJS)
ifeq ($(DARWIN_BUILD),yes)
	$(LINKD) -o $@ $(RELEASE_OBJS) $(LFLAGS) $(LIBS) -lc
else
	$(LINKD) -o $@ $(RELEASE_OBJS) $(LFLAGS) $(LIBS)
endif

%.debug.o %.release.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
%.debug.o %.release.o : %.cc
	$(CCC) $(CFLAGS) -c $< -o $@
%.debug.o %.release.o : %.cpp
	$(CCC) $(CFLAGS) -c $< -o $@
%.debug.i %.release.i : %.c
	$(CC) $(CFLAGS) -E $< -o $@
%.debug.i %.release.i : %.cc
	$(CCC) $(CFLAGS) -E $< -o $@
%.debug.i %.release.i : %.cpp
	$(CCC) $(CFLAGS) -E $< -o $@

%.debug.o %.release.o : %.nasm
	$(NASM) -f elf -d OBJ_FORMAT_elf -i ia32/ $< -o $@

.PHONY : clean
clean :
	-rm -f $(DEBUG_OBJS) $(RELEASE_OBJS) $(OBJPATH)/*/lib/$(STATIC_LIB_NAME) $(OBJPATH)/*/lib/$(DYNAMIC_LIB_NAME) $(ORDINALS_H)

.PHONY : depend
depend:
	makedepend -fMakefile.lite -- $(CFLAGS) $(INCLUDES) -- *.c *.cc *.cpp
