#
# configure.d shell fixups
#
# Copyright (c) 2000-2002 Fabrice Bellard
# Copyright (c) 2005-2008 Diego Biurrun
# Copyright (c) 2005-2008 Mans Rullgard
#

# Prevent locale nonsense from breaking basic text processing.
LC_ALL=C
export LC_ALL

# make sure we are running under a compatible shell
# try to make this part work with most shells

try_exec(){
    echo "Trying shell $1"
    type "$1" > /dev/null 2>&1 && exec "$@"
}

unset foo
(: ${foo%%bar}) 2> /dev/null
E1="$?"

(: ${foo?}) 2> /dev/null
E2="$?"

if test "$E1" != 0 || test "$E2" = 0; then
    echo "Broken shell detected.  Trying alternatives."
    export FF_CONF_EXEC
    if test "0$FF_CONF_EXEC" -lt 1; then
        FF_CONF_EXEC=1
        try_exec bash "$0" "$@"
    fi
    if test "0$FF_CONF_EXEC" -lt 2; then
        FF_CONF_EXEC=2
        try_exec ksh "$0" "$@"
    fi
    if test "0$FF_CONF_EXEC" -lt 3; then
        FF_CONF_EXEC=3
        try_exec /usr/xpg4/bin/sh "$0" "$@"
    fi
    echo "No compatible shell script interpreter found."
    echo "This configure script requires a POSIX-compatible shell"
    echo "such as bash or ksh."
    echo "THIS IS NOT A BUG IN LIBAV, DO NOT REPORT IT AS SUCH."
    echo "Instead, install a working POSIX-compatible shell."
    echo "Disabling this configure test will create a broken Libav."
    if test "$BASH_VERSION" = '2.04.0(1)-release'; then
        echo "This bash version ($BASH_VERSION) is broken on your platform."
        echo "Upgrade to a later version if available."
    fi
    exit 1
fi

test -d /usr/xpg4/bin && PATH=/usr/xpg4/bin:$PATH


