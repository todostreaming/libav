#
# configure.d configuration variables management
#
# Copyright (c) 2000-2002 Fabrice Bellard
# Copyright (c) 2005-2008 Diego Biurrun
# Copyright (c) 2005-2008 Mans Rullgard
#

filter(){
    pat=$1
    shift
    for v; do
        eval "case $v in $pat) echo $v ;; esac"
    done
}

filter_out(){
    pat=$1
    shift
    for v; do
        eval "case $v in $pat) ;; *) echo $v ;; esac"
    done
}

map(){
    m=$1
    shift
    for v; do eval $m; done
}

add_suffix(){
    suffix=$1
    shift
    for v; do echo ${v}${suffix}; done
}

set_all(){
    value=$1
    shift
    for var in $*; do
        eval $var=$value
    done
}

set_weak(){
    value=$1
    shift
    for var; do
        eval : \${$var:=$value}
    done
}

sanitize_var_name(){
    echo $@ | sed 's/[^A-Za-z0-9_]/_/g'
}

set_safe(){
    var=$1
    shift
    eval $(sanitize_var_name "$var")='$*'
}

get_safe(){
    eval echo \$$(sanitize_var_name "$1")
}

pushvar(){
    for pvar in $*; do
        eval level=\${${pvar}_level:=0}
        eval ${pvar}_${level}="\$$pvar"
        eval ${pvar}_level=$(($level+1))
    done
}

popvar(){
    for pvar in $*; do
        eval level=\${${pvar}_level:-0}
        test $level = 0 && continue
        eval level=$(($level-1))
        eval $pvar="\${${pvar}_${level}}"
        eval ${pvar}_level=$level
        eval unset ${pvar}_${level}
    done
}

enable(){
    set_all yes $*
}

disable(){
    set_all no $*
}

enable_weak(){
    set_weak yes $*
}

disable_weak(){
    set_weak no $*
}

enable_safe(){
    for var; do
        enable $(echo "$var" | sed 's/[^A-Za-z0-9_]/_/g')
    done
}

disable_safe(){
    for var; do
        disable $(echo "$var" | sed 's/[^A-Za-z0-9_]/_/g')
    done
}

do_enable_deep(){
    for var; do
        enabled $var && continue
        eval sel="\$${var}_select"
        eval sgs="\$${var}_suggest"
        pushvar var sgs
        enable_deep $sel
        popvar sgs
        enable_deep_weak $sgs
        popvar var
    done
}

enable_deep(){
    do_enable_deep $*
    enable $*
}

enable_deep_weak(){
    for var; do
        disabled $var && continue
        pushvar var
        do_enable_deep $var
        popvar var
        enable_weak $var
    done
}

enabled(){
    test "${1#!}" = "$1" && op== || op=!=
    eval test "x\$${1#!}" $op "xyes"
}

disabled(){
    test "${1#!}" = "$1" && op== || op=!=
    eval test "x\$${1#!}" $op "xno"
}

enabled_all(){
    for opt; do
        enabled $opt || return 1
    done
}

disabled_all(){
    for opt; do
        disabled $opt || return 1
    done
}

enabled_any(){
    for opt; do
        enabled $opt && return 0
    done
}

disabled_any(){
    for opt; do
        disabled $opt && return 0
    done
    return 1
}

set_default(){
    for opt; do
        eval : \${$opt:=\$${opt}_default}
    done
}

is_in(){
    value=$1
    shift
    for var in $*; do
        [ $var = $value ] && return 0
    done
    return 1
}

do_check_deps(){
    for cfg; do
        cfg="${cfg#!}"
        enabled ${cfg}_checking && die "Circular dependency for $cfg."
        disabled ${cfg}_checking && continue
        enable ${cfg}_checking
        append allopts $cfg

        eval dep_all="\$${cfg}_deps"
        eval dep_any="\$${cfg}_deps_any"
        eval dep_sel="\$${cfg}_select"
        eval dep_sgs="\$${cfg}_suggest"
        eval dep_ifa="\$${cfg}_if"
        eval dep_ifn="\$${cfg}_if_any"

        pushvar cfg dep_all dep_any dep_sel dep_sgs dep_ifa dep_ifn
        do_check_deps $dep_all $dep_any $dep_sel $dep_sgs $dep_ifa $dep_ifn
        popvar cfg dep_all dep_any dep_sel dep_sgs dep_ifa dep_ifn

        [ -n "$dep_ifa" ] && { enabled_all $dep_ifa && enable_weak $cfg; }
        [ -n "$dep_ifn" ] && { enabled_any $dep_ifn && enable_weak $cfg; }
        enabled_all  $dep_all || disable $cfg
        enabled_any  $dep_any || disable $cfg
        disabled_any $dep_sel && disable $cfg

        if enabled $cfg; then
            enable_deep $dep_sel
            enable_deep_weak $dep_sgs
        fi

        disable ${cfg}_checking
    done
}

check_deps(){
    unset allopts

    do_check_deps "$@"

    for cfg in $allopts; do
        enabled $cfg || continue
        eval dep_extralibs="\$${cfg}_extralibs"
        test -n "$dep_extralibs" && add_extralibs $dep_extralibs
    done
}

print_config(){
    pfx=$1
    files=$2
    shift 2
    map 'eval echo "$v \${$v:-no}"' "$@" |
    awk "BEGIN { split(\"$files\", files) }
        {
            c = \"$pfx\" toupper(\$1);
            v = \$2;
            sub(/yes/, 1, v);
            sub(/no/,  0, v);
            for (f in files) {
                file = files[f];
                if (file ~ /\\.h\$/) {
                    printf(\"#define %s %d\\n\", c, v) >>file;
                } else if (file ~ /\\.asm\$/) {
                    printf(\"%%define %s %d\\n\", c, v) >>file;
                } else if (file ~ /\\.mak\$/) {
                    n = -v ? \"\" : \"!\";
                    printf(\"%s%s=yes\\n\", n, c) >>file;
                }
            }
        }"
}

print_enabled(){
    suf=$1
    shift
    for v; do
        enabled $v && printf "%s\n" ${v%$suf};
    done
}

append(){
    var=$1
    shift
    eval "$var=\"\$$var $*\""
}

prepend(){
    var=$1
    shift
    eval "$var=\"$* \$$var\""
}
