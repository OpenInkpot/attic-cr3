#CRENGINE_FLAGS = ${CRENGINE_CFLAGS}
#CRENGINE_LIBS = ${CRENGINE_LIBS}

INCLUDES = @FREETYPE_CFLAGS@ @PNG_CFLAGS@ @CRENGINE_CFLAGS@ -I$(top_srcdir)/crengine/include
AM_CFLAGS=-Wall
AM_CXXFLAGS=-Wall
AM_CPPFLAGS = -I$(top_srcdir)/crengine/include

SUBDIRS =

COMMON_LDADD = @FREETYPE_LIBS@ \
	@XCB_KEYSYMS_LIBS@ \
	@XCB_LIBS@ @XCB_AUX_LIBS@ @XCB_IMAGE_LIBS@ @PNG_LIBS@ @CRENGINE_LIBS@ @TINYDICT_LIBS@ \
	@FONTCONFIG_LIBS@ \
	-lfreetype -ljpeg -lpng -lz
AM_CPPFLAGS += @CRENGINE_CFLAGS@ @TINYDICT_CFLAGS@ @FONTCONFIG_CFLAGS@

AM_CPPFLAGS += -DCRSKIN=\"$(pkgdatadir)\"

COMMON_SOURCES = cr3main.cpp settings.cpp mainwnd.cpp cr3_def_cover.cpp bgfit.cpp \
                 bmkdlg.cpp linksdlg.cpp tocdlg.cpp numedit.cpp fsmenu.cpp recentdlg.cpp \
		 viewdlg.cpp scrkbd.cpp selnavig.cpp
COMMON_SOURCES += citedlg.cpp

#if WITH_DICT
COMMON_SOURCES += dictdlg.cpp
#endif

noinst_LTLIBRARIES = libcommon.la
libcommon_la_SOURCES = ${COMMON_SOURCES}

if WITH_JINKE

if WITH_JINKE_APP

AM_CPPFLAGS += -DJINKE_VIEWER
bin_PROGRAMS = cr3
cr3_SOURCES = cr3nanox.cpp
cr3_LDADD = libcommon.la ${COMMON_LDADD} -lnano-X -lpthread

else

lib_LTLIBRARIES = libfb2.la
libfb2_la_SOURCES = cr3jinke.cpp
libfb2_la_LIBADD =  libcommon.la ${COMMON_LDADD}
libfb2_la_LDFLAGS = --static-libtool-libs -rpath ${libdir} 

endif

if WITH_OLD_NANOX
AM_CPPFLAGS += -DUSE_OLD_NANOX
endif

else

AM_CPPFLAGS += -DWITH_XCB
bin_PROGRAMS = cr3
cr3_SOURCES = cr3xcb.cpp
cr3_LDADD = libcommon.la ${COMMON_LDADD}

endif


