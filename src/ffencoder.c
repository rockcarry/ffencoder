// 包含头文件
#include <stdlib.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include "ffencoder.h"
#include "log.h"

// 内部类型定义
typedef struct
{
    FFENCODER_PARAMS   params;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;

    AVStream          *astream;
    AVStream          *vstream;
    AVFrame           *aframe0;
    AVFrame           *aframe1;
    AVFrame           *vframe0;
    AVFrame           *vframe1;
    int64_t            next_apts;
    int64_t            next_vpts;

    AVFormatContext   *ofctxt;
    AVCodec           *acodec;
    AVCodec           *vcodec;
    AVDictionary      *avopt;

    int                have_audio;
    int                have_video;
} FFENCODER;

// 内部全局变量定义
static FFENCODER_PARAMS DEF_FFENCODER_PARAMS =
{
    "test.mp4",         // filename
    0,                  // audio_disable
    128000,             // audio_bitrate
    48000,              // sample_rate
    AV_CH_LAYOUT_STEREO,// audio stereo
    0,                  // video_disable
    512000,             // video_bitrate
    320,                // video_width
    240,                // video_height
    25,                 // frame_rate
    AV_PIX_FMT_ARGB,    // pixel_fmt
    SWS_FAST_BILINEAR,  // scale_flags
};

// 内部函数实现
static int add_astream(FFENCODER *encoder)
{
    enum AVCodecID  codec_id = encoder->ofctxt->oformat->audio_codec;
    AVCodecContext *c        = NULL;
    int i;

    if (encoder->params.audio_disable || codec_id == AV_CODEC_ID_NONE) return 0;

    encoder->acodec = avcodec_find_encoder(codec_id);
    if (!encoder->acodec) {
        log_printf("could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        return -1;
    }

    encoder->astream = avformat_new_stream(encoder->ofctxt, encoder->acodec);
    if (!encoder->astream) {
        log_printf("could not allocate stream\n");
        return -1;
    }

    encoder->astream->id = encoder->ofctxt->nb_streams - 1;
    c                    = encoder->astream->codec;

    c->sample_fmt  = encoder->acodec->sample_fmts ? encoder->acodec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    c->bit_rate    = encoder->params.audio_bitrate;
    c->sample_rate = encoder->params.sample_rate;
    if (encoder->acodec->supported_samplerates)
    {
        c->sample_rate = encoder->acodec->supported_samplerates[0];
        for (i=0; encoder->acodec->supported_samplerates[i]; i++) {
            if (encoder->acodec->supported_samplerates[i] == encoder->params.sample_rate)
                c->sample_rate = encoder->params.sample_rate;
        }
    }

    c->channel_layout = encoder->params.channel_layout;
    if (encoder->acodec->channel_layouts)
    {
        c->channel_layout = encoder->acodec->channel_layouts[0];
        for (i=0; encoder->acodec->channel_layouts[i]; i++) {
            if (encoder->acodec->channel_layouts[i] == encoder->params.channel_layout)
                c->channel_layout = encoder->params.channel_layout;
        }
    }
    c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
    encoder->astream->time_base = (AVRational){ 1, c->sample_rate };

    /* some formats want stream headers to be separate. */
    if (encoder->ofctxt->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    encoder->have_audio = 1;
    return 0;
}

static int add_vstream(FFENCODER *encoder)
{
    enum AVCodecID  codec_id = encoder->ofctxt->oformat->video_codec;
    AVCodecContext *c        = NULL;

    if (encoder->params.video_disable || codec_id == AV_CODEC_ID_NONE) return 0;

    encoder->vcodec = avcodec_find_encoder(codec_id);
    if (!encoder->vcodec) {
        log_printf("could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        return -1;
    }

    encoder->vstream = avformat_new_stream(encoder->ofctxt, encoder->vcodec);
    if (!encoder->vstream) {
        log_printf("could not allocate stream\n");
        return -1;
    }

    encoder->vstream->id = encoder->ofctxt->nb_streams - 1;
    c                    = encoder->vstream->codec;

    c->codec_id = codec_id;
    c->bit_rate = encoder->params.video_bitrate;
    /* Resolution must be a multiple of two. */
    c->width    = encoder->params.video_width;
    c->height   = encoder->params.video_height;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    encoder->vstream->time_base = (AVRational){ 1, encoder->params.frame_rate };
    c->time_base = encoder->vstream->time_base;
    c->gop_size  = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt   = encoder->params.pixel_fmt;
    if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        /* just for testing, we also add B frames */
        c->max_b_frames = 2;
    }
    if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
        /* needed to avoid using macroblocks in which some coeffs overflow.
         * This does not happen with normal video, it just happens here as
         * the motion of the chroma plane does not match the luma plane. */
        c->mb_decision = 2;
    }

    /* some formats want stream headers to be separate. */
    if (encoder->ofctxt->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    encoder->have_video = 1;
    return 0;
}

static AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout, int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame) {
        log_printf("error allocating an audio frame\n");
        exit(1);
    }

    frame->format         = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate    = sample_rate;
    frame->nb_samples     = nb_samples;

    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            log_printf("error allocating an audio buffer\n");
            exit(1);
        }
    }

    return frame;
}

static void open_audio(FFENCODER *encoder)
{
    AVCodec        *codec     = encoder->acodec;
    AVDictionary   *opt_arg   = encoder->avopt;
    AVCodecContext *c         = encoder->astream->codec;
    AVDictionary   *opt       = NULL;
    int             in_sfmt   = AV_SAMPLE_FMT_S16;
    int             in_layout = encoder->params.channel_layout;
    int             in_rate   = encoder->params.sample_rate;
    int             in_chnb   = av_get_channel_layout_nb_channels(in_layout);
    int             nb_samples;
    int             ret;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        log_printf("could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }

    if (c->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;

    encoder->aframe0 = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                         c->sample_rate, nb_samples);
    encoder->aframe1 = alloc_audio_frame(in_sfmt, in_layout,
                                         in_rate, nb_samples);

    /* create resampler context */
    encoder->swr_ctx = swr_alloc();
    if (!encoder->swr_ctx) {
        log_printf("could not allocate resampler context\n");
        exit(1);
    }

    /* set options */
    av_opt_set_int       (encoder->swr_ctx, "in_channel_count",  in_chnb,           0);
    av_opt_set_int       (encoder->swr_ctx, "in_sample_rate",    in_rate,           0);
    av_opt_set_sample_fmt(encoder->swr_ctx, "in_sample_fmt",     in_sfmt,           0);
    av_opt_set_int       (encoder->swr_ctx, "out_channel_count", c->channels,       0);
    av_opt_set_int       (encoder->swr_ctx, "out_sample_rate",   c->sample_rate,    0);
    av_opt_set_sample_fmt(encoder->swr_ctx, "out_sample_fmt",    c->sample_fmt,     0);

    /* initialize the resampling context */
    if ((ret = swr_init(encoder->swr_ctx)) < 0) {
        log_printf("failed to initialize the resampling context\n");
        exit(1);
    }
}

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
        return NULL;

    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        log_printf("could not allocate frame data.\n");
        exit(1);
    }

    return picture;
}

static void open_video(FFENCODER *encoder)
{
    AVCodec        *codec   = encoder->vcodec;
    AVDictionary   *opt_arg = encoder->avopt;
    AVCodecContext *c       = encoder->vstream->codec;
    AVDictionary   *opt     = NULL;
    int             ret;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        log_printf("could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* allocate and init a re-usable frame */
    encoder->vframe0 = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!encoder->vframe0) {
        log_printf("could not allocate video frame\n");
        exit(1);
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    encoder->vframe1 = NULL;
    if (c->pix_fmt != encoder->params.pixel_fmt) {
        encoder->vframe1 = alloc_picture(encoder->params.pixel_fmt, c->width, c->height);
        if (!encoder->vframe1) {
            log_printf("could not allocate temporary picture\n");
            exit(1);
        }
    }

    encoder->sws_ctx = sws_getContext(c->width, c->height,
                                      encoder->params.pixel_fmt,
                                      c->width, c->height,
                                      c->pix_fmt,
                                      encoder->params.scale_flags, NULL, NULL, NULL);
    if (!encoder->sws_ctx) {
        log_printf("could not initialize the conversion context\n");
        exit(1);
    }
}

static void close_astream(FFENCODER *encoder)
{
    avcodec_close(encoder->astream->codec);
    av_frame_free(&(encoder->aframe0));
    av_frame_free(&(encoder->aframe1));
    swr_free     (&(encoder->swr_ctx));
}

static void close_vstream(FFENCODER *encoder)
{
    avcodec_close(encoder->vstream->codec);
    av_frame_free(&(encoder->vframe0));
    av_frame_free(&(encoder->vframe1));
    sws_freeContext(encoder->sws_ctx);
}

// 函数实现
void* ffencoder_init(FFENCODER_PARAMS *params)
{
    int ret;

    // allocate context for ffencoder
    FFENCODER *encoder = malloc(sizeof(FFENCODER));
    if (encoder) memset(encoder, 0, sizeof(FFENCODER));
    else return NULL;

    // using default params if not set
    if (params == NULL) params = &DEF_FFENCODER_PARAMS;
    if (!params->audio_bitrate ) params->audio_bitrate = 128000;
    if (!params->sample_rate   ) params->sample_rate   = 48000;
    if (!params->channel_layout) params->channel_layout= AV_CH_LAYOUT_STEREO;
    if (!params->video_bitrate ) params->video_bitrate = 512000;
    if (!params->video_width   ) params->video_width   = 320;
    if (!params->video_height  ) params->video_height  = 240;
    if (!params->frame_rate    ) params->frame_rate    = 25;
    if (!params->pixel_fmt     ) params->pixel_fmt     = AV_PIX_FMT_ARGB;
    if (!params->scale_flags   ) params->scale_flags   = SWS_FAST_BILINEAR;
    memcpy(&(encoder->params), params, sizeof(FFENCODER_PARAMS));

    /* initialize libavcodec, and register all codecs and formats. */
    av_register_all();

    /* allocate the output media context */
    avformat_alloc_output_context2(&(encoder->ofctxt), NULL, NULL, params->filename);
    if (!encoder->ofctxt)
    {
        log_printf("could not deduce output format from file extension: using MPEG.\n");
        goto failed;
    }

    /* add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (add_astream(encoder) < 0)
    {
        log_printf("failed to add audio stream.\n");
        goto failed;
    }

    if (add_vstream(encoder) < 0)
    {
        log_printf("failed to add video stream.\n");
        goto failed;
    }

    /* now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (encoder->have_audio) open_audio(encoder);
    if (encoder->have_video) open_video(encoder);

    /* open the output file, if needed */
    if (!(encoder->ofctxt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&encoder->ofctxt->pb, params->filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            log_printf("could not open '%s': %s\n", params->filename, av_err2str(ret));
            goto failed;
        }
    }

    /* write the stream header, if any. */
    ret = avformat_write_header(encoder->ofctxt, &(encoder->avopt));
    if (ret < 0) {
        log_printf("error occurred when opening output file: %s\n", av_err2str(ret));
        goto failed;
    }

    // successed
    return encoder;

failed:
    ffencoder_free(encoder);
    return NULL;
}

void ffencoder_free(void *ctxt)
{
    FFENCODER *encoder = (FFENCODER*)ctxt;
    if (!ctxt) return;
    
    /* write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(encoder->ofctxt);

    /* close each codec. */
    if (encoder->have_video) close_astream(encoder);
    if (encoder->have_audio) close_vstream(encoder);

    /* close the output file. */
    if (!(encoder->ofctxt->oformat->flags & AVFMT_NOFILE)) avio_close(encoder->ofctxt->pb);

    /* free the stream */
    avformat_free_context(encoder->ofctxt);
    
    // free encoder context
    free(encoder);
}

void ffencoder_audio(void *ctxt, void *data[8], int nbsample)
{
    FFENCODER *encoder = (FFENCODER*)ctxt;
    if (!ctxt) return;
}

void ffencoder_video(void *ctxt, void *data[8], int linesize[8])
{
    FFENCODER *encoder = (FFENCODER*)ctxt;
    if (!ctxt) return;
}
