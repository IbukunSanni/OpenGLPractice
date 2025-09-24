@echo off
echo OpenGL Project Builder
echo ======================

if "%1"=="" (
    echo Usage: build_project.bat [project_name]
    echo.
    echo Available projects:
    echo   triangle     - Run the pulsing triangle animation
    echo   experiment   - Run new experiment template
    echo   main         - Run the original Main.cpp
    echo.
    goto :eof
)

if "%1"=="triangle" (
    echo Building Triangle Animation...
    msbuild OpenGLPractice.sln /p:Configuration=TriangleAnimation /p:Platform=x64
    if %ERRORLEVEL%==0 (
        echo Starting Triangle Animation...
        "x64\TriangleAnimation\OpenGLPractice.exe"
    )
    goto :eof
)

if "%1"=="experiment" (
    echo Building New Experiment...
    msbuild OpenGLPractice.sln /p:Configuration=NewExperiment /p:Platform=x64
    if %ERRORLEVEL%==0 (
        echo Starting New Experiment...
        "x64\NewExperiment\OpenGLPractice.exe"
    )
    goto :eof
)

if "%1"=="main" (
    echo Building Main Project...
    msbuild OpenGLPractice.sln /p:Configuration=Debug /p:Platform=x64
    if %ERRORLEVEL%==0 (
        echo Starting Main Project...
        "x64\Debug\OpenGLPractice.exe"
    )
    goto :eof
)

echo Unknown project: %1
echo Use: triangle, experiment, or main