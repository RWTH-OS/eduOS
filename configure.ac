#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

m4_include(config/acx.m4)
m4_include(config/elf.m4)

AC_PREREQ(2.69)
AC_INIT(eduOS, 0.1, slankes@eonerc.rwth-aachen.de, eduOS, http://rwth-os.github.io/eduOS/)
AC_CONFIG_SRCDIR(kernel/main.c)
AC_CONFIG_HEADERS(include/eduos/config.h)

# Find the build, host, and target systems.
ACX_NONCANONICAL_BUILD
ACX_NONCANONICAL_HOST
ACX_NONCANONICAL_TARGET

# Find the build, host, and target systems.
AC_CANONICAL_SYSTEM
AC_ARG_PROGRAM

m4_pattern_allow([^AS_FOR_TARGET$])dnl
m4_pattern_allow([^AS_FOR_BUILD$])dnl

# Get 'install' or 'install-sh' and its variants.
AC_PROG_INSTALL
ACX_PROG_LN
AC_PROG_LN_S
AC_PROG_SED
AC_PROG_AWK

if test "${build}" != "${host}" ; then
  AR_FOR_BUILD=${AR_FOR_BUILD-ar}
  AS_FOR_BUILD=${AS_FOR_BUILD-as}
  CC_FOR_BUILD=${CC_FOR_BUILD-gcc}
  CXX_FOR_BUILD=${CXX_FOR_BUILD-g++}
  GCJ_FOR_BUILD=${GCJ_FOR_BUILD-gcj}
  GFORTRAN_FOR_BUILD=${GFORTRAN_FOR_BUILD-gfortran}
  GOC_FOR_BUILD=${GOC_FOR_BUILD-gccgo}
  LD_FOR_BUILD=${LD_FOR_BUILD-ld}
  NM_FOR_BUILD=${NM_FOR_BUILD-nm}
  RANLIB_FOR_BUILD=${RANLIB_FOR_BUILD-ranlib}
else
  AR_FOR_BUILD="\$(AR)"
  AS_FOR_BUILD="\$(AS)"
  CC_FOR_BUILD="\$(CC)"
  CXX_FOR_BUILD="\$(CXX)"
  GCJ_FOR_BUILD="\$(GCJ)"
  GFORTRAN_FOR_BUILD="\$(GFORTRAN)"
  GOC_FOR_BUILD="\$(GOC)"
  LD_FOR_BUILD="\$(LD)"
  NM_FOR_BUILD="\$(NM)"
  RANLIB_FOR_BUILD="\$(RANLIB)"
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

m4_pattern_allow([^AS_FOR_TARGET$])dnl
m4_pattern_allow([^AS_FOR_BUILD$])dnl

# Build module lists & subconfigure args.
AC_SUBST(build_configargs)
AC_SUBST(build_configdirs)

# Host module lists & subconfigure args.
AC_SUBST(host_configargs)
AC_SUBST(configdirs)
AC_SUBST(target_configdirs)

# Target module lists & subconfigure args.
AC_SUBST(target_configargs)

NCN_STRICT_CHECK_TARGET_TOOLS(CC_FOR_TARGET, cc gcc)
NCN_STRICT_CHECK_TARGET_TOOLS(CXX_FOR_TARGET, c++ g++ cxx gxx)
NCN_STRICT_CHECK_TARGET_TOOLS(GCC_FOR_TARGET, gcc, ${CC_FOR_TARGET})

ACX_CHECK_INSTALLED_TARGET_TOOL(CPP_FOR_TARGET, cpp)
ACX_CHECK_INSTALLED_TARGET_TOOL(AR_FOR_TARGET, ar)
ACX_CHECK_INSTALLED_TARGET_TOOL(AS_FOR_TARGET, as)
ACX_CHECK_INSTALLED_TARGET_TOOL(LD_FOR_TARGET, ld)
ACX_CHECK_INSTALLED_TARGET_TOOL(LIPO_FOR_TARGET, lipo)
ACX_CHECK_INSTALLED_TARGET_TOOL(NM_FOR_TARGET, nm)
ACX_CHECK_INSTALLED_TARGET_TOOL(OBJDUMP_FOR_TARGET, objdump)
ACX_CHECK_INSTALLED_TARGET_TOOL(OBJCOPY_FOR_TARGET, objcopy)
ACX_CHECK_INSTALLED_TARGET_TOOL(RANLIB_FOR_TARGET, ranlib)
ACX_CHECK_INSTALLED_TARGET_TOOL(READELF_FOR_TARGET, readelf)
ACX_CHECK_INSTALLED_TARGET_TOOL(STRIP_FOR_TARGET, strip)

# define eduOS specific constants
AC_DEFINE(HAVE_ARCH_MEMSET, 1, Define to use machine specific version of memset)
AC_DEFINE(HAVE_ARCH_MEMCPY, 1, Define to use machine specific version of memcpy)
AC_DEFINE(HAVE_ARCH_STRLEN, 1, Define to use machine specific version of strlen)
AC_DEFINE(HAVE_ARCH_STRCPY, 1, Define to use machine specific version of strcpy)
AC_DEFINE(HAVE_ARCH_STRNCPY, 1, Define to use machine specific version of strncpy)

AC_ARG_WITH(max-tasks, [AS_HELP_STRING([--with-max-tasks=N], [specify the maximum number of running tasks])])
if test "x$with_max_tasks" != x; then
AC_DEFINE_UNQUOTED(MAX_TASKS, $with_max_tasks, Define the maximum number of running tasks)
else
AC_DEFINE(MAX_TASKS, 16, Define the maximum number of running tasks)
fi

AC_ARG_WITH(stack-size, [AS_HELP_STRING([--with-stack-size=SIZE], [specify the size of the kernel stack])])
if test "x$with_stack_size" != x; then
AC_DEFINE_UNQUOTED(KERNEL_STACK_SIZE, $with_stack_size, Define the size of the kernel stack)
else
AC_DEFINE(KERNEL_STACK_SIZE, 8192, Define the size of the kernel stack)
fi

AC_ARG_WITH(bitmap-size, [AS_HELP_STRING([--with-bitmap-size=N], [specify the bitmap size])])
if test "x$with_bitmap_size" != x; then
AC_DEFINE_UNQUOTED(BITMAP_SIZE, $with_bitmap_size, Define the bitmap isze)
else
AC_DEFINE(BITMAP_SIZE, 4096, Define the bitmap size)
fi

AC_CONFIG_FILES(Makefile)
AC_OUTPUT
