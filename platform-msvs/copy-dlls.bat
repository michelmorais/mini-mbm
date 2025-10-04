rem -> expected destination folder
rem -> command line: Call $(SolutionDir)copy-dlls.bat $(OutDir)
@echo off
setlocal enabledelayedexpansion

set destinationFolder=%1
set is64bits=%2

pushd %~dp0
pushd "..\"
set mypath=%CD%
popd
popd

echo !mypath!
echo "may mypath is !mypath!"

if %is64bits%=="x64" (

echo "64 bits build"
echo "Copying needed 64 bits DLLs (for Windows) to  %destinationFolder%"
    
set audiere_source=%mypath%\third-party\audiere-1.9.4\bin\x64\audiere.dll
set audiere_lib=%mypath%\third-party\audiere-1.9.4\bin\x64\audiere.lib

set libEGL_dll=%mypath%\third-party\gles\bin\libEGL.dll
set libEGL_source=%mypath%\third-party\gles\libs\libEGL.dll.lib

set libEGLv2_dll=%mypath%\third-party\gles\bin\libGLESv2.dll
set libEGLv2_source=%mypath%\third-party\gles\libs\libGLESv2.dll.lib

echo "COPY !audiere_source! !destinationFolder!"
COPY "!audiere_source!" "!destinationFolder!"

echo "COPY !audiere_lib! !destinationFolder!"
COPY "!audiere_lib!" "!destinationFolder!"

echo "COPY !libEGL_source! !destinationFolder!"
COPY "!libEGL_source!" "!destinationFolder!"

echo "COPY !libEGL_dll! !destinationFolder!"
COPY "!libEGL_dll!" "!destinationFolder!"

echo "COPY !libEGLv2_source! !destinationFolder!"
COPY "!libEGLv2_source!" "!destinationFolder!"

echo "COPY !libEGLv2_dll! !destinationFolder!"
COPY "!libEGLv2_dll!" "!destinationFolder!"

) else (
    echo "32 bits build"
    echo "Copying needed 32 bits DLLs (for Windows) to  %destinationFolder%"

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

    rem -> get mini-mbm-lib.h path
    pushd %mypath%\mini-mbm-launcher\
    set mini_mbm_lib_source=%CD%\mini-mbm-lib.h
    popd

    rem -> get editor path
    pushd %mypath%\..\editor\
    set editor_source=%CD%\
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

    rem -> set mini_mbm_lib path
    pushd %destinationFolder%
    set mini_mbm_lib_destin=%CD%\mini-mbm-lib.h
    popd

    rem -> set editor path
    pushd %destinationFolder%
    set editor_destin=%CD%\
    popd

    echo "COPY %d3dcompiler_source% %d3dcompiler_destin%"
    COPY "%d3dcompiler_source%" "%d3dcompiler_destin%"

    echo "COPY %libEGL_source% %libEGL_destin%"
    COPY "%libEGL_source%" "%libEGL_destin%"

    echo "COPY %libGLESv2_source% %libGLESv2_destin%"
    COPY "%libGLESv2_source%" "%libGLESv2_destin%"

    echo "COPY %audiere_source% %audiere_destin%"
    COPY "%audiere_source%" "%audiere_destin%"

    echo "COPY %mini_mbm_lib_source% %mini_mbm_lib_dest%"
    COPY "%mini_mbm_lib_source%" "%mini_mbm_lib_destin%"

    echo "Copying Editor LUA to  %destinationFolder%"

    echo "XCOPY %editor_source% %editor_dest%"
    XCOPY "%editor_source%" "%editor_destin%" /E /I /Y
)


exit /b 0

