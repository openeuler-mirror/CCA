
# CVM镜像定制工具

本工具用于度量基于openEuler的CVM镜像(.qcow2)，以支持CCA grub启动及远程证明功能。主要提供以下功能：

- **镜像度量：** 计算CVM镜像各组件的SHA-256哈希值（包括GRUB EFI二进制文件、grub配置、内核及initramfs镜像），生成JSON格式参考度量文件(image_reference_measurement.json)，用于后续远程证明流程。

## 前置条件

请在openEuler主机上安装以下软件包：

```shell
yum install -y libguestfs-tools virt-install qemu-img genisoimage guestfs-tools cloud-utils-growpart jq
```

## 使用说明

通过命令行参数调用脚本：

```shell
$ sh create-oe-image.sh -h
Usage: create-oe-image.sh [选项]...
  -h                        显示帮助信息
  -i <输入镜像>             指定待度量的qcow2镜像
```

示例命令

- 度量现有CVM镜像：

```shell
sh create-oe-image.sh -i /path/to/cvm_image.qcow2
```

## 工作流程

### 镜像度量流程

使用guestmount挂载CVM镜像至临时目录，然后度量它的启动项。

- GRUB度量：

编译measure_pe.c生成MeasurePe工具，计算GRUB EFI二进制文件(BOOTAA64.EFI)的SHA-256哈希与GRUB配置文件(grub.cfg)的哈希值。

- 内核与initramfs度量：

在已挂载镜像的/boot目录中扫描所有内核镜像文件（命名格式为vmlinuz-*），并自动排除救援内核（rescue kernels）。

对每个检测到的内核镜像执行以下操作：首先尝试解压缩内核镜像文件（如使用gzip/xz等工具），然后计算解压后内核镜像的SHA-256哈希值。

所有度量结果将按以下JSON结构聚合至image_reference_measurement.json文件：

```json
{
    "grub": "<GRUB EFI哈希>",
    "grub.cfg": "<GRUB配置哈希>",
    "kernels": [
        {
            "version": "<kernel版本>",
            "kernel": "<kernel哈希>",
            "initramfs": "<initramfs哈希或NOT_FOUND>"
        },
        ...
    ],
    "hash_alg": "sha-256"
}
```
该JSON文件将在远程证明流程中作为CVM镜像的基准度量值使用。
