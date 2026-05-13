# Introduction to Arm CCA

## Overview

Arm Confidential Compute Architecture (CCA) is a set of hardware and software architecture innovations launched by Arm for its Armv9-A architecture. It aims to provide confidentiality and integrity protection for all data and code in use. With the hardware, firmware, and software support, this architecture provides a strongly isolated execution environment (confidential VMs) for VMs, ensuring that even the system administrator cannot access sensitive data in the confidential VMs.

## Overall Architecture

In addition to original Normal and Trustzone worlds, CCA newly introduces the Realm and Root worlds. The overall architecture is as follows:

- **Root**: has the highest privilege level and runs at Exception Level 3 (EL3). It is responsible for secure boot and switching between different environments.
- **Realm**: provides a protected execution environment for running the Realm Management Monitor (RMM) at the security virtualization layer and sensitive VMs (confidential VMs).
- **Normal**: provides a traditional operating environment for running the hypervisor and common VMs.
- **Secure**: corresponds to the original Trustzone environment for running sensitive applications.

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

## Key Features

- **Hardware isolation**: implements forcible isolation of Realms based on Realm Management Extension (RME).
- **Memory protection**: The memory management unit (MMU) uses the granule protection check (GPC) to control memory access permissions.
- **Remote attestation**: provides the integrity measurement capability for Realms.
- **Ecosystem compatibility**: VM-level isolation, compatible with the traditional application ecosystems.

## CCA Implementation in openEuler

Running on the CCA hardware and firmware, openEuler adds the native support for CCA credential VMs, that is, adds the CCA support on the libvirt, QEMU, and kernel (KVM and guest kernel) software packages. The current version only supports the creation and destruction of CCA confidential VMs, as well as obtaining attestation reports of confidential VMs. The overall architecture is as follows:

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

### Component Functions

| Component        | Function                                  |
|--------------|-----------------------------------------------|
| libvirt     | Supports CCA after `launchSecurity` is added in `domain` and parsed, including assembling CCA command parameters to call QEMU commands.|
| QEMU         | `ConfidentialGuestSupport` supports `RmeGuest`. Manages Realms and resources, and calls the KVM interface.      |
| Host OS (KVM)| Implements realm management, REC, and RMI interfaces, and calls the RMM interface to manage the lifecycle and resources of confidential VMs.                  |
| Guest OS     | OS running in the Realm VM. Supports CCA resource encryption and RSI interface implementation, and supports obtaining the Realm remote attestation report.          |
| RMM         | Underlying firmware at the security virtualization layer, which is used for isolation and resource allocation.               |
| TF-A        | Underlying firmware, which is used to initialize the RMM/Root environment.            |

## Application Scenarios

- **Confidential cloud host**: To eliminate the customer's concerns about the security of service migration to cloud, technical measures are required to ensure that even the cloud service provider has no access to the data and code processed by the customer in the VM provided by the cloud service provider.

- **Model and data protection**: Protect dedicated AI algorithm models from being stolen by infrastructure providers and protect the privacy of user input data used for inference.
