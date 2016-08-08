make clean
make
modprobe uio
insmod memflex.ko
swapon /dev/mapper/ubuntu--vg-swap_1
