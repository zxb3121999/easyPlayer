// 音视频的编解码
// Created by Administrator on 2018/6/13 0013.
//

#include <thread>
#include "coder.h"
int Encoder::init(AVCodecContext *ctx) {
    if (avctx) {
        avcodec_free_context(&avctx);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    avctx = ctx;
    frame = av_frame_alloc();
    frame_queue = new FrameQueue;
    pkt_queue = new PacketQueue;
    return init_swr();
}

Encoder::~Encoder() {
    if(avctx){
        avcodec_free_context(&avctx);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    frame = NULL;
    if(frame_queue){
        frame_queue->flush();
        delete(frame_queue);
        frame_queue = NULL;
    }
    if(pkt_queue){
        pkt_queue->flush();
        delete(pkt_queue);
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
    if (!(avctx->codec->capabilities & CODEC_CAP_DELAY))
        return 0;
    while (!avcodec_receive_packet(avctx, pkt)) {
        pkt_queue->put_packet(pkt);
    }
    av_packet_free(&pkt);
    return 0;
}
void Encoder::flush_queue() {
    if(frame_queue){
        frame_queue->set_stop_state(true);
        frame_queue->flush();
    }
    if(!data_queue.empty()){
        data_queue.clear();
    }
    if(pkt_queue){
        pkt_queue->set_abort(true);
        pkt_queue->flush();
    }
}
void Encoder::loop() {
    while (encoder_encode_frame()>=0) {
    }
    if(!is_finish)
        flush_encoder();
    pkt_queue->put_nullpacket();
    av_frame_free(&frame);
    delete(frame);
    is_finish = false;
}
VideoEncoder::~VideoEncoder() {
    if(frame_out){
        av_frame_free(&frame_out);
    }
    frame_out = NULL;
    if(filter_graph){
        avfilter_graph_free(&filter_graph);
    }
    if(buffersrc_ctx)
        try {
            avfilter_free(buffersrc_ctx);
            buffersrc_ctx = NULL;
        }catch (...){
            buffersrc_ctx = NULL;
        }
    if(buffersink_ctx)
        try{
            avfilter_free(buffersink_ctx);
            buffersink_ctx = NULL;
        }catch (...){
            buffersink_ctx = NULL;
        }
    av_log(NULL,AV_LOG_FATAL,"释放视频编码资源完成");
}
int VideoEncoder::init_swr() {
    return 0;
}
int VideoEncoder::init_filter(char *filter_desc,AVRational time_base) {
    char args[512];
    int ret = 0;
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");/**/
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    enum AVPixelFormat pix_fmts[] = { avctx->pix_fmt, AV_PIX_FMT_NONE };

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             avctx->width, avctx->height, avctx->pix_fmt,
             time_base.num,time_base.den,
             avctx->sample_aspect_ratio.num, avctx->sample_aspect_ratio.den);
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }
    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_desc, &inputs, &outputs, NULL)) < 0)
        goto end;
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    return ret;
    end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    if(buffersink_ctx)
        buffersink_ctx = NULL;
    if(buffersrc_ctx)
        buffersrc_ctx = NULL;
    if(filter_graph)
        avfilter_graph_free(&filter_graph);
    return ret;
}
void VideoEncoder::encode() {
    if (frame) {
        av_frame_free(&frame);
    }
    if(frame_out){
        av_frame_free(&frame_out);
    }
    buf_size = avctx->width * avctx->height;
    frame = av_frame_alloc();
    frame_out = av_frame_alloc();
    frame->format = avctx->pix_fmt;
    frame->width = avctx->width;
    frame->height = avctx->height;
    int picture_size = av_image_get_buffer_size(avctx->pix_fmt, avctx->width, avctx->height, 1);
    uint8_t *buf = (uint8_t *) av_malloc(picture_size);
    av_image_fill_arrays(frame->data, frame->linesize, buf, avctx->pix_fmt, avctx->width, avctx->height, 1);
    loop();
    av_log(NULL,AV_LOG_FATAL,"写入空视频数据");
    delete (buf);
    if(buffersink_ctx){
        avfilter_free(buffersink_ctx);
        buffersink_ctx = NULL;
    }
    if(buffersrc_ctx){
        avfilter_free(buffersrc_ctx);
        buffersrc_ctx = NULL;
    }
    if(filter_graph){
        avfilter_graph_free(&filter_graph);
        filter_graph = NULL;
    }
    if(frame_out){
        av_frame_free(&frame_out);
        frame_out = NULL;
    }
}

int VideoEncoder::encoder_encode_frame() {
    int ret;
    uint8_t *data = nullptr;
    if(!use_frame_queue){
        data = *data_queue.wait_and_pop().get();
        if (data == nullptr) {
            return -1;
        }
        filter(data);
    } else{
        auto av_frame = frame_queue->get_frame();
        if (av_frame == nullptr) {//文件结束
            return -1;
        }
        ret = av_frame_ref(frame,av_frame->frame);
        av_frame_free(&av_frame->frame);
        if(ret!=0){
            return -1;
        }
    }
    if (frame_count == 0) {
        frame->pict_type = AV_PICTURE_TYPE_I;
    }
    frame->pts = frame_count++;
    if(buffersrc_ctx&&buffersink_ctx){
        if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
            delete(data);
            return 0;
        }
        ret = av_buffersink_get_frame(buffersink_ctx, frame_out);
        if (ret < 0){
            delete(data);
            return  0;
        }
        ret = avcodec_send_frame(avctx, frame_out);
    } else{
        ret = avcodec_send_frame(avctx, frame);
    }
    if (ret < 0 && ret != AVERROR_EOF) {
        if(ret == AVERROR(EINVAL)){
            av_log(NULL, AV_LOG_FATAL, "codec not opened %d.\n", ret);
        } else if(ret == AVERROR(ENOMEM)){
            av_log(NULL, AV_LOG_FATAL, "failed to add packet to internal queue %d.\n", ret);
        } else if(ret == AVERROR(EAGAIN)){
            av_log(NULL, AV_LOG_FATAL, "input is not accepted in the current state %d.\n", ret);
        }
        av_log(NULL, AV_LOG_FATAL, "video avcodec_send_frame error %d.\n", ret);
        delete (data);
        return 0;
    }
    AVPacket *pkt = av_packet_alloc();
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
    av_packet_free(&pkt);
    delete (data);
    return 0;
}

void VideoEncoder::filter(uint8_t *picture_buf) {
    int y_height_start_index = 0;
    //   uv值在H方向开始行
    int uv_height_start_index = 0;
    for (int i = y_height_start_index; i < avctx->width; i++) {
        for (int j = 0; j < avctx->height; j++) {
            int index = avctx->height * i + j;
            uint8_t value = *(picture_buf + index);
            *(frame->data[0] + j * avctx->width + (avctx->width - (i - y_height_start_index) - 1)) = value;
        }
    }
    for (int i = uv_height_start_index; i < avctx->width / 2; i++) {
        for (int j = 0; j < avctx->height / 2; j++) {
            int index = avctx->height / 2 * i + j;
            uint8_t v = *(picture_buf + buf_size + index);
            uint8_t u = *(picture_buf + buf_size * 5 / 4 + index);
            *(frame->data[2] + (j * avctx->width / 2 + (avctx->width / 2 - (i - uv_height_start_index) - 1))) = v;
            *(frame->data[1] + (j * avctx->width / 2 + (avctx->width / 2 - (i - uv_height_start_index) - 1))) = u;
        }
    }
}
AudioEncoder::~AudioEncoder() {
    if(swr)
        swr_free(&swr);
    av_log(NULL,AV_LOG_FATAL,"释放音频编码资源完成");
}
int AudioEncoder::init_swr() {
    swr = swr_alloc();
    swr = swr_alloc_set_opts(NULL, avctx->channel_layout, avctx->sample_fmt, avctx->sample_rate,
                             avctx->channel_layout, AV_SAMPLE_FMT_S16, avctx->sample_rate,
                             0, NULL);
    if (!swr || swr_init(swr) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Cannot create sample rate converter for conversion channels!\n");
        swr_free(&swr);
        return -1;
    }
    return 0;
}

void AudioEncoder::encode() {
    if (frame) {
        av_frame_free(&frame);
    }
    frame = av_frame_alloc();
    frame->nb_samples = avctx->frame_size;
    frame->format = avctx->sample_fmt;
    frame->sample_rate = avctx->sample_rate;
    frame->channels = avctx->channels;
    frame->channel_layout = avctx->channel_layout;
    int audioSize = av_samples_get_buffer_size(NULL, avctx->channels, avctx->frame_size, avctx->sample_fmt, 0);
    uint8_t *audio_buf = (uint8_t *) av_malloc(audioSize);
    avcodec_fill_audio_frame(frame, avctx->channels, avctx->sample_fmt, (const uint8_t *) audio_buf, audioSize, 0);;
    loop();
    if(audio_buf)
        delete(audio_buf);
    av_log(NULL,AV_LOG_FATAL,"写入空音频数据");
    swr_free(&swr);
    delete(swr);
}

int AudioEncoder::encoder_encode_frame() {
    int ret;
    uint8_t *outs[2]={0};
    uint8_t *data = nullptr;
    AVPacket *pkt = NULL;
    av_frame_unref(frame);
    if(!use_frame_queue) {
        data = *data_queue.wait_and_pop().get();
        if (data == nullptr) {
            goto fail;
        }
        outs[0] = (uint8_t *) malloc(buf_size);
        outs[1] = (uint8_t *) malloc(buf_size);
        ret = swr_convert(swr, (uint8_t **) &outs, buf_size * 4, (const uint8_t **) &data, buf_size / 4);
        if (ret < 0) {
            goto fail;
        }
        frame->data[0] = outs[0];
        frame->data[1] = outs[1];
    }else{
        auto av_frame = frame_queue->get_frame();
        if (av_frame == nullptr) {//文件结束
            goto fail;
        }
        ret = av_frame_ref(frame,av_frame->frame);
        av_frame_free(&av_frame->frame);
        if(ret!=0){
            goto fail;
        }
    }
    if (frame_count == 0) {
        frame->pict_type = AV_PICTURE_TYPE_I;
    }
    frame->pts = frame_count * frame->nb_samples;
    frame_count++;
    ret = avcodec_send_frame(avctx, frame);
    if (ret) {
        if(ret == AVERROR(EINVAL)){
            av_log(NULL, AV_LOG_FATAL, "codec not opened %d.\n", ret);
        } else if(ret == AVERROR(ENOMEM)){
            av_log(NULL, AV_LOG_FATAL, "failed to add packet to internal queue %d.\n", ret);
        } else if(ret == AVERROR(EAGAIN)){
            av_log(NULL, AV_LOG_FATAL, "input is not accepted in the current state %d.\n", ret);
        }
        av_log(NULL, AV_LOG_FATAL, "video avcodec_send_frame error %d.\n", ret);
        goto success;
    }
    pkt = av_packet_alloc();
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
    av_packet_free(&pkt);
    success:
    if(outs[0]&&outs[1]){
        delete[](outs[0]);
        delete[](outs[1]);
    }
    delete (data);
    return 0;
    fail:
    if(outs[0]&&outs[1]){
        delete[](outs[0]);
        delete[](outs[1]);
    }
    delete (data);
    return -1;
}

void Decoder::init(AVCodecContext *ctx) {
    if (avctx) {
        avcodec_free_context(&avctx);
    }
    if (pkt != nullptr) {
        av_packet_unref(pkt);
        av_packet_free(&pkt);
    }
    avctx = ctx;
    pkt = av_packet_alloc();
    pkt_queue = new PacketQueue;
    frame_queue = new FrameQueue;
}

Decoder::~Decoder() {
    flush();
    wait_thread_stop();
    if(pkt_queue){
        delete(pkt_queue);
        pkt_queue = nullptr;
    }
    if(frame_queue){
        delete(frame_queue);
        frame_queue = nullptr;
    }
    if(pkt){
        av_packet_free(&pkt);
    }
    pkt = nullptr;

    if (avctx) {
        avcodec_free_context(&avctx);
        avctx = NULL;
    }
}

void Decoder::flush() {
    if(pkt_queue){
        pkt_queue->set_abort(1);
        pkt_queue->flush();
    }
    if(frame_queue){
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

//从AVPacket中解码出无压缩的视频数据
int VideoDecoder::decoder_decode_frame() {
    int ret;//解码结果
    do {
        if (pkt_queue->get_abort())
            return -1;
        if (!packet_pending || pkt_queue->get_serial() != pkt_serial) {
            if (pkt_queue->get_packet(pkt) < 0) return -1;
            if(pkt->size==0){
                av_packet_unref(pkt);
                return -1;
            }
        }
        ret = avcodec_send_packet(avctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            av_log(NULL, AV_LOG_FATAL, "video avcodec_send_packet error %d.\n", ret);
            break;
        }

        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avctx, frame);
        if (ret < 0 && ret != AVERROR_EOF) {
            av_log(NULL, AV_LOG_FATAL, "video avcodec_receive_frame error %d.\n", ret);
            break;
        }
        frame->pts = av_frame_get_best_effort_timestamp(frame);
        frame_queue->put_frame(frame);
        av_frame_free(&frame);
        if (ret < 0) {
            packet_pending = 0;
        }
    } while (ret != 0);
    return 0;
}

//视频解码器
void VideoDecoder::decode() {
    av_log(NULL, AV_LOG_FATAL, "video decoder thread is start");
    for (;;) {
        if (pkt_queue->get_abort()) break;
        if (decoder_decode_frame() < 0) {
            frame_queue->put_null_frame();
            break;
        }
    }
    is_thread_running = false;
    av_log(NULL, AV_LOG_FATAL, "video decoder thread is completed");
}

//从AVPacket解码出无压缩音频数据
int AudioDecoder::decoder_decode_frame() {
    int ret;
    do {
        if (pkt_queue->get_abort())
            return -1;
        if (!packet_pending || pkt_queue->get_serial() != pkt_serial) {
            if (pkt_queue->get_packet(pkt) < 0) return -1;
        }
        if (pkt->size == 0) {
            av_packet_unref(pkt);
            return -1;
        }
        //从AVPacket中解码出AVFrame
        ret = avcodec_send_packet(avctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            av_log(NULL, AV_LOG_FATAL, "audio avcodec_send_packet error %d.\n", ret);
            break;
        }
        AVFrame *frame = av_frame_alloc();
        ret = avcodec_receive_frame(avctx, frame);
        if (ret < 0 && ret != AVERROR_EOF) {
            av_log(NULL, AV_LOG_FATAL, "video avcodec_receive_frame error %d.\n", ret);
            break;
        }
        frame->pts = av_frame_get_best_effort_timestamp(frame);//获取AVFrame中的显示时间戳
        frame_queue->put_frame(frame);
        av_frame_free(&frame);
        if (ret < 0) {
            packet_pending = 0;
        }
    } while (ret != 0);
    return 0;
}

//音频解码器
void AudioDecoder::decode() {
    av_log(NULL, AV_LOG_FATAL, "audio decoder thread is start");
    for (;;) {
        if (pkt_queue->get_abort()) break;
        if (decoder_decode_frame() < 0) {
            frame_queue->put_null_frame();
            break;
        }
    }
    is_thread_running = false;
    av_log(NULL, AV_LOG_FATAL, "audio decoder thread is completed");
}

//获取channels
int AudioDecoder::get_channels() {
    return avctx->channels;
}

//获取音频采样率
int AudioDecoder::get_sample_rate() {
    return avctx->sample_rate;
}