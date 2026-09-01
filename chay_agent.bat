@echo off
where wsl >nul 2>nul
if errorlevel 1 (
    echo Khong tim thay WSL tren may nay.
    echo Hay lam theo huong dan cai WSL truoc, sau do chay lai file nay.
    pause
    exit /b 1
)
wsl.exe bash -c "bash $HOME/mo_du_an.sh"
