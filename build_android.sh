#!/bin/bash

### Describe Your Target Architectures ###
ARCH_LIST=( "armv8a" "armv7a" "x86" "x86-64") 

### Change As per Yours ####
ANDROID_API_LEVEL="21"
ANDROID_NDK_PATH="/work/ffmpeg/android-ndk-r26d"
FFMPEG_SOURCE_DIR="/work/ffmpeg/ffmpeg-7.1"
FFMPEG_BUILD_DIR="/work/ffmpeg/ffmpeg_android"


### Enable FFMPEG BUILD MODULES ####
ENABLED_CONFIG="\
        --enable-small \
		--enable-avcodec \
		--enable-avformat \
		--enable-avutil \
		--enable-swscale \
		--enable-swresample \
		--enable-demuxers \
		--enable-parser=* \
		--enable-decoders \
		--enable-pic \
		--enable-cross-compile \
		--enable-protocols \
		--enable-static"
#--enable-shared


### Disable FFMPEG BUILD MODULES ####
DISABLED_CONFIG="\
		--disable-zlib \
		--disable-v4l2-m2m \
		--disable-cuda-llvm \
		--disable-indevs \
		--disable-libxml2 \
		--disable-avdevice \
		--disable-ffplay \
		--disable-doc \
		--disable-symver \
		--disable-gpl \
		--disable-programs \
		--disable-shared"



### Dont Change ####
SYSROOT="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot"
LLVM_AR="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ar"
LLVM_NM="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm"
LLVM_RANLIB="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-ranlib"
LLVM_STRIP="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip"


configure_ffmpeg(){
   TARGET_ARCH=$1
   TARGET_CPU=$2
   CROSS_PREFIX=$3
   EXTRA_CFLAGS=$4
   EXTRA_CONFIG=$5
   
   CLANG="${CROSS_PREFIX}clang"
   CLANGXX="${CROSS_PREFIX}clang++"
   PREFIX="${FFMPEG_BUILD_DIR}/$TARGET_ARCH-$ANDROID_API_LEVEL"
   
   cd $FFMPEG_SOURCE_DIR
   
   ./configure \
   --disable-everything \
   --target-os=android \
   --arch=$TARGET_ARCH \
   --cpu=$TARGET_CPU \
   --enable-cross-compile \
   --cross-prefix="$CROSS_PREFIX" \
   --cc="$CLANG" \
   --cxx="$CLANGXX" \
   --sysroot="$SYSROOT" \
   --prefix="$PREFIX" \
   --extra-cflags="-fPIC -DANDROID $EXTRA_CFLAGS" \
   --extra-ldflags="-L$SYSROOT/usr/lib/$TARGET_ARCH-linux-android/$ANDROID_API_LEVEL" \
   ${ENABLED_CONFIG} \
   ${DISABLED_CONFIG} \
   --ar="$LLVM_AR" \
   --nm="$LLVM_NM" \
   --ranlib="$LLVM_RANLIB" \
   --strip="$LLVM_STRIP" \
   ${EXTRA_CONFIG}

   make clean
   make -j2
   make install -j2
   
}

echo -e "\e[1;32mCompiling FFMPEG for Android...\e[0m"

for ARCH in "${ARCH_LIST[@]}"; do
    case "$ARCH" in
        "armv8-a"|"aarch64"|"arm64-v8a"|"armv8a")
            echo -e "\e[1;32m$ARCH Libraries\e[0m"
            TARGET_ARCH="aarch64"
            TARGET_CPU="armv8-a"
            TARGET_ABI="aarch64"
            CROSS_PREFIX="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/$TARGET_ABI-linux-android${ANDROID_API_LEVEL}-"
            EXTRA_CFLAGS="-O3 -marm -march=$TARGET_CPU -mfpu=neon -fomit-frame-pointer"
            EXTRA_CONFIG="\
            		--enable-neon "
            ;;
        "armv7-a"|"armeabi-v7a"|"armv7a")
            echo -e "\e[1;32m$ARCH Libraries\e[0m"
            TARGET_ARCH="arm"
            TARGET_CPU="armv7-a"
            TARGET_ABI="armv7a"
            CROSS_PREFIX="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/$TARGET_ABI-linux-androideabi${ANDROID_API_LEVEL}-"
            EXTRA_CFLAGS="-O3 -marm -march=$TARGET_CPU -mfpu=neon -fomit-frame-pointer"
            EXTRA_CONFIG="\
            		--disable-armv5te \
            		--disable-armv6 \
            		--disable-armv6t2 \
            		--enable-neon "
            ;;
        "x86-64"|"x86_64")
            echo -e "\e[1;32m$ARCH Libraries\e[0m"
            TARGET_ARCH="x86_64"
            TARGET_CPU="x86-64"
            TARGET_ABI="x86_64"
            CROSS_PREFIX="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/$TARGET_ABI-linux-android${ANDROID_API_LEVEL}-"
            EXTRA_CFLAGS="-O3 -march=$TARGET_CPU -fomit-frame-pointer"
            		
            EXTRA_CONFIG="\
            		  "
            ;;
        "x86"|"i686")
            echo -e "\e[1;32m$ARCH Libraries\e[0m"
            TARGET_ARCH="i686"
            TARGET_CPU="i686"
            TARGET_ABI="i686"
            CROSS_PREFIX="$ANDROID_NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin/$TARGET_ABI-linux-android${ANDROID_API_LEVEL}-"
            EXTRA_CFLAGS="-O3 -march=$TARGET_CPU -fomit-frame-pointer"
            EXTRA_CONFIG="\
            		--disable-asm "
            ;;
           * )
            echo "Unknown architecture: $ARCH"
            exit 1
            ;;
    esac
    configure_ffmpeg "$TARGET_ARCH" "$TARGET_CPU" "$CROSS_PREFIX" "$EXTRA_CFLAGS" "$EXTRA_CONFIG"
done
