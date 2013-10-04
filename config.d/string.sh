#
# configure.d string functions
#
# Copyright (c) 2000-2002 Fabrice Bellard
# Copyright (c) 2005-2008 Diego Biurrun
# Copyright (c) 2005-2008 Mans Rullgard
#

# Avoid locale weirdness, besides we really just want to translate ASCII.
toupper(){
    echo "$@" | tr abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ
}

tolower(){
    echo "$@" | tr ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz
}

c_escape(){
    echo "$*" | sed 's/["\\]/\\\0/g'
}

sh_quote(){
    v=$(echo "$1" | sed "s/'/'\\\\''/g")
    test "x$v" = "x${v#*[!A-Za-z0-9_/.+-]}" || v="'$v'"
    echo "$v"
}

cleanws(){
    echo "$@" | sed 's/^ *//;s/  */ /g;s/ *$//'
}
