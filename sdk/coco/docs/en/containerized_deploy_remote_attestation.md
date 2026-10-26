# Containerized Deployment of Remote Attestation Components

<!-- md-trans-meta sourceCommit=d61d3641cfc9fd7ad38931709ec5c550ed59d4ff translatedAt=2026-09-18T11:22:48.751Z pushedAt=2026-09-20T11:15:25.049Z -->

This document provides guidance on containerized compilation and deployment of the remote attestation components Key Broker Service (KBS) and Attestation Service (AS), helping users quickly complete verification of the Confidential Containers (CoCo) remote attestation feature on the Kunpeng Confidential Compute Architecture (CCA) platform. For more configuration and deployment forms, refer to the upstream community: [trustee](https://github.com/confidential-containers/trustee)

- A separate ordinary environment can be prepared to deploy Docker and a local image registry, while the environment with CCA security capabilities is dedicated as the confidential container execution environment.
- `kbs` and `grpc-as` use the same image but run as two independent containers. This allows logs to be viewed, and services to be restarted and upgraded separately, avoiding the management of multiple resident processes in a single container.
- The two services communicate through the Docker Compose internal network, and only the `8080` port of KBS needs to be exposed to the remote attestation client.

> The remote attestation service node needs to have Docker Engine and Docker Compose V2 installed, and the CPU architecture must be consistent with that of the image build node.

Docker Engine 20.10.0 or later is required. Using an earlier version may cause API incompatibility, and upgrading to a newer version is recommended.

Reference installation version of Docker Compose:

```shell
export COMPOSE_ARCH=aarch64
export COMPOSE_VERSION=v5.1.2
sudo mkdir -p /usr/local/lib/docker/cli-plugins
sudo curl -fL "https://github.com/docker/compose/releases/download/${COMPOSE_VERSION}/docker-compose-linux-${COMPOSE_ARCH}" -o /usr/local/lib/docker/cli-plugins/docker-compose
sudo chmod +x /usr/local/lib/docker/cli-plugins/docker-compose
```

> If the Docker version is earlier than 20.10.0, the standalone `docker-compose` command can be used as a quick workaround, for example, `docker-compose --version`, but upgrading Docker Engine to 20.10.0 or later is still recommended.

## Deploying Remote Attestation Components

### 1. Building the Image Using the Build Script

The build script automatically completes source preparation, compilation, and image build. The following commands are executed on the image build node.

```shell
mkdir -p /home/coco && cd /home/coco
git clone https://gitcode.com/openeuler/CCA.git

cd /home/coco/CCA/sdk/coco
```

When a proxy is used, the script automatically collects the proxy gateway CA certificate from `/etc/pki/ca-trust/source/anchors`. If the proxy gateway certificate is not in the default directory, specify it with `--proxy-ca`.

```shell
export http_proxy=http://IP:PORT
export https_proxy=http://IP:PORT
export no_proxy=127.0.0.1,localhost

./tools/build-rats-image.sh --source-dir /home/coco
```

After the build is complete, the image is named `cca-rats:v0.18.0-cca`.

Source preparation is idempotent:

- The target repository is automatically cloned when it does not exist.
- The Git version of the target repository is verified when it exists.
- Automatic skipping is performed if patch and `Cargo.toml` modifications have already been applied.
- Execution stops when the version does not match, the directory is not a Git repository, or patches conflict with local modifications. In this case, `git reset --hard` is not executed and existing modifications are not overwritten.

If the source code has already been prepared, source download and patch checking can be skipped.

```shell
./tools/build-rats-image.sh \
    --source-dir /home/coco \
    --skip-source-prepare
```

If the binary files have also been compiled, both the source preparation and compilation steps can be skipped.

```shell
./tools/build-rats-image.sh \
    --source-dir /home/coco \
    --image cca-rats \
    --tag v0.18.0-cca \
    --skip-source-prepare \
    --skip-compile
```

### 2. Preparing the Deployment Files

Copy the remote attestation deployment files from the repository to the remote attestation service node.

```shell
mkdir -p /home/coco
cp -a \
    /home/coco/CCA/sdk/coco/remote-attestation \
    /home/coco/
cd /home/coco/remote-attestation
```

Create a persistent directory. The services in the image run with UID and GID `10001`, so the data directory must be granted the write permission for the corresponding user.

```shell
mkdir -p \
    data/as/token/simple/policies/opa \
    data/kbs/repository
chown -R 10001:10001 data
```

The deployment directory structure is as follows:

```text
remote-attestation/
├── compose.yaml
├── config/
│   ├── as-config.json
│   ├── config.json
│   ├── default.rego
│   ├── kbs-config-grpc.toml
│   ├── rvstore.json
│   └── tastore.json
└── data/
    ├── as/token/simple/policies/opa/
    └── kbs/
        └── repository/
```

The service invocation relationships in the default configuration are as follows:

- KBS accesses AS through `http://as:50004`.
- The AS port is exposed only on the Compose internal network.
- KBS listens on `0.0.0.0:8080` inside the container.

When service parameters need to be adjusted, modify the corresponding configuration file in the `config` directory. Rebuilding the image is not required.

### 3. Configuring the Verification Policy and Reference Values

The default CCA measurement report verification policy is located in `config/default.rego`. After modifying this file based on the actual measurement baseline, rebuild the AS container to remount and load the policy file:

```shell
docker compose up -d --force-recreate --no-deps as
```

The reference values and trust anchors used by the CCA verifier are stored in `config/rvstore.json` and `config/tastore.json`, respectively. Modify these two files based on the actual platform measurement baseline, and rebuild the AS container for the changes to take effect.

`rvstore.json` contains two types of reference values:

- `platform` node: measurement values of the platform firmware software components (ipu, hilink, serdes, imu, imf\_ap, atf, sef, rmm, stmm, etc.).
- `realm` node: Runtime Integrity Measurement (RIM) baseline of the confidential container. The `initial-measurement` field is the initial RIM measurement value when the confidential container Pod starts, and the `rak-hash-algorithm` field must match the hash algorithm used during RIM generation (`sha-512` is used as an example).

The measurement values of the platform firmware software components can be obtained from the three documents in the [Enabling Remote Attestation](https://www.hikunpeng.com/document/detail/en/kunpengcctrustzone/ccakit/fg-tz/kunpengtee_16_0074.html) section, and filled into the `platform` node of `config/rvstore.json`.

Before enabling remote attestation, ensure that the confidential VM (cVM) can start normally. For details, see [Configuring cVMs](https://www.hikunpeng.com/document/detail/en/kunpengcctrustzone/ccakit/fg-tz/kunpengtee_16_0054.html).

Generation example:

```shell
# 1. Obtain the cVM baseline measurement value
#    <firmware_path>: UEFI firmware path configured for the cVM.
#    <dtb_path>: cVM device tree binary file path (reference document).
#    <vcpu_num>: Number of virtual CPUs configured for the cVM.
#    <ram_size>: Memory size configured for the cVM, which must be 4 KB-aligned.
#    --ccal: Whether to enable hisi-cca for the cVM. Passing this parameter enables it; omitting it disables it.
git clone https://gitcode.com/openeuler/CCA.git
cd CCA/sdk/attestation/rim_ref/
cmake -S . -B build
cmake --build build

cd build
./gen_rim_ref -f <firmware_path> -d <dtb_path> -v <vcpu_num> -r <ram_size> [--ccal]

# 2. Obtain the remote attestation report.
# Modify the cVM configuration file and add the following remote attestation-related configuration options.
# <launchSecurity type='cca' hisi-cca-enable='yes'>
#     <measurement-algo>sha256</measurement-algo>
#     <personalization-value>lmrZvioYuJWTFN/ue5wAKSS70X4MPS+r3lBibrh1pixZyPAcduru93YOyXsFcnqH4+qM4kkjxwaPVYrm/Yj2fA==</personalization-value>
# </launchSecurity>

# Run the following commands in the cVM. The generated $report/outblob is the remote attestation report, and $report/auxblob is the device certificate. After generation, copy the $report/auxblob device certificate to the host for later use in parsing the x/y values in config/tastore.json.
modprobe tsm
modprobe arm-cca-guest
mount -t configfs none /sys/kernel/config
report=/sys/kernel/config/tsm/report/report0
mkdir $report
dd if=/dev/urandom bs=64 count=1 > $report/inblob

# 3. Obtain the values in the report.
# On the host, compile and install the QCBOR component and the t_cose component, and compile the reference code for obtaining and verifying the remote attestation report. After successful compilation, you can view the generated client and server executable files in the CCA/sdk/attestation/samples/build directory.
git clone https://github.com/laurencelundblade/QCBOR.git -b v1.2
cd QCBOR
make
make install
cd ..

git clone https://github.com/laurencelundblade/t_cose.git -b v1.1.2
cd t_cose
cmake -S . -B build -DCRYPTO_PROVIDER=OpenSSL
cmake --build build
cmake --install build
cd ..

cd CCA/sdk/attestation/samples
cmake -S . -B build
cmake --build build
cd -

# Deploy the server to any directory on the VM and start the server inside the VM.
./server -i <ip> -p <port>

# Initiate a remote attestation request on the client. measurement is the cVM initial measurement baseline value obtained in step 1 of this example.
./client -i <ip> -p <port> -m <measurement>

```

The obtained remote attestation report contains `Implementation ID` and the measurement values of software components such as ipu/hilink/serdes/imu/imf_ap/atf/sef/rmm/stmm, which are used to replace the `platform` node in `config/rvstore.json`. The report also contains `Instance ID`, which is used to replace the `instance-id` node in `config/tastore.json`.

The RIM value of a confidential container can be generated by the **baseline measurement value tool** `gen_rim_ref`. Based on the Kata runtime configuration `configuration.toml` and the Pod configuration `pod.yaml` actually used by the confidential container, the tool simulates and assembles a QEMU startup command consistent with the real deployment, dumps the DTB, and then calculates the Realm startup measurement RIM.

Generation example:

```shell
# 1. Compile the baseline measurement value tool (executed on the code repository node).
cd /home/coco/CCA/sdk/attestation/rim_ref
mkdir -p build && cd build
cmake ..
make
cp ./gen_rim_ref /usr/local/bin

# 2. Copy the configuration files actually used by the confidential container runtime environment to the execution node.
#    configuration.toml: Kata runtime configuration (information such as the QEMU, memory, and kernel paths must be consistent with the real deployment.)
#    pod.yaml: confidential container Pod configuration (such as the number of vCPUs)
mkdir -p /tmp/rim-ref
cp <running node>/configuration.toml /tmp/rim-ref/
cp <running node>/pod.yaml /tmp/rim-ref/

# 3. Generate the confidential container RIM value.
#    -c/-p: Kata runtime configuration and Pod configuration actually used by the confidential container
#    -r: Realm memory size, in bytes, which must match the -m option of the QEMU startup command (for example, 2048M corresponds to 2147483648)
#    -a sha512: consistent with rak-hash-algorithm (sha-512) in rvstore.json
gen_rim_ref \
    -c /tmp/rim-ref/configuration.toml \
    -p /tmp/rim-ref/pod.yaml \
    -r 2147483648 \
    -a sha512
```

> When the platform IPA width is not the default value (`44`), you can append `--ipa-width <width>`.

The tool outputs `RIM-HASH: <hex>`. Fill `<hex>` into the `realm[0].initial-measurement` field of `config/rvstore.json`.

```json
{
  "realm": [
    {
      "initial-measurement": "<RIM hash output by gen_rim_ref>",
      "rak-hash-algorithm": "sha-512",
      "extensible-measurements": [
        "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
        "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
      ],
      "personalization-value": "00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    }
  ]
}
```

The `instance-id` and `implementation-id` contained in `tastore.json` correspond to the `Instance ID` and `Implementation ID` in the remote attestation report, respectively. `pkey` uses the P-521 curve, and `raw_rak` is 133 bytes (1 byte for the curve identifier + 66 bytes for the X coordinate + 66 bytes for the Y coordinate). `pkey` contains the public key of the device certificate and is used to verify the signature of the remote attestation report. `pkey` is parsed from `$report/auxblob`.

Example of obtaining the public key:

```shell
# Locate the $report/auxblob file copied from the VM on the host, and replace <auxblob> below with the actual file name.
openssl x509 -in <auxblob> -pubkey -noout \
| openssl ec -pubin -outform DER \
| tail -c 133 \
| tail -c +2 > xy.bin

echo -n "X: "
head -c 66 xy.bin | base64 -w 0
echo

echo -n "Y: "
tail -c 66 xy.bin | base64 -w 0
echo

# Replace <X>/<Y> below with the actual values, and convert the printed X/Y values to base64url format.
echo "<X>" | tr '+/' '-_' | tr -d '='
echo "<Y>" | tr '+/' '-_' | tr -d '='
```

Replace the corresponding fields in `config/tastore.json` based on the obtained information.

```json
[
  {
    "instance-id": "01699f26d3c5596b010dd57e27ef0c2a5596c26d257f02d8eb0591f447a5a8f8f0",
    "implementation-id": "00010001010000000000000000000000000000000000000000000000fc9b5e58",
    "pkey": {
      "crv": "P-521",
      "kty": "EC",
      "x": "AJ1UIMfsoiifDZ5cRz5XIepeHI9YuqvID6usWC0IywRQF9zgYE89wwf7oJT3ysRpypZJwr1AucZprqLMmn93cIXI",
      "y": "AX9TQ2LxYDhTvR6E0h8m2UFqISsOTm7Rx77Xvf3sBL7VP4JKUfQwvGLEfqrv6OlqjkxaTA0pMRME4bv-xAsUqpC2"
    }
  }
]

```

After modifying `config/rvstore.json` and `config/tastore.json`, rebuild the AS container so that the new reference values take effect.

The AS `data/as` directory is mounted to the default working directory `/opt/confidential-containers/attestation-service` of CoCo AS v0.18. The policy engine creates and manages the policy directory under `data/as/token/simple/policies`; `config/default.rego` overrides the default CCA verification policy therein in read-only mode. Therefore, `data/as` must remain writable by UID `10001`.

KBS resources are stored in `data/kbs/repository`, and the resource path rules are consistent with the LocalFs repository of native KBS. On first startup, KBS generates a default resource access policy in `data/kbs/policy.rego`. The entire `data/kbs` directory must remain writable by UID `10001`; otherwise, KBS cannot be initialized or update this policy through the management interface.

Create the image verification policy directory and file.

```shell
mkdir -p data/kbs/repository/default/security-policy
```

Edit `data/kbs/repository/default/security-policy/test`. The reference configuration is as follows:

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

Ensure that the policy file is readable by the container user.

```shell
chown -R 10001:10001 data/kbs/repository/default/security-policy
```

`data/kbs/policy.rego` is the resource access authorization policy of KBS itself, while `data/kbs/repository/default/security-policy/test` is the image verification policy delivered to the confidential container. The two serve different purposes.

### 4. Pulling the Docker Image and Starting the Service

To change the KBS port exposed by the host, set `KBS_PORT` before startup. The port inside the container remains `8080`.

```shell
export KBS_PORT=18080
docker compose up -d
```

When a remote image repository is not used, set `RATS_IMAGE` to the name of the locally built image.

```shell
export RATS_IMAGE="cca-rats:v0.18.0-cca"
```

The default log level is `INFO`. For debugging, set `RUST_LOG=DEBUG` and rebuild the container.

```shell
export RUST_LOG=DEBUG
docker compose up -d --force-recreate
```

### 5. Checking the Service

```shell
docker compose ps
docker compose logs --tail=100 as kbs
```

Continuously view logs.

```shell
docker compose logs -f as kbs
```

Stop the services while retaining configuration and persistent data.

```shell
docker compose down
```

## Manually Compiling Components

To debug a single component or check compilation outputs, see [Remote Attestation Component Deployment](./deploy_remote_attestation.md). When only the remote attestation service image needs to be built, it is recommended that you use the script described above for automatic build.
