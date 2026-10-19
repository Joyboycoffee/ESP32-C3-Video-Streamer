@echo off
echo ===================================================
echo   Building Standalone ESP32-C3 Video Streamer EXE
echo ===================================================
echo.
py -m pip install -r requirements.txt pyinstaller
py -m PyInstaller --onefile --noconsole --name "ESP32-C3-Video-Streamer" streamer_gui.py
echo.
echo Build complete! The executable is located at dist\ESP32-C3-Video-Streamer.exe
pause
