# Run docker in realm world on FVP && clip kernel image

## Run Docker in Realm World on FVP 

### 利用Arm官方提供的文档在FVP中构建基本的RME stack

[Arm官方build RME stack相关文档](https://learn.arm.com/learning-paths/servers-and-cloud-computing/rme-cca-basics/rme-cca-fvp/)

### 在此基础之上构建可在Realm World中运行docker的stack

```
为了能够成功运行docker，有以下几点必须要做到

- 整个系统的启动引导需要从busybox替换为systemd，docker要求如此
- 需要让/lib/modules/`uname -r` 目录下有对应的内核模块内容
    - 在buildroot中需要选定kernel这个选项，然后custom git repository，这里可以指定为本机上的git目录，可以用绝对路径；随后需要指定git hash以及相应的config文件，config文件也可以用绝对路径
        - 这里有一个点需要注意，由于安装内核模块必须要求这些模块使用的source代码与kernel的uname -r完全一致，buildroot编译出来的这些模块不会带有git信息，然而编译内核时默认会带有相关git信息（也许可以修改关掉），因此在编译内核之前需要先将.git目录进行重命名，编译完内核后再改回来
- 在编译之前先把buildroot替换到一个新的版本，可以用官网最新的stable版本，CCA官方软件栈中的buildroot版本太老，有一些奇怪的问题
- 将build-scripts/build-buildroot.sh文件中的e2fsck以及resize2fs这两行代码注释掉，因为CCA官方给出的本机的dockerfile中的下载源拿到的e2fsprogs版本太老，不适用于用新buildroot编译出来的镜像。
```

```bash
cd ~/cca_stack
cp $(addition_files/custom.sh) .
cp $(addition_files/check-config.sh) .
vim custom.sh   # 修改该文件内关于check-config.sh的绝对路径
# 以上操作用于将check-config.sh文件复制到根目录中，此脚本文件来自docker开源仓库，用于检测当前环境是否具有运行docker所需要的必要支持


cd build-scripts
git apply $(addition_files/build-scripts.patch)
# 以上操作是因为arm官方提供的docker镜像的下载源对应的e2fsck以及resize2fs对应的软件版本太老，对于新版本的buildroot不适用，同时由于这两步操作意义不大，这里选择直接将其注释掉
# 同时，为了保证linux编译得到的内核镜像不含git信息，在build-linux.sh中进行了一点修改

cd ..
wget https://buildroot.org/downloads/buildroot-2024.11.1.tar.gz
# 将其解压并替换掉本目录中的buildroot文件夹

cd buildroot
cp $(addition_files/cmd_before_build.sh) . # 此文件需要在每次修改完buildroot的.config文件后执行一遍，主要原因是用于迎合arm原本的脚本文件的逻辑
cp $(addition_files/buildroot_config) ./.config
make ARCH=arm64 menuconfig
# BR2_ROOTFS_POST_BUILD_SCRIPT="/home/yishan_li/cca-stack/custom.sh"
# BR2_LINUX_KERNEL_CUSTOM_REPO_URL="/home/yishan_li/cca-stack/linux"
# BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="/home/yishan_li/cca-stack/linux/.config"
# 将以上三行config对应的文件的绝对路径进行相应的修改
./cmd_before_build.sh
cd $(dir_of_whole_project)
```


### 在Realm World中运行docker

```
脚本使用方法
./build-scripts/aemfvp-a-rme/build-test-buildroot.sh -p aemfvp-a-rme all # 清理当前的编译内容并从头开始一起编译以及output package的工作

./build-scripts/aemfvp-a-rme/build-test-buildroot.sh -p aemfvp-a-rme build # 在当前的基础上build（快）
./build-scripts/aemfvp-a-rme/build-test-buildroot.sh -p aemfvp-a-rme package # build结束之后执行这个，用于将各项编译结果复制到指定的位置

export MODEL=~/cca-stack/Base_RevC_AEMvA_pkg/models/Linux64_GCC-9.3/FVP_Base_RevC-2xAEMvA
./model-scripts/aemfvp-a-rme/boot.sh -p aemfvp-a-rme shell
```

运行FVP进入normal world之后，可以在主机 Linux 提示符下使用 `kvmtool` 在 Realm 中启动 guest VM。**Realm 的内核镜像和文件系统 `realm-fs.ext4` 被打包到buildroot 主机文件系统中。**

```bash
lkvm run --realm -c 2 -m 256 -k /realm/Image -d /realm/realm-fs.ext4 -p earlycon
```

您应该看到 guest Linux 内核开始在 Realm 中启动。此步骤可能需要几分钟时间。

在此期间，您应该在 RMM 控制台 `terminal_3` 上看到输出消息，表明正在创建并激活 Realm。

```output
SMC_RMM_REC_CREATE            88232d000 8817b2000 88231a000 > RMI_SUCCESS
SMC_RMM_REALM_ACTIVATE        8817b2000 > RMI_SUCCESS
```

启动后，系统将提示您在 guest Linux buildroot 提示符处登录。再次使用 root 作为用户名和密码。

您已使用 Arm CCA 参考软件堆栈在 Realm 中成功创建了虚拟 guest。docker在systemd作为init进程的系统中会在开机时自动开始运行，因此不需要显式地启动docker

```
# docker --version
Docker version 27.3.1, build 27.3.1
```

以上输出表明docker已成功安装到了realm world中

```
# systemctl status docker
● docker.service - Docker Application Container Engine
     Loaded: loaded (/usr/lib/systemd/system/docker.service; enabled; preset: en
     Active: active (running) since Tue 2024-10-08 16:50:04 UTC; 1 day 22h ago
 Invocation: 902a8916d5b34ca285a1054ab44fc89f
TriggeredBy: ● docker.socket
       Docs: https://docs.docker.com
   Main PID: 199 (dockerd)
      Tasks: 9
     Memory: 40.3M (peak: 65.8M)
        CPU: 8min 17.476s
     CGroup: /system.slice/docker.service
             └─199 /usr/bin/dockerd -H fd:// --containerd=/run/containerd/co

Oct 08 16:49:58 buildroot dockerd[199]: time="2024-10-08T16:49:58.819147930Z" le
Oct 08 16:49:58 buildroot dockerd[199]: time="2024-10-08T16:49:58.945431390Z" le
Oct 08 16:50:00 buildroot dockerd[199]: time="2024-10-08T16:50:00.860524640Z" le
Oct 08 16:50:02 buildroot dockerd[199]: time="2024-10-08T16:50:02.494608660Z" le
Oct 08 16:50:02 buildroot dockerd[199]: time="2024-10-08T16:50:02.938713370Z" le
Oct 08 16:50:02 buildroot dockerd[199]: time="2024-10-08T16:50:02.939164450Z" le
Oct 08 16:50:02 buildroot dockerd[199]: time="2024-10-08T16:50:02.941048590Z" le
Oct 08 16:50:02 buildroot dockerd[199]: time="2024-10-08T16:50:02.944343810Z" le
Oct 08 16:50:04 buildroot systemd[1]: Started Docker Application Container Engin
Oct 08 16:50:04 buildroot dockerd[199]: time="2024-10-08T16:50:04.364990720Z" le
```

以上输出表明docker已成功在realm world中运行

```
需要注意，当前配置下，realm world中的虚拟机无法连接互联网，因此如果需要用docker运行容器，需要事先将镜像放到根文件系统中或是在normal world中下载镜像并将对应目录挂载到realm world中；后续会研究能否通过相应配置让realm world中的虚拟机能够连接互联网
```

## Modify Kernel Config to Reduce Image Size

```bash
docker start $(container_name)
docker exec -it $(container_name) /bin/bash
cd ~/cca_stack
cd linux
make ARCH=arm64 menuconfig
```

通过menuconfig生成的交互界面对kernel的相关配置进行修改，将realm world运行docker时所不需要的配置以及驱动支持等从内核中移除，从而减小内核大小。

在realm world中，通过脚本check_config.sh，可用来检测在内核修改之后是否仍然满足docker的运行条件。  

### 内核裁剪实例

依照[Arm官方build RME stack相关文档](https://learn.arm.com/learning-paths/servers-and-cloud-computing/rme-cca-basics/rme-cca-fvp/), 在完成第一次build之后，得到的内核镜像的大小约为40MB。

依照上面的方法，在进入menuconfig界面之后，移除了realm world虚拟机运行docker不需要使用的`LED support`, `Trusted Execution Environment support`, `FPGA Configuration Framework`, `SoundWire support`, `Sound card support`, `Multimedia support`等设备驱动以及`ARM Scalable Vector Extension support`, `ARMv8.1--8.7 all kernel features except 8.2 persistent memory`等内核特性之后，最终编译得到的内核镜像变为39MB

### 内核裁剪注意事项

[Arm官方build RME stack相关文档](https://learn.arm.com/learning-paths/servers-and-cloud-computing/rme-cca-basics/rme-cca-fvp/)中normal world以及realm world所使用的内核镜像为同一个，这在验证RME软件栈时确实是一个方便的选择，但是对于我们的需求，即得到一个尽可能小的可以在realm world中运行docker的内核，并不是一个理想的方案。例如normal world所使用的内核需要具有各种为虚拟化服务的功能支持，而realm world中的内核并不需要这一功能，因此合理的方案应该是对normal world所使用的内核不做干涉，而只对realm world所使用的内核进行裁剪工作。

要做到这一点，具体可通过修改`build-scripts/build-buildroot.sh`中的`cp $LINUX_PATH/arch/arm64/boot/Image ./tmp_realm_overlay/realm/.`一行改变realm world虚拟机所使用的内核镜像

# Appendix

Author Name: 孙涌豪，龙旭东，王杰

Affiliation：Trusted System Lab，School of Cyber Science and Engineering, Huazhong University of Science and Technology
