#!/usr/bin/bash
#
# Find all .adf Amiga disk images, mount them temporarily and search their content for valid ILBM images.
#

DIR=/tmp/adf_mount
VERBOSITY=-

mkdir -p $DIR

find "$1" -name *.adf \
    -exec sudo mount -t affs -o loop "{}" $DIR ';'\
    -exec echo "  * {}" ';'\
    -exec sudo chmod a+r $DIR ';'\
    -exec sh -c "find $DIR -exec ./ilbm_cli $VERBOSITY {\} \;" 2>/dev/null ';'\
    -exec sudo umount $DIR ';'

while `sudo umount $DIR 2>/dev/null`
do
    echo -n ""
done

rm -rf $DIR