#!/bin/bash
# Script to install and build kernel and rootfs for QEMU
# Author: Siddhant Jajoo

set -e
set -u

OUTDIR=/tmp/aeld-autograder
KERNEL_REPO=https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.1.10
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

# 如果系统中使用的工具链前缀是 aarch64-linux-gnu-，则自动适配
if ! command -v ${CROSS_COMPILE}gcc &> /dev/null; then
    CROSS_COMPILE=aarch64-linux-gnu-
fi

# The test environment may not provide an interactive sudo session.  All
# output is created in user-writable directories, so privileged operations
# are optional.
SUDO=""
if [ "$(id -u)" -ne 0 ] && command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
    SUDO="sudo -n"
fi

# Git servers occasionally terminate long HTTPS transfers on the self-hosted
# runner.  Retry cleanly with HTTP/1.1 so a partial clone is never reused.
clone_release() {
    local url="$1"
    local ref="$2"
    local destination="$3"
    local attempt

    for attempt in 1 2 3; do
        rm -rf "${destination}"
        echo "Cloning ${url} (${ref}), attempt ${attempt}/3"
        if git -c http.version=HTTP/1.1 clone "${url}" \
            --depth 1 --single-branch --branch "${ref}" "${destination}"; then
            return 0
        fi
        echo "Clone attempt ${attempt} failed; retrying..." >&2
        sleep 5
    done

    echo "Unable to clone ${url} after 3 attempts" >&2
    return 1
}

if [ $# -lt 1 ]
then
    echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p ${OUTDIR}

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable/.git" ] || \
   ! git -C "${OUTDIR}/linux-stable" rev-parse --verify "${KERNEL_VERSION}^{commit}" >/dev/null 2>&1; then
    echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
    clone_release "${KERNEL_REPO}" "${KERNEL_VERSION}" "${OUTDIR}/linux-stable"
fi

if [ ! -e ${OUTDIR}/Image ]; then
    cd linux-stable
    echo "Checking out version ${KERNEL_VERSION}"
    git checkout ${KERNEL_VERSION}

    echo "Building the Linux kernel..."
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

    # 修复 GCC 10+ 导致的 yylloc 重复定义错误
    # 清理之前可能的编译产物
    rm -rf scripts/dtc/*.o scripts/dtc/*.c_shipped scripts/dtc/dtc 2>/dev/null || true
    # 修改 dtc-lexer.l 中的 yylloc 定义为 extern
    if [ -f "scripts/dtc/dtc-lexer.l" ]; then
        sed -i 's/^YYLTYPE yylloc;/extern YYLTYPE yylloc;/' scripts/dtc/dtc-lexer.l
    fi

    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs

    echo "Copying Image to ${OUTDIR}"
    cp arch/${ARCH}/boot/Image ${OUTDIR}/
fi

echo "Adding the Gateway to the Rootfs"
cd "$OUTDIR"
if [ -d "${OUTDIR}/rootfs" ]
then
    echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
    rm -rf "${OUTDIR}/rootfs"
fi

# 1. 创建 rootfs 目录结构
mkdir -p ${OUTDIR}/rootfs
cd ${OUTDIR}/rootfs
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log
mkdir -p home/conf conf

# 2. 编译并安装 Busybox
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox/.git" ] || \
   ! git -C "${OUTDIR}/busybox" rev-parse --verify "${BUSYBOX_VERSION}^{commit}" >/dev/null 2>&1
then
    # Only the pinned release is needed; cloning the full BusyBox history can
    # leave the self-hosted runner processing hundreds of thousands of objects.
    clone_release "https://git.busybox.net/busybox.git" "${BUSYBOX_VERSION}" "${OUTDIR}/busybox"
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make defconfig
else
    cd busybox
fi

make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

# 3. Copy the shared libraries required by the cross-compiled binaries.
cd ${OUTDIR}/rootfs
copy_cross_library() {
    local library="$1"
    local destination="$2"
    local source
    source=$(${CROSS_COMPILE}gcc -print-file-name="${library}")
    if [ ! -f "${source}" ]; then
        echo "Missing cross-compiler runtime library ${library} (${source})" >&2
        exit 1
    fi
    cp -L "${source}" "${destination}/"
}

# The cross toolchain's AArch64 loader searches /lib by default.  Keep the
# loader and its glibc dependencies together so dynamically linked programs
# such as BusyBox and writer can start in the minimal rootfs.
copy_cross_library ld-linux-aarch64.so.1 lib
copy_cross_library libc.so.6 lib
copy_cross_library libm.so.6 lib
copy_cross_library libresolv.so.2 lib

# 4. Create device nodes when permitted.  The QEMU init script mounts
# devtmpfs as a fallback for unprivileged build environments.
if ${SUDO} mknod -m 666 dev/null c 1 3 2>/dev/null; then
    ${SUDO} mknod -m 600 dev/console c 5 1
else
    echo "Unable to create static device nodes; devtmpfs will provide /dev at boot"
fi

# 5. 编译并安装 writer 工具与测试脚本
cd ${FINDER_APP_DIR}
make clean
make CROSS_COMPILE=${CROSS_COMPILE}

# 复制应用程序和运行脚本到 rootfs 的 /home 目录
cp writer ${OUTDIR}/rootfs/home/
cp finder.sh ${OUTDIR}/rootfs/home/
cp finder-test.sh ${OUTDIR}/rootfs/home/
cp autorun-qemu.sh ${OUTDIR}/rootfs/home/
cp conf/username.txt ${OUTDIR}/rootfs/home/conf/
cp conf/assignment.txt ${OUTDIR}/rootfs/home/conf/
cp conf/username.txt ${OUTDIR}/rootfs/conf/
cp conf/assignment.txt ${OUTDIR}/rootfs/conf/

# 修改 finder-test.sh 中的路径以适配 QEMU rootfs 运行环境
sed -i 's|\.\./conf/assignment.txt|conf/assignment.txt|g' ${OUTDIR}/rootfs/home/finder-test.sh

# 6. Package the initramfs.  cpio records root ownership without requiring
# ownership changes in the staging directory.
cd ${OUTDIR}/rootfs
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
