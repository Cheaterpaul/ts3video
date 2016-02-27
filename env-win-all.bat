@echo off

rem List of paths
set OCS_BUILD_DIR_PATH=%~dp0%OCS_BUILD_DIR_NAME%
set OCS_DEPLOY_DIR_PATH=%OCS_BUILD_DIR_PATH%\deploy
set OCS_SETUP_DIR_PATH=%OCS_BUILD_DIR_PATH%\setup

rem Print used environment
echo
echo OCS Build Environment
echo ---------------------
echo OCS_BUILD_DIR_PATH  : %OCS_BUILD_DIR_PATH%
echo OCS_DEPLOY_DIR_PATH : %OCS_DEPLOY_DIR_PATH%
echo OCS_SETUP_DIR_PATH  : %OCS_SETUP_DIR_PATH%
echo ---------------------
echo 
