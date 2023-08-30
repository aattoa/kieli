#!/bin/sh

if [ "$#" = "1" ] && [ -e "$1" ]
then
    find "$1" -type f -name "*[ch]pp" -exec clang-format -i {} \;
else
    echo "format.sh: Invalid argument"
    exit 1
fi
