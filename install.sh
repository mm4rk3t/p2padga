#!/bin/bash
if [ $UID -ne 0 ]
then
	echo "you must run as root"
	exit
fi

target_dir=/usr/local/bin
target_file=p2padga
target_path=${target_dir}/${target_file}
binary_dir=bin/
binary_path=${binary_dir}/${target_file}

if [ -f "${binary_path}" ]; then
	sudo cp ${binary_path} ${target_dir}
	echo "${target_file} has been copied into ${target_dir}"
else
	echo "can't find the binary, be sure to run 'make' first!"
	exit
fi
