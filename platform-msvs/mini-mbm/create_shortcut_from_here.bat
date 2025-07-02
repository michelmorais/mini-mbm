rem -> Create shortcut for editors :)
rem -> Is expected all editors and executable in this same folder
@echo off

SETLOCAL
@echo off
set default_args="-w 800 -h 600 -nosplash --showconsole"
CALL :CreateShortcutFromHere scene_editor,     scene_editor2d.lua,   %default_args%
CALL :CreateShortcutFromHere sprite_maker,     sprite_maker.lua,     %default_args%
CALL :CreateShortcutFromHere shader_editor,    shader_editor.lua,    %default_args%
CALL :CreateShortcutFromHere particle_editor,  particle_editor.lua,  %default_args%
CALL :CreateShortcutFromHere physic_editor,    physic_editor.lua,    %default_args%
CALL :CreateShortcutFromHere font_maker,       font_maker.lua,       %default_args%
CALL :CreateShortcutFromHere asset_packager,   asset_packager.lua,   %default_args%
CALL :CreateShortcutFromHere tilemap_editor,   tilemap_editor.lua,   %default_args%
CALL :CreateShortcutFromHere texture_packer,   texture_packer.lua,   %default_args%

PAUSE
EXIT /B %ERRORLEVEL%

:CreateShortcutFromHere
@echo off
set nickName=%1
set script=%2
set args=%~3

echo "Creating shortcut on desktop for %nickName%"

rem -> get current path
pushd %~dp0
set mypath=%CD%
popd

rem -> get minimbm path
pushd %mypath%
set minimbm=%CD%\mini_mbm.exe
popd

rem -> get lua editor scripts path
pushd %mypath%
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
EXIT /B 0