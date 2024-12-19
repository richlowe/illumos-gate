
LIBRARY = libsqlite3-sys.a

VERS=.3.47.2
OBJECTS=sqlite3.o

include $(SRC)/lib/Makefile.lib

include $(SRC)/lib/Makefile.rootfs

CSTD = $(CSTD_GNU99)

SRCDIR=$(SRC)/common/sqlite3

$(DYNLIB) := LDLIBS += -lm -lc 

LIBS = $(DYNLIB)

SRCS=$(SRCDIR)/sqlite3.c

MYCPPFLAGS = -D_REENTRANT -DTHREADSAFE=1 -DHAVE_USLEEP=1 -I. -I.. -I$(SRCDIR)

SQLITE3_CPPFLAGS=	-DSQLITE_DQS=0 \
			-DSQLITE_DEFAULT_MEMSTATUS=0 \
			-DSQLITE_LIKE_DOESNT_MATCH_BLOBS \
			-DSQLITE_MAX_EXPR_DEPTH=0 \
			-DSQLITE_OMIT_DECLTYPE \
			-DSQLITE_OMIT_DEPRECATED \
			-DSQLITE_OMIT_PROGRESS_CALLBACK \
			-DSQLITE_OMIT_SHARED_CACHE \
			-DSQLITE_OMIT_AUTOINIT \
			-DSQLITE_STRICT_SUBTYPE=1 \
			-DSQLITE_OMIT_UTF16

CPPFLAGS += $(SQLITE3_CPPFLAGS)
CPPFLAGS += $(MYCPPFLAGS)

MAPFILES = $(SRC)/lib/libsqlite3/mapfile-sqlite3


# This is the default Makefile target.  The objects listed here
# are what get build when you type just "make" with no arguments.
#
all:            $(LIBS)

install:        all

$(ROOTLINK): $(ROOTLIBDIR) $(ROOTLIBDIR)/$(DYNLIB)
	$(INS.liblink)
