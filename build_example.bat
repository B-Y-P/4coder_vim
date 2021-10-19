@echo off
call ..\custom\bin\buildsuper_x64-win.bat .\4coder_vim_example.cpp %1
copy .\custom_4coder.* ..\custom_4coder.*
