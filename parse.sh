#!/bin/bash

list_sources() {
    echo "---> Listing sources..."

    find .                                   \
        -path "./arch*" -prune -o            \
        -path "./tmp*" -prune -o             \
        -path "./Documentation*" -prune -o   \
        -path "./scripts*" -prune -o         \
        -type f -name "*.[chsS]" -print >cscope.files

    find arch/arm/include/                   \
        arch/arm/kernel/                     \
        arch/arm/common/                     \
        arch/arm/boot/                       \
        arch/arm/lib/                        \
        arch/arm/mm/                         \
        arch/arm/mach-omap2/                 \
        arch/arm/plat-omap/                  \
        -type f -name "*.[chsS]" -print >>cscope.files
}

create_cscope_db() {
    echo "---> Creating cscope DB..."
    cscope -k -b -q
}

create_ctags_db() {
    echo "---> Creating CTags DB..."
    ctags -L cscope.files
}

cleanup() {
    echo "---> Removing garbage..."
    rm -f cscope.files
}

list_sources
create_cscope_db
create_ctags_db
cleanup
