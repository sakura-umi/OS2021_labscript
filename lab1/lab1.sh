#!/bin/bash
echo -e "\033[32m 作者 Pipixia244 项目github https://github.com/pipixia244/OS2021_labscript \033[0m"
echo -e "\033[31m 请在root用户下运行此脚本。确认请按任意键。取消请按Ctrl+C \033[0m"
read cho
echo -e "\033[31m 是否曾经完整运行过此脚本？(Y/N) \033[0m"
read choo
if [ $choo == "N" ] || [ $choo == "n" ]
then
echo -e "\033[32m 正在安装必须的环境组件......  \033[0m"
sudo apt install -y screen xrdp curl wget axel nano vim net-tools ssh git gdb qemu
echo -e "\033[32m 安装完成。  \033[0m"

echo -e "\033[32m 开始配置OS实验lab1环境。。。  \033[0m"
apt install -y build-essential xz-utils libssl-dev bc libncurses5-dev libncursesw5-dev qemu
mkdir oslab
cd oslab
echo -e "\033[32m 开始下载linux源码。。。大小约80M，等待时间可能稍长。  \033[0m"
wget https://od.srpr.cc/acgg0/linux-4.9.263.tar.xz -O linux-4.9.263.tar.xz
tar -xvJf linux-4.9.263.tar.xz
wget https://busybox.net/downloads/busybox-1.32.1.tar.bz2 -O busybox-1.32.1.tar.bz2
tar -jxvf busybox-1.32.1.tar.bz2
cd linux-4.9.263
echo -e "\033[32m 编译参数因人而异可能会占用大量空间。这里直接应用vlab参数，编译1.2G版本(基本不含驱动)。按任意键以继续... \033[0m"
read cho9
wget http://222.186.10.65:8080/directlink/3/.config
echo -e "\033[32m 即将开始编译。按任意键以继续... \033[0m"
read cho1
make -j $((`nproc`-1))
echo -e "\033[32m linux编译完成。  \033[0m"

echo -e "\033[32m 开始编译busybox。按任意键以继续... \033[0m"
cd ../busybox-1.32.1
echo -e "\033[32m 直接应用编译参数。按任意键以继续... \033[0m"
read pre1
wget http://222.186.10.65:8080/directlink/3/.config.busy -O .config
echo -e "\033[32m 即将开始编译。按任意键以继续... \033[0m"
read cho2
make -j $((`nproc`-1))
make install

sudo mkdir _install/dev
sudo mknod _install/dev/console c 5 1
sudo mknod _install/dev/ram b 1 0
touch _install/init
cat <<'TEXT' > _install/init
#!/bin/sh
echo "INIT SCRIPT"
mkdir /proc
mkdir /sys
mount -t proc none /proc
mount -t sysfs none /sys
mkdir /tmp
mount -t tmpfs none /tmp
echo -e "\nThis boot took $(cut -d' ' -f1 /proc/uptime) seconds\n"
exec /bin/sh
TEXT
chmod +x _install/init
cd _install
find . -print0 | cpio --null -ov --format=newc | gzip -9 > ../../initramfs-busybox-x64.cpio.gz
cd ../..
else
cd oslab
fi
echo -e "\033[32m 是否加载qemu虚拟机？(Y/N)  \033[0m"
read chooo
if [ $chooo == "Y" ] || [ $chooo == "y" ]
then
echo -e "\033[32m 是否具有图形桌面环境？(Y/N)  \033[0m"
read cho3
case $cho3 in
	"Y") qemu-system-x86_64 -kernel linux-4.9.263/arch/x86_64/boot/bzImage -initrd initramfs-busybox-x64.cpio.gz --append "root=/dev/ram init=/init"
		;;
	"y") qemu-system-x86_64 -kernel linux-4.9.263/arch/x86_64/boot/bzImage -initrd initramfs-busybox-x64.cpio.gz --append "root=/dev/ram init=/init"
		;;
	"N") qemu-system-x86_64 -kernel linux-4.9.263/arch/x86_64/boot/bzImage -initrd initramfs-busybox-x64.cpio.gz --append "root=/dev/ram init=/init console=ttyS0" -nographic
		;;
	"n") qemu-system-x86_64 -kernel linux-4.9.263/arch/x86_64/boot/bzImage -initrd initramfs-busybox-x64.cpio.gz --append "root=/dev/ram init=/init console=ttyS0" -nographic
		;;
esac
fi
echo -e "\033[32m 配置完成！剩余调试部分需要手动完成。  \033[0m"


