@echo off
echo Building SysvadLoopback_Dev driver with compatibility options...

rem Clean any previous build
msbuild /t:Clean /nologo

rem Build with special options to bypass compatibility issues
msbuild /nologo /p:AdditionalOptions="/FIprecomp.h /wd4603 /wd4627 /wd4986 /wd4987 /Zc:wchar_t- /Zc:forScope- /Zc:rvalueCast-" /p:LanguageStandard=stdcpp14

rem Report build status
if %ERRORLEVEL% EQU 0 (
    echo Build completed successfully!
) else (
    echo Build failed with error level %ERRORLEVEL%.
)
