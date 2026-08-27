# 容器化部署远程证明组件

此文档提供远程证明组件kbs、as容器化编译部署指导，帮助用户快速基于鲲鹏CCA平台完成coco机密容器远程证明特性验证，更多配置和部署形态可参考上游社区：<https://github.com/confidential-containers/trustee>

- 远程证明镜像参考[容器化编译CoCo组件](#1-使用构建脚本制作镜像)完成构建。
- `kbs`和`grpc-as`使用同一个镜像，但作为两个独立容器运行。这样可以分别查看日志、重启和升级服务，避免在单个容器中管理多个常驻进程。
- 两个服务通过Docker Compose内部网络通信，只需要对远程证明客户端开放KBS的`8080`端口。

> 远程证明服务节点需要安装Docker Engine和Docker Compose V2，并且CPU架构需要与镜像构建节点一致。

Docker Engine 版本要求：20.10.0及以上，使用更早版本可能造成API不兼容，推荐升级到新版本。

docker compose 参考安装版本：

```shell
export COMPOSE_ARCH=aarch64
export COMPOSE_VERSION=v5.1.2
sudo mkdir -p /usr/local/lib/docker/cli-plugins
sudo curl -fL "https://github.com/docker/compose/releases/download/${COMPOSE_VERSION}/docker-compose-linux-${COMPOSE_ARCH}" -o /usr/local/lib/docker/cli-plugins/docker-compose
sudo chmod +x /usr/local/lib/docker/cli-plugins/docker-compose
```

> 若docker版本低于20.10.0，可以使用独立的 docker-compose 命令作为快速解决方案，例如docker-compose --version，但仍推荐升级Docker Engine到20.10.0及以上版本。

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

推送到华为云SWR：

```shell
docker login swr.cn-north-4.myhuaweicloud.com

./tools/build-rats-image.sh \
    --source-dir /home/coco \
    --image swr.cn-north-4.myhuaweicloud.com/<组织名称>/cca-rats \
    --tag v0.18.0-cca \
    --push
```

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

创建持久化目录。镜像内的服务使用UID和GID `10001`运行，因此数据目录需要授予对应用户写权限。

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

默认CCA度量报告验证策略位于`config/default.rego`。根据实际度量基线修改该文件后，重建AS容器以重新挂载并加载策略文件：

```shell
docker compose up -d --force-recreate --no-deps as
```

CCA验证器使用的参考值和信任锚分别保存在`config/rvstore.json`和`config/tastore.json`。根据实际平台度量基线修改这两个文件，重建AS容器生效。

AS的`data/as`挂载到CoCo AS v0.18默认工作目录`/opt/confidential-containers/attestation-service`。策略引擎会在`data/as/token/simple/policies`下创建并管理策略目录；`config/default.rego`以只读方式覆盖其中的默认CCA验证策略。因此`data/as`必须保持UID `10001`可写。

KBS资源保存在`data/kbs/repository`，资源路径规则与原生KBS的LocalFs repository一致。KBS首次启动时会在`data/kbs/policy.rego`生成默认资源访问策略。整个`data/kbs`目录必须保持UID `10001`可写，否则KBS无法初始化或通过管理接口更新该策略。

创建镜像验证策略目录和文件：

```shell
mkdir -p data/kbs/repository/default/security-policy
```

编辑`data/kbs/repository/default/security-policy/test`，参考配置如下：

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

登录镜像仓并拉取镜像。以下以华为云SWR为例：

```shell
docker login swr.cn-north-4.myhuaweicloud.com

export RATS_IMAGE="swr.cn-north-4.myhuaweicloud.com/<组织名称>/cca-rats:v0.18.0-cca"
docker compose pull
docker compose up -d
```

如需修改宿主机对外提供的KBS端口，可在启动前设置`KBS_PORT`，容器内端口仍为`8080`：

```shell
export KBS_PORT=18080
docker compose up -d
```

不使用远程镜像仓时，可以将`RATS_IMAGE`设置为本地构建的镜像名称。

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

以下步骤用于调试单个组件或检查编译产物。详细步骤参见[部署远程证明组件](./deploy-remote-attestation.md)。只需要构建远程证明服务镜像时，推荐使用前述自动构建脚本。
