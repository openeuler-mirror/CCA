# 容器化部署远程证明组件

此文档提供远程证明组件kbs、as容器化编译部署指导，帮助用户快速基于鲲鹏CCA平台完成coco机密容器远程证明特性验证，更多配置和部署形态可参考上游社区：[trustee](https://github.com/confidential-containers/trustee)

- 可单独准备一台普通环境部署docker以及registry本地镜像仓，而具备CCA安全能力的环境则专门作为机密容器执行环境。
- `kbs`和`grpc-as`使用同一个镜像，但作为两个独立容器运行。这样可以分别查看日志、重启和升级服务，避免在单个容器中管理多个常驻进程。
- 两个服务通过Docker Compose内部网络通信，只需要对远程证明客户端开放KBS的`8080`端口。

> 远程证明服务节点需要安装Docker Engine和Docker Compose V2，并且CPU架构需要与镜像构建节点一致。

Docker Engine版本要求：20.10.0及以上，使用更早版本可能造成API不兼容，推荐升级到新版本。

docker compose参考安装版本：

```shell
export COMPOSE_ARCH=aarch64
export COMPOSE_VERSION=v5.1.2
sudo mkdir -p /usr/local/lib/docker/cli-plugins
sudo curl -fL "https://github.com/docker/compose/releases/download/${COMPOSE_VERSION}/docker-compose-linux-${COMPOSE_ARCH}" -o /usr/local/lib/docker/cli-plugins/docker-compose
sudo chmod +x /usr/local/lib/docker/cli-plugins/docker-compose
```

> 若docker版本低于20.10.0，可以使用独立的docker-compose命令作为快速解决方案，例如docker-compose --version，但仍推荐升级Docker Engine到20.10.0及以上版本。

## 远程证明组件部署

### 1. 使用构建脚本制作镜像

构建脚本会自动完成源码准备、编译和镜像构建。以下命令在镜像构建节点上执行。

```shell
mkdir -p /home/coco && cd /home/coco
git clone https://gitcode.com/openeuler/CCA.git

cd /home/coco/CCA/sdk/coco
```

使用代理时，脚本会自动从`/etc/pki/ca-trust/source/anchors`收集代理网关CA证书。代理网关证书不在默认目录时，通过`--proxy-ca`指定。

```shell
export http_proxy=http://IP:PORT
export https_proxy=http://IP:PORT
export no_proxy=127.0.0.1,localhost

./tools/build-rats-image.sh --source-dir /home/coco
```

构建完成后，镜像名为`cca-rats:v0.18.0-cca`。

源码准备具有幂等性：

- 目标仓库不存在时自动克隆。
- 目标仓库存在时校验其Git版本。
- patch和Cargo.toml修改已经应用时自动跳过。
- 版本不匹配、目录不是Git仓库或patch与本地修改冲突时停止执行，不会运行`git reset --hard`或覆盖已有修改。

如果源码已经准备完成，可以跳过源码下载和patch检查：

```shell
./tools/build-rats-image.sh \
    --source-dir /home/coco \
    --skip-source-prepare
```

如果二进制也已经编译完成，可以同时跳过源码准备和编译步骤：

```shell
./tools/build-rats-image.sh \
    --source-dir /home/coco \
    --image cca-rats \
    --tag v0.18.0-cca \
    --skip-source-prepare \
    --skip-compile
```

### 2. 准备部署文件

将仓库中的远程证明部署文件复制到远程证明服务节点：

```shell
mkdir -p /home/coco
cp -a \
    /home/coco/CCA/sdk/coco/remote-attestation \
    /home/coco/
cd /home/coco/remote-attestation
```

创建持久化目录。镜像内的服务使用UID和GID`10001`运行，因此数据目录需要授予对应用户写权限。

```shell
mkdir -p \
    data/as/token/simple/policies/opa \
    data/kbs/repository
chown -R 10001:10001 data
```

部署目录结构如下：

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

默认配置中的服务调用关系为：

- KBS通过`http://as:50004`访问AS。
- AS端口只暴露在Compose内部网络。
- KBS监听容器内的`0.0.0.0:8080`。

需要调整服务参数时，修改`config`目录中的对应配置文件，不需要重新构建镜像。

### 3. 配置验证策略和参考值

默认CCA度量报告验证策略位于`config/default.rego`。根据实际度量基线修改该文件后，重建AS容器以重新挂载并加载策略文件。

```shell
docker compose up -d --force-recreate --no-deps as
```

CCA验证器使用的参考值和信任锚分别保存在`config/rvstore.json`和`config/tastore.json`。根据实际平台度量基线修改这两个文件，重建AS容器生效。

`rvstore.json`包含两类参考值：

- `platform`节点：平台固件各软件组件的度量值（ipu、hilink、serdes、imu、imf\_ap、atf、sef、rmm、stmm等）。
- `realm`节点：机密容器的RIM基线。其中`initial-measurement`字段即机密容器Pod启动时的初始RIM度量值，`rak-hash-algorithm`字段必须与生成该RIM时使用的哈希算法一致（示例为`sha-512`）。

平台固件各软件组件的度量值可通过[使能远程证明章节](https://www.hikunpeng.com/document/detail/zh/kunpengcctrustzone/ccakit/fg-tz/kunpengtee_16_0074.html)的三篇文档获取，填写到`config/rvstore.json`的`platform`节点。

在使能远程证明前，需要确保机密虚机可正常启动，可参考[配置机密虚拟机](https://www.hikunpeng.com/document/detail/zh/kunpengcctrustzone/ccakit/fg-tz/kunpengtee_16_0054.html)。

生成示例：

```shell
# 1. 获取机密虚拟机基线度量值
#    <firmware_path>: cVM配置的UEFI固件路径。
#    <dtb_path>: cVM的设备树二进制文件路径（参考文档）。
#    <vcpu_num>: cVM虚拟机配置虚拟CPU个数。
#    <ram_size>: cVM配置的内存大小，要求4KB对齐。
#    --ccal：cVM配置是否开启hisi-cca，传递表示开启，不传递则未开启。
git clone https://gitcode.com/openeuler/CCA.git
cd CCA/sdk/attestation/rim_ref/
cmake -S . -B build
cmake --build build

cd build
./gen_rim_ref -f <firmware_path> -d <dtb_path> -v <vcpu_num> -r <ram_size> [--ccal]

# 2. 获取远程证明报告
# 修改机密虚拟机配置文件，增加如下远程证明相关配置选项。
# <launchSecurity type='cca' hisi-cca-enable='yes'>
#     <measurement-algo>sha256</measurement-algo>
#     <personalization-value>lmrZvioYuJWTFN/ue5wAKSS70X4MPS+r3lBibrh1pixZyPAcduru93YOyXsFcnqH4+qM4kkjxwaPVYrm/Yj2fA==</personalization-value>
# </launchSecurity>

# 在机密虚机内执行以下命令，生成的$report/outblob为远程证明报告，$report/auxblob为设备证书，生成后将$report/auxblob设备证书复制到host备用，用于解析出config/tastore.json的x/y值。
modprobe tsm
modprobe arm-cca-guest
mount -t configfs none /sys/kernel/config
report=/sys/kernel/config/tsm/report/report0
mkdir $report
dd if=/dev/urandom bs=64 count=1 > $report/inblob

# 3. 获取报告中的值
# 在host中编译、安装QCBOR组件、t_cose组件、编译远程证明报告获取和验证的参考代码，编译成功后，可在“CCA/sdk/attestation/samples/build”目录下查看生成的client、server可执行文件。
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

# 将server部署到虚拟机任意目录下，在虚拟机内启动server。
./server -i <ip> -p <port>

# 在client发起远程证明请求。measurement为本示例步骤1中获取的cVM初始度量基线值。
./client -i <ip> -p <port> -m <measurement>

```

获取的远程证明报告包含Implementation ID、ipu/hilink/serdes/imu/imf_ap/atf/sef/rmm/stmm等软件组件的度量值，替换`config/rvstore.json`中的`platform`节点；包含Instance ID，替换`config/tastore.json`中的`instance-id`节点。

机密容器的RIM值可通过**基线度量值工具**`gen_rim_ref`生成。工具根据机密容器实际使用的Kata运行时配置`configuration.toml`和Pod配置`pod.yaml`，模拟拼装出与真实部署一致的QEMU启动命令，dump出DTB，进而计算Realm启动度量RIM（Runtime Integrity Measurement）。

生成示例：

```shell
# 1. 编译基线度量值工具（在代码仓节点执行）
cd /home/coco/CCA/sdk/attestation/rim_ref
mkdir -p build && cd build
cmake ..
make
cp ./gen_rim_ref /usr/local/bin

# 2. 拷贝机密容器运行环境实际使用的配置文件到执行节点
#    configuration.toml：Kata运行时配置（其中QEMU、内存、kernel路径等须与真实部署一致）
#    pod.yaml：机密容器Pod配置（vcpu数量等）
mkdir -p /tmp/rim-ref
cp <运行节点>/configuration.toml /tmp/rim-ref/
cp <运行节点>/pod.yaml /tmp/rim-ref/

# 3. 生成机密容器RIM值
#    -c/-p：机密容器实际使用的Kata运行时配置与Pod配置
#    -r：Realm内存大小，单位字节，须与QEMU启动命令的-m一致（如2048MB对应2147483648）
#    -a sha512：与rvstore.json中rak-hash-algorithm为sha-512保持一致
gen_rim_ref \
    -c /tmp/rim-ref/configuration.toml \
    -p /tmp/rim-ref/pod.yaml \
    -r 2147483648 \
    -a sha512
```

> 平台IPA宽度非默认值（44）时，可追加`--ipa-width <宽度>`。

工具输出`RIM-HASH: <hex>`，将`<hex>`填入`config/rvstore.json`的`realm[0].initial-measurement`字段。

根据以上获取的信息替换`config/rvstore.json`对应字段：

```json
{
  "platform": [
    {
      "implementation-id": "<Implementation ID>",
      "sw-components": [
        {
          "measurement-value": "<ipu的度量值>",
          "signer-id": "",
          "component-type": "ipu"
        },
        {
          "measurement-value": "<hilink的度量值>",
          "signer-id": "",
          "component-type": "hilink"
        },
        {
          "measurement-value": "<serdes的度量值>",
          "signer-id": "",
          "component-type": "serdes"
        },
        {
          "measurement-value": "<imu的度量值>",
          "signer-id": "",
          "component-type": "imu"
        },
        {
          "measurement-value": "<imf_ap的度量值>",
          "signer-id": "",
          "component-type": "imf_ap"
        },
        {
          "measurement-value": "<atf的度量值>",
          "signer-id": "",
          "component-type": "atf"
        },
        {
          "measurement-value": "<sef的度量值>",
          "signer-id": "",
          "component-type": "sef"
        },
        {
          "measurement-value": "<rmm的度量值>",
          "signer-id": "",
          "component-type": "rmm"
        },
        {
          "measurement-value": "<stmm的度量值>",
          "signer-id": "",
          "component-type": "stmm"
        }
      ],
      "platform-configuration": ""
    }
  ],
  "realm": [
    {
      "initial-measurement": "<gen_rim_ref输出的RIM哈希>",
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

`tastore.json`包含的`instance-id`和`implementation-id`，分别对应远程证明报告中的Instance ID和Implementation ID。`pkey`使用P-521曲线，`raw_rak`大小为133字节（1+66+66），其中1字节为曲线标识，66字节为X坐标，66字节为Y坐标。`pkey`包含设备证书的公钥，用于验证远程证明报告的签名，`pkey`从`$report/auxblob`中解析。

公钥获取示例：

```shell
# 在host找到从虚机中复制的$report/auxblob文件，替换下面<auxblob>为实际文件名称
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

# 替换下面<X>/<Y>为实际值，将打印出的X/Y值转换为base64url格式
echo "<X>" | tr '+/' '-_' | tr -d '='
echo "<Y>" | tr '+/' '-_' | tr -d '='
```

根据获取的信息替换`config/tastore.json`对应字段：

```json
[
  {
    "instance-id": "<Instance ID>",
    "implementation-id": "<Implementation ID>",
    "pkey": {
      "crv": "P-521",
      "kty": "EC",
      "x": "<X值，base64url格式>",
      "y": "<Y值，base64url格式>"
    }
  }
]

```

修改`config/rvstore.json`和`config/tastore.json`后重建AS容器，使新的参考值生效。

AS的`data/as`挂载到CoCo AS v0.18默认工作目录`/opt/confidential-containers/attestation-service`。策略引擎会在`data/as/token/simple/policies`下创建并管理策略目录；`config/default.rego`以只读方式覆盖其中的默认CCA验证策略。因此`data/as`必须保持UID `10001`可写。

KBS资源保存在`data/kbs/repository`，资源路径规则与原生KBS的LocalFs repository一致。KBS首次启动时会在`data/kbs/policy.rego`生成默认资源访问策略。整个`data/kbs`目录必须保持UID `10001`可写，否则KBS无法初始化或通过管理接口更新该策略。

创建镜像验证策略目录和文件：

```shell
mkdir -p data/kbs/repository/default/security-policy
```

编辑`data/kbs/repository/default/security-policy/test`，参考配置如下：
说明：`insecureAcceptAnything` 策略仅用于功能验证，生产环境中必须替换为 signedBy/reject 等真实策略并配置签名者。`registry.hw.com:5000` 为容器镜像仓库地址，根据实际情况修改。

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

确保策略文件可被容器用户读取：

```shell
chown -R 10001:10001 data/kbs/repository/default/security-policy
```

`data/kbs/policy.rego`是KBS自身的资源访问授权策略，而`data/kbs/repository/default/security-policy/test`是下发给机密容器的镜像验证策略，两者用途不同。

### 4. 拉取并启动服务

如需修改宿主机对外提供的KBS端口，可在启动前设置`KBS_PORT`，容器内端口仍为`8080`：

```shell
export KBS_PORT=18080
docker compose up -d
```

不使用远程镜像仓时，可以将`RATS_IMAGE`设置为本地构建的镜像名称。

```shell
export RATS_IMAGE="cca-rats:v0.18.0-cca"
```

默认日志级别为`INFO`。调试时可以设置`RUST_LOG=DEBUG`并重建容器：

```shell
export RUST_LOG=DEBUG
docker compose up -d --force-recreate
```

### 5. 检查服务

```shell
docker compose ps
docker compose logs --tail=100 as kbs
```

持续查看日志：

```shell
docker compose logs -f as kbs
```

停止服务但保留配置和持久化数据：

```shell
docker compose down
```

## 手动编译组件

以下步骤用于调试单个组件或检查编译产物。详细步骤参见[部署远程证明组件](./deploy_remote_attestation.md)。只需要构建远程证明服务镜像时，推荐使用前述自动构建脚本。
