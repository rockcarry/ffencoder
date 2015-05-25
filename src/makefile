# makefile for ffnes project
# written by rockcarry

# for ffmpeg
FFMPEG_INC = ../ffmpeg_sdk_win32/include
FFMPEG_LIB = ../ffmpeg_sdk_win32/lib

# 编译器定义
CC      = i586-mingw32msvc-gcc
CCFLAGS = -Wall -I$(FFMPEG_INC) -L$(FFMPEG_LIB)

# 所有的目标文件
OBJS = \
    ffencoder.o \

# 所有的可执行目标
EXES = \
    test1.exe \
    test2.exe \

# 输出的库文件
DLL = ffencoder.dll

# 编译规则
all : $(DLL) $(EXES)

$(DLL) : $(OBJS)
	$(CC) $(CCFLAGS) -o $@ $(OBJS) --share -lavformat -lavcodec -lavutil -lswscale -lswresample -lx264 -lfaac

%.o : %.c %.h stdefine.h
	$(CC) $(CCFLAGS) -o $@ $< -c

%.o : %.cpp %.h stdefine.h
	$(CC) $(CCFLAGS) -o $@ $< -c

%.o : %.c stdefine.h
	$(CC) $(CCFLAGS) -o $@ $< -c

%.o : %.cpp stdefine.h
	$(CC) $(CCFLAGS) -o $@ $< -c

%.exe : %.c $(DLL)
	$(CC) $(CCFLAGS) -o $@ $< $(DLL) -lavformat -lavcodec -lavutil -lswscale -lswresample -lx264 -lfaac

%.exe : %.cpp $(DLL)
	$(CC) $(CCFLAGS) -o $@ $< $(DLL) -lavformat -lavcodec -lavutil -lswscale -lswresample -lx264 -lfaac

clean :
	-rm -f *.o
	-rm -f *.dll
	-rm -f *.exe
	-rm -f *.mp4
	-rm -f *.log

# rockcarry
# 2006.10.4
# THE END



