#!/bin/bash

if [ "$#" -lt 2 ]; then
    echo "Not enough arguments specified"
    exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d "$1" ]; then
    echo "$1 not a directory"
    exit 1
fi

numfiles=$(find "$filesdir" -type f | wc -l)
numlines=$(grep -r "$searchstr" "$filesdir" | wc -l)
echo "The number of files are $numfiles and the number of matching lines are $numlines"