# CCA 使用指南

该文档用于描述在openEuler系统下搭建QEMU虚拟CCA环境，并运行CCA虚拟机以及输出证明报告。

## 环境说明

物理机处理器架构：aarch64  
物理机操作系统：openEuler-25.09  
虚拟机kernel版本（Host及Guest）：Linux 6.6.0-102.0.0.5.oe2509.aarch64

## 安装依赖

命令行终端工具：建议使用 MobaXterm Professional Edition v10.5及更高版本

repo工具初始化：

```sh
curl -L https://mirrors.tuna.tsinghua.edu.cn/git/git-repo -o repo
chmod +x repo
```

rpmbuild工具初始化：

```sh
yum install rpm-build
mkdir -p /root/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
```

安装依赖包：

```sh
pip install cryptography -i https://pypi.tuna.tsinghua.edu.cn/simple
yum install python3-pyelftools
yum install acpica-tools
yum install cmake
yum install elfutils-libelf-devel dwarves
yum install xterm xorg-x11-xauth
yum install go
```

关闭wget证书验证，创建文件~/.wgetrc，内容如下：

```sh
check_certificate = off
```

## QEMU虚拟环境初始化

虚拟机模型：virt Machine

### 初始化repo仓库

```sh
mkdir ~/cca
cd ~/cca
repo init -u https://git.codelinaro.org/linaro/dcap/op-tee-4.2.0/manifest.git -b cca/v9 -m qemu_v8_cca.xml --repo-url https://mirrors.tuna.tsinghua.edu.cn/git/git-repo
repo sync -j8 --no-clone-bundle
cd build
make -j8 toolchains
make -j8
```

#### 常见编译错误

- 错误：PHDR segment not covered by LOAD segment  
解决方法：修改 ~/cca/build/Makefile

```sh
...
################################################################################
# ARM Trusted Firmware
################################################################################
TF_A_EXPORTS ?= \
        CROSS_COMPILE="$(AARCH64_CROSS_COMPILE)" \
        LD="$(CCACHE)$(AARCH64_CROSS_COMPILE)ld" #添加该行
...
```

- wget下载失败  
例如如下命令失败

```sh
wget -nd -t 3 -O '~/cca/out-aarch64/build/.gcc-14.2.0.tar.xz.rBexNn/output' 'http://ftpmirror.gnu.org/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz'
```

解决方法：手动下载gcc-14.2.0.tar.xz到~/cca/buildroot/dl/gcc/

### 启动Host机器

```sh
cd ~/cca/build
make run-only
```

正常执行可出现4个终端窗口：FirmWare，Host（对应Host OS），Secure，Realm（对应Guest OS）

### 配置源码

以下版本以openEuler-25.09为例。单击[此处](https://dl-cdn.openeuler.openatom.cn/)下载rpm包。

#### 创建kernel源码目录

获取kernel源码

```sh
rpm -ivh kernel-6.6.0-102.0.0.5.oe2509.src.rpm
rpmbuild -bp /root/rpmbuild/SPECS/kernel.spec --nodeps
```

拷贝kernel源码到cca目录下

```sh
mv ~/cca/linux ~/cca/linux-default
cp -r /root/rpmbuild/BUILD/kernel-6.6.0/linux-6.6.0-102.0.0.5.aarch64 ~/cca/linux
```

#### 创建QEMU源码目录

获取QEMU源码

```sh
rpm -ivh qemu-8.2.0-44.oe2509.src.rpm
rpmbuild -bp /root/rpmbuild/SPECS/qemu.spec --nodeps
```

拷贝QEMU源码到cca目录下

```sh
cp -r /root/rpmbuild/BUILD/qemu-8.2.0 ~/cca/
```

#### 创建libvirt源码目录

获取libvirt源码

```sh
rpm -ivh libvirt-9.10.0-18.oe2509.src.rpm
rpmbuild -bp /root/rpmbuild/SPECS/libvirt.spec --nodeps
```

拷贝libvirt源码到cca目录下

```sh
cp -r /root/rpmbuild/BUILD/libvirt-9.10.0 ~/cca/
```

#### 配置QEMU和libvirt源码路径

```sh
cd ~/cca/out-br
vim local.mk
QEMU_CCA_OVERRIDE_SRCDIR=~/cca/qemu-8.2.0
LIBVIRT_OVERRIDE_SRCDIR=~/cca/libvirt-9.10.0
```

Linux 源码路径默认使用~/cca/linux，无需更改。

### 编译kernel、QEMU和libvirt

#### 编译kernel

```sh
cd ~/cca/linux
make openeuler_defconfig
```

使用make menuconfig开启如下选项：

```sh
CONFIG_NET_9P=y
CONFIG_VIRTIO_CONSOLE=y
CONFIG_NET_9p_VIRTIO=Y
CONFIG_NET_FAILOVER=y
CONFIG_VIRTIO_BLK=y
CONFIG_SCSI_VIRTIO=y
CONFIG_MACVLAN=Y
CONFIG_MACVTAP=Y
CONFIG_VIRTIO_NET=y
CONFIG_VIRTIO_PCI=y
CONFIG_VIRTIO_MMIO=y
CONFIG_EXT4_FS=y
CONFIG_NETFS_SUPPORT=y
CONFIG_9P_FS=y
CONFIG_TSM_REPORT=y
CONFIG_ARM_CCA_GUEST=y
```

编译生成Image文件

```sh
make -j Image
```

#### 编译QEMU

```sh
cd ~/cca/buildroot-external-cca
```

应用如下补丁

```sh
From 2d96a8b09ebdb005b5ee5b65a7924245c8fcbdcc Mon Sep 17 00:00:00 2001
From: Xu Raoqing <xuraoqing@huawei.com>
Date: Fri, 12 Sep 2025 15:54:23 +0800
Subject: [PATCH] enable attestation

---
 package/cca-workload-attestation/Config.in    |  4 +--
 .../cca-workload-attestation.mk               | 31 ++++++++++++++++++-
 package/qemu-cca/qemu.mk                      |  2 +-
 3 files changed, 33 insertions(+), 4 deletions(-)

diff --git a/package/cca-workload-attestation/Config.in b/package/cca-workload-attestation/Config.in
index 733ff31..3342498 100644
--- a/package/cca-workload-attestation/Config.in
+++ b/package/cca-workload-attestation/Config.in
@@ -1,7 +1,7 @@
 config BR2_PACKAGE_CCA_WORKLOAD_ATTESTATION
     bool "CCA Workload Attestation PoC"
-   depends on BR2_PACKAGE_HOST_GO_TARGET_ARCH_SUPPORTS
-   depends on BR2_PACKAGE_HOST_GO_TARGET_CGO_LINKING_SUPPORTS
+#  depends on BR2_PACKAGE_HOST_GO_TARGET_ARCH_SUPPORTS
+#  depends on BR2_PACKAGE_HOST_GO_TARGET_CGO_LINKING_SUPPORTS
     help
       A proof-of-concept, self-contained executable that
       demonstrates the use of CCA attestation in a number
diff --git a/package/cca-workload-attestation/cca-workload-attestation.mk b/package/cca-workload-attestation/cca-workload-attestation.mk
index 8edd83f..0547924 100644
--- a/package/cca-workload-attestation/cca-workload-attestation.mk
+++ b/package/cca-workload-attestation/cca-workload-attestation.mk
@@ -17,4 +17,33 @@ CCA_WORKLOAD_ATTESTATION_GOMOD = git.codelinaro.org/linaro/dcap/cca-demos/cca-wo
 #CCA_WORKLOAD_ATTESTATION_LDFLAGS = -s -w
 #CCA_WORKLOAD_ATTESTATION_GO_ENV = GOFLAGS=-mod=mod
 
-$(eval $(golang-package))
+#$(eval $(golang-package))
+
+CCA_WORKLOAD_ATTESTATION_GO_BIN = /usr/bin/go
+ 
+CCA_WORKLOAD_ATTESTATION_GO_ENV = \
+    CGO_ENABLED=0 \
+    GOARCH=arm64 \
+    GOOS=linux \
+    PATH=/usr/bin:$(PATH)
+ 
+CCA_WORKLOAD_ATTESTATION_GO_ENV += \
+    GOPROXY=http://proxy.golang.org,direct \
+    GOSUMDB=off \
+    GOINSECURE=proxy.golang.org
+ 
+define CCA_WORKLOAD_ATTESTATION_BUILD_CMDS
+    cd $(@D) && \
+    $(CCA_WORKLOAD_ATTESTATION_GO_ENV) \
+    $(CCA_WORKLOAD_ATTESTATION_GO_BIN) build -mod=mod -o $(@D)/bin/attestation $(@D)/main.go
+endef
+ 
+define CCA_WORKLOAD_ATTESTATION_INSTALL_TARGET_CMDS
+    $(INSTALL) -D -m 0755 $(@D)/bin/attestation $(TARGET_DIR)/usr/bin/cca-workload-attestation
+endef
+ 
+CCA_WORKLOAD_ATTESTATION_AUTORULES = NO
+ 
+CCA_WORKLOAD_ATTESTATION_DEPENDENCIES = host-pkgconf
+ 
+$(eval $(generic-package))
diff --git a/package/qemu-cca/qemu.mk b/package/qemu-cca/qemu.mk
index 584eb67..dd5d97b 100644
--- a/package/qemu-cca/qemu.mk
+++ b/package/qemu-cca/qemu.mk
@@ -303,7 +303,6 @@ define QEMU_CCA_CONFIGURE_CMDS
             --disable-jack \
             --disable-libiscsi \
             --disable-linux-aio \
-           --disable-linux-io-uring \
             --disable-malloc-trim \
             --disable-membarrier \
             --disable-mpath \
@@ -352,6 +351,7 @@ HOST_QEMU_CCA_DEPENDENCIES = \
     host-python3 \
     host-python-distlib \
     host-slirp \
+   liburing \
     host-zlib
 
 #       BR ARCH         qemu
-- 
2.43.0

```

构建qemu-cca

```sh
cd ~/cca/buildroot
make qemu-cca-dirclean O=../out-br
make -j qemu-cca O=../out-br
```

#### 编译libvirt

```sh
cd ~/cca/buildroot
```

应用如下补丁

```sh
From cf8b402aa96559d113b6d40a2d79c297eef3a5ac Mon Sep 17 00:00:00 2001
From: Xu Raoqing <xuraoqing@huawei.com>
Date: Fri, 12 Sep 2025 15:58:29 +0800
Subject: [PATCH] support qemu cca

---
 package/libvirt/Config.in  | 4 ++--
 package/libvirt/libvirt.mk | 2 +-
 2 files changed, 3 insertions(+), 3 deletions(-)

diff --git a/package/libvirt/Config.in b/package/libvirt/Config.in
index c55ab429..8d79b664 100644
--- a/package/libvirt/Config.in
+++ b/package/libvirt/Config.in
@@ -74,8 +74,8 @@ config BR2_PACKAGE_LIBVIRT_QEMU
     select BR2_PACKAGE_HWDATA         # libpciaccess
     select BR2_PACKAGE_HWDATA_PCI_IDS # libpciaccess
     select BR2_PACKAGE_LIBSECCOMP
-   select BR2_PACKAGE_QEMU
-   select BR2_PACKAGE_QEMU_SYSTEM
+   select BR2_PACKAGE_QEMU_CCA
+   select BR2_PACKAGE_QEMU_CCA_SYSTEM
     select BR2_PACKAGE_YAJL
     help
       QEMU/KVM support
diff --git a/package/libvirt/libvirt.mk b/package/libvirt/libvirt.mk
index 7f2a33e9..9e7cae00 100644
--- a/package/libvirt/libvirt.mk
+++ b/package/libvirt/libvirt.mk
@@ -21,7 +21,7 @@ LIBVIRT_DEPENDENCIES = \
     libpciaccess \
     libtirpc \
     libxml2 \
-   udev \
+   eudev \
     zlib \
     $(TARGET_NLS_DEPENDENCIES)
 
-- 
2.43.0

```

生成cca配置文件

```sh
make BR2_EXTERNAL=../buildroot-external-cca cca_defconfig
```

通过make menuconfig 开启选项如下：

```yaml
Target packages
    System tools
        [*] util-linux
            -*- libblkid
            [*] libmount
            [*] libuuid

System Configuration
    /dev management
        (X)Dynamic using devtmpfs + eudev

Toolchain
    [*] Enable C++ support

Target packages
    Hardware handling
        -*- eudev
    System tools
        [*] libvirt
            [*] libvirtd
            [*] qemu
```

执行如下命令，自动保存到~/cca/buildroot-external-cca/configs/cca_defconfig

```sh
make savedefconfig
```

构建libvirt

```sh
make libvirt-dirclean O=../out-br
make -j libvirt O=../out-br
```

#### 生成rootfs

```sh
cd ~/cca/build
make buildroot
```

### 启动Host虚拟机

```sh
cd ~/cca/build
make run-only
```

Host虚拟机启动后，物理机~/cca目录自动挂载到Host OS下的/mnt目录

### 启动Guest虚拟机

#### 创建虚拟机配置文件

例如：realm.xml

```xml
<domain type='kvm' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>
  <name>realm</name>
  <memory unit='MiB'>2048</memory>
  <vcpu placement='static'>2</vcpu>
  <os>
    <type arch='aarch64' machine='virt'>hvm</type>
    <kernel>/mnt/out/bin/Image</kernel>
    <initrd>/mnt/out-br/images/rootfs.cpio</initrd>
    <cmdline>rodata=full earlycon=pl011,0x10009000000 console=ttyAMA0</cmdline>-->
  </os>
  <launchSecurity type='cca'>
    <measurement-algo>sha256</measurement-algo>
    <personalization-value>ICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgIEknbSBhIHRlYXBvdA==</personalization-value>
    <measurement-log state='off'/>
  </launchSecurity>
  <features>
    <gic version='3' its='on'/>
    <kvm>
      <hidden state='off'/>
    </kvm>
  </features>
  <cpu mode='host-passthrough'/>
  <devices>
    <console type='pty'>
      <target type='serial' port='0'/>
    </console>
  </devices>
  <qemu:commandline>
    <qemu:arg value='-nographic'/>
  </qemu:commandline>
</domain>
```

将realm.xml文件保存到~/cca目录下

#### 创建虚拟机

在Host OS中配置libvirtd服务

```sh
vim /etc/libvirt/libvirtd.conf
...
#ovs_timeout = 5
listen_tls = 0
log_level = 1
log_outputs = "1:file:/var/log/libvirt/libvirtd.log"
```

启动libvirtd服务

```sh
mkdir /var/log/libvirt && touch /var/log/libvirt/libvirtd.log
virtlogd &
libvirtd --daemon --listen --config /etc/libvirt/libvirtd.conf &
```

创建并启动虚拟机

```sh
cd /mnt
virsh define realm.xml
virsh start realm
```

#### 登录虚拟机

Host OS中登录虚拟机（内核启动较耗时）

```sh
virsh console realm
```

#### 输出证明报告

Guest OS中执行命令生成证明报告

```sh
cca-workload-attestation report
```

## 参考链接

[Building an RME stack for QEMU CCA](https://linaro.atlassian.net/wiki/spaces/QEMU/pages/29051027459/Building+an+RME+stack+for+QEMU)
