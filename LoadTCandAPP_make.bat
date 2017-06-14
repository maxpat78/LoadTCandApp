@echo off
cl -DVERA_CRYPT -I. -MD -Ox -EHsc LoadTCandAPP.cpp
call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\bin\amd64\vcvars64.bat"
cl -DVERA_CRYPT -I. -MD -Ox -EHsc -FeLoadTCandAPP64.exe LoadTCandAPP.cpp
del LoadTCandAPP*.obj
7z a Load_TC_and_APP.7z LoadTCandAPP*.*
