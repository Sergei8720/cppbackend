#!/bin/bash

# Этот скрипт будет выполнен перед run.sh
export CONAN_USER_HOME=/tmp/conan-home
export CONAN_PROFILE=/tmp/conan-profile

# Создаем чистый профиль с правильным ABI
rm -rf /tmp/conan-home
conan profile new default --detect --force
conan profile update settings.compiler.libcxx=libstdc++11 default

# Очищаем кэш jsoncpp чтобы переустановить с правильным ABI
conan remove jsoncpp/1.9.5 -f

echo "Conan profile configured with libstdc++11"