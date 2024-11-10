#!/usr/bin/bash

# Find all .iso disk images, mount them temporarily and search their content for valid ILBM images.

DIR=/tmp/iso_mount
VERBOSITY=-

mkdir -p $DIR

find "$1" -name *.iso \
    -exec sudo mount "{}" $DIR ';'\
    -exec echo "  * {}" ';'\
    -exec sh -c "find $DIR -exec ./ilbm_cli $VERBOSITY {\} \;" 2>/dev/null ';'\
    -exec sudo umount $DIR ';'

while `sudo umount $DIR 2>/dev/null`
do
    echo -n ""
done

rm -rf $DIR