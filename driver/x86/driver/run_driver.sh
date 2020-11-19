#!/bin/bash
NUM=$1
echo "run $NUM times"
MODULE_NAME=bmsophon

for((i=1;i<=$NUM;i++));
do
echo "install driver the $i time, total $NUM times";
sudo insmod ../out/bm1684_x86_pcie_device/${MODULE_NAME}.ko;
sleep 0.1 ;
echo "remove driver";
sudo rmmod ${MODULE_NAME};
sleep 0.1;
done
