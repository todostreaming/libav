#
# configure.d compiler-related checks
#
# Copyright (c) 2000-2002 Fabrice Bellard
# Copyright (c) 2005-2008 Diego Biurrun
# Copyright (c) 2005-2008 Mans Rullgard
#

add_cppflags(){
    append CPPFLAGS "$@"
}

add_cflags(){
    append CFLAGS $($cflags_filter "$@")
}

add_asflags(){
    append ASFLAGS $($asflags_filter "$@")
}

add_ldflags(){
    append LDFLAGS $($ldflags_filter "$@")
}

add_extralibs(){
    prepend extralibs $($ldflags_filter "$@")
}

add_host_cppflags(){
    append host_cppflags "$@"
}

add_host_cflags(){
    append host_cflags $($host_cflags_filter "$@")
}

add_host_ldflags(){
    append host_ldflags $($host_ldflags_filter "$@")
}

add_compat(){
    append compat_objs $1
    shift
    map 'add_cppflags -D$v' "$@"
}

check_cmd(){
    log "$@"
    "$@" >> $logfile 2>&1
}

cc_o(){
    eval printf '%s\\n' $CC_O
}

cc_e(){
    eval printf '%s\\n' $CC_E
}

check_cc(){
    log check_cc "$@"
    cat > $TMPC
    log_file $TMPC
    check_cmd $cc $CPPFLAGS $CFLAGS "$@" $CC_C $(cc_o $TMPO) $TMPC
}

check_cpp(){
    log check_cpp "$@"
    cat > $TMPC
    log_file $TMPC
    check_cmd $cc $CPPFLAGS $CFLAGS "$@" $(cc_e $TMPO) $TMPC
}

as_o(){
    eval printf '%s\\n' $AS_O
}

check_as(){
    log check_as "$@"
    cat > $TMPS
    log_file $TMPS
    check_cmd $as $CPPFLAGS $ASFLAGS "$@" $AS_C $(as_o $TMPO) $TMPS
}

check_inline_asm(){
    log check_inline_asm "$@"
    name="$1"
    code="$2"
    shift 2
    disable $name
    check_cc "$@" <<EOF && enable $name
void foo(void){ __asm__ volatile($code); }
EOF
}

check_insn(){
    log check_insn "$@"
    check_inline_asm ${1}_inline "\"$2\""
    echo "$2" | check_as && enable ${1}_external || disable ${1}_external
}

check_yasm(){
    log check_yasm "$@"
    echo "$1" > $TMPS
    log_file $TMPS
    shift 1
    check_cmd $yasmexe $YASMFLAGS "$@" -o $TMPO $TMPS
}

ld_o(){
    eval printf '%s\\n' $LD_O
}

check_ld(){
    log check_ld "$@"
    flags=$(filter_out '-l*' "$@")
    libs=$(filter '-l*' "$@")
    check_cc $($cflags_filter $flags) || return
    flags=$($ldflags_filter $flags)
    libs=$($ldflags_filter $libs)
    check_cmd $ld $LDFLAGS $flags $(ld_o $TMPE) $TMPO $libs $extralibs
}

check_code(){
    log check_code "$@"
    check=$1
    headers=$2
    code=$3
    shift 3
    {
        for hdr in $headers; do
            echo "#include <$hdr>"
        done
        echo "int main(void) { $code; return 0; }"
    } | check_$check "$@"
}

check_cppflags(){
    log check_cppflags "$@"
    check_cc "$@" <<EOF && append CPPFLAGS "$@"
int x;
EOF
}

check_cflags(){
    log check_cflags "$@"
    set -- $($cflags_filter "$@")
    check_cc "$@" <<EOF && append CFLAGS "$@"
int x;
EOF
}

test_ldflags(){
    log test_ldflags "$@"
    check_ld "$@" <<EOF
int main(void){ return 0; }
EOF
}

check_ldflags(){
    log check_ldflags "$@"
    test_ldflags "$@" && add_ldflags "$@"
}

check_header(){
    log check_header "$@"
    header=$1
    shift
    disable_safe $header
    check_cpp "$@" <<EOF && enable_safe $header
#include <$header>
int x;
EOF
}

check_func(){
    log check_func "$@"
    func=$1
    shift
    disable $func
    check_ld "$@" <<EOF && enable $func
extern int $func();
int main(void){ $func(); }
EOF
}

check_mathfunc(){
    log check_mathfunc "$@"
    func=$1
    narg=$2
    shift 2
    test $narg = 2 && args="f, g" || args="f"
    disable $func
    check_ld "$@" <<EOF && enable $func
#include <math.h>
float foo(float f, float g) { return $func($args); }
int main(void){ return 0; }
EOF
}

check_func_headers(){
    log check_func_headers "$@"
    headers=$1
    funcs=$2
    shift 2
    {
        for hdr in $headers; do
            echo "#include <$hdr>"
        done
        for func in $funcs; do
            echo "long check_$func(void) { return (long) $func; }"
        done
        echo "int main(void) { return 0; }"
    } | check_ld "$@" && enable $funcs && enable_safe $headers
}

check_cpp_condition(){
    log check_cpp_condition "$@"
    header=$1
    condition=$2
    shift 2
    check_cpp "$@" <<EOF
#include <$header>
#if !($condition)
#error "unsatisfied condition: $condition"
#endif
EOF
}

check_lib(){
    log check_lib "$@"
    header="$1"
    func="$2"
    shift 2
    check_header $header && check_func $func "$@" && add_extralibs "$@"
}

check_lib2(){
    log check_lib2 "$@"
    headers="$1"
    funcs="$2"
    shift 2
    check_func_headers "$headers" "$funcs" "$@" && add_extralibs "$@"
}

check_pkg_config(){
    log check_pkg_config "$@"
    pkg="$1"
    headers="$2"
    funcs="$3"
    shift 3
    check_cmd $pkg_config --exists --print-errors $pkg || return
    pkg_cflags=$($pkg_config --cflags $pkg)
    pkg_libs=$($pkg_config --libs $pkg)
    check_func_headers "$headers" "$funcs" $pkg_cflags $pkg_libs "$@" &&
        set_safe ${pkg}_cflags $pkg_cflags   &&
        set_safe ${pkg}_libs   $pkg_libs
}

check_exec(){
    check_ld "$@" && { enabled cross_compile || $TMPE >> $logfile 2>&1; }
}

check_exec_crash(){
    code=$(cat)

    # exit() is not async signal safe.  _Exit (C99) and _exit (POSIX)
    # are safe but may not be available everywhere.  Thus we use
    # raise(SIGTERM) instead.  The check is run in a subshell so we
    # can redirect the "Terminated" message from the shell.  SIGBUS
    # is not defined by standard C so it is used conditionally.

    (check_exec "$@") >> $logfile 2>&1 <<EOF
#include <signal.h>
static void sighandler(int sig){
    raise(SIGTERM);
}
int foo(void){
    $code
}
int (*func_ptr)(void) = foo;
int main(void){
    signal(SIGILL, sighandler);
    signal(SIGFPE, sighandler);
    signal(SIGSEGV, sighandler);
#ifdef SIGBUS
    signal(SIGBUS, sighandler);
#endif
    return func_ptr();
}
EOF
}

check_type(){
    log check_type "$@"
    headers=$1
    type=$2
    shift 2
    disable_safe "$type"
    check_code cc "$headers" "$type v" "$@" && enable_safe "$type"
}

check_struct(){
    log check_struct "$@"
    headers=$1
    struct=$2
    member=$3
    shift 3
    disable_safe "${struct}_${member}"
    check_code cc "$headers" "const void *p = &(($struct *)0)->$member" "$@" &&
        enable_safe "${struct}_${member}"
}

check_builtin(){
    log check_builtin "$@"
    name=$1
    headers=$2
    builtin=$3
    shift 3
    disable "$name"
    check_code ld "$headers" "$builtin" "$@" && enable "$name"
}

require(){
    name="$1"
    header="$2"
    func="$3"
    shift 3
    check_lib $header $func "$@" || die "ERROR: $name not found"
}

require2(){
    name="$1"
    headers="$2"
    func="$3"
    shift 3
    check_lib2 "$headers" $func "$@" || die "ERROR: $name not found"
}

require_pkg_config(){
    pkg="$1"
    check_pkg_config "$@" || die "ERROR: $pkg not found"
    add_cflags    $(get_safe ${pkg}_cflags)
    add_extralibs $(get_safe ${pkg}_libs)
}

hostcc_o(){
    eval printf '%s\\n' $HOSTCC_O
}

check_host_cc(){
    log check_host_cc "$@"
    cat > $TMPC
    log_file $TMPC
    check_cmd $host_cc $host_cflags "$@" $HOSTCC_C $(hostcc_o $TMPO) $TMPC
}

check_host_cppflags(){
    log check_host_cppflags "$@"
    check_host_cc "$@" <<EOF && append host_cppflags "$@"
int x;
EOF
}

check_host_cflags(){
    log check_host_cflags "$@"
    set -- $($host_cflags_filter "$@")
    check_host_cc "$@" <<EOF && append host_cflags "$@"
int x;
EOF
}

apply(){
    file=$1
    shift
    "$@" < "$file" > "$file.tmp" && mv "$file.tmp" "$file" || rm "$file.tmp"
}

cp_if_changed(){
    cmp -s "$1" "$2" && echo "$2 is unchanged" && return
    mkdir -p "$(dirname $2)"
    $cp_f "$1" "$2"
}

