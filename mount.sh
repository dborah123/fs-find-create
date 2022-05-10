#!/bin/sh
sudo newfs -U /dev/ada1
sudo mount /dev/ada1 ../mnt
sudo mkdir ../mnt/dir1
sudo mkdir ../mnt/dir2
sudo chmod -R 777 ../mnt
sudo chmod 777 ../mnt/dir1
sudo touch ../mnt/dir1/file1
sudo chmod 777 ../mnt/dir1/file1
sudo echo hello > ../mnt/dir1/file1
