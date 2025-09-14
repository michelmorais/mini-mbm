rem -> 
@echo off

echo "Removed unsued file from release folder"

rem -> get current path
pushd %~dp0
set mypath=%CD%
popd

rem ->remove all *.exp files in Release folder
pushd %mypath%\..\Release\
del %CD%\*.exp
popd

rem ->remove all *.pdb files in Release folder
pushd %mypath%\..\Release\
del %CD%\*.pdb
popd