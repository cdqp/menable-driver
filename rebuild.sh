set -e

cd linux
sudo modprobe -r menable
make clean
make
sudo make install
sudo modprobe menable

