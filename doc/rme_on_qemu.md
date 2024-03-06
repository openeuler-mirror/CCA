# Building an RME Stack for QEMU
Arm机密计算架构是Arm在2021年中发布的针对下一代机密计算需求的参考软件实现。实现完整的Arm CCA需要硬件支持Realm Management Extension，即RME扩展指令。
目前扔未有实际的硬件设备提供RME扩展指令支持。但基于丰富软件生态的原则，业界分别出现了两种针对Arm CCA的软件模拟，通过软件方式来实现RME特性，并基于软件模拟器环境提供更上层的开发。
主流的方案包括Arm公司发布的AEM-FVP, RD-Fremont FVP模拟器，以及通用的开源方案Qemu。

本文主要聚焦于如何在Qemu Emulator环境中，完整编译并运行Arm机密计算架构（CCA）的软件栈。根据编译模式不同，可以分为：
- 利用既有的OP-TEE编译环境进行自动化编译
- 完整手动编译所有CCA需要的组件
接下来会针对两部分进行分别介绍。

## 利用OP-TEE编译环境自动化编译CCA软件栈
### 编译环境和依赖
编译环境为：
- Ubuntu 22.04 + X86_64

其他操作系统会在后续进行测试。需要安装如下依赖包
```
repo 

python3-pyelftools, python3-venv

acpica-tools

openssl (debian libssl-dev)

libglib2.0-dev, libpixman-1-dev

dtc (debian device-tree-compiler)

flex, bison

make, cmake, ninja (debian ninja-build), curl, rsync
```
### 编译
运行如下命令，即可进行编译
```
mkdir v1.0-eac5
cd v1.0-eac5
repo init -u https://git.codelinaro.org/linaro/dcap/op-tee/manifest.git -b v1.0-eac5
 -m qemu_v8_cca.xml
repo sync -j8 --no-clone-bundle
cd build
make -j8 CCA_SUPPORT=y toolchains
make -j8 CCA_SUPPORT=y
```
请注意repo命令拉取的branch为v1.0-eac5，配置文件为qemu_v8_cca.xml。编译完成后，images存储在v1.0-eac5/out/和v1.0-eac5/out-br/文件夹中。所有设计的组件，包括支持RME的linux内核，支持RME能力的Qemu emulator，基于open-embeded生成的rootfs，EDK2，TF-A，TF-RMM均为进行编译。

### 启动Host
启动带有RME能力的的Qemu模拟器：
```
make CCA_SUPPORT=y run-only
```
该命令会启动配置4个命令行终端，分别运行Firmware，Host，Secure和Realm。启动后首先会在firmware界面看到输出，紧接着在Host终端能看到Linux的启动信息。
启动命令完成后，输入`root`进入终端。上面的启动命令会将v1.0-eac5通过9p文件系统mount到Qemu的emulator中。需要注意，此处我们模拟的是RME的Host，在x86_64环境中，Qemu-system-aarch64的TCG模式启动。

## 启动Realm功能验证
Qemu命令行中的`x-rme=on`使能了CPU能力FEAT_RME。在Host kernel的log中，可以通过验证KVM与RMM的交互信息来确定该Host具有启动Realm能力：
```
[    0.893261] kvm [1]: Using prototype RMM support (version 66.0)
```
需要注意，当前的模拟系统内存被设置为8G RAM，目前足够来进行CCA功能的模拟和开发测试。如果选择不同的RAM大小，需要引入对TF-A和RMM的改动。

## Launch Realm Guest
mount共享文件夹
```
mount -t 9p shr0 /mnt
```
使用如下脚本启动Realm Guest：
```
#!/bin/sh

USE_VIRTCONSOLE=true
USE_EDK2=false
USE_INITRD=true
DIRECT_KERNEL_BOOT=true
USE_OPTEE_BUILD=true
VM_MEMORY=512M

if $USE_OPTEE_BUILD; then
    KERNEL=/mnt/out/bin/Image
    INITRD=/mnt/out-br/images/rootfs.cpio
    EDK2=TODO
    DISK=TODO
else
    # Manual method:
    KERNEL=/mnt/linux/arch/arm64/boot/Image
    INITRD=/mnt/buildroot/output/images/rootfs.cpio
    EDK2=/mnt/edk2/Build/ArmVirtQemu-AARCH64/DEBUG_GCC5/FV/QEMU_EFI.fd
    DISK=/mnt/buildroot/output/images/disk.img
fi

add_qemu_arg () {
    QEMU_ARGS="$QEMU_ARGS $@"
}
add_kernel_arg () {
    KERNEL_ARGS="$KERNEL_ARGS $@"
}

add_qemu_arg -M virt,acpi=off,gic-version=3 -cpu host -enable-kvm
add_qemu_arg -smp 2 -m $VM_MEMORY -overcommit mem-lock=on
add_qemu_arg -M confidential-guest-support=rme0
add_qemu_arg -object rme-guest,id=rme0,measurement-algo=sha512,num-pmu-counters=6,sve-vector-length=256
add_qemu_arg -device virtio-net-pci,netdev=net0,romfile=""
add_qemu_arg -netdev user,id=net0

if $USE_VIRTCONSOLE; then
    add_kernel_arg console=hvc0
    add_qemu_arg -nodefaults
    add_qemu_arg -chardev stdio,mux=on,id=hvc0,signal=off
    add_qemu_arg -device virtio-serial-pci -device virtconsole,chardev=hvc0
else
    add_kernel_arg console=ttyAMA0 earlycon
    add_qemu_arg -nographic
fi

if $USE_EDK2; then
    add_qemu_arg -bios $EDK2
fi

if $DIRECT_KERNEL_BOOT; then
    add_qemu_arg -kernel $KERNEL
else
    $USE_INITRD && echo "Initrd requires direct kernel boot" && exit 1
fi

if $USE_INITRD; then
    add_qemu_arg -initrd $INITRD
else
    add_qemu_arg -device virtio-blk-pci,drive=rootfs0
    add_qemu_arg -drive format=raw,if=none,file="$DISK",id=rootfs0
    add_kernel_arg root=/dev/vda2
fi

$USE_EDK2 && $USE_VIRTCONSOLE && ! $USE_INITRD && \
    echo "Don't forget to add console=hvc0 to grub.cfg"

if $DIRECT_KERNEL_BOOT; then
    set -x
    qemu-system-aarch64 $QEMU_ARGS  \
        -append "$KERNEL_ARGS"      \
            </dev/hvc1 >/dev/hvc1
else
    set -x
    qemu-system-aarch64 $QEMU_ARGS  \
            </dev/hvc1 >/dev/hvc1
fi
```
参数 `-M confidential-guest-support=rme0`和`-object rme-guest,id=rme0,measurement-algo=sha512,num-pmu-counters=6,sve-vector-length=256`参数声明这是一个Realm VM，同时配置特定参数。
其中，`-M confidential-guest-support=rme0`和`-object rme-guest,id=rme0`为必要参数，measurement-algo，num-pmu-counters和sve-vector-length如果不指定，Qemu-kvm会自动检测Host信息并填充，如果指定则会覆盖Host中检测到的信息。同时需要注意，Realm Guest的launch过程仅仅是试验性支持，未来SVE和PMU的参数指定位置和方式可能会发生改变。

运行如上脚本，可以在Firmware的终端中看到RMM的log输出。
```
# RMI (Realm Management Interface) is the protocol that host uses to
# communicate with the RMM
SMC_RMM_REC_CREATE            45659000 456ad000 446b1000 > RMI_SUCCESS
SMC_RMM_REALM_ACTIVATE        45659000 > RMI_SUCCESS

# RSI (Realm Service Interface) is the protocol that the guest uses to
# communicate with the RMM
SMC_RSI_ABI_VERSION           > d0000
SMC_RSI_REALM_CONFIG          41afe000 > RSI_SUCCESS
SMC_RSI_IPA_STATE_SET         40000000 60000000 1 0 > RSI_SUCCESS 60000000
```
紧接着在几分钟之后可以关注到Realm的终端有Linux的boot信息产生，最终boot进入Guest Linux环境。

也可以通过kvm-tools进行启动，命令如下：
```
lkvm run --realm -c 2 -m 2G -k /mnt/out/bin/Image -d /mnt/out-br/images/rootfs.ext4 -p "console=hvc0 root=/dev/vda" < /dev/hvc1 > /dev/hvc1
```

## Reference
[Building an RME stack for QEMU](https://linaro.atlassian.net/wiki/x/A4CTwwY)
