#!/bin/sh

clone () {
    git clone "https://github.com/$1/$2.git" "dependencies/$2"
}

clone ericniebler range-v3
clone TartanLlama expected
clone catchorg    Catch2
