#!/bin/bash
cse536dir=$HOME/cse536
if [ "$#" -ne 2 ]; then
	echo "Please pass cse536monitor IP address as argument"
	echo "Usage test.sh 192.168.0.14 192.168.140.144"

	echo " Usage test.sh <cse536monitor IP> <Destination IP>"
	exit
fi

if [ -d $cse536dir ];then
	chmod 777 *
	cp -f cse536app.c $cse536dir/cse536app.c
	cp -f cse5361.c $cse536dir/linux-3.13.0/drivers/char/cse536
	cd $cse536dir
else
	
	 for i in `ls -d /home/*/cse536`
	 do
		cse536dir=`echo $i` 
		echo " Using $cse536dir as path to cse536"
		break
	done
	

	if [ -d $cse536dir ];then
		chmod 777 *
		cp -f cse536app.c $cse536dir/cse536app.c
		cp -f cse5361.c $cse536dir/linux-3.13.0/drivers/char/cse536
		cd $cse536dir
	else
		echo "cse536 directory not found in user home directory:$HOME"
		echo "Please provide path to cse536 (ex: /home/nagarjuna/cse536):"
		read cse536dir
		if [ -d $cse536dir ];then
			chmod 777 *
			cp -f cse536app.c $cse536dir/cse536app.c
			cp -f cse5361.c $cse536dir/linux-3.13.0/drivers/char/cse536
			cd $cse536dir
		else
			echo "cse536 directory not found in the given path $cse536dir please rerun"
			exit
		fi
	fi

fi

if [ -d $cse536dir ];then
	if [ -d $cse536dir/linux-3.13.0/drivers/char/cse536 ]; then

		export APP=cse536app
		export DEVICE=/dev/cse5361
		export MODULE=linux-3.13.0/drivers/char/cse536

		cd $cse536dir
		rm -f $APP || true
		gcc $APP.c -o $APP -g

		make -C linux-3.13.0/ M=drivers/char/cse536 modules
		if [ -c /dev/cse5361 ]; then
			echo " No need to create character device one  present"
			else 
			echo "creating $DEVICE"
			sudo mknod $DEVICE c 234 0 || true
			sudo chmod 777 $DEVICE || true
		fi
		sudo rmmod cse5361 || true
		sudo insmod $MODULE/cse5361.ko debug_enable=1 || true
		cd $cse536dir
		clear
		./$APP $1 $2

	else
		echo "missing directory path $cse536dir/linux-3.13.0/drivers/char/cse536"
	fi
else
  echo "cse536 directory not found"
fi

