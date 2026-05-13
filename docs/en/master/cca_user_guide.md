# CCA Usage Guide

This document describes how to set up a virtual CCA environment using QEMU in openEuler, run a CCA VM, and generate an attestation report.

## Environment

Physical machine processor architecture: AArch64 
Physical machine OS: openEuler 25.09 
VM kernel version (for the host and guest OSs): Linux 6.6.0-102.0.0.5.oe2509.aarch64

## Installing Dependencies

Command line terminal tool: MobaXterm Professional Edition v10.5 or later is recommended.

Initialize the repo tool.

```sh
curl -L https://mirrors.tuna.tsinghua.edu.cn/git/git-repo -o repo
chmod +x repo
```

Initialize the rpmbuild tool.

```sh
yum install rpm-build
mkdir -p /root/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
```

Install the dependency packages.

```sh
pip install cryptography -i https://pypi.tuna.tsinghua.edu.cn/simple
yum install python3-pyelftools
yum install acpica-tools
yum install cmake
yum install elfutils-libelf-devel dwarves
yum install xterm xorg-x11-xauth
yum install go
```

Create a file named `~/.wgetrc` file to disable the Wget certificate verification. The file content is as follows:

```sh
check_certificate = off
```

## Initializing the QEMU Virtualization Environment

VM model: virt Machine

### Initializing the repo Repository

```sh
mkdir ~/cca
cd ~/cca
repo init -u https://git.codelinaro.org/linaro/dcap/op-tee-4.2.0/manifest.git -b cca/v8 -m qemu_v8_cca.xml --repo-url https://mirrors.tuna.tsinghua.edu.cn/git/git-repo
repo sync -j8 --no-clone-bundle
cd build
make -j8 toolchains
make -j8
```

#### Common Compilation Errors

- Error: PHDR segment not covered by LOAD segment 
Solution: Modify `~/cca/build/Makefile`.

```sh
...
################################################################################
# ARM Trusted Firmware
################################################################################
TF_A_EXPORTS ?= \
        CROSS_COMPILE="$(AARCH64_CROSS_COMPILE)" \
        LD="$(CCACHE)$(AARCH64_CROSS_COMPILE)ld" # Add this line.
...
```

- Wget download failure 
For example, the following command fails:

```sh
wget -nd -t 3 -O '~/cca/out-aarch64/build/.gcc-14.2.0.tar.xz.rBexNn/output' 'http://ftpmirror.gnu.org/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz'
```

Solution: Manually download `gcc-14.2.0.tar.xz` to `~/cca/buildroot/dl/gcc/`.

### Starting the Host

```sh
cd ~/cca/build
make run-only
```

Four terminal windows are displayed: FirmWare, Host (corresponding to the host OS), Secure, and Realm (corresponding to the guest OS).

### Configuring the Source Code

The following uses openEuler 25.09 as an example. Click [here](https://dl-cdn.openeuler.openatom.cn/) to download the RPM package.

#### Creating a Kernel Source Code Directory

Obtain the kernel source code.

```sh
rpm -ivh kernel-6.6.0-102.0.0.5.oe2509.src.rpm
rpmbuild -bp /root/rpmbuild/SPECS/kernel.spec --nodeps
```

Copy the kernel source code to the `cca` directory.

```sh
mv ~/cca/linux ~/cca/linux-default
cp -r /root/rpmbuild/BUILD/kernel-6.6.0/linux-6.6.0-102.0.0.5.aarch64 ~/cca/linux
```

#### Creating a QEMU Source Code Directory

Obtain the QEMU source code.

```sh
rpm -ivh qemu-8.2.0-44.oe2509.src.rpm
rpmbuild -bp /root/rpmbuild/SPECS/qemu.spec --nodeps
```

Copy the QEMU source code to the `cca` directory.

```sh
cp -r /root/rpmbuild/BUILD/qemu-8.2.0 ~/cca/
```

#### Creating a libvirt Source Code Directory

Obtain the libvirt source code.

```sh
rpm -ivh libvirt-9.10.0-18.oe2509.src.rpm
rpmbuild -bp /root/rpmbuild/SPECS/libvirt.spec --nodeps
```

Copy the libvirt source code to the `cca` directory.

```sh
cp -r /root/rpmbuild/BUILD/libvirt-9.10.0 ~/cca/
```

#### Configuring the Source Code Paths for QEMU and libvirt

```sh
cd ~/cca/out-br
vim local.mk
QEMU_CCA_OVERRIDE_SRCDIR=~/cca/qemu-8.2.0
LIBVIRT_OVERRIDE_SRCDIR=~/cca/libvirt-9.10.0
```

The default Linux source code path is `~/cca/linux`. You do not need to change it.

### Compiling the Kernel, QEMU, and libvirt

#### Compiling the Kernel

```sh
cd ~/cca/linux
make openeuler_defconfig
```

Run `make menuconfig` to enable the following options:

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

Compile and generate the `image` file.

```sh
make -j Image
```

#### Compiling QEMU

```sh
cd ~/cca/buildroot-external-cca
```

Apply the following patches:

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

Build qemu-cca.

```sh
cd ~/cca/buildroot
make qemu-cca-dirclean O=../out-br
make -j qemu-cca O=../out-br
```

#### Compiling libvirt

```sh
cd ~/cca/buildroot
```

Apply the following patches:

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

Generate the CCA configuration file.

```sh
make BR2_EXTERNAL=../buildroot-external-cca cca_defconfig
```

Run `make menuconfig` to enable the following options:

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

Run the following command to automatically save the configuration to `~/cca/buildroot-external-cca/configs/cca_defconfig`:

```sh
make savedefconfig
#Rebuild buildroot after adding dependencies.
cd ~/cca/build
make buildroot
cd ~/cca/buildroot
```

#### Common Compilation Errors

- Error: could not found hash linux-6.6.32.tar.xz 
Solution: Manually calculate the SHA256 value of `linux-6.6.32.tar.xz` and add it to the corresponding location in `~/cca/cca-qemu/buildroot/package/linux-headers`.

Build libvirt.

```sh
make libvirt-dirclean O=../out-br
make -j libvirt O=../out-br
```

#### Generating rootfs

```sh
cd ~/cca/build
make buildroot
```

### Starting the Host VM

```sh
cd ~/cca/build
make run-only
```

After the host VM is started, the `~/cca` directory on the physical machine is automatically mounted to the `/mnt` directory on the host OS.

### Starting a Guest VM

#### Creating a VM Configuration File

Example: `realm.xml`

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

Save the `realm.xml` file to the `~/cca` directory.

#### Creating a VM

Configure the libvirtd service in the host OS.

```sh
vim /etc/libvirt/libvirtd.conf
...
#ovs_timeout = 5
listen_tls = 0
log_level = 1
log_outputs = "1:file:/var/log/libvirt/libvirtd.log"
```

Start the libvirtd service.

```sh
mkdir /var/log/libvirt && touch /var/log/libvirt/libvirtd.log
virtlogd &
libvirtd --daemon --listen --config /etc/libvirt/libvirtd.conf &
```

Create and start the VM.

```sh
cd /mnt
virsh define realm.xml
virsh start realm
```

#### Logging In to the VM

Log in to the VM in the host OS (kernel startup will take a long time).

```sh
virsh console realm
```

#### Generating an Attestation Report

Run the following command in the guest OS to generate an attestation report:

```sh
cca-workload-attestation report
```

## Reference Link

[Building an RME stack for QEMU CCA](https://linaro.atlassian.net/wiki/spaces/QEMU/pages/29051027459/Building+an+RME+stack+for+QEMU)
