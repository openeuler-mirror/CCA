# 鲲鹏环境下CCA使用指南

该文档用于描述在鲲鹏920新版本环境下，如何创建机密虚机以及使用设备直通能力。

## 环境说明

物理机处理器架构：aarch64  
物理机型号：kunpeng920新版本  
物理机操作系统：openEuler-24.03-LTS-SP3  

kernel版本（Host及Guest）：kernel-6.6.0-142.0.0.127.oe2403sp3.aarch64.rpm  
qemu版本： qemu-8.2.0-71.oe2403sp3.aarch64  
libvirt版本：libvirt-9.10.0-27.oe2403sp3.aarch64  

[镜像下载](http://121.36.84.172/dailybuild/EBS-openEuler-24.03-LTS-SP3/openeuler-2026-03-27-02-11-15/virtual_machine_img/aarch64/)  
[Repo源](http://121.36.84.172/dailybuild/EBS-openEuler-24.03-LTS-SP3/openeuler-2026-03-27-02-11-15/everything/aarch64/)

下文涉及的设备直通功能需要BIOS 导入对应的License，以支持CCA、SMMU和KAE。  

## 设备直通准备

默认根目录：/home/test/

1. 修改Host kernel启动参数

    Host kernel新增如下启动参数：

    ```sh
    vfio_pci.disable_idle_d3=1 # 强制 VFIO 驱动让设备始终保持在 D0（全功能/活跃）状态，避免空闲时进入D3状态，增强设备直通稳定性
    ```

2. 修改Guest kernel启动参数

    先进入普通虚机，增加kernel启动参数:

    ```sh
    rodata=full # 用于将内核的只读数据区域（包括内核模块的只读段）映射为严格的只读内存，保证guest内核安全性
    ```

3. 一份完整的机密虚机xml文件如下：

    ```xml
    <domain type='kvm' xmlns:qemu="http://libvirt.org/schemas/domain/qemu/1.0">
        <name>cvm</name>
        <memory unit='GiB'>8</memory>
        <vcpu placement='static'>4</vcpu>
        <cputune>
            <vcpupin vcpu='0' cpuset='0'/>
            <vcpupin vcpu='1' cpuset='1'/>
            <vcpupin vcpu='2' cpuset='2'/>
            <vcpupin vcpu='3' cpuset='3'/>
            <emulatorpin cpuset='0-3'/>
        </cputune>
        <numatune>
            <memnode cellid='0' mode='strict' nodeset='0'/>
        </numatune>
        <os>
            <type arch='aarch64' machine='virt'>hvm</type>
            <loader readonly='yes' type='rom'>/home/test/cca-da/edk2/Build/ArmVirtQemu-AARCH64/RELEASE_GCC5/FV/QEMU_EFI.fd</loader>
            <boot dev='hd'/>
        </os>
        <features>
            <gic version='3'/>
        </features>
        <cpu mode='host-passthrough'>
            <topology sockets='1' dies='1' clusters='1' cores='4' threads='1'/>
            <numa>
                <cell id='0' cpus='0-3' memory='8' unit='GiB'/>
            </numa>
        </cpu>
        <on_poweroff>destroy</on_poweroff>
        <on_reboot>restart</on_reboot>
        <on_crash>destroy</on_crash>
        <devices>
            <emulator>/usr/bin/qemu-system-aarch64</emulator>
            <console type='pty'/>
            <disk type='file' device='disk' model='virtio-non-transitional'>
                <driver name='qemu' type='qcow2' queues='2' cache='none' iommu='on'/>
                <source file='/home/test/cca-da/openEuler-24.03-LTS-SP3-aarch64.qcow2'/>
                <target dev='vda' bus='virtio'/>
            </disk>
            <interface type='bridge'>
                <source bridge='virbr0'/>
                <driver iommu='on' event_idx='off'/>
                <rom bar='off'/>
                <model type='virtio-non-transitional'/>
            </interface>
        </devices>
    <!-- 相比较普通虚机，机密虚机新增LaunchSecurity配置以及qemu:commandline配置 !-->
    <launchSecurity type='cca' hisi-cca-enable='yes'/>
    <qemu:commandline>
        <qemu:arg value='-object'/>
        <qemu:arg value='tmm-guest,id=tmm0,sve-vector-length=128,num-pmu-counters=1'/>
    </qemu:commandline>
    </domain>
    ```

## 设备直通操作说明

### 机密虚机操作说明

1. 创建并启动机密虚机

    ```sh
    virsh define cvm.xml
    virsh start cvm
    ```

2. 修改配置并重启虚机（后续设备直通时可参考该方式修改配置）

    ```sh
    virsh destroy cvm
    virsh edit cvm
    virsh start cvm
    ```

### SP680网卡PF直通

1. 查看Host侧已有网卡

    ```sh
    [root@server1 ~]# lspci | grep Ethernet | grep Huawei
    28:00.0 Ethernet controller: Huawei Technologies Co., Ltd. Hi1822 Family (rev 21)
    28:00.1 Ethernet controller: Huawei Technologies Co., Ltd. Hi1822 Family (rev 21)
    28:00.2 Ethernet controller: Huawei Technologies Co., Ltd. Hi1822 Family (rev 21)
    28:00.3 Ethernet controller: Huawei Technologies Co., Ltd. Hi1822 Family (rev 21)
    ```

2. 网卡直通

    以28:00.0为例，机密虚机xml文件中的devices元素下，新增如下配置：

    ```xml
    <hostdev mode='subsystem' type='pci' managed='yes'>
        <driver name='vfio'/>
        <source>
            <address domain='0x0000' bus='0x28' slot='0x00' function='0x0'/>
        </source>
    </hostdev>
    ```

3. 机密虚机内查看网卡

    Hi1822网卡直通后，机密虚机内可以看到该设备，如下：

    ```sh
    [root@localhost ~]# lspci -tv
    -[0000:00]-+-00.0  Red Hat, Inc. QEMU PCIe Host bridge
            +-01.0-[01]----00.0  Virtio: Virtio 1.0 network device
            +-01.1-[02]----00.0  Virtio: Virtio 1.0 block device
            +-01.2-[03]----00.0  Huawei Technologies Co., Ltd. Hi1822 Family
            \-01.3-[04]--
    ```

### NVME磁盘PF直通

1. 查看Host侧已有磁盘

    ```sh
    [root@openEuler ~]# lspci -tv | grep 3755
    \-[0000:a0]-+-00.0-[a1]----00.0  Huawei Technologies Co., Ltd. Device 3755
                +-02.0-[a2]----00.0  Huawei Technologies Co., Ltd. Device 3755
                \-06.0-[a4]----00.0  Huawei Technologies Co., Ltd. Device 3755
    ```

2. 磁盘设备直通

    机密虚机xml文件中的devices元素下，新增如下配置：

    ```xml
    <hostdev mode='subsystem' type='pci' managed='yes'>
        <driver name='vfio'/>
        <source>
            <address domain='0x0000' bus='0xa2' slot='0x00' function='0x0'/>
        </source>
    </hostdev>
    ```

3. 机密虚机内查看磁盘设备

    NVME磁盘直通后，机密虚机内可以看到该设备，例如lspci显示的Device 3755即为直通的NVME磁盘设备。

    ```sh
    [root@localhost ~]# lspci -tv
    -[0000:00]+-00.0 Red Hat, Inc. QEMU PCIE Host bridge
    +-01.0-[01]----00.0 Virtio: Virtio 1.0 block device
    +-01.1-[02]----00.0 Huawei Technologies Co., Ltd. Device 3755
    \-01.2-[03]----00.0 Virtio: Virtio 1.0 network device

### 国密硬件KAE PF直通

1. 查看Host已有KAE设备

    ```sh
    [root@server1 ~]# lspci | grep HiSilicon | grep Engine
    09:00.0 Processing accelerators: Huawei Technologies Co., Ltd. HiSilicon ZIP Engine (rev 30)
    0a:00.0 Network and computing encryption device: Huawei Technologies Co., Ltd. HiSilicon HPRE Engine (rev 30)
    0d:00.0 Network and computing encryption device: Huawei Technologies Co., Ltd. HiSilicon SEC Engine (rev 30)
    ```

2. KAE设备直通

    机密虚机xml文件中的devices元素下，新增如下配置：

    ```xml
    <hostdev mode='subsystem' type='pci' managed='yes'>
        <driver name='vfio'/>
        <source>
            <address domain='0x0000' bus='0x09' slot='0x00' function='0x0'/>
        </source>
    </hostdev>
    <hostdev mode='subsystem' type='pci' managed='yes'>
        <driver name='vfio'/>
        <source>
            <address domain='0x0000' bus='0x0a' slot='0x00' function='0x0'/>
        </source>
    </hostdev>
    <hostdev mode='subsystem' type='pci' managed='yes'>
        <driver name='vfio'/>
        <source>
            <address domain='0x0000' bus='0x0d' slot='0x00' function='0x0'/>
        </source>
    </hostdev>
    ```

3. 机密虚机内查看KAE设备

    KAE设备直通后，机密虚机内可以看到该设备，例如lspci显示的HiSilicon ZIP Engine、HiSilicon HPRE Engine、HiSilicon SEC Engine即为直通的KAE设备。

    ```sh
    [root@localhost ~]# lspci -tv
    -[0000:00]-+-00.0  Red Hat, Inc. QEMU PCIe Host bridge
            +-01.0-[01]----00.0  Virtio: Virtio 1.0 network device
            +-01.1-[02]----00.0  Virtio: Virtio 1.0 block device
            +-01.2-[03]----00.0  Huawei Technologies Co., Ltd. Hi1822 Family
            +-01.3-[04]----00.0  Huawei Technologies Co., Ltd. HiSilicon ZIP Engine
            +-01.4-[05]----00.0  Huawei Technologies Co., Ltd. HiSilicon HPRE Engine
            \-01.5-[06]----00.0  Huawei Technologies Co., Ltd. HiSilicon SEC Engine
    ```

### 国密硬件KAE VF直通

1. 构建并加载RME ACC驱动

    ```sh
    git clone https://gitcode.com/openeuler/CCA.git  -b master
    cd CCA/driver/rme_acc
    make
    insmod rme_acc.ko
    ```

2. 设备绑定到RME ACC驱动

    ```sh
    echo rme_acc > /sys/bus/pci/devices/0000:09:00.0/driver_override 
    echo 0000:09:00.0 > /sys/bus/pci/devices/0000:09:00.0/driver/unbind 
    echo 0000:09:00.0 > /sys/bus/pci/drivers_probe
    ```

3. 创建VF

    ```sh
    echo 2 > /sys/devices/pci0000:08/0000:08:00.0/0000:09:00.0/sriov_numvfs
    ```

4. VF直通配置

    ```sh
    [root@server1 ~]# ls -l /sys/bus/pci/devices/0000:09:00.0/ | grep virtfn
    lrwxrwxrwx 1 root root 0 Mar 12 16:53 virtfn0 -> ../0000:09:00.1
    lrwxrwxrwx 1 root root 0 Mar 12 16:53 virtfn1 -> ../0000:09:00.2
    ```

    机密虚机xml文件中的devices元素下，新增如下配置：

    ```xml
    <hostdev mode='subsystem' type='pci' managed='yes'>
        <driver name='vfio'/>
        <source>
            <address domain='0x0000' bus='0x09' slot='0x00' function='0x1'/>
        </source>
        <address type='pci' domain='0x0000' bus='0x03' slot='0x00' function='0x0'/>
    </hostdev>
    ```

5. 机密虚机内查看KAE VF设备

    KAE VF直通后，机密虚机内可以看到该设备，例如lspci显示的HiSilicon ZIP Engine即为直通的KAE VF设备。

    ```sh
    [root@localhost ~]# lspci -tv
    -[0000:00]-+-00.0  Red Hat, Inc. QEMU PCIe Host bridge
        +01.0-[01]----00.0  Virtio: Virtio 1.0 network device
        +01.1-[02]----00.0  Virtio: Virtio 1.0 block device
        +01.2-[03]----00.0  Huawei Technologies Co., Ltd. HiSilicon ZIP Engine(Virtual Function)
        \-01.3-[04]--
    ```
