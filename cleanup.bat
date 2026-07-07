@echo off
echo === 清理 DevEco Studio 残留进程 ===

echo [1/3] 杀掉 node.exe（预览器/模拟器 Node 服务）
taskkill /f /im node.exe 2>nul

echo [2/3] 重启 hdc（鸿蒙设备连接器）
hdc kill 2>nul
timeout /t 2 /nobreak >nul
hdc start 2>nul

echo [3/3] 检查残留端口...
netstat -ano | findstr "37000 37001 37002" 2>nul
if %errorlevel% equ 0 (
    echo ⚠ 仍有端口占用，请手动检查
) else (
    echo ✅ 端口已释放
)

echo === 清理完成，可以重新运行了 ===
pause
