@echo off
REM ============================================================
REM  Chuandi Demo 录制启动器
REM  作用：以「独立游戏模式」启动，画面干净（无编辑器 UI），
REM        窗口固定 1920x1080，方便 OBS / 录屏软件捕获。
REM  用法：双击本文件即可。
REM ============================================================
set "ENGINE=D:\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
set "PROJECT=D:\UnrealProjects\Chuandi\Chuandi.uproject"

if not exist "%ENGINE%" (
    echo [错误] 找不到引擎：%ENGINE%
    pause
    exit /b 1
)
if not exist "%PROJECT%" (
    echo [错误] 找不到项目：%PROJECT%
    pause
    exit /b 1
)

REM -game        独立游戏模式（不是编辑器）
REM -windowed    窗口化（OBS 用「窗口捕获」最稳；想全屏改 -fullscreen）
REM -resx/-resy  分辨率
REM 若默认进错地图，可在末尾加： -map=/Game/Chuandi/Maps/你的关卡
"%ENGINE%" "%PROJECT%" -game -windowed -resx=1920 -resy=1080
