#!/usr/bin/env bash
# 用 CLion 自带的工具

export CUSTOM_CMAKE_COMMAND=""
export CUSTOM_NINJA_COMMAND=""


function use_clion() {
  local os_name="unknown"
  case "$OSTYPE" in
      solaris*) os_name="solaris" ;;
      darwin*)  os_name="darwin" ;;
      linux*)   os_name="linux" ;;
      bsd*)     os_name="bsd" ;;
      msys*)    os_name="windows" ;;
      cygwin*)  os_name="windows" ;;
      *)        echo "unknown: $OSTYPE" ;;
  esac

  case "${os_name}" in
    darwin*)
      echo "Using CLion CMake in ${os_name}"
      CUSTOM_CMAKE_COMMAND="$HOME/Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake"
      CUSTOM_NINJA_COMMAND="$HOME/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja"
      llvm_binpath="$(brew --prefix llvm)/bin"
      ;;
    linux*)
      echo "Using CLion CMake in ${os_name}"
      CUSTOM_CMAKE_COMMAND="$HOME/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake"
      CUSTOM_NINJA_COMMAND="$HOME/.local/share/JetBrains/Toolbox/apps/clion/bin/ninja/linux/x64/ninja"
      ;;
  esac
}

use_clion
