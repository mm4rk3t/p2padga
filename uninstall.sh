#!/bin/bash
if [ $UID -ne 0 ]; then
	echo "you must run as root"
	exit
fi

target_dir=/usr/local/bin
target_file=p2padga
target_path=$target_dir/$target_file

if [ -f "$target_path" ]; then
	sudo rm $target_path
else
	echo "file does not exist"
	exit
fi

echo "${target_file} has been removed from ${target_dir}";
