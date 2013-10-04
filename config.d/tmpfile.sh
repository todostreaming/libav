#
# configure.d temporary files
#
# Copyright (c) 2000-2002 Fabrice Bellard
# Copyright (c) 2005-2008 Diego Biurrun
# Copyright (c) 2005-2008 Mans Rullgard
#

# set temporary file name
: ${TMPDIR:=$TEMPDIR}
: ${TMPDIR:=$TMP}
: ${TMPDIR:=/tmp}

if ! check_cmd mktemp -u XXXXXX; then
    # simple replacement for missing mktemp
    # NOT SAFE FOR GENERAL USE
    mktemp(){
        echo "${2%%XXX*}.${HOSTNAME}.${UID}.$$"
    }
fi

tmpfile(){
    tmp=$(mktemp -u "${TMPDIR}/ffconf.XXXXXXXX")$2 &&
        (set -C; exec > $tmp) 2>/dev/null ||
        die "Unable to create temporary file in $TMPDIR."
    append TMPFILES $tmp
    eval $1=$tmp
}

trap 'rm -f -- $TMPFILES' EXIT

tmpfile TMPASM .asm
tmpfile TMPC   .c
tmpfile TMPE   $EXESUF
tmpfile TMPH   .h
tmpfile TMPO   .o
tmpfile TMPS   .S
tmpfile TMPSH  .sh
tmpfile TMPV   .ver

unset -f mktemp

chmod +x $TMPE

# make sure we can execute files in $TMPDIR
cat > $TMPSH 2>> $logfile <<EOF
#! /bin/sh
EOF
chmod +x $TMPSH >> $logfile 2>&1
if ! $TMPSH >> $logfile 2>&1; then
    cat <<EOF
Unable to create and execute files in $TMPDIR.  Set the TMPDIR environment
variable to another directory and make sure that it is not mounted noexec.
EOF
    die "Sanity test failed."
fi
