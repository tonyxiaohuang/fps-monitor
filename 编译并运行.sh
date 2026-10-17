#!/system/bin/sh
# =============================================================
# fps_monitor 编译 + 部署 + 运行脚本（AIDE 的 ndk-build）
# 用法： su -c "sh /storage/emulated/0/fps_monitor/编译并运行.sh"
# =============================================================

# AIDE 自带的 ndk-build（不同版本目录名可能不同，如报错请改这里）
NDK_BUILD="/data/user/0/com.idragoncheats.studiopro/no_backup/ndksupport-1710240003/android-ndk-aide/ndk-build"

OUTPUT_DIR="/data/local/tmp"

cd "$(dirname "$0")" || exit 1
mkdir -p "$OUTPUT_DIR"

echo "[1/3] 编译中..."
"$NDK_BUILD"

if [ $? -ne 0 ]; then
    echo "编译失败."
    exit 1
fi

echo "[2/3] 部署到 $OUTPUT_DIR ..."
if [ -d "./libs" ]; then
    find ./libs -type f -exec cp -f {} "$OUTPUT_DIR/" \;
    chmod 755 "$OUTPUT_DIR/fps_monitor"
    echo "已部署: $OUTPUT_DIR/fps_monitor"
else
    echo "未找到 libs 目录."
    exit 1
fi

echo "[3/3] 运行（需要 root）..."
if [ "$(id -u)" = "0" ]; then
    "$OUTPUT_DIR/fps_monitor"
else
    echo "当前非 root，使用 su 运行："
    su -c "$OUTPUT_DIR/fps_monitor"
fi
