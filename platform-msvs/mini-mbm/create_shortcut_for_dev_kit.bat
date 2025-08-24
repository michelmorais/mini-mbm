rem -> Create shortcut for dev tool-kit :)
rem -> Is expected all editors and executable in this same folder
@echo on

set nickName=Liquid-Fun-MiniMBM-dev-tool-kit

echo "Creating shortcut on desktop for %nickName%"

rem -> get current path
pushd %~dp0
set mypath=%CD%
popd

rem -> get minimbm path
pushd %mypath%\..\Release\
set devpath=%CD%
set minimbm=%CD%\mini-mbm-dev.exe
popd

rem -> temp file
set SCRIPT="%TEMP%\%RANDOM%-%RANDOM%-%RANDOM%-%RANDOM%.vbs"

echo Set oWS = WScript.CreateObject("WScript.Shell") >> %SCRIPT%
echo sLinkFile = "%USERPROFILE%\Desktop\%nickName%.lnk" >> %SCRIPT%
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> %SCRIPT%
echo oLink.TargetPath = "%minimbm%" >> %SCRIPT%
echo Arguments="--addpath %devpath%"  >> %SCRIPT%
echo Arguments = Replace(Arguments, Chr(39), Chr(34)) >> %SCRIPT%
echo oLink.Arguments = Arguments >> %SCRIPT%
echo oLink.Save >> %SCRIPT%

rem -> call temp file
cscript /nologo %SCRIPT%

rem -> delete temp file
del %SCRIPT%
rem echo SCRIPT: %SCRIPT%
@echo on
EXIT /B 0