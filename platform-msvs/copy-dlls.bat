rem -> expected destination folder
rem -> command line: Call $(SolutionDir)copy-dlls.bat $(OutDir)

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

    set audiere_source=%mypath%\third-party\audiere-1.9.4\bin\audiere.dll
    set audiere_lib=%mypath%\third-party\audiere-1.9.4\lib\audiere.lib

    set libEGL_dll=%mypath%\third-party\gles\bin\libEGL.dll
    set libEGL_source=%mypath%\third-party\gles\libs\libEGL.dll.lib

    set libEGLv2_dll=%mypath%\third-party\gles\bin\libGLESv2.dll
    set libEGLv2_source=%mypath%\third-party\gles\libs\libGLESv2.dll.lib

    set d3dcompiler_source=%mypath%\third-party\gles\bin\d3dcompiler_47.dll

    set mini_mbm_lib_source=%mypath%\src\mini-mbm-lib\mini-mbm-lib.h

    set editor_source=%mypath%\editor

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

    echo "COPY !d3dcompiler_source! !destinationFolder!"
    COPY "!d3dcompiler_source!" "!destinationFolder!"

    echo "COPY !mini_mbm_lib_source! !destinationFolder!"
    COPY "!mini_mbm_lib_source!" "!destinationFolder!"

    echo "XCOPY !editor_source! !destinationFolder!"
    XCOPY "!editor_source!" "!destinationFolder!" /E /I /Y
)


exit /b 0

