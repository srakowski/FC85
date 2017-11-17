@echo off
set platform=%1
set varspath=%2

echo starting windows %platform% build

echo creating output directories
mkdir .\bin\win\%platform%
mkdir .\obj\win\%platform%

echo setting build variables
call %varspath%

cl /TC /GL /WX /W3 /DEBUG /Zi /D "_CRT_SECURE_NO_WARNINGS"^
  .\src\fc85.c^
  /Fo:".\obj\win\%platform%\fc85.obj"^
  /Fe:".\bin\win\%platform%\fc85.exe"^
  /I ".\lib\sdl2\include"^
  /I ".\lib\lua\include"^
  /link ".\lib\sdl2\lib\win\%platform%\SDL2.lib"^
  ".\lib\lua\lib\%platform%\lua53.lib"


REM cl /TC /GL /WX /W3 /O2 /Os /D "_CRT_SECURE_NO_WARNINGS"^
REM   .\src\fc85.c^
REM   /Fo:".\obj\win\%platform%\fc85.obj"^
REM   /Fe:".\bin\win\%platform%\fc85.exe"^
REM   /I ".\lib\sdl2\include"^
REM   /link ".\lib\sdl2\lib\win\%platform%\SDL2.lib"

echo copying sdl2 .dll to output directory
copy .\lib\sdl2\lib\win\%platform%\SDL2.dll .\bin\win\%platform%\SDL2.dll

echo build complete
