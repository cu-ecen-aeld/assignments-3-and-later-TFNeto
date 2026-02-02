#!/bin/sh
# Script for assignment 1
# Author: Tiago Neto

#Check for correct number of arguments
if [ $# -lt 2 ]
then
    echo "Too few arguments"
    exit 1
else
    filesdir=$1
    searchstr=$2
fi

#Check if directory exists
if [ ! -d "$filesdir" ]
then
    echo "Directory does not exist"
    exit 1
fi

numberFile=$(find "$filesdir" -type f | wc -l)
numberLines=$(grep -r "$searchstr" "$filesdir" 2>/dev/null | wc -l)

echo "The number of files are $numberFile and the number of matching lines are $numberLines"
exit 0