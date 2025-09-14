@echo off
setlocal
set "PROJ=%~dp0ThirdPersonCameraMan.uproject"
call "C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" ThirdPersonCameraManEditor Win64 Development -Project="%PROJ%" -WaitMutex -FromMsBuild
exit /b %errorlevel%
