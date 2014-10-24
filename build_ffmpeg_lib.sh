#!/bin/bash

# clone ffmpeg
git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg

#++ patch for mingw32 compile error
sed '/check_cflags -Werror=missing-prototypes/d' ffmpeg/configure > ffmpeg/configure.new
mv ffmpeg/configure.new ffmpeg/configure
chmod +x ffmpeg/configure
#-- patch for mingw32 compile error

cd ffmpeg

./configure \
--arch=x86 \
--cpu=i586 \
--target-os=mingw32 \
--cross-prefix=i586-mingw32msvc- \
--pkg-config=pkg-config \
--prefix=../ffmpeglib \
--enable-static \
--enable-shared \
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
--enable-encoder=mpeg4 \
--enable-encoder=aac \
--enable-muxer=mp4 \
--enable-protocol=file

make -j8
make install

cd -

rm -rf ffmpeg

echo done
