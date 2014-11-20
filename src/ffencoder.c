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
    AVStream *st;
    AVFrame  *frame;
    AVFrame  *tmp_frame;

    int64_t   next_pts;
    int       samples_count;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OUTSTREAM;

typedef struct
{
    OUTSTREAM        astream;
    OUTSTREAM        vstream;
    AVOutputFormat  *outfmt;
    AVFormatContext *ofctxt;
    AVCodec         *acodec;
    AVCodec         *vcodec;
    AVDictionary    *avopt;
    int              have_audio;
    int              have_video;
} FFENCODER;

// 内部全局变量定义
FFENCODER_PARAMS DEF_FFENCODER_PARAMS =
{
    "test.mp4",         // filename
    0,                  // audio_disable
    128000,             // audio_bitrate
    48000,              // sample_rate
    0,                  // video_disable
    512000,             // video_bitrate
    320,                // video_width
    240,                // video_height
    25,                 // frame_rate
    AV_PIX_FMT_ARGB,    // pixel_fmt_src
    AV_PIX_FMT_YUV420P, // pixel_fmt_dst
    SWS_FAST_BILINEAR,  // scale_flags
};

// 内部函数实现
/* add an output stream. */
static void add_stream(OUTSTREAM *ost, AVFormatContext *oc, AVCodec **codec, enum AVCodecID codec_id, FFENCODER_PARAMS *params)
{
    AVCodecContext *c;
    int i;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        log_printf("could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        exit(1);
    }

    ost->st = avformat_new_stream(oc, *codec);
    if (!ost->st) {
        log_printf("could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams - 1;
    c = ost->st->codec;

    switch ((*codec)->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = params->audio_bitrate;
        c->sample_rate = params->sample_rate;
        if ((*codec)->supported_samplerates) {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                if ((*codec)->supported_samplerates[i] == params->sample_rate)
                    c->sample_rate = params->sample_rate;
            }
        }
        c->channels       = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        if ((*codec)->channel_layouts) {
            c->channel_layout = (*codec)->channel_layouts[0];
            for (i = 0; (*codec)->channel_layouts[i]; i++) {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
        c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
        ost->st->time_base = (AVRational){ 1, c->sample_rate };
        break;

    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;
        c->bit_rate = params->video_bitrate;
        /* Resolution must be a multiple of two. */
        c->width    = params->video_width;
        c->height   = params->video_height;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){ 1, params->frame_rate };
        c->time_base       = ost->st->time_base;
        c->gop_size        = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt         = params->pixel_fmt_dst;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
            /* just for testing, we also add B frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
        break;

    default: break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
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

static void open_audio(AVFormatContext *oc, AVCodec *codec, OUTSTREAM *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = ost->st->codec;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        log_printf("could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }

    if (c->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = c->frame_size;
    else
        nb_samples = c->frame_size;

    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                       c->sample_rate, nb_samples);

    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx) {
        log_printf("could not allocate resampler context\n");
        exit(1);
    }

    /* set options */
    av_opt_set_int       (ost->swr_ctx, "in_channel_count",  c->channels,       0);
    av_opt_set_int       (ost->swr_ctx, "in_sample_rate",    c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",     AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int       (ost->swr_ctx, "out_channel_count", c->channels,       0);
    av_opt_set_int       (ost->swr_ctx, "out_sample_rate",   c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",    c->sample_fmt,     0);

    /* initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0) {
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

static void open_video(AVFormatContext *oc, AVCodec *codec, OUTSTREAM *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->st->codec;
    AVDictionary *opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        log_printf("could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        log_printf("could not allocate video frame\n");
        exit(1);
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P) {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            log_printf("could not allocate temporary picture\n");
            exit(1);
        }
    }
}

static void close_stream(AVFormatContext *oc, OUTSTREAM *ost)
{
    avcodec_close(ost->st->codec);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

// 函数实现
void* ffencoder_init(FFENCODER_PARAMS *params)
{
    int ret;

    // allocate context for ffencoder
    FFENCODER *ctxt = malloc(sizeof(FFENCODER));
    if (ctxt) memset(ctxt, 0, sizeof(FFENCODER));
    else return NULL;

    // using default params if not set
    if (params == NULL) params = &DEF_FFENCODER_PARAMS;
    if (!params->audio_bitrate) params->audio_bitrate = 128000;
    if (!params->sample_rate  ) params->sample_rate   = 48000;
    if (!params->video_bitrate) params->video_bitrate = 512000;
    if (!params->video_width  ) params->video_width   = 320;
    if (!params->video_height ) params->video_height  = 240;
    if (!params->frame_rate   ) params->frame_rate    = 25;
    if (!params->pixel_fmt_src) params->pixel_fmt_src = AV_PIX_FMT_ARGB;
    if (!params->pixel_fmt_dst) params->pixel_fmt_dst = AV_PIX_FMT_YUV420P;
    if (!params->scale_flags  ) params->scale_flags   = SWS_FAST_BILINEAR;

    /* initialize libavcodec, and register all codecs and formats. */
    av_register_all();

    /* allocate the output media context */
    avformat_alloc_output_context2(&(ctxt->ofctxt), NULL, NULL, params->filename);
    if (!ctxt->ofctxt)
    {
        log_printf("could not deduce output format from file extension: using MPEG.\n");
        goto failed;
    }
    else ctxt->outfmt = ctxt->ofctxt->oformat;

    /* add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (!params->audio_disable && ctxt->outfmt->audio_codec != AV_CODEC_ID_NONE) {
        add_stream(&(ctxt->astream), ctxt->ofctxt, &(ctxt->acodec), ctxt->outfmt->audio_codec, params);
        ctxt->have_audio = 1;
    }
    if (!params->video_disable && ctxt->outfmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(&(ctxt->vstream), ctxt->ofctxt, &(ctxt->vcodec), ctxt->outfmt->video_codec, params);
        ctxt->have_video = 1;
    }

    /* now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (ctxt->have_audio) open_audio(ctxt->ofctxt, ctxt->acodec, &(ctxt->astream), ctxt->avopt);
    if (ctxt->have_video) open_video(ctxt->ofctxt, ctxt->vcodec, &(ctxt->vstream), ctxt->avopt);

    /* open the output file, if needed */
    if (!(ctxt->outfmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctxt->ofctxt->pb, params->filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            log_printf("could not open '%s': %s\n", params->filename, av_err2str(ret));
            goto failed;
        }
    }

    /* write the stream header, if any. */
    ret = avformat_write_header(ctxt->ofctxt, &(ctxt->avopt));
    if (ret < 0) {
        log_printf("error occurred when opening output file: %s\n", av_err2str(ret));
        goto failed;
    }

    // successed
    return ctxt;

failed:
    ffencoder_free(ctxt);
    return NULL;
}

void ffencoder_free(void *encoder)
{
    FFENCODER *ctxt = (FFENCODER*)encoder;
    if (!ctxt) return;
    
    /* write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(ctxt->ofctxt);

    /* close each codec. */
    if (ctxt->have_video) close_stream(ctxt->ofctxt, &(ctxt->vstream));
    if (ctxt->have_audio) close_stream(ctxt->ofctxt, &(ctxt->astream));

    /* close the output file. */
    if (!(ctxt->outfmt->flags & AVFMT_NOFILE)) avio_close(ctxt->ofctxt->pb);

    /* free the stream */
    avformat_free_context(ctxt->ofctxt);
    
    // free encoder context
    free(ctxt);
}

void ffencoder_audio(void *encoder)
{
}

void ffencoder_video(void *encoder)
{
}
