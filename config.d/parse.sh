#
# configure.d commandline and configuration parsing
#
# Copyright (c) 2000-2002 Fabrice Bellard
# Copyright (c) 2005-2008 Diego Biurrun
# Copyright (c) 2005-2008 Mans Rullgard
#

# find source path
if test -f configure; then
    source_path=.
else
    source_path=$(cd $(dirname "$0"); pwd)
    echo "$source_path" | grep -q '[[:blank:]]' &&
        die "Out of tree builds are impossible with whitespace in source path."
    test -e "$source_path/config.h" &&
        die "Out of tree builds are impossible with config.h in source dir."
fi

for v in "$@"; do
    r=${v#*=}
    l=${v%"$r"}
    r=$(sh_quote "$r")
    LIBAV_CONFIGURATION="${LIBAV_CONFIGURATION# } ${l}${r}"
done

find_things(){
    thing=$1
    pattern=$2
    file=$source_path/$3
    sed -n "s/^[^#]*$pattern.*([^,]*, *\([^,]*\)\(,.*\)*).*/\1_$thing/p" "$file"
}
