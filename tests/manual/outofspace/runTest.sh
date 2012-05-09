#!/bin/bash
#############################################################################
##
## Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
## Contact: http://www.qt-project.org/
##
## This file is part of the QtAddOn.Jsondb module of the Qt Toolkit.
##
## $QT_BEGIN_LICENSE:LGPL$
## GNU Lesser General Public License Usage
## This file may be used under the terms of the GNU Lesser General Public
## License version 2.1 as published by the Free Software Foundation and
## appearing in the file LICENSE.LGPL included in the packaging of this
## file. Please review the following information to ensure the GNU Lesser
## General Public License version 2.1 requirements will be met:
## http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
##
## In addition, as a special exception, Nokia gives you certain additional
## rights. These rights are described in the Nokia Qt LGPL Exception
## version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
##
## GNU General Public License Usage
## Alternatively, this file may be used under the terms of the GNU General
## Public License version 3.0 as published by the Free Software Foundation
## and appearing in the file LICENSE.GPL included in the packaging of this
## file. Please review the following information to ensure the GNU General
## Public License version 3.0 requirements will be met:
## http://www.gnu.org/copyleft/gpl.html.
##
## Other Usage
## Alternatively, this file may be used in accordance with the terms and
## conditions contained in a signed written agreement between you and Nokia.
##
##
##
##
##
##
## $QT_END_LICENSE$
##
#############################################################################

# We assume that both jsondb and jsondb-client are on your PATH.
# FS_PATH points to the mount point of the attached file system image.
# The default mount point is a directory inside this directoy.
export FS_PATH=mountpoint
echo "Using $FS_PATH as mounting point"
LOCAL_DIR=`pwd`

# Create the mount point
if [ -e $FS_PATH && -d $FS_PATH ]
then
    echo "$FS_PATH already exists, using it!"
else
    echo "Creating $FS_PATH"
    mkdir -p $FS_PATH
fi

# Create the image
echo "Creating the image"
dd if=/dev/zero of=fsimage.img bs=1M count=2
sudo /sbin/mkfs.ext2 fsimage.img

# Create a weigth
echo "Creating a 100k weight"
dd if=/dev/zero of=100k bs=100k count=1

# Mount the image
echo "Mounting the image"
sudo /bin/mount -t ext2 fsimage.img $FS_PATH
sudo chown $USER $FS_PATH

# Populate the image with a few files
echo "Prepopulating the FS image"
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17
do
    cp 100k $FS_PATH/$i
done

# Start the database
echo "Starting the DB"
cd $FS_PATH
jsondb &
cd $LOCAL_DIR
sleep 10

# Populate the database and fill the available space
echo "Creating items on the DB"
./create.sh 75

# Check that the db is still responding to queries
echo "Issuing queries to test if the DB is responding"
./query.sh 1

# Free some space
echo "Freeing up space"
sudo rm -f $FS_PATH/1

# See if the DB recovered
echo "Creating more items to check if the DB recovered"
./create.sh 5

# Stop the DB
echo "Stopping the DB"
sudo killall jsondb

# Clean the image
echo "Cleaning up the image"
rm -f $FS_PATH/*.db
for i in 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17
do
    rm -f $i
done

# Unmount the image
echo "Unmounting the image"
sleep 5
sudo /bin/umount $FS_PATH
rm -f fsimage.img
rm -f 100k
rm -rf $FS_PATH

echo "Done"

