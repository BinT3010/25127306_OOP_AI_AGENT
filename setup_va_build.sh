#!/usr/bin/env bash
# ============================================================================
# setup_va_build.sh
# Script tu dong cai dat + build + chay thu du an AI Agent (OOP).
# Cach dung: mo Ubuntu (trong Windows, qua WSL), go:
#     bash setup_va_build.sh
# ============================================================================
set -e

PROJECT_NAME="Agent_MSSV1_MSSV2_MSSV3"
ZIP_NAME="${PROJECT_NAME}.zip"

echo "======================================================"
echo " BUOC 1/5: Cai dat cac goi can thiet (can mang + mat khau)"
echo "======================================================"
sudo apt update
sudo apt install -y g++-14 gcc-14 cmake libcurl4-openssl-dev libsqlite3-dev nlohmann-json3-dev unzip

echo ""
echo "======================================================"
echo " BUOC 2/5: Tim va giai nen du an"
echo "======================================================"
cd "$HOME"

if [ -d "$HOME/$PROJECT_NAME" ]; then
    echo "-> Da thay thu muc $HOME/$PROJECT_NAME san co, bo qua buoc giai nen."
else
    ZIP_PATH=""
    # Tim file zip o vai cho pho bien: thu muc home hien tai, va Downloads/Desktop
    # cua TAT CA user Windows (vi ten dang nhap Windows va ten dang nhap Ubuntu
    # co the khac nhau).
    SEARCH_LOCATIONS=(
        "$HOME/$ZIP_NAME"
        /mnt/c/Users/*/Downloads/"$ZIP_NAME"
        /mnt/c/Users/*/Desktop/"$ZIP_NAME"
    )
    for candidate in "${SEARCH_LOCATIONS[@]}"; do
        if [ -f "$candidate" ]; then
            ZIP_PATH="$candidate"
            break
        fi
    done

    if [ -z "$ZIP_PATH" ]; then
        echo ""
        echo "!! KHONG TIM THAY file $ZIP_NAME trong Downloads/Desktop cua Windows."
        echo "!! Hay lam thu cong: mo Ubuntu, go lenh sau (doi duong dan cho dung):"
        echo "     cp /mnt/c/Users/TEN_WINDOWS_CUA_BAN/Downloads/$ZIP_NAME ~/"
        echo "!! Roi chay lai: bash setup_va_build.sh"
        exit 1
    fi

    echo "-> Tim thay: $ZIP_PATH"
    cp "$ZIP_PATH" "$HOME/"
    cd "$HOME"
    unzip -q "$ZIP_NAME"
    echo "-> Da giai nen xong vao $HOME/$PROJECT_NAME"
fi

cd "$HOME/$PROJECT_NAME"

echo ""
echo "======================================================"
echo " BUOC 3/5: Don dep ban build cu (neu co) de build lai sach"
echo "======================================================"
rm -rf build
mkdir build
cd build

echo ""
echo "======================================================"
echo " BUOC 4/5: Bien dich (build) du an"
echo "======================================================"
cmake .. -DCMAKE_CXX_COMPILER=g++-14
make -j"$(nproc)"

echo ""
echo "======================================================"
echo " BUOC 5/5: Chay thu bo test tu dong"
echo "======================================================"
cd "$HOME/$PROJECT_NAME"
./build/bin/run_tests

echo ""
echo "======================================================"
echo " XONG! Neu ban thay dong 'Status: SUCCESS' o tren la moi"
echo " thu da chay dung. Du an nam o: $HOME/$PROJECT_NAME"
echo ""
echo " Thu chay agent:"
echo "   ./build/bin/agent --mock \"Tinh 15 nhan 17\""
echo "======================================================"

# Tao san mot script nho de lan sau chi can double-click file .bat ben Windows
# la tu dong nhay thang vao dung thu muc du an, khong can go lai duong dan.
cat > "$HOME/mo_du_an.sh" << 'HELPEREOF'
#!/usr/bin/env bash
cd "$HOME/Agent_MSSV1_MSSV2_MSSV3" 2>/dev/null
if [ "$PWD" = "$HOME" ]; then
    echo "Khong tim thay thu muc du an. Hay chay lai: bash setup_va_build.sh"
else
    echo "Da vao thu muc du an. Vi du go lenh:"
    echo "  ./build/bin/agent --mock \"Tinh 15 nhan 17\""
    echo "  ./build/bin/run_eval --mock"
    echo "  ./build/bin/run_tests"
fi
exec bash
HELPEREOF
chmod +x "$HOME/mo_du_an.sh"
echo ""
echo "(Da tao san $HOME/mo_du_an.sh -- dung file chay_agent.bat ben Windows"
echo " de lan sau mo thang vao day, khong can go lai tu dau.)"
