# CCA 测试

## 添加测试工程kvm-unit-tests-cca

```sh
cd ~/cca/buildroot-external-cca
```

应用如下补丁

```patch
From c30380f043e8d8402de1384d25d0a4ba38c57beb Mon Sep 17 00:00:00 2001
From: Xu Raoqing <xuraoqing@huawei.com>
Date: Wed, 17 Sep 2025 14:12:33 +0800
Subject: [PATCH] add kvm-unit-tests-cca

---
 Config.in                                     |  1 +
 package/kvm-unit-tests-cca/Config.in          | 30 +++++++++
 .../kvm-unit-tests-cca.hash                   |  4 ++
 .../kvm-unit-tests-cca/kvm-unit-tests-cca.mk  | 64 +++++++++++++++++++
 4 files changed, 99 insertions(+)
 create mode 100644 package/kvm-unit-tests-cca/Config.in
 create mode 100644 package/kvm-unit-tests-cca/kvm-unit-tests-cca.hash
 create mode 100644 package/kvm-unit-tests-cca/kvm-unit-tests-cca.mk

diff --git a/Config.in b/Config.in
index aa073dc..fec1613 100644
--- a/Config.in
+++ b/Config.in
@@ -3,3 +3,4 @@ source "$BR2_EXTERNAL_LINARO_CCA_PATH/package/cca-workload-attestation/Config.in
 source "$BR2_EXTERNAL_LINARO_CCA_PATH/package/keybroker-demo/Config.in"
 source "$BR2_EXTERNAL_LINARO_CCA_PATH/package/qemu-cca/Config.in"
 source "$BR2_EXTERNAL_LINARO_CCA_PATH/package/kvmtool-cca/Config.in"
+source "$BR2_EXTERNAL_LINARO_CCA_PATH/package/kvm-unit-tests-cca/Config.in"
diff --git a/package/kvm-unit-tests-cca/Config.in b/package/kvm-unit-tests-cca/Config.in
new file mode 100644
index 0000000..fe9f888
--- /dev/null
+++ b/package/kvm-unit-tests-cca/Config.in
@@ -0,0 +1,30 @@
+config BR2_PACKAGE_KVM_UNIT_TESTS_CCA
+        bool "kvm-unit-tests-cca"
+        depends on BR2_PACKAGE_KVM_UNIT_TESTS
+        # on i386 and x86-64, __builtin_reachable is used, so we need
+        # gcc 4.5 at least. on i386, we use the target gcc, while on
+        # x86-64 we use the host gcc (see .mk file for details)
+        depends on BR2_TOOLCHAIN_GCC_AT_LEAST_4_5 || !BR2_i386
+        depends on BR2_HOSTARCH = "x86_64" || !BR2_x86_64
+        select BR2_HOSTARCH_NEEDS_IA32_COMPILER if BR2_x86_64
+        help
+          kvm-unit-tests-cca is a project as old as KVM. As its name
+          suggests, it's purpose is to provide unit tests for KVM. The
+          unit tests are tiny guest operating systems that generally
+          execute only tens of lines of C and assembler test code in
+          order to obtain its PASS/FAIL result. Unit tests provide KVM
+          and virt hardware functional testing by targeting the
+          features through minimal implementations of their use per
+          the hardware specification. The simplicity of unit tests
+          make them easy to verify they are correct, easy to maintain,
+          and easy to use in timing measurements. Unit tests are also
+          often used for quick and dirty bug reproducers. The
+          reproducers may then be kept as regression tests.  It's
+          strongly encouraged that patches implementing new KVM
+          features are submitted with accompanying unit tests.
+
+          https://gitlab.arm.com/linux-arm/kvm-unit-tests-cca
+
+comment "kvm-unit-tests-cca needs a toolchain w/ gcc >= 4.5"
+        depends on !BR2_TOOLCHAIN_GCC_AT_LEAST_4_5
+
diff --git a/package/kvm-unit-tests-cca/kvm-unit-tests-cca.hash b/package/kvm-unit-tests-cca/kvm-unit-tests-cca.hash
new file mode 100644
index 0000000..87e9757
--- /dev/null
+++ b/package/kvm-unit-tests-cca/kvm-unit-tests-cca.hash
@@ -0,0 +1,4 @@
+# Locally computed
+sha256  88130cc6cce9fe75cffc11813589d444d0688ebce39fcc226165f5a784a476af  kvm-unit-tests-cca-cca-rfc-v1.tar.bz2
+sha256  b3c9ca9e257f2eaae070cf0ccdf8770764f05a947a39a835e633413750a5777b  COPYRIGHT
+sha256  8177f97513213526df2cf6184d8ff986c675afb514d4e68a404010521b880643  LICENSE
diff --git a/package/kvm-unit-tests-cca/kvm-unit-tests-cca.mk b/package/kvm-unit-tests-cca/kvm-unit-tests-cca.mk
new file mode 100644
index 0000000..6c948e4
--- /dev/null
+++ b/package/kvm-unit-tests-cca/kvm-unit-tests-cca.mk
@@ -0,0 +1,64 @@
+################################################################################
+#
+# kvm-unit-tests-cca
+#
+################################################################################
+
+KVM_UNIT_TESTS_CCA_VERSION = cca-rfc-v1
+KVM_UNIT_TESTS_CCA_SOURCE = kvm-unit-tests-cca-$(KVM_UNIT_TESTS_CCA_VERSION).tar.bz2
+KVM_UNIT_TESTS_CCA_SITE = https://gitlab.arm.com/linux-arm/kvm-unit-tests-cca/-/archive/$(KVM_UNIT_TESTS_CCA_VERSION)
+KVM_UNIT_TESTS_CCA_LICENSE = GPL-2.0, LGPL-2.0
+KVM_UNIT_TESTS_CCA_LICENSE_FILES = COPYRIGHT LICENSE
+
+ifeq ($(BR2_aarch64)$(BR2_aarch64_be),y)
+KVM_UNIT_TESTS_CCA_ARCH = aarch64
+else ifeq ($(BR2_arm),y)
+KVM_UNIT_TESTS_CCA_ARCH = arm 
+else ifeq ($(BR2_i386),y)
+KVM_UNIT_TESTS_CCA_ARCH = i386
+else ifeq ($(BR2_powerpc64)$(BR2_powerpc64le),y)
+KVM_UNIT_TESTS_CCA_ARCH = ppc64
+else ifeq ($(BR2_s390x),y)
+KVM_UNIT_TESTS_CCA_ARCH = s390x
+else ifeq ($(BR2_x86_64),y)
+KVM_UNIT_TESTS_CCA_ARCH = x86_64
+endif
+
+ifeq ($(BR2_ENDIAN),"LITTLE")
+KVM_UNIT_TESTS_CCA_ENDIAN = little
+else
+KVM_UNIT_TESTS_CCA_ENDIAN = big 
+endif
+
+KVM_UNIT_TESTS_CCA_CONF_OPTS =\
+        --disable-werror \
+        --arch="$(KVM_UNIT_TESTS_CCA_ARCH)" \
+        --processor="$(GCC_TARGET_CPU)" \
+        --endian="$(KVM_UNIT_TESTS_CCA_ENDIAN)"
+
+# For all architectures but x86-64, we use the target
+# compiler. However, for x86-64, we use the host compiler, as
+# kvm-unit-tests builds 32 bit code, which Buildroot toolchains for
+# x86-64 cannot do.
+ifeq ($(BR2_x86_64),)
+KVM_UNIT_TESTS_CCA_CONF_OPTS += --cross-prefix="$(TARGET_CROSS)"
+endif
+
+define KVM_UNIT_TESTS_CCA_CONFIGURE_CMDS
+        cd $(@D) && ./configure $(KVM_UNIT_TESTS_CCA_CONF_OPTS)
+endef
+
+define KVM_UNIT_TESTS_CCA_BUILD_CMDS
+        $(TARGET_MAKE_ENV) $(MAKE) $(KVM_UNIT_TESTS_CCA_MAKE_OPTS) -C $(@D) \
+                standalone
+endef
+
+define KVM_UNIT_TESTS_CCA_INSTALL_TARGET_CMDS
+        $(TARGET_MAKE_ENV) $(MAKE) $(KVM_UNIT_TESTS_CCA_MAKE_OPTS) -C $(@D) \
+                DESTDIR=$(TARGET_DIR)/usr/share/kvm-unit-tests-cca/ \
+                install
+endef
+
+# Does use configure script but not an autotools one
+$(eval $(generic-package))
+
-- 
2.43.0


```

## 开启测试依赖项

```sh
cd ~/cca/buildroot
make BR2_EXTERNAL=../buildroot-external-cca/ cca_defconfig
make menuconfig
```

开启如下配置：  
BR2_cortex_a55=y  
BR2_PACKAGE_KVM_UNIT_TESTS=y  
BR2_PACKAGE_GLIBC_UTILS=y  
BR2_PACKAGE_COREUTILS=y  
BR2_PACKAGE_KVMTOOL_CCA=y  
BR2_PACKAGE_KVM_UNIT_TESTS_CCA=y  

```sh
make savedefconfig
make glibc-rebuild -j O=../out-br
make kvm-unit-tests-cca -j O=../out-br
```

### 修复QCBOR头文件缺失问题
下载CCA测试仓库

```sh
cd ~/cca
git clone --recursive https://git.gitlab.arm.com/linux-arm/kvm-unit-tests-cca.git -b cca/latest
cd ~/cca/out-br/build/kvm-unit-tests-cca-cca-rfc-v1
rm ./* -rf
cp ~/cca/kvm-unit-tests-cca/* ./ -rf
```

重新编译测试用例

```sh
cd ~/cca/buildroot
make kvm-unit-tests-cca -j O=../out-br
```

## 修改kvmtool-cca宏与内核版本一致

修改：~/cca/out-br/build/kvmtool-cca-cca_2025-04-16/arm/aarch64/include/asm/kvm.h
```c
//define KVM_ARM_VCPU_HAS_EL2_E2H0  8   /* Limit NV support to E2H RES0 */
#define KVM_ARM_VCPU_REC            8   /* VCPU REC state as part of Realm */

```

修改：~/cca/out-br/build/kvmtool-cca-cca_2025-04-16/include/linux/kvm.h

```c
#define KVM_CAP_ARM_RME 300
```

## 修改测试用例配置

修改脚本：~/cca/kvm-unit-tests-cca/arm/run-realm-tests

```patch
diff --git a/arm/run-realm-tests b/arm/run-realm-tests
index b1324cca..0eb1ccba 100755
--- a/arm/run-realm-tests
+++ b/arm/run-realm-tests
@@ -6,8 +6,8 @@
 
 TASKSET=${TASKSET:-taskset}
 LKVM=${LKVM:-lkvm}
-ARGS="--realm --restricted_mem --irqchip=gicv3-its --console=serial --network mode=none --nodefaults --loglevel error"
-PMU_ARGS="--pmu --pmu-counters=8"
+ARGS="--realm --irqchip=gicv3-its --console=serial --network mode=none --nodefaults --loglevel error"
+PMU_ARGS="--pmu --pmu-counters=6"
 
 TESTDIR="."
 while getopts "d:" option; do
```

重新编译

```sh
cd ~/cca/buildroot
make kvm-unit-tests-cca -j O=../out-br

cd ~/cca/build
make buildroot
```

## 执行测试用例

启动Host
```sh
cd ~/cca/build
make run-only
```

在Host中运行测试用例

```sh
cd /mnt/out-br/build/kvm-unit-tests-cca-cca-rfc-v1/arm/
./run-realm-tests
```
