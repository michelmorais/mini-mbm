rem -> expected destination folder
rem -> command line: Call $(SolutionDir)copy-dlls.bat $(OutDir)
@echo off

set destinationFolder=%1

echo "Copying needed DLLs (for Windows) to  %destinationFolder%"

rem -> get current path
pushd %~dp0
set mypath=%CD%
popd

rem -> get d3dcompiler_source path
pushd %mypath%\..\third-party\gles\bin\
set d3dcompiler_source=%CD%\d3dcompiler_47.dll
popd

rem -> get libEGL path
pushd %mypath%\..\third-party\gles\bin\
set libEGL_source=%CD%\libEGL.dll
popd

rem -> get libGLESv2 path
pushd %mypath%\..\third-party\gles\bin\
set libGLESv2_source=%CD%\libGLESv2.dll
popd

rem -> get audiere path
pushd %mypath%\..\third-party\audiere-1.9.4\bin
set audiere_source=%CD%\audiere.dll
popd


rem -> destination -------------------------------

rem -> set d3dcompiler_destin path
pushd %destinationFolder%
set d3dcompiler_destin=%CD%\d3dcompiler_47.dll
popd

rem -> set libEGL_destin path
pushd %destinationFolder%
set libEGL_destin=%CD%\libEGL.dll
popd

rem -> set libGLESv2_destin path
pushd %destinationFolder%
set libGLESv2_destin=%CD%\libGLESv2.dll
popd

rem -> set audiere_destin path
pushd %destinationFolder%
set audiere_destin=%CD%\audiere.dll
popd

echo "COPY %d3dcompiler_source% %d3dcompiler_destin%"
COPY "%d3dcompiler_source%" "%d3dcompiler_destin%"

echo "COPY %libEGL_source% %libEGL_destin%"
COPY "%libEGL_source%" "%libEGL_destin%"

echo "COPY %libGLESv2_source% %libGLESv2_destin%"
COPY "%libGLESv2_source%" "%libGLESv2_destin%"

echo "COPY %audiere_source% %audiere_destin%"
COPY "%audiere_source%" "%audiere_destin%"

exit /b 0

