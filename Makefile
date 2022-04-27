default: build


###############################################################################
ARCH            = $(shell uname -m)

ifeq ($(ARCH),x86_64)
arch_LIB        = x86_64
else ifeq ($(ARCH),i386)
arch_LIB        = i386
else ifeq ($(ARCH),i686)
arch_LIB        = i686
else
$(error target ${ARCH} not supported)
endif


###############################################################################
OBJDIR     = $(PWD)/obj
INCDIR     = $(PWD)/inc
SRCDIR     = $(PWD)/src
EXEDIR     = $(PWD)/bin


###############################################################################
CC         = g++
CFLAGS     = -Werror -Wall -fPIC
CPPFLAGS   = -D_GNU_SOURCE -I $(INCDIR)
LDFLAGS    = -L/usr/lib/$(arch_LIB)-linux-gnu/
LIBS       = 

dev_OPT    = -g3
build_OPT  = -O2 -g


###############################################################################
ifeq ($(ARCH),i386)
CPPFLAGS   += -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
else ifeq ($(ARCH),i686)
CPPFLAGS   += -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
endif


###############################################################################
OBJS += \
  $(OBJDIR)/main.o


###############################################################################
$(OBJDIR)/main.o: $(SRCDIR)/main.cpp $(INCDIR)/common.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(OPT) $< -c -o $@
	
	
###############################################################################
$(EXEDIR)/main: $(OBJS)
	$(CC) -o $@ $^ $(LIBS)


###############################################################################
.PHONY: dev build clean dirs

dev: dirs
	$(MAKE) $(OBJS) \
	  CFLAGS="$(CFLAGS)" OPT="$(dev_OPT)" CPPFLAGS="$(CPPFLAGS)"
	$(MAKE) $(EXEDIR)/main OBJS="$(OBJS)" \
	  LDFLAGS="$(LDFLAGS)" LIBS="$(LIBS)"

build: dirs
	$(MAKE) $(OBJS) \
	  CFLAGS="$(CFLAGS)" OPT="$(build_OPT)" CPPFLAGS="$(CPPFLAGS)"
	$(MAKE) $(EXEDIR)/main OBJS="$(OBJS)" \
	  LDFLAGS="$(LDFLAGS)" LIBS="$(LIBS)"


##############################################################################
clean:
	@startdir=$(PWD)
	rm -rf $(OBJDIR)*
	rm -rf $(EXEDIR)*


##############################################################################
dirs:
	mkdir -p $(OBJDIR) $(EXEDIR)

