#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <writefile> <writestr>" >&2
    exit 1
fi

writefile="$1"
writestr="$2"
writedir=$(dirname "$writefile")


mkdir -p "$writedir" || exit 1
printf '%s\n' "$writestr" > "$writefile" || exit 1