# makefile for ffnes project
# written by rockcarry

# for ffmpeg
FFMPEG_INC = ${PWD}/../ffmpeg_sdk_win32/include
FFMPEG_LIB = ${PWD}/../ffmpeg_sdk_win32/lib

# 编译器定义
CC      = i686-w64-mingw32-gcc
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
	$(CC) $(CCFLAGS) -static-libgcc -shared -o $@ $(OBJS) -lavformat -lavcodec -lavutil -lswscale -lswresample -lx264 -lwsock32 -lws2_32

%.o : %.c %.h stdefine.h
	$(CC) $(CCFLAGS) -o $@ $< -c

%.o : %.cpp %.h stdefine.h
	$(CC) $(CCFLAGS) -o $@ $< -c

%.o : %.c stdefine.h
	$(CC) $(CCFLAGS) -o $@ $< -c

%.o : %.cpp stdefine.h
	$(CC) $(CCFLAGS) -o $@ $< -c

%.exe : %.c $(DLL)
	$(CC) $(CCFLAGS) -o $@ $< $(DLL)

%.exe : %.cpp $(DLL)
	$(CC) $(CCFLAGS) -o $@ $< $(DLL)

install :
	mkdir -p ${PWD}/../sdk
	cp ffencoder.dll ${PWD}/../sdk
	cp ffencoder.h   ${PWD}/../sdk

clean :
	-rm -f *.o
	-rm -f *.dll
	-rm -f *.exe
	-rm -f *.mp4
	-rm -f *.log

# rockcarry
# 2006.10.4
# THE END



