#!/usr/bin/bash

for dep in ./dependencies/*
do
    cd "$dep" || exit
    git pull
    cd ../..
done

