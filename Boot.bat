@echo off
:: Compila o proprio builder (chicken-and-egg: ele ainda nao existe pra
:: gerar o build.ninja de si mesmo). Rode uma vez, depois so use o binario.
setlocal enabledelayedexpansion

:: Garante que o script rode no diretorio onde ele esta salvo
cd /d "%~dp0"

:: Verifica se a variavel CXX ja esta definida
if "%CXX%"=="" (
    :: Tenta encontrar o clang++
    where clang++ >nul 2>&1
    if !errorlevel! equ 0 (
        set "COMPILER=clang++"
    ) else (
        :: Tenta encontrar o g++
        where g++ >nul 2>&1
        if !errorlevel! equ 0 (
            set "COMPILER=g++"
        ) else (
            echo Nenhum compilador C++ encontrado ^(clang++/g++^). >&2
            exit /b 1
        )
    )
) else (
    set "COMPILER=%CXX%"
)

:: Cria o diretorio de build se nao existir
if not exist "Build\bin" mkdir "Build\bin"

:: Executa a compilacao
"%COMPILER%" -std=c++20 -O2 -ISources -ITools Tools/Toolchain/Build.cpp Tools/Toolchain/Boot.cpp Tools/Toolchain/Parser.cpp Tools/Tool.cpp -o Build/bin/builder.exe

if !errorlevel! equ 0 (
    echo Builder compilado em: Build/bin/builder.exe
) else (
    echo Erro ao compilar o Builder. >&2
    exit /b 1
)

endlocal
