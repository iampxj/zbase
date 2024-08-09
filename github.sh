#!/bin/sh

if [ `grep -c "embedded-teams" .git/config` -ne '0' ]; then
    echo "the current is gitlab"
    rm  -rf   gitlab.tar.gz
    tar -czvf gitlab.tar.gz .git/
    rm  -rf  .git

    tar -xzvf github.tar.gz
else
    echo "the current is github"
fi