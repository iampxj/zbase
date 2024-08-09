#!/bin/sh

if [ `grep -c "embedded-teams" .git/config` -eq '0' ]; then
    echo "the current is gitHub"
    rm  -rf   github.tar.gz
    tar -czvf github.tar.gz .git/
    rm  -rf  .git

    tar -xzvf gitlab.tar.gz
else
    echo "the current is gitLab"
fi