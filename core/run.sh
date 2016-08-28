swapoff -a
rmmod memflex.ko
make clean
make
modprobe uio
insmod memflex.ko
swapon /dev/vda5
swapon /home/swapfile
