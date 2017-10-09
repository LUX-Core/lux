export NDK_PATH=/path/to/ndk # you must edit this path !!!

export ANDROID_DEV=$NDK_PATH/platforms/android-9/arch-arm/usr
export AR=$NDK_PATH/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ar
export AS=$NDK_PATH/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-as
export CC=$NDK_PATH/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-gcc
export CFLAGS=--sysroot=$NDK_PATH/platforms/android-9/arch-arm/
export CPP=$NDK_PATH/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-cpp
export CPPFLAGS=--sysroot=$NDK_PATH/platforms/android-9/arch-arm/
export CXX=$NDK_PATH/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-g++
export CXXFLAGS="--sysroot=$NDK_PATH/platforms/android-9/arch-arm/ -I$NDK_PATH/sources/cxx-stl/gnu-libstdc++/4.9/include -I$NDK_PATH/sources/cxx-stl/gnu-libstdc++/4.9/libs/armeabi-v7a/include"
export LD=$NDK_PATH/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ld
export NDK_PROJECT_PATH=$NDK_PATH
export RANLIB=$NDK_PATH/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/arm-linux-androideabi-ranlib
export PATH=$NDK_PATH/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64/bin/:$PATH

