#
# configure.d build directive defaults
#
# Copyright (c) 2000-2002 Fabrice Bellard
# Copyright (c) 2005-2008 Diego Biurrun
# Copyright (c) 2005-2008 Mans Rullgard
#

SHFLAGS='-shared -Wl,-soname,$$(@F)'
LIBPREF="lib"
LIBSUF=".a"
FULLNAME='$(NAME)$(BUILDSUF)'
LIBNAME='$(LIBPREF)$(FULLNAME)$(LIBSUF)'
SLIBPREF="lib"
SLIBSUF=".so"
SLIBNAME='$(SLIBPREF)$(FULLNAME)$(SLIBSUF)'
SLIBNAME_WITH_VERSION='$(SLIBNAME).$(LIBVERSION)'
SLIBNAME_WITH_MAJOR='$(SLIBNAME).$(LIBMAJOR)'
LIB_INSTALL_EXTRA_CMD='$$(RANLIB) "$(LIBDIR)/$(LIBNAME)"'
SLIB_INSTALL_NAME='$(SLIBNAME_WITH_VERSION)'
SLIB_INSTALL_LINKS='$(SLIBNAME_WITH_MAJOR) $(SLIBNAME)'

asflags_filter=echo
cflags_filter=echo
ldflags_filter=echo

AS_C='-c'
AS_O='-o $@'
CC_C='-c'
CC_E='-E -o $@'
CC_O='-o $@'
LD_O='-o $@'
LD_LIB='-l%'
LD_PATH='-L'
HOSTCC_C='-c'
HOSTCC_O='-o $@'
HOSTLD_O='-o $@'

host_cflags='-O3 -g'
host_cppflags='-D_ISOC99_SOURCE -D_XOPEN_SOURCE=600'
host_libs='-lm'
host_cflags_filter=echo
host_ldflags_filter=echo

target_path='$(CURDIR)'

# since the object filename is not given with the -MM flag, the compiler
# is only able to print the basename, and we must add the path ourselves
DEPCMD='$(DEP$(1)) $(DEP$(1)FLAGS) $($(1)DEP_FLAGS) $< | sed -e "/^\#.*/d" -e "s,^[[:space:]]*$(*F)\\.o,$(@D)/$(*F).o," > $(@:.o=.d)'
DEPFLAGS='-MM'
