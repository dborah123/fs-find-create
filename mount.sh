#!/bin/sh
sudo newfs -U /dev/ada1
sudo mount /dev/ada1 ../mnt

sudo mkdir ../mnt/dir1
sudo mkdir ../mnt/dir2
sudo mkdir ../mnt/dir3
sudo mkdir ../mnt/dir3
sudo mkdir ../mnt/dir1/dir11
sudo mkdir ../mnt/dir1/dir111

sudo chmod -R 777 ../mnt
sudo chmod 777 ../mnt/dir1
sudo touch ../mnt/dir1/file1
sudo touch ../mnt/dir2/file2

sudo chmod 777 ../mnt/dir1/file1
sudo chmod 777 ../mnt/dir2/file2
sudo dd if=/dev/urandom of=../mnt/dir2/file2 bs=1M count=1
sudo echo hello > ../mnt/dir1/file1
