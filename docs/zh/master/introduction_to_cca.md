# ARM CCA (Confidential Computing Architecture)

## 概述

ARM CCA (Confidential Computing Architecture) 是ARM公司为其 Armv9-A 架构推出的一套硬件和软件架构创新，旨在为所有数据和使用中的代码提供机密性和完整性保护。该架构通过硬件、固件及软件支持，为虚拟机提供了强隔离的执行环境（机密虚机），确保即使系统管理员也无法访问机密虚机内的敏感数据。

## CCA总体架构

Realm和Root是CCA架构引入的两个新的世界，加上原有的Normal和Trustzone共有四个世界，总体架构如下图所示。

- **Root**：具有最高特权级别，运行在EL3异常级别，负责安全启动和不同环境之间的切换。
- **Realm**：提供受保护的执行环境，用于运行安全虚拟化层RMM（Realm Management Monitor）和敏感的虚拟机（机密虚机）。
- **Normal**：传统的运行环境，用于运行Hypervisor和普通虚拟机。
- **Secure**: 原来的Trustzone环境，运行敏感的应用。

```markdown
+------------+------------+------------+
|   Realm    |   Normal   |   Secure   |
|            |            |    TA      |
|  Realm VM  |  Normal VM |   TEEOS    |
+------------+------------+------------+   
|    RMM     | Hypervisor |    SPM     |
+------------+------------+------------+
|                  Root                |
+--------------------------------------+
```

## 关键特性

- **硬件隔离**: 基于RME（Realm Management Extension）实现Realm域的强制隔离。
- **内存保护**: 内存管理单元（MMU）通过Granule Protection Check (GPC)控制内存访问权限。
- **远程证明**: 提供Realm的完整性度量能力。
- **生态兼容**：虚拟机级别隔离，兼容传统应用生态。

## openEuler CCA实现

openEuler在CCA硬件及固件之上，构建原生支持CCA机密虚机的操作系统，分别在libvirt、qemu、kernel（KVM和Guest kernel）软件包上开发支持CCA，当前版本仅支持CCA机密虚机的创建、销毁和启动机密虚机后获取机密虚机的证明报告，总体框架如下。

```mardkown
+------------+------------+
|  libvirt   |    Realm   |
|------------|            |
|   qemu     |            |
|------------|  Guest OS  |
|            |            |
|  Host OS   |------------|
|   (KVM)    |     RMM    |
+------------+------------|
|           TF-A          |
+-------------------------+
```

### 组件功能简介

| 组件         | 主要功能描述                                   |
|--------------|-----------------------------------------------|
| libvirt      | Domain配置解析launchSecurity支持CCA，支持组装CCA命令参数，调用QEMU命令 |
| QEMU         | ConfidentialGuestSupport新增RmeGuest支持，实现Realm和资源管理，调用KVM接口       |
| Host OS (KVM)| 实现realm管理、REC、RMI接口实现，调用RMM接口管理机密虚机生命周期和资源                   |
| Guest OS     | 运行于Realm VM中的操作系统，支持CCA资源加密、RSI接口实现，支持获取realm远程证明报告           |
| RMM          | 底层固件，安全虚拟化层，Realm管理监控器，隔离与资源分配                |
| TF-A         | 底层固件，初始化RMM/Root环境             |

## 应用场景

- **机密云主机**：云服务商可以向客户提供 VM，保证云提供商自身也无法看到客户在 VM 中处理的数据和代码，通过技术手段打消客户对业务上云的安全顾虑。

- **保护模型与数据**：保护专有的 AI 算法模型不被基础设施提供商窃取，同时保护用于推理的用户输入数据的隐私。
