// 音视频的编解码
// Created by Administrator on 2018/6/13 0013.
//

#include <thread>
#include <unistd.h>
#include "coder.h"

Coder::~Coder() {
    if (frame_out) {
        av_frame_free(&frame_out);
    }
    if (buffersink_ctx)
        avfilter_free(buffersink_ctx);
    buffersink_ctx = NULL;
    if (buffersrc_ctx)
        avfilter_free(buffersrc_ctx);
    buffersrc_ctx = NULL;

    if (filter_graph != NULL) {
        avfilter_graph_free(&filter_graph);
    }
    filter_graph = NULL;
}

int Coder::init_filter(char *filter_desc, AVRational time_base) {
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");/**/
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    enum AVPixelFormat pix_fmts[] = {avctx->pix_fmt, AV_PIX_FMT_NONE};

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             avctx->width, avctx->height, avctx->pix_fmt,
             time_base.num, time_base.den,
             avctx->sample_aspect_ratio.num, avctx->sample_aspect_ratio.den);
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }
    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL,
                                       filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_desc, &inputs, &outputs, NULL)) < 0)
        goto end;
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    if (!frame_out) {
        frame_out = av_frame_alloc();
    }
    end:
    if (inputs)
        avfilter_inout_free(&inputs);
    if (outputs)
        avfilter_inout_free(&outputs);
    return ret;
}

int Coder::filter_frame(AVFrame *frame) {
    int ret = RESULT_FAIL;

    if (frame &&
        (ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) <
               0)) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }
    AVFrame *filter_frame = av_frame_alloc();
    ret = av_buffersink_get_frame(buffersink_ctx, filter_frame) == AVERROR_EOF ? RESULT_FAIL : ret;
    if (ret < 0 || ret == AVERROR_EOF) {
        av_frame_free(&filter_frame);
        return ret;
    }
    frame_queue->put_frame(filter_frame);
    return ret;
}

int Encoder::init(AVCodecContext *ctx) {
    if (avctx) {
        avcodec_free_context(&avctx);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    avctx = ctx;
    frame_queue = new FrameQueue;
    pkt_queue = new PacketQueue;
    return init_swr();
}

Encoder::~Encoder() {
    if (avctx) {
        avcodec_flush_buffers(avctx);
        avcodec_free_context(&avctx);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (frame_queue) {
        frame_queue->flush();
        delete (frame_queue);
        frame_queue = NULL;
    }
    if (pkt_queue) {
        pkt_queue->flush();
        delete (pkt_queue);
        pkt_queue = NULL;
    }
}

void Encoder::start_encode_thread() {
    pkt_queue->set_abort(0);
    std::thread t(&Encoder::encode, this);
    t.detach();//线程分离
}

/**
 * 将编码线程中未完成编码的编码完成
 * @return
 */
int Encoder::flush_encoder() {
    int ret;
    AVPacket *pkt = av_packet_alloc();
    if (!(avctx->codec->capabilities & AV_CODEC_CAP_DELAY))
        return 0;
    while (!avcodec_receive_packet(avctx, pkt)) {
        pkt_queue->put_packet(pkt);
    }
    av_packet_free(&pkt);
    return 0;
}

void Encoder::flush_queue() {
    if (frame_queue) {
        frame_queue->set_stop_state(true);
        frame_queue->flush();
    }
    if (pkt_queue) {
        pkt_queue->set_abort(1);
        pkt_queue->flush();
    }
}

void Encoder::loop() {
    AVPacket *pkt = av_packet_alloc();
    while (encoder_encode_frame(pkt) >= 0) {
    }
    flush_encoder();
    pkt_queue->put_nullpacket();
    av_frame_free(&frame);
    av_packet_free(&pkt);
}

VideoEncoder::~VideoEncoder() {

    av_log(NULL, AV_LOG_FATAL, "释放视频编码资源完成");
}

int VideoEncoder::init_swr() {
    return 0;
}

void VideoEncoder::encode() {
    if (frame) {
        av_frame_free(&frame);
    }
    buf_size = avctx->width * avctx->height;
    loop();
    av_log(NULL, AV_LOG_FATAL, "写入空视频数据");
}

int VideoEncoder::encoder_encode_frame(AVPacket *pkt) {
    int ret;
    uint8_t *buf = NULL;
    if (!use_frame_queue) {
        frame = av_frame_alloc();
        frame->format = avctx->pix_fmt;
        frame->width = avctx->width;
        frame->height = avctx->height;
        int picture_size = av_image_get_buffer_size(avctx->pix_fmt, avctx->width, avctx->height, 1);
        buf = (uint8_t *) av_malloc(picture_size);
        av_image_fill_arrays(frame->data, frame->linesize, buf, avctx->pix_fmt, avctx->width,
                             avctx->height, 1);
        auto data = get_data();
        if (data == nullptr) {
            frame_count++;
            ret = -1;
            goto result;
        }
        filter(data);
        free(data);
    } else {
        frame = frame_queue->get_frame();
        if (!frame) {//文件结束
            ret = -1;
            goto result;
        }
    }
    if (frame_count == 0) {
        frame->pict_type = AV_PICTURE_TYPE_I;
    }
    frame->pts = frame_count++;
    if (buffersrc_ctx && buffersink_ctx) {
        av_frame_unref(frame_out);
        if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            ret = 0;
            goto result;
        }
        ret = av_buffersink_get_frame(buffersink_ctx, frame_out);
        if (ret < 0) {
            ret = 0;
            goto result;
        }
        ret = avcodec_send_frame(avctx, frame_out);
    } else {
        ret = avcodec_send_frame(avctx, frame);
    }
    if (ret < 0 && ret != AVERROR_EOF) {
        if (ret == AVERROR(EINVAL)) {
            av_log(NULL, AV_LOG_FATAL, "codec not opened %d.\n", ret);
        } else if (ret == AVERROR(ENOMEM)) {
            av_log(NULL, AV_LOG_FATAL, "failed to add packet to internal queue %d.\n", ret);
        } else if (ret == AVERROR(EAGAIN)) {
            av_log(NULL, AV_LOG_FATAL, "input is not accepted in the current state %d.\n", ret);
        }
        av_log(NULL, AV_LOG_FATAL, "video avcodec_send_frame error %d.\n", ret);
        ret = 0;
        goto result;
    }
    do {
        ret = avcodec_receive_packet(avctx, pkt);
        if (ret == 0) {
            pkt_queue->put_packet(pkt);
        } else if (ret == AVERROR(EAGAIN)) {
            //需要send新的frame
            break;
        } else if (ret == AVERROR_EOF) {
            //结束了
            break;
        } else if (ret < 0) {
            break;
        }
    } while (ret);
    ret = 0;
    result:
    if (frame)
        av_frame_free(&frame);
    if (buf)
        av_free(buf);
    return ret;
}

void VideoEncoder::filter(uint8_t *picture_buf) {
    int y_height_start_index = 0;
    //   uv值在H方向开始行
    int uv_height_start_index = 0;
    for (int i = y_height_start_index; i < avctx->width; i++) {
        for (int j = 0; j < avctx->height; j++) {
            int index = avctx->height * i + j;
            uint8_t value = *(picture_buf + index);
            *(frame->data[0] + j * avctx->width +
              (avctx->width - (i - y_height_start_index) - 1)) = value;
        }
    }
    for (int i = uv_height_start_index; i < avctx->width / 2; i++) {
        for (int j = 0; j < avctx->height / 2; j++) {
            int index = avctx->height / 2 * i + j;
            uint8_t v = *(picture_buf + buf_size + index);
            uint8_t u = *(picture_buf + buf_size * 5 / 4 + index);
            *(frame->data[2] +
              (j * avctx->width / 2 + (avctx->width / 2 - (i - uv_height_start_index) - 1))) = v;
            *(frame->data[1] +
              (j * avctx->width / 2 + (avctx->width / 2 - (i - uv_height_start_index) - 1))) = u;
        }
    }
}

AudioEncoder::~AudioEncoder() {
    if (swr)
        swr_free(&swr);
    swr = NULL;
    av_log(NULL, AV_LOG_FATAL, "释放音频编码资源完成");
}

int AudioEncoder::init_swr() {
    swr = swr_alloc_set_opts(NULL, avctx->channel_layout, avctx->sample_fmt, avctx->sample_rate,
                             avctx->channel_layout, AV_SAMPLE_FMT_S16, avctx->sample_rate,
                             0, NULL);
    if (!swr || swr_init(swr) < 0) {
        av_log(NULL, AV_LOG_FATAL,
               "Cannot create sample rate converter for conversion channels!\n");
        swr_free(&swr);
        return -1;
    }
    return 0;
}

void AudioEncoder::encode() {
    if (frame) {
        av_frame_free(&frame);
    }
    loop();
    av_log(NULL, AV_LOG_FATAL, "写入空音频数据");
}

int AudioEncoder::encoder_encode_frame(AVPacket *pkt) {
    int ret;
    uint8_t *outs[2] = {0};
    uint8_t *data = NULL;
    uint8_t *audio_buf = NULL;
    if (!use_frame_queue) {
        if (buf_size <= 0) {
            buf_size = get_buf_size();
        }
        if (buf_size <= 0) {
            return 0;
        }
        frame = av_frame_alloc();
        frame->nb_samples = avctx->frame_size;
        frame->format = avctx->sample_fmt;
        frame->sample_rate = avctx->sample_rate;
        frame->channels = avctx->channels;
        frame->channel_layout = avctx->channel_layout;
        int audioSize = av_samples_get_buffer_size(NULL, avctx->channels, avctx->frame_size,
                                                   avctx->sample_fmt, 0);
        audio_buf = (uint8_t *) av_malloc(audioSize);
        avcodec_fill_audio_frame(frame, avctx->channels, avctx->sample_fmt,
                                 (const uint8_t *) audio_buf, audioSize, 0);
        data = get_data();
        if (data == nullptr) {
            frame_count++;
            ret = -1;
            goto success;
        }
        outs[0] = (uint8_t *) malloc(buf_size);
        outs[1] = (uint8_t *) malloc(buf_size);
        ret = swr_convert(swr, (uint8_t **) &outs, buf_size * 4, (const uint8_t **) &data,
                          buf_size / 4);
        free(data);
        if (ret < 0) {
            goto success;
        }
        frame->data[0] = outs[0];
        frame->data[1] = outs[1];
    } else {
        frame = frame_queue->get_frame();
        if (!frame) {//文件结束
            ret = -1;
            goto success;
        }
    }
    if (frame_count == 0) {
        frame->pict_type = AV_PICTURE_TYPE_I;
    }
    frame->pts = frame_count * frame->nb_samples;
    frame_count++;
    ret = avcodec_send_frame(avctx, frame);
    if (ret) {
        if (ret == AVERROR(EINVAL)) {
            av_log(NULL, AV_LOG_FATAL, "codec not opened %d.\n", ret);
        } else if (ret == AVERROR(ENOMEM)) {
            av_log(NULL, AV_LOG_FATAL, "failed to add packet to internal queue %d.\n", ret);
        } else if (ret == AVERROR(EAGAIN)) {
            av_log(NULL, AV_LOG_FATAL, "input is not accepted in the current state %d.\n", ret);
        }
        av_log(NULL, AV_LOG_FATAL, "video avcodec_send_frame error %d.\n", ret);
        ret = 0;
        goto success;
    }
    while (true) {
        ret = avcodec_receive_packet(avctx, pkt);
        if (ret == 0) {
            pkt_queue->put_packet(pkt);
        } else if (ret == AVERROR_EOF) {
            break;
        } else if (ret == AVERROR(EAGAIN)) {
            break;
        } else if (ret == AVERROR(EINVAL)) {
            break;
        } else {
            break;
        }
    }
    ret = 0;
    success:
    if (outs[0] && outs[1]) {
        delete[](outs[0]);
        outs[0] = NULL;
        delete[](outs[1]);
        outs[1] = NULL;
    }
    *outs = NULL;
    if (frame)
        av_frame_free(&frame);
    if (audio_buf)
        av_free(audio_buf);
    return ret;
}

void Decoder::init(AVCodecContext *ctx) {
    if (avctx) {
        avcodec_free_context(&avctx);
    }
    if (pkt) {
        av_packet_free(&pkt);
    }
    avctx = ctx;
    pkt = av_packet_alloc();
    pkt_queue = new PacketQueue;
    frame_queue = new FrameQueue;
    tag = get_tag();
}

Decoder::~Decoder() {
    flush();
    wait_thread_stop();
    if (pkt_queue) {
        delete (pkt_queue);
        pkt_queue = nullptr;
    }
    if (frame_queue) {
        delete (frame_queue);
        frame_queue = NULL;
    }
    if (pkt) {
        av_packet_free(&pkt);
    }
    if (avctx) {
        avcodec_free_context(&avctx);
        avctx = NULL;
    }
    av_log(NULL, AV_LOG_FATAL, "释放%s资源完成", tag);
}

void Decoder::flush() {
    if (pkt_queue) {
        pkt_queue->set_abort(1);
        pkt_queue->flush();
    }
    if (frame_queue) {
        frame_queue->set_stop_state(true);
        frame_queue->flush();
    }
}

//开启解码线程
void Decoder::start_decode_thread() {
    pkt_queue->set_abort(0);
    is_thread_running = true;
    std::thread t(&Decoder::decode, this);
    t.detach();//线程分离
}

/**
 * 获取解码器中缓存的AVFrame
 */
void Decoder::flush_codec() {
    while (!decode_frame());//刷新avcodec_receive_frame中的AVFrame
    if (buffersink_ctx && buffersrc_ctx)
        while (filter_frame(NULL) >= 0);//刷新av_buffersink_get_frame中的AVFrame
    avcodec_flush_buffers(avctx);
}

/**
 * 获取解码的AVFrame
 * @return
 */
int Decoder::decode_frame() {
    AVFrame *frame = av_frame_alloc();
    int ret = avcodec_receive_frame(avctx, frame);
    if (!ret) {
        frame->pts = frame->best_effort_timestamp;
        if (buffersrc_ctx && buffersink_ctx) {
            filter_frame(frame);
            av_frame_free(&frame);
        } else {
            frame_queue->put_frame(frame);
        }
    } else {
        av_frame_free(&frame);
    }
    return ret;
}

//从AVPacket中解码出无压缩的数据
int Decoder::decoder_decode_frame() {
    int ret;//解码结果
    do {
        if (pkt_queue->get_abort())
            return -1;
        if (pkt_queue->get_packet(pkt) < 0) return -1;
        if (pkt->size == 0) {
            av_packet_unref(pkt);
            flush_codec();
            return -1;
        }
        ret = avcodec_send_packet(avctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            av_log(NULL, AV_LOG_FATAL, "video avcodec_send_packet error %d.\n", ret);
            break;
        }
        decode_frame();
    } while (true);
    flush_codec();
    return 0;
}

//视频的高度
int VideoDecoder::get_height() {
    if (!avctx) return 0;
    return avctx->height;
}

//视频的宽度
int VideoDecoder::get_width() {
    if (!avctx) return 0;
    return avctx->width;
}

//视频解码器
void Decoder::decode() {
    for (;;) {
        if (pkt_queue->get_abort()) break;
        if (decoder_decode_frame() < 0) {
            frame_queue->put_null_frame();
            break;
        }
    }
    is_thread_running = false;
    av_log(NULL, AV_LOG_FATAL, "decode thread %s finish", get_tag());
}

//获取channels
int AudioDecoder::get_channels() {
    return avctx->channels >= 2 ? 2 : avctx->channels;
}

//获取音频采样率
int AudioDecoder::get_sample_rate() {
    return avctx->sample_rate;
}