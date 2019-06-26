#!/bin/sh -e

mk_xdg_dirs()
{   # need quotes to handle spaces, but then better test for emptiness
    for dir in "$XDG_CACHE_HOME" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME"; do
        if [ ! -z "$dir" ]; then
            mkdir -p "$dir"
        fi
    done
}
