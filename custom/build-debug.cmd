@echo off
setlocal
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Qt\6.11.1\msvc2022_64\bin;C:\gstreamer\1.0\msvc_x86_64\bin;%PATH%"
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.44
if errorlevel 1 exit /b %errorlevel%
set "GSTREAMER_1_0_ROOT_MSVC_X86_64=C:\gstreamer\1.0\msvc_x86_64"
set "GSTREAMER_1_0_ROOT_X86_64=C:\gstreamer\1.0\msvc_x86_64"
for %%I in ("%~dp0..") do set "QGC_SOURCE=%%~fI"
set "QGC_BUILD=%QGC_SOURCE%\build-herelink-debug"
if "%~1"=="test" goto test
if "%~1"=="ui-test" goto uitest
if "%~1"=="run" goto run
call qt-cmake.bat -S "%QGC_SOURCE%" -B "%QGC_BUILD%" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DQGC_BUILD_TESTING=ON -DQGC_ENABLE_GST_VIDEOSTREAMING=ON -DPython3_EXECUTABLE="%QGC_SOURCE%\.venv\Scripts\python.exe"
if errorlevel 1 exit /b %errorlevel%
cmake --build "%QGC_BUILD%" --parallel
exit /b %errorlevel%
:test
set "QGC_TEST_VERBOSE=1"
set "QT_FORCE_STDERR_LOGGING=1"
ctest --test-dir "%QGC_BUILD%" --output-on-failure -L Unit
exit /b %errorlevel%
:uitest
set "QGC_TEST_VERBOSE=1"
set "QT_FORCE_STDERR_LOGGING=1"
ctest --test-dir "%QGC_BUILD%" --output-on-failure -R "^HerelinkFlyViewTest$"
exit /b %errorlevel%
:run
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run-debug.ps1"
exit /b %errorlevel%
