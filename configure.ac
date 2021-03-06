# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT(cr3, 3.0.19, vadim.lopatin@coolreader.org)
AC_PREREQ(2.52)
AC_CONFIG_SRCDIR(src/cr3main.cpp)
#AC_CONFIG_SRCDIR(crengine/configure.ac)
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_ISC_POSIX
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE(1.6 dist-bzip2)

AC_ARG_WITH(jinke,AC_HELP_STRING([--with-jinke],[path to jinke SDK]),
    [
    ac_cv_use_jinke=$withval
    with_jinke=true
    case ${host_cpu} in
        i?86) cpu_libdir=i386 ;;
        *) cpu_libdir="${host_cpu}";;
    esac
    JINKEPATH="$withval"
    CPPFLAGS="-DLBOOK=1 ${CPPFLAGS} -I${JINKEPATH}/include"
    #LDFLAGS="${LDFLAGS} -L${JINKEPATH}/thirdparty/lib/${cpu_libdir} "
    export CPPFLAGS
    export LDFLAGS
    export JINKEPATH
    AC_DEFINE(CR_USE_JINKE)
],[
with_jinke=false
ac_cv_use_jinke=none
AC_DEFINE(CR_USE_XCB)
])

AC_ARG_WITH(jinke-app,AC_HELP_STRING([--with-jinke-app],[to build standalone viewer instead of plugin]),
    [
    ac_cv_use_jinke_app=$withval
    with_jinke_app=true
    AC_DEFINE(CR_USE_JINKE_VIEWER)
],[
    with_jinke_app=false
    ac_cv_use_jinke_app=none
])

AC_ARG_WITH(old-nanox,AC_HELP_STRING([--with-old-nanox],[use old nanoX build]),
    [
    ac_cv_use_old_nanox=$withval
    with_old_nanox=true
    AC_DEFINE(CR_USE_OLD_NANOX)
],[
    with_old_nanox=false
    ac_cv_use_old_nanox=none
])

localedir='${prefix}/share/locale'
AC_SUBST(localedir)
#AC_DEFINE(LOCALEDIR,${localedir})
AC_DEFINE_UNQUOTED(LOCALEDIR, ["$localedir"],
          [Define to the location of locale files.])

AC_PROG_CXX
AC_PROG_CC
#AC_GNU_SOURCE
AC_PROG_CC_STDC
AC_GNU_SOURCE
AC_HEADER_STDC
AC_C_BIGENDIAN
AC_C_CONST
AM_PROG_LIBTOOL
AM_GNU_GETTEXT([external])
#don't submit!
AM_GNU_GETTEXT_VERSION([0.16])

if test "${with_jinke}" == "false"; then
if test "${with_jinke_app}" == "false"; then
    PKG_PROG_PKG_CONFIG
    #PKG_CHECK_MODULES(EWL, [ewl >= 0.5.2.042])
    PKG_CHECK_MODULES(XCB, [xcb >= 1.1])
    PKG_CHECK_MODULES(XCB_AUX, [xcb-aux >= 0.2])
    PKG_CHECK_MODULES(XCB_IMAGE, [xcb-image >= 0.2])
    PKG_CHECK_MODULES(XCB_KEYSYMS, [xcb-keysyms >= 0.2])
    PKG_CHECK_MODULES(FREETYPE, [freetype2 >= 9.10])
    PKG_CHECK_MODULES(FONTCONFIG, [fontconfig >= 0.22])
    PKG_CHECK_MODULES(PNG, [libpng >= 1.2])
fi
fi

SUBDIRS=""

if test -r "${srcdir}/tinydict/configure"; then
    AC_CONFIG_SUBDIRS([tinydict])
    SUBDIRS="${SUBDIRS} tinydict"
    TINYDICT_CFLAGS="-I\$(top_srcdir)/tinydict"
    TINYDICT_LIBS="\$(top_builddir)/tinydict/libtinydict.la"
    AC_SUBST([TINYDICT_CFLAGS])
    AC_SUBST([TINYDICT_LIBS])
else
    PKG_CHECK_MODULES(TINYDICT, [libtinydict >= 0.1])
fi

SUBDIRS="${SUBDIRS} crengine"
echo "${srcdir}/crengine/configure"
if test -r "${srcdir}/crengine/configure"; then
    AC_CONFIG_SUBDIRS([crengine])
    CRENGINE_CFLAGS="-I\$(srcdir)/crengine/include -DLINUX -D_LINUX -DCOLOR_BACKBUFFER=0"
    CRENGINE_LIBS="\$(top_builddir)/crengine/src/libcrengine.la"
else
    PKG_CHECK_MODULES(CRENGINE, [crengine >= 3.0.16])
fi

SUBDIRS="${SUBDIRS} src"

AC_ARG_ENABLE(cite,
    AC_HELP_STRING([enable cite module]),
    AC_DEFINE(WITH_CITE, 1))

dnl color backbuffer option
AC_ARG_ENABLE(color-backbuffer,
   AC_HELP_STRING(
      [enable color backbuffer (gray otherwise)]),
   [AC_DEFINE(COLOR_BACKBUFFER, 1, [Build with color backbuffer])],
   [AC_DEFINE(COLOR_BACKBUFFER, 0, [Build with gray backbuffer])]
)

dnl crengine memory manager option
AC_ARG_ENABLE(crengine-memory-manager,
   AC_HELP_STRING(
      [enable crengine internal memory manager]),
   [AC_DEFINE(LDOM_USE_OWN_MEM_MAN, 1, [Build with internal memory manager support])],
   [AC_DEFINE(LDOM_USE_OWN_MEM_MAN, 0, [Build without internal memory manager support])]
)

AC_SUBST([CRENGINE_CFLAGS])
AC_SUBST([CRENGINE_LIBS])

AC_SUBST([SUBDIRS])

AM_CONDITIONAL(WITH_JINKE,$with_jinke)
AM_CONDITIONAL(WITH_JINKE_APP,$with_jinke_app)
AM_CONDITIONAL(WITH_OLD_NANOX,$with_old_nanox)
#AC_CONFIG_FILES([Makefile])

AC_OUTPUT(Makefile src/Makefile po/Makefile.in)
