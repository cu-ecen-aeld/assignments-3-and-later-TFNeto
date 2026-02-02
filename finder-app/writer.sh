#!/bin/sh
# Script for assignment 1
# Author: Tiago Neto

#Check for correct number of arguments
if [ $# -lt 2 ]
then
    echo "Too few arguments"
    exit 1
else
    writefile=$1
    writestr=$2
fi 

DIR=$(dirname "$writefile")
#Check if directory exists
if [ ! -d "$DIR" ]
then
    mkdir -p "$DIR"
    #check if directory was created
    if [ ! -d "$DIR" ]
    then
        echo "Directory could not be created"
        exit 1
    fi
fi

echo "$writestr" > "$writefile"
exit 0