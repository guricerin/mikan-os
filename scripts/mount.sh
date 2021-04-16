#!/bin/bash
set -eux

if [ $# -lt 2 ] 
then
    echo "Usage: $0 <MOUNT_PATH> <DISK_IMG_PATH>"
    exit 1
fi

mkdir -p $1
sudo mount -o loop $2 $1
