@echo off
REM SOCS_HLS目录整理脚本
REM 用于整理source/SOCS_HLS目录，移动散落的文档到doc目录

echo ========================================
echo SOCS_HLS 目录整理工具
echo ========================================
echo.

REM 设置Python路径
set PYTHON_PATH=python
set SCRIPT_DIR=%~dp0
set WORKSPACE_ROOT=e:\fpga-litho-accel

echo 工作空间: %WORKSPACE_ROOT%
echo 脚本目录: %SCRIPT_DIR%
echo.

REM 检查Python是否可用
%PYTHON_PATH% --version >nul 2>&1
if errorlevel 1 (
    echo 错误: Python未找到或未安装
    echo 请确保Python已安装并添加到PATH
    pause
    exit /b 1
)

REM 运行目录整理脚本
echo 开始整理目录...
echo.
%PYTHON_PATH% "%SCRIPT_DIR%organize_directory.py"

if errorlevel 1 (
    echo.
    echo 错误: 目录整理失败
    pause
    exit /b 1
)

echo.
echo ========================================
echo 目录整理完成
echo ========================================
echo.
echo 整理结果:
echo - 散落的文档已移动到 doc/ 目录
echo - 目录结构已优化
echo - 文档索引已创建
echo.
echo 下一步:
echo 1. 检查 doc/ 目录中的文档
echo 2. 验证文件移动是否正确
echo 3. 更新相关引用路径
echo.

pause