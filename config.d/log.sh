#
# configure.d logging functions
#
# Copyright (c) 2000-2002 Fabrice Bellard
# Copyright (c) 2005-2008 Diego Biurrun
# Copyright (c) 2005-2008 Mans Rullgard
#

quotes='""'

log(){
    echo "$@" >> $logfile
}

log_file(){
    log BEGIN $1
    pr -n -t $1 >> $logfile
    log END $1
}

echolog(){
    log "$@"
    echo "$@"
}

warn(){
    log "WARNING: $*"
    WARNINGS="${WARNINGS}WARNING: $*\n"
}

die(){
    echolog "$@"
    cat <<EOF

If you think configure made a mistake, make sure you are using the latest
version from Git.  If the latest version fails, report the problem to the
${REPORT}.
EOF
    if disabled logging; then
        cat <<EOF
Rerun configure with logging enabled (do not use --disable-logging), and
include the log this produces with your report.
EOF
    else
        cat <<EOF
Include the log file "$logfile" produced by configure as this will help
solving the problem.
EOF
    fi
    exit 1
}

