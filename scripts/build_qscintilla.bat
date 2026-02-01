@echo off
set "PATH=C:\Qt\Tools\mingw1120_64\bin;C:\Qt\6.5.2\mingw_64\bin;%PATH%"
cd /d "E:\game_dev\util\reclass2027-main\third_party\qscintilla\src"
echo Current dir: %cd%
echo Running qmake...
"C:\Qt\6.5.2\mingw_64\bin\qmake.exe" qscintilla.pro "CONFIG+=staticlib"
echo qmake exit code: %errorlevel%
echo Running mingw32-make...
"C:\Qt\Tools\mingw1120_64\bin\mingw32-make.exe" -j8
echo make exit code: %errorlevel%
echo Done. Checking for .a files:
dir /b *.a 2>nul
dir /b release\*.a 2>nul
