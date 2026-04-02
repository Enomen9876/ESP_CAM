@echo off
setlocal enabledelayedexpansion

REM --- Ping esp-kamera.local once and capture output ---
for /f "tokens=2 delims=[]" %%a in ('ping -n 1 esp-kamera.local ^| findstr "["') do (
    set "IP=%%a"
)

REM --- Check if IP was found ---
if defined IP (
    echo ESP32 IP: !IP!
    REM --- Open default browser with IP ---
    start "" "http://!IP!"
) else (
    echo esp-kamera.local could not be resolved!
    pause
)