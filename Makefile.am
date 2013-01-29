ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = m5
m5_SOURCES = src/m5-token.c \
	     src/m5-input.c \
	     src/m5.c

m5_CPPFLAGS = $(GLIB_CFLAGS) -std=c99 -O2
m5_LDADD   = $(GLIB_LIBS)

