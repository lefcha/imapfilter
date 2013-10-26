#!/bin/sh
#
# Simple script to make tar.bz2 files from git tags
#
# Copyright (c) 2013 David Sommerseth
# See LICENSE for details
#

if [ $# = 0 ]; then
    echo "Usage: $0 <tag name>"
    exit
fi

# Check if tag exists
git tag -l | grep -q $1
if [ $? -ne 0 ];then
    echo "No such tag: $1"
    exit 1
fi

# Create a tar.bz2 archive based on the tag
git archive -v --format=tar --prefix=imapfilter-$1/ $1 | bzip2 -9c > imapfilter-$1.tar.bz2

