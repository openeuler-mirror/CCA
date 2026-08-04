#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SDK_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
IMAGE_CONTEXT_DIR="${SDK_DIR}/remote-attestation"

SOURCE_DIR="/home/coco"
BUILDER_IMAGE="coco-builder:latest"
IMAGE_REPOSITORY="cca-rats"
IMAGE_TAG="v0.18.0-cca"
PUSH_IMAGE="false"
SKIP_COMPILE="false"
SKIP_SOURCE_PREPARE="false"
DEFAULT_PROXY_CA_DIR="/etc/pki/ca-trust/source/anchors"
GO_DOWNLOAD_MIRRORS="${GO_DOWNLOAD_MIRRORS:-https://mirrors.aliyun.com/golang https://golang.google.cn/dl https://go.dev/dl}"
PROXY_CA_PATHS=()

TRUSTEE_VERSION="v0.18.0"
CCATOKEN_COMMIT="6d5b8db"

TRUSTEE_REPOSITORY="https://github.com/confidential-containers/trustee.git"
CCATOKEN_REPOSITORY="https://github.com/veraison/rust-ccatoken.git"

usage()
{
    cat <<'EOF'
Build the CCA remote attestation service image.

Usage:
  build-rats-image.sh [options]

Options:
  --source-dir PATH       Source root containing trustee and rust-ccatoken
                          repositories (default: /home/coco)
  --builder-image IMAGE   CoCo build environment image (default: coco-builder:latest)
  --image REPOSITORY      Output image repository, including registry if needed
                          (default: cca-rats)
  --tag TAG               Output image tag (default: v0.18.0-cca)
  --proxy-ca PATH         Proxy CA file or directory containing .crt files;
                          may be specified more than once
  --go-download-mirrors URLS
                          Space-separated Go download base URLs
  --skip-source-prepare   Do not clone, verify or patch source repositories
  --skip-compile          Package existing trustee release binaries
  --push                  Push the image after a successful build
  -h, --help              Show this help

Examples:
  ./build-rats-image.sh --source-dir /home/coco

  docker login swr.cn-north-4.myhuaweicloud.com
  ./build-rats-image.sh \
    --image swr.cn-north-4.myhuaweicloud.com/example/cca-rats \
    --tag v0.18.0-cca \
    --push
EOF
}

die()
{
    echo "error: $*" >&2
    exit 1
}

verify_repository_ref()
{
    local repository_dir="$1"
    local expected_ref="$2"
    local component_name="$3"
    local expected_commit
    local actual_commit

    [ -d "${repository_dir}/.git" ] \
        || die "${component_name} directory is not a Git repository: ${repository_dir}"

    if ! expected_commit="$(
        git -C "${repository_dir}" rev-parse --verify "${expected_ref}^{commit}" 2>/dev/null
    )"; then
        die "${component_name} expected ref is unavailable: ${expected_ref}"
    fi
    actual_commit="$(git -C "${repository_dir}" rev-parse HEAD)"

    [ "${actual_commit}" = "${expected_commit}" ] || die \
        "${component_name} is at ${actual_commit}, expected ${expected_ref} (${expected_commit}); refusing to overwrite it"
}

clone_tagged_repository()
{
    local repository_url="$1"
    local expected_ref="$2"
    local repository_dir="$3"
    local component_name="$4"

    if [ -e "${repository_dir}" ]; then
        verify_repository_ref "${repository_dir}" "${expected_ref}" "${component_name}"
        echo "==> Source ready: ${component_name} (${expected_ref})"
        return
    fi

    echo "==> Cloning ${component_name}: ${expected_ref}"
    git clone \
        --branch "${expected_ref}" \
        --single-branch \
        "${repository_url}" \
        "${repository_dir}"
    verify_repository_ref "${repository_dir}" "${expected_ref}" "${component_name}"
}

clone_commit_repository()
{
    local repository_url="$1"
    local expected_commit="$2"
    local repository_dir="$3"
    local component_name="$4"

    if [ -e "${repository_dir}" ]; then
        verify_repository_ref "${repository_dir}" "${expected_commit}" "${component_name}"
        echo "==> Source ready: ${component_name} (${expected_commit})"
        return
    fi

    echo "==> Cloning ${component_name}: ${expected_commit}"
    git clone "${repository_url}" "${repository_dir}"
    git -C "${repository_dir}" checkout --detach "${expected_commit}"
    verify_repository_ref "${repository_dir}" "${expected_commit}" "${component_name}"
}

apply_patch_once()
{
    local repository_dir="$1"
    local patch_file="$2"
    local patch_name

    patch_name="$(basename -- "${patch_file}")"
    [ -f "${patch_file}" ] || die "patch file not found: ${patch_file}"

    if git -C "${repository_dir}" apply --reverse --check "${patch_file}" >/dev/null 2>&1; then
        echo "==> Patch already applied: ${patch_name}"
        return
    fi

    if ! git -C "${repository_dir}" apply --check "${patch_file}"; then
        die "cannot apply ${patch_name}; check the source version and local changes in ${repository_dir}"
    fi

    echo "==> Applying patch: ${patch_name}"
    git -C "${repository_dir}" apply "${patch_file}"
}

apply_cargo_patch_once()
{
    local cargo_toml="$1"
    local component_name="$2"

    [ -f "${cargo_toml}" ] || die "${component_name} Cargo.toml not found: ${cargo_toml}"

    if grep -q 'ccatoken = { path = "../../../rust-ccatoken"' "${cargo_toml}"; then
        echo "==> Cargo.toml already patched: ${component_name}"
        return
    fi

    if ! grep -q 'ccatoken = { git = "https://github.com/veraison/rust-ccatoken"' "${cargo_toml}"; then
        die "cannot find ccatoken git dependency in ${cargo_toml}; check the trustee version"
    fi

    echo "==> Patching Cargo.toml: ${component_name} (ccatoken -> local path)"
    sed -i 's#ccatoken = { git = "https://github.com/veraison/rust-ccatoken", rev = "6d5b8db", optional = true }#ccatoken = { path = "../../../rust-ccatoken", optional = true }#' "${cargo_toml}"
}

copy_proxy_ca_certificates()
{
    local destination_dir="$1"
    local source_path
    local certificate
    local copied=0
    local certificate_files=()

    mkdir -p "${destination_dir}"
    cp "${SDK_DIR}/config/certs/.gitkeep" "${destination_dir}/.gitkeep"

    for source_path in "${PROXY_CA_PATHS[@]}"; do
        if [ -d "${source_path}" ]; then
            shopt -s nullglob
            certificate_files=("${source_path}"/*.crt)
            shopt -u nullglob
            [ "${#certificate_files[@]}" -gt 0 ] \
                || die "no .crt proxy CA files found in: ${source_path}"

            for certificate in "${certificate_files[@]}"; do
                copied=$((copied + 1))
                cp "${certificate}" "${destination_dir}/proxy-ca-${copied}.crt"
            done
        elif [ -f "${source_path}" ]; then
            copied=$((copied + 1))
            cp "${source_path}" "${destination_dir}/proxy-ca-${copied}.crt"
        else
            die "proxy CA path does not exist: ${source_path}"
        fi
    done

    if [ "${copied}" -gt 0 ]; then
        echo "==> Added ${copied} proxy CA certificate(s) to the builder context"
    else
        echo "==> No additional proxy CA certificates configured"
    fi
}

prepare_source()
{
    local source_parent
    local source_name
    local trustee_dir
    local ccatoken_dir

    source_parent="$(dirname -- "${SOURCE_DIR}")"
    source_name="$(basename -- "${SOURCE_DIR}")"
    [ "${source_name}" != "." ] && [ "${source_name}" != "/" ] \
        || die "invalid source directory: ${SOURCE_DIR}"

    mkdir -p "${source_parent}"
    source_parent="$(cd -- "${source_parent}" && pwd)"
    SOURCE_DIR="${source_parent}/${source_name}"

    trustee_dir="${SOURCE_DIR}/trustee"
    ccatoken_dir="${SOURCE_DIR}/rust-ccatoken"

    clone_tagged_repository \
        "${TRUSTEE_REPOSITORY}" \
        "${TRUSTEE_VERSION}" \
        "${trustee_dir}" \
        "trustee"
    clone_commit_repository \
        "${CCATOKEN_REPOSITORY}" \
        "${CCATOKEN_COMMIT}" \
        "${ccatoken_dir}" \
        "rust-ccatoken"

    apply_patch_once \
        "${ccatoken_dir}" \
        "${SDK_DIR}/patches/CCA-Token-Parsing-Adaptation-for-Kunpeng-Platform.patch"
    apply_cargo_patch_once \
        "${trustee_dir}/deps/verifier/Cargo.toml" \
        "trustee"
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --source-dir)
            [ "$#" -ge 2 ] || die "--source-dir requires a value"
            SOURCE_DIR="$2"
            shift 2
            ;;
        --builder-image)
            [ "$#" -ge 2 ] || die "--builder-image requires a value"
            BUILDER_IMAGE="$2"
            shift 2
            ;;
        --image)
            [ "$#" -ge 2 ] || die "--image requires a value"
            IMAGE_REPOSITORY="$2"
            shift 2
            ;;
        --tag)
            [ "$#" -ge 2 ] || die "--tag requires a value"
            IMAGE_TAG="$2"
            shift 2
            ;;
        --proxy-ca)
            [ "$#" -ge 2 ] || die "--proxy-ca requires a value"
            PROXY_CA_PATHS+=("$2")
            shift 2
            ;;
        --go-download-mirrors)
            [ "$#" -ge 2 ] || die "--go-download-mirrors requires a value"
            GO_DOWNLOAD_MIRRORS="$2"
            shift 2
            ;;
        --skip-source-prepare)
            SKIP_SOURCE_PREPARE="true"
            shift
            ;;
        --skip-compile)
            SKIP_COMPILE="true"
            shift
            ;;
        --push)
            PUSH_IMAGE="true"
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            die "unknown option: $1"
            ;;
    esac
done

command -v docker >/dev/null 2>&1 || die "docker is not installed"
command -v git >/dev/null 2>&1 || die "git is not installed"

HTTP_PROXY_VALUE="${http_proxy:-${HTTP_PROXY:-}}"
HTTPS_PROXY_VALUE="${https_proxy:-${HTTPS_PROXY:-}}"
NO_PROXY_VALUE="${no_proxy:-${NO_PROXY:-}}"

if [ "${SKIP_COMPILE}" = "false" ] \
    && [ "${#PROXY_CA_PATHS[@]}" -eq 0 ] \
    && { [ -n "${HTTP_PROXY_VALUE}" ] || [ -n "${HTTPS_PROXY_VALUE}" ]; }; then
    if [ -d "${DEFAULT_PROXY_CA_DIR}" ]; then
        PROXY_CA_PATHS+=("${DEFAULT_PROXY_CA_DIR}")
    else
        die "proxy is configured but ${DEFAULT_PROXY_CA_DIR} is unavailable; specify the gateway certificate with --proxy-ca"
    fi
fi

if [ "${SKIP_SOURCE_PREPARE}" = "false" ]; then
    echo "==> Preparing source repositories"
    prepare_source
else
    echo "==> Skipping source preparation"
    SOURCE_DIR_INPUT="${SOURCE_DIR}"
    if ! SOURCE_DIR="$(cd -- "${SOURCE_DIR_INPUT}" 2>/dev/null && pwd)"; then
        die "source directory does not exist: ${SOURCE_DIR_INPUT}"
    fi
fi
TRUSTEE_DIR="${SOURCE_DIR}/trustee"
ARTIFACT_DIR="${TRUSTEE_DIR}/target/release"
BUILDER_DOCKERFILE="${SDK_DIR}/config/Dockerfile"
FULL_IMAGE="${IMAGE_REPOSITORY}:${IMAGE_TAG}"
TEMP_BUILD_ROOT="$(mktemp -d)"
BUILDER_BUILD_CONTEXT="${TEMP_BUILD_ROOT}/builder"
RUNTIME_BUILD_CONTEXT="${TEMP_BUILD_ROOT}/runtime"
trap 'rm -rf -- "${TEMP_BUILD_ROOT}"' EXIT

[ -d "${TRUSTEE_DIR}" ] || die "trustee source not found: ${TRUSTEE_DIR}"
[ -f "${IMAGE_CONTEXT_DIR}/Dockerfile" ] \
    || die "runtime Dockerfile not found: ${IMAGE_CONTEXT_DIR}/Dockerfile"

if [ "${SKIP_COMPILE}" = "false" ]; then
    [ -f "${BUILDER_DOCKERFILE}" ] \
        || die "CoCo builder Dockerfile not found: ${BUILDER_DOCKERFILE}"

    mkdir -p "${BUILDER_BUILD_CONTEXT}"
    cp "${BUILDER_DOCKERFILE}" "${BUILDER_BUILD_CONTEXT}/Dockerfile"
    copy_proxy_ca_certificates "${BUILDER_BUILD_CONTEXT}/certs"

    build_args=()
    if [ -n "${HTTP_PROXY_VALUE}" ]; then
        build_args+=(--build-arg "http_proxy=${HTTP_PROXY_VALUE}")
    fi
    if [ -n "${HTTPS_PROXY_VALUE}" ]; then
        build_args+=(--build-arg "https_proxy=${HTTPS_PROXY_VALUE}")
    fi
    if [ -n "${NO_PROXY_VALUE}" ]; then
        build_args+=(--build-arg "no_proxy=${NO_PROXY_VALUE}")
    fi
    build_args+=(--build-arg "GO_DOWNLOAD_MIRRORS=${GO_DOWNLOAD_MIRRORS}")

    echo "==> Building CoCo builder image: ${BUILDER_IMAGE}"
    docker build \
        "${build_args[@]}" \
        --tag "${BUILDER_IMAGE}" \
        "${BUILDER_BUILD_CONTEXT}"

    run_env=()
    if [ -n "${HTTP_PROXY_VALUE}" ]; then
        run_env+=(--env "http_proxy=${HTTP_PROXY_VALUE}")
    fi
    if [ -n "${HTTPS_PROXY_VALUE}" ]; then
        run_env+=(--env "https_proxy=${HTTPS_PROXY_VALUE}")
    fi
    if [ -n "${NO_PROXY_VALUE}" ]; then
        run_env+=(--env "no_proxy=${NO_PROXY_VALUE}")
    fi

    echo "==> Compiling grpc-as and kbs"
    docker run --rm \
        "${run_env[@]}" \
        --volume "${SOURCE_DIR}:/coco" \
        --workdir /coco/trustee \
        "${BUILDER_IMAGE}" \
        /bin/bash -euo pipefail -c '
            make -C attestation-service VERIFIER=cca-verifier
            make -C kbs background-check-kbs COCO_AS_INTEGRATION_TYPE=grpc
        '
else
    echo "==> Skipping source compilation"
fi

for binary in kbs grpc-as; do
    [ -x "${ARTIFACT_DIR}/${binary}" ] \
        || die "build artifact is missing or not executable: ${ARTIFACT_DIR}/${binary}"
done

mkdir -p "${RUNTIME_BUILD_CONTEXT}/bin"

cp "${IMAGE_CONTEXT_DIR}/Dockerfile" "${RUNTIME_BUILD_CONTEXT}/Dockerfile"
cp "${IMAGE_CONTEXT_DIR}/entrypoint.sh" "${RUNTIME_BUILD_CONTEXT}/entrypoint.sh"
cp \
    "${ARTIFACT_DIR}/kbs" \
    "${ARTIFACT_DIR}/grpc-as" \
    "${RUNTIME_BUILD_CONTEXT}/bin/"

echo "==> Building remote attestation image: ${FULL_IMAGE}"
runtime_build_args=()
if [ -n "${HTTP_PROXY_VALUE}" ]; then
    runtime_build_args+=(--build-arg "http_proxy=${HTTP_PROXY_VALUE}")
fi
if [ -n "${HTTPS_PROXY_VALUE}" ]; then
    runtime_build_args+=(--build-arg "https_proxy=${HTTPS_PROXY_VALUE}")
fi
if [ -n "${NO_PROXY_VALUE}" ]; then
    runtime_build_args+=(--build-arg "no_proxy=${NO_PROXY_VALUE}")
fi

docker build \
    "${runtime_build_args[@]}" \
    --tag "${FULL_IMAGE}" \
    "${RUNTIME_BUILD_CONTEXT}"

if [ "${PUSH_IMAGE}" = "true" ]; then
    echo "==> Pushing remote attestation image: ${FULL_IMAGE}"
    docker push "${FULL_IMAGE}"
fi

echo "==> Image ready: ${FULL_IMAGE}"
