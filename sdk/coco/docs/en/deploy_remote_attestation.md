# Remote Attestation Component Deployment

<!-- md-trans-meta sourceCommit=d61d3641cfc9fd7ad38931709ec5c550ed59d4ff translatedAt=2026-09-18T11:27:54.973Z pushedAt=2026-09-20T11:15:27.002Z -->

This document provides guidance on simple compilation and deployment of the remote attestation components Key Broker Service (KBS) and Attestation Service (AS), helping users quickly complete verification of the Confidential Containers (CoCo) remote attestation feature on the Kunpeng Confidential Compute Architecture (CCA) platform. For more configuration and deployment forms, refer to the upstream community: [trustee](https://github.com/confidential-containers/trustee)

## 1. Building the Container Compilation Environment

```shell
mkdir /home/coco && cd /home/coco
git clone https://gitcode.com/openeuler/CCA.git

cd /home/coco/CCA/sdk/coco/config
mkdir certs

# Copy the proxy server certificate.
cp GatewayCA.crt certs
# Specify the proxy when building the image.
docker build --no-cache --build-arg http_proxy=http://IP:PORT --build-arg https_proxy=http://IP:PORT -t coco-builder:1.90.0 .

# Create a compilation environment container based on the built image. /home/coco is the source code path and can be customized by the user.
docker run -itd --name coco-build-env -v /home/coco:/coco -e http_proxy="http://IP:PORT" -e https_proxy="http://IP:PORT" coco-builder:1.90.0
```

## 2. Preparing the Remote Attestation Source Code

```shell
mkdir /home/coco && cd /home/coco

# Prepare the ccatoken source code and apply the Kunpeng platform adaptation patch.
git clone https://github.com/veraison/rust-ccatoken.git
cd rust-ccatoken
git reset --hard 6d5b8db
git apply /home/coco/CCA/sdk/coco/patches/CCA-Token-Parsing-Adaptation-for-Kunpeng-Platform.patch

# Prepare the Trustee source code and change the ccatoken dependency package to the local version.
cd ..
git clone https://github.com/confidential-containers/trustee.git -b v0.18.0
sed -i 's#ccatoken = { git = "https://github.com/veraison/rust-ccatoken", rev = "6d5b8db", optional = true }#ccatoken = { path = "../../../rust-ccatoken", optional = true }#' trustee/deps/verifier/Cargo.toml
```

## 3. Compiling `kbs` and `grpc-as`

```shell
# Enter the build environment container.
docker exec -it coco-build-env /bin/bash

# Compile AS.
cd /coco/trustee/attestation-service
make VERIFIER=cca-verifier

# Compile KBS.
cd /coco/trustee/kbs
make background-check-kbs COCO_AS_INTEGRATION_TYPE=grpc
```

The path to the compiled binaries is `/home/coco/trustee/target/release`. **Copy it to the `/usr/local/bin` directory of the remote attestation running node**.

## 4. Deploying KBS

mkdir /etc/coco && vim /etc/coco/kbs-config-grpc.toml

The reference configuration is as follows:

```toml
[http_server]
sockets = ["0.0.0.0:8080"]
insecure_http = true
worker_count = 4

[attestation_token]
insecure_key = true

[admin]
type = "InsecureAllowAll"

[attestation_service]
type = "coco_as_grpc"
as_addr = "http://127.0.0.1:50004"

[[plugins]]
name = "resource"
type = "LocalFs"
dir_path = "/opt/confidential-containers/kbs/repository"
```

mkdir -p /opt/confidential-containers/kbs/repository/default/security-policy && vim /opt/confidential-containers/kbs/repository/default/security-policy/test

The reference configuration is as follows:

```json
{
    "default": [
        {
            "type": "insecureAcceptAnything"
        }
    ],
    "transports": {
        "docker": {
            "registry.hw.com:5000": [
                {
                    "type": "insecureAcceptAnything"
                }
            ]
        }
    }
}
```

Start the KBS resident process by running `RUST_LOG=DEBUG kbs -c /etc/coco/kbs-config-grpc.toml`.

## 5. Deploying AS

```shell
# Copy the reference device certificate and baseline file.
mkdir -p /opt/confidential-containers/attestation-service/cca
cp /home/coco/CCA/sdk/coco/config/* /opt/confidential-containers/attestation-service/cca
```

vim /etc/coco/as-config.json  
The reference configuration is as follows:

```json
{
    "work_dir": "/opt/confidential-containers/attestation-service",
    "policy_engine": "opa",
    "attestation_token_broker": {
        "type": "Simple",
        "duration_min": 5
    }
}
```

Start the AS resident process by running `RUST_LOG=DEBUG grpc-as -s 127.0.0.1:50004 -c /etc/coco/as-config.json`.
