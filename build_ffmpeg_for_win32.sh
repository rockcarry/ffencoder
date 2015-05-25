#!/bin/bash

#wget http://www.tortall.net/projects/yasm/releases/yasm-1.2.0.tar.gz
#tar xvf yasm-1.2.0.tar.gz
#cd yasm-1.2.0
#./configure
#make -j8
#sudo make install


#++ build faac ++#
wget http://downloads.sourceforge.net/faac/faac-1.28.tar.gz
tar xvf faac-1.28.tar.gz
cd faac-1.28
./configure --prefix=$PWD/../ffmpeg_sdk_win32 \
--enable-static \
--host=i586-mingw32msvc \
--without-mp4v2
make -j8 && make install
cd -
#-- build faac --#


#++ build x264 ++#
git clone git://git.videolan.org/x264.git
cd x264
./configure --prefix=$PWD/../ffmpeg_sdk_win32 \
--enable-static \
--host=i586-mingw32msvc \
--cross-prefix=i586-mingw32msvc-
make -j8 && make install
cd -
#-- build x264 --#


#++ build ffmpeg ++#
git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg
#+ patch for mingw32 compile error
sed -i '/check_cflags -Werror=missing-prototypes/d' ffmpeg/configure
#- patch for mingw32 compile error
cd ffmpeg
./configure \
--arch=x86 \
--cpu=i586 \
--target-os=mingw32 \
--cross-prefix=i586-mingw32msvc- \
--pkg-config=pkg-config \
--prefix=$PWD/../ffmpeg_sdk_win32 \
--enable-static \
--enable-small \
--enable-memalign-hack \
--disable-swscale-alpha \
--disable-programs \
--disable-doc \
--disable-avdevice \
--disable-postproc \
--disable-avfilter \
--disable-encoders \
--disable-decoders \
--disable-hwaccels \
--disable-muxers \
--disable-demuxers \
--disable-parsers \
--disable-bsfs \
--disable-protocols \
--disable-devices \
--disable-filters \
--enable-gpl \
--enable-nonfree \
--enable-libx264 \
--enable-libfaac \
--enable-encoder=libx264 \
--enable-encoder=libfaac \
--enable-muxer=mp4 \
--enable-protocol=file \
--extra-cflags="-I$PWD/../ffmpeg_sdk_win32/include" \
--extra-ldflags="-L$PWD/../ffmpeg_sdk_win32/lib"
make -j8 && make install
cd -
#++ build ffmpeg ++#


rm -rf faac-1.28.tar.gz faac-1.28 x264 ffmpeg

echo done
