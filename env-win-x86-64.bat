@echo off
call "%VS140COMNTOOLS%\..\..\Vc\vcvarsall.bat" x64

rem Prepare basic environment.
set MF_BUILD_DIR_NAME=build-win-x86-64
set MF_BUILD_DIR_PATH=%~dp0%MF_BUILD_DIR_NAME%
set QTDIR=G:\Libraries\Qt\qt-5.5.1-x86-64-vc14-opengl-openssl-build
set PATH=%PATH%;%QTDIR%\bin
