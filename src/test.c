// 包含头文件
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include "ffencoder.h"

#define ENCODE_MAX_RETRY     100
#define ENCODE_BY_FRAME_RATE   1

static void rand_buf(void *buf, int size)
{
    uint32_t *ptr32 = (uint32_t*)buf;
    while (size) {
        *ptr32++ = rand();
        size -= 4;
    }
}

static uint32_t get_tick_count()
{  
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}  

int main(void)
{
    static uint8_t abuf[44100 / 30 * 2 * 1];
    static uint8_t vbuf[320 * 240 * 2];
    void     *encoder = NULL;
    void     *adata   [8] = { abuf };
    void     *vdata   [8] = { vbuf };
    int       linesize[8] = { 320*2};
    int       i, j;
    uint32_t  tickstart, tickcur, ticklast;
    int32_t   tickdiff, ticksleep, tickframe;
    int32_t   ticknow, tickacc;

    printf("encode start\n");
    FFENCODER_PARAMS param = {
        // input params
        AV_CH_LAYOUT_MONO,         // in_audio_channel_layout
        AV_SAMPLE_FMT_S16,         // in_audio_sample_fmt
        44100,                     // in_audio_sample_rate
        320,                       // in_video_width
        240,                       // in_video_height
        AV_PIX_FMT_YUYV422,        // in_video_pixfmt
        30,                        // in_video_frame_rate

        // output params
//      (char*)"test.mp4",         // filename
        (char*)"rtmp://localhost:1983/live/test.flv",
        32000,                     // out_audio_bitrate
        AV_CH_LAYOUT_MONO,         // out_audio_channel_layout
        44100,                     // out_audio_sample_rate
        256000,                    // out_video_bitrate
        320,                       // out_video_width
        240,                       // out_video_height
        25,                        // out_video_frame_rate

        // other params
        SWS_POINT,                 // scale_flags
        5,                         // audio_buffer_number
        5,                         // video_buffer_number
        1,                         // timebase by frame rate
    };

    // init vars
    ticksleep = tickframe = 1000 / param.in_video_frame_rate;
    tickstart = get_tick_count();

    // init encoder
    encoder = ffencoder_init(&param);

    for (i=0; i<1800; i++)
    {
        rand_buf(abuf, sizeof(abuf));
        for (j=0; j<ENCODE_MAX_RETRY; j++) {
            if (0 == ffencoder_audio(encoder, adata, 44100/30, -1)) {
                break;
            }
            usleep(20*1000);
        }
        if (j == ENCODE_MAX_RETRY) {
            printf("encode audio, retry timeout !\n");
        }

        rand_buf(vbuf, sizeof(vbuf));
        for (j=0; j<ENCODE_MAX_RETRY; j++) {
            if (0 == ffencoder_video(encoder, vdata, linesize, -1)) {
                break;
            }
            usleep(20*1000);
        }
        if (j == ENCODE_MAX_RETRY) {
            printf("encode video, retry timeout !\n");
        }

        tickcur  = get_tick_count();
        tickdiff = tickcur - ticklast;
        ticklast = tickcur;
        ticknow  = tickcur - tickstart;
        tickacc  = i * 1000 / param.in_video_frame_rate;
        if (tickdiff - tickframe >  3 ) ticksleep--;
        if (tickdiff - tickframe < -3 ) ticksleep++;
        if (ticknow  - tickacc   >  20) ticksleep--;
        if (ticknow  - tickacc   < -20) ticksleep++;
#if ENCODE_BY_FRAME_RATE
        if (ticksleep > 0) usleep(ticksleep * 1000);
#endif
//      printf("tickdiff = %d, ticksleep = %d\n", tickdiff, ticksleep);
    }

    // free encoder
    ffencoder_free(encoder);
    printf("encode done\n");
    return 0;
}
