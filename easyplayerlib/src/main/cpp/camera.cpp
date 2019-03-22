//
// Created by Administrator on 2018/12/19 0019.
//
#include <thread>
#include "camera.h"

void CameraDemo::recorder(const std::string path) {
    if (path.empty()) {
        return;
    }
    avdevice_register_all();
    char *url = av_strdup(path.c_str());
    AVInputFormat *iformat = av_find_input_format("android_camera");
    if (!iformat) {
        av_log(NULL, AV_LOG_FATAL, "未找到android_camera");
        return;
    }
    AVDictionary *params = NULL;
//    av_dict_set_int(&params, "camera_index", 0, 0);
    int err = 0;
    err = avformat_open_input(&ic, "", iformat, &params);
    av_dict_free(&params);
    if (err) {
        av_log(NULL, AV_LOG_ERROR, "打开AVFormatContext失败，错误码:%d", err);
        return;
    }
    err = avformat_find_stream_info(ic, NULL);
    if (err < 0) {
        avformat_free_context(ic);
        ic = NULL;
        av_log(NULL, AV_LOG_ERROR, "avformat_find_stream_info fail,errcode:%d", err);
        return;
    }
    for (int i = 0; i < ic->nb_streams; i++) {
        if (ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
        }
    }
    if (video_index == -1) {
        avformat_free_context(ic);
        ic = NULL;
        av_log(NULL, AV_LOG_ERROR, "video stream not found");
        return;
    }
    err = stream_component_open(video_index);
    if (err < 0) {
        avformat_free_context(ic);
        ic = NULL;
        av_log(NULL, AV_LOG_ERROR, "open codec failed,errCode:%d", err);
        return;
    }
    err = avformat_alloc_output_context2(&out, NULL, NULL, url);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "avformat_alloc_output_context2 failed,errCode:%d", err);
        return;
    }
    err = avio_open2(&out->pb, out->url, AVIO_FLAG_READ_WRITE, NULL, 0);
    if (err < 0) {
        av_log(NULL, AV_LOG_ERROR, "avio_open2 failed,errCode:%d", err);
        return;
    }
    err = recorder_component_open(video_index);
    if (err) {
        av_log(NULL, AV_LOG_ERROR, "recorder_component_open failed,errCode:%d", err);
        return;
    }
    av_dump_format(out, 0, out->url, 1);
    for (int i = 0; i < out->nb_streams; i++) {
        if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            recorder_video_index = i;
        }
    }
    std::thread r(&CameraDemo::read, this);
    r.detach();
}

int CameraDemo::stream_component_open(int stream_index) {
    if (stream_index < 0)
        return stream_index;
    AVCodecContext *avctx;
    AVCodec *codec;
    int ret = 0;
    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx)
        return AVERROR(ENOMEM);
    ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
    if (ret < 0) {
        avcodec_free_context(&avctx);
        return AVERROR(ENOMEM);
    }
    avctx->pkt_timebase = ic->streams[stream_index]->time_base;
    codec = avcodec_find_decoder(avctx->codec_id);
    if (!codec)
        return AVERROR(ENOMEM);;
    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_FATAL, "Fail to open codec on stream %d\n", stream_index);
        avcodec_free_context(&avctx);
        return ret;
    }
    //根据音视频类型，获取SwrContext，用于转换数据
    return ret;
}

int CameraDemo::recorder_component_open(int index) {
    AVCodecContext *avctx_out;
    AVCodec *codec_out;
    AVStream *out_stream;
    AVDictionary *param = 0;
    out_stream = avformat_new_stream(out, 0);
    avctx_out = avcodec_alloc_context3(NULL);
    avcodec_parameters_copy(out_stream->codecpar, ic->streams[index]->codecpar);
    avcodec_parameters_to_context(avctx_out, out_stream->codecpar);
    avctx_out->max_b_frames = ic->streams[index]->codec->max_b_frames;
    switch (ic->streams[index]->codecpar->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            break;
        case AVMEDIA_TYPE_VIDEO:
            avctx_out->gop_size = ic->streams[index]->codec->gop_size;
            avctx_out->thread_count = ic->streams[index]->codec->thread_count;
            avctx_out->qmin = 10;
            avctx_out->qmax = 51;
            avctx_out->time_base = ic->streams[index]->time_base;
            if (out->oformat->flags & AVFMT_GLOBALHEADER)
                avctx_out->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            if (avctx_out->codec_id == AV_CODEC_ID_H264) {
                avctx_out->me_pre_cmp = 1;
                avctx_out->me_range = 16;
                avctx_out->max_qdiff = 4;
                avctx_out->qcompress = 0.6;
                av_dict_set(&param, "tune", "zerolatency", 0);
                av_opt_set(avctx_out->priv_data, "preset", "ultrafast", 0);
                av_dict_set(&param, "profile", "baseline", 0);
            }
            break;
        default:
            break;
    }
    avctx_out->pkt_timebase = ic->streams[index]->time_base;
    codec_out = avcodec_find_encoder(avctx_out->codec_id);
    if (!codec_out) {
        av_log(NULL, AV_LOG_FATAL, "can not find codec");
        avcodec_free_context(&avctx_out);
        return -1;
    }
    if (avcodec_open2(avctx_out, codec_out, NULL) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Fail to open codec");
        avcodec_free_context(&avctx_out);
        return -1;
    }
    avcodec_free_context(&avctx_out);
    return 0;
}

void CameraDemo::read() {
    AVPacket *packet = av_packet_alloc();
    int count = 0;
    int err = 0;
    if(!out){
        av_log(NULL,AV_LOG_ERROR,"out is null");
        return;
    }
    av_log(NULL,AV_LOG_ERROR,"write header");
    avformat_write_header(out, NULL);
    while (count <= 500) {
        av_log(NULL,AV_LOG_ERROR,"start read frame");
        err = av_read_frame(ic, packet);
        av_log(NULL,AV_LOG_ERROR,"end read frame");
        if (err) {
            break;
        }

        if (packet->stream_index == video_index) {
            packet->pts = av_rescale_q_rnd(packet->pts,
                                           ic->streams[packet->stream_index]->time_base,
                                           out->streams[recorder_video_index]->time_base,
                                           (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            packet->dts = av_rescale_q_rnd(packet->dts,
                                           ic->streams[packet->stream_index]->time_base,
                                           out->streams[recorder_video_index]->time_base,
                                           (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
            packet->duration = av_rescale_q(packet->duration,
                                            ic->streams[packet->stream_index]->time_base,
                                            out->streams[recorder_video_index]->time_base);
            av_interleaved_write_frame(out, packet);
        }
        av_packet_unref(packet);
        count++;
    }
    av_write_trailer(out);
    av_packet_free(&packet);
    if (out && !(out->oformat->flags & AVFMT_NOFILE))
        avio_close(out->pb);
    avformat_free_context(out);
    out = NULL;
    if (ic) {
        avformat_free_context(ic);
        ic = NULL;
    }
}

void CameraDemo::write() {

}
