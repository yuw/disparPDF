#!/bin/sh
# Checks that every option "PROGRAM --help" lists is offered by the shell
# completions, so that they stay in step.
#   completion-options.sh PROGRAM COMPLETION_FILE...
program=$1
shift
options=$("$program" --help | grep -E '^ +-' |
          grep -oE -- '(^| |,)--?[a-zA-Z][a-zA-Z0-9-]*' | tr -d ' ,' | sort -u)
[ -n "$options" ] || { echo "FAIL: no options found in --help"; exit 1; }
status=0
for file in "$@"; do
    for option in $options; do
        # Not just part of a longer option, e.g. -a in --appearance
        if ! grep -qE -- "(^|[^a-zA-Z-])$option([^a-zA-Z0-9-]|\$)" "$file"; then
            echo "FAIL: $option is not in $file"
            status=1
        fi
    done
done
exit $status
