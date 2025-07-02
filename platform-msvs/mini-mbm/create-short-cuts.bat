rem -> expected nickName lua_script args"(-w 800 -h 600 -nosplash)"
@echo off

set nickName=%1
set script=%2
set args=%~3

echo "Creating desktop shortcut for %nickName%"

rem -> get current path
pushd %~dp0
set mypath=%CD%
popd

rem -> get minimbm path
pushd %mypath%\..\Release\
set minimbm=%CD%\mini_mbm.exe
popd

rem -> get lua editor scripts path
pushd %mypath%\..\..\editor
set lua_editor=%CD%\%script%
popd

rem -> temp file
set SCRIPT="%TEMP%\%RANDOM%-%RANDOM%-%RANDOM%-%RANDOM%.vbs"

echo Set oWS = WScript.CreateObject("WScript.Shell") >> %SCRIPT%
echo sLinkFile = "%USERPROFILE%\Desktop\%nickName%.lnk" >> %SCRIPT%
echo Set oLink = oWS.CreateShortcut(sLinkFile) >> %SCRIPT%
echo oLink.TargetPath = "%minimbm%" >> %SCRIPT%
echo Arguments="-scene '%lua_editor%' %args% -name '%nickName%'"  >> %SCRIPT%
echo Arguments = Replace(Arguments, Chr(39), Chr(34)) >> %SCRIPT%
echo oLink.Arguments = Arguments >> %SCRIPT%
echo oLink.Save >> %SCRIPT%

rem -> call temp file
cscript /nologo %SCRIPT%

rem -> delete temp file
del %SCRIPT%
@echo on
exit /b 0