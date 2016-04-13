#!/bin/bash
set -e

#++ build yasm, ffmpeg need it
if [ ! -d yasm-1.2.0 ]; then
wget http://www.tortall.net/projects/yasm/releases/yasm-1.2.0.tar.gz
tar xvf yasm-1.2.0.tar.gz
rm yasm-1.2.0.tar.gz
fi
cd yasm-1.2.0
./configure --prefix=$PWD/../yasm-install
make -j8
make install
export PATH=$PATH:$PWD/../yasm-install/bin
cd -
#-- build yasm

# speed of faac is slower than ffmpeg native aac encoder
# so we do not use faac
if false; then
#++ build faac ++#
if [ ! -d faac-1.28 ]; then
wget http://downloads.sourceforge.net/faac/faac-1.28.tar.gz
tar xvf faac-1.28.tar.gz
fi
cd faac-1.28
./configure --prefix=$PWD/../ffmpeg_sdk_win32 \
--host=i586-mingw32msvc \
--enable-static \
--enable-shared \
--without-mp4v2
make -j8 && make install
cd -
#-- build faac --#
fi

#++ build x264 ++#
if [ ! -d x264 ]; then
  git clone git://git.videolan.org/x264.git
fi
cd x264
./configure --prefix=$PWD/../ffmpeg_sdk_win32 \
--enable-strip \
--enable-static \
--disable-shared \
--host=i586-mingw32msvc \
--cross-prefix=i586-mingw32msvc-
make -j8 && make install
cd -
#-- build x264 --#

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
git clone git://source.ffmpeg.org/ffmpeg.git
fi
#+ patch for mingw32 compile error
sed -i '/check_cflags -Werror=missing-prototypes/d' ffmpeg/configure
#- patch for mingw32 compile error
cd ffmpeg
./configure \
--arch=x86 \
--cpu=i586 \
--target-os=mingw32 \
--enable-cross-compile \
--cross-prefix=i586-mingw32msvc- \
--pkg-config=pkg-config \
--prefix=$PWD/../ffmpeg_sdk_win32 \
--enable-static \
--disable-shared \
--enable-small \
--enable-memalign-hack \
--disable-swscale-alpha \
--disable-symver \
--disable-debug \
--disable-programs \
--disable-doc \
--disable-avdevice \
--disable-avfilter \
--disable-postproc \
--disable-everything \
--enable-encoder=libx264 \
--enable-encoder=aac \
--enable-muxer=mp4 \
--enable-protocol=file \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-libx264 \
--extra-cflags="-I$PWD/../ffmpeg_sdk_win32/include" \
--extra-ldflags="-L$PWD/../ffmpeg_sdk_win32/lib"
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

