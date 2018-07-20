// 视频录制
// Created by Administrator on 2018/6/13 0013.
//
extern "C"{
#include "libavutil/channel_layout.h"
}
#include "easyPlayer.h"
const char *filter_descr = "movie=/storage/emulated/0/ffmpeg/icon.png[wm];[in][wm]overlay=20:20[out]";
EasyRecorder::~EasyRecorder() {
    release();
}

void EasyRecorder::wait_state(RecorderState need_state) {
    std::unique_lock<std::mutex> lock(mutex);
    state_condition.wait(lock, [this, need_state] {
        return this->state >= need_state;
    });
}

void EasyRecorder::on_state_change(RecorderState state) {
    std::unique_lock<std::mutex> lock(mutex);
    this->state = state;
    state_condition.notify_all();
}

void EasyRecorder::release() {
    avfilter_uninit();
    if(file_name){
        delete(file_name);
    }
    if (out!=NULL) {
        if(out->pb){
            avio_close(out->pb);
        }
        avformat_free_context(out);
    }
    out = NULL;
    if(videnc){
        delete(videnc);
    }
    if(audenc){
        delete(audenc);
    }
}

/**/void EasyRecorder::set_file_save_path(const std::string output_file_name) {
    if (output_file_name.empty()) {
        av_log(nullptr, AV_LOG_FATAL, "An outpout file must be specified\n");
        return;
    }
    file_name = av_strdup(output_file_name.c_str());
}

void EasyRecorder::prepare() {
    if (!out) {
        avfilter_register_all();
        avformat_network_init();
    }
    on_state_change(RecorderState::UNKNOWN);
    std::thread thread(&EasyRecorder::write, this);
    thread.detach();
}

void EasyRecorder::write() {
    int err;
    int cur_pts_v = 0;
    int cur_pts_a = 0;
    int frame_video_index = 0;
    int frame_audio_index = 0;
    int64_t video_calc_duration =0;
    int64_t video_duration = 0;
    bool is_video_complete = false;
    bool is_audio_complete = false;
    AVDictionary *param = 0;
    AVBitStreamFilterContext *h264bsfc = NULL;
    AVBitStreamFilterContext *aacbsfc = NULL;
    avformat_alloc_output_context2(&out, NULL, NULL, file_name);
    AVPacket *packet = av_packet_alloc();
    if (out == NULL) {
        notify_message(RECORDER_STATE_ERROR,ERROR_MALLOC_CONTEXT,"分配context出错");
        return;
    }
    if (!(out->oformat->flags & AVFMT_NOFILE)){
        err = avio_open(&out->pb, out->filename, AVIO_FLAG_WRITE);
        if (err < 0) {
            notify_message(RECORDER_STATE_ERROR,ERROR_OPEN_FILE,"无法打开指定存储位置文件");
           goto result;
        }
    }
    int i;
    err = stream_component_open(AVMEDIA_TYPE_VIDEO);
    if (err < 0) {
        notify_message(RECORDER_STATE_ERROR,err,"打开视频编码器失败");
        goto result;
    }
    err = stream_component_open(AVMEDIA_TYPE_AUDIO);
    if ( err < 0) {
        notify_message(RECORDER_STATE_ERROR,err,"打开音频编码器失败");
        goto result;
    }
    av_dump_format(out, 0, out->filename, 1);
    for (i = 0; i < out->nb_streams; i++) {
        if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream = i;
        } else if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream = i;
        }
    }
    if(strcmp("mp4",out->oformat->name)==0){
        h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
        aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
        av_dict_set(&param,"movflags","faststart",0);
    }
    err = avformat_write_header(out, &param);
    av_dict_free(&param);
    if(err < 0){
        notify_message(RECORDER_STATE_ERROR,ERROR_WRITE_HEADER,"写入视频头文件出错");
        goto result;
    }
    if (video_stream>=0&&out->streams[video_stream]->time_base.den) {
        videnc->t = out->streams[video_stream]->time_base;
    }
    notify_message(RECORDER_STATE_READY,0,"准备好录制了");
    on_state_change(RecorderState::RECORDING);
    video_calc_duration = (double) AV_TIME_BASE / av_q2d((AVRational) {frame_rate, 1});
    video_duration = (double) video_calc_duration / (double) (av_q2d(out->streams[video_stream]->time_base) * AV_TIME_BASE);
    while (!(is_audio_complete&&is_video_complete)) {
        av_packet_unref(packet);
        if (av_compare_ts(cur_pts_v, out->streams[video_stream]->time_base, cur_pts_a, out->streams[audio_stream]->time_base) <= 0) {
            //写视频数据
            if(is_video_complete){
                cur_pts_v = cur_pts_a*1000;
                continue;
            }
            if (videnc->pkt_queue->get_packet(packet)<0||packet->size == 0) {
                is_video_complete = true;
                av_log(NULL,AV_LOG_FATAL,"video recorder completed");
                continue;
            }
            packet->stream_index = video_stream;
            av_packet_rescale_ts(packet, videnc->avctx->time_base, out->streams[video_stream]->time_base);
            packet->duration = video_duration;
            frame_video_index++;
            cur_pts_v = packet->pts;
            if(h264bsfc){
                av_bitstream_filter_filter(h264bsfc,videnc->avctx, NULL, &packet->data, &packet->size, packet->data, packet->size, 0);
            }
        } else {
            //写音频数据
            if(is_audio_complete){
                cur_pts_a = cur_pts_v*1000;
                continue;
            }
            if (audenc->pkt_queue->get_packet(packet)<0||packet->size == 0) {
                is_audio_complete = true;
                av_log(NULL,AV_LOG_FATAL,"audio recorder completed");
                continue;
            }
            packet->stream_index = audio_stream;
            av_packet_rescale_ts(packet, audenc->avctx->time_base, out->streams[audio_stream]->time_base);
            frame_audio_index++;
            cur_pts_a = packet->pts;
            if(aacbsfc){
                av_bitstream_filter_filter(aacbsfc,audenc->avctx, NULL, &packet->data, &packet->size, packet->data, packet->size, 0);
            }
        }
        av_interleaved_write_frame(out, packet);
    }
    if(h264bsfc)
        av_bitstream_filter_close(h264bsfc);
    if(aacbsfc)
        av_bitstream_filter_close(aacbsfc);
    av_write_trailer(out);
    av_log(NULL,AV_LOG_FATAL,"录制完成");
    result:
    av_packet_unref(packet);
    av_packet_free(&packet);
    if(audenc){
        delete(audenc);
        audenc = NULL;
    }
    if(videnc){
        delete(videnc);
        videnc = NULL;
    }
    if (out &&  out->pb)
        avio_close(out->pb);
    avformat_free_context(out);
    out = NULL;
    notify_message(RECORDER_STATE_COMPLETED,0,"录制完成");
}

int EasyRecorder::stream_component_open(int stream_index) {
    AVCodec *codec_out;
    AVDictionary *param = 0;
    AVStream *out_stream = avformat_new_stream(out, NULL);
    AVCodecContext *avctx_out = avcodec_alloc_context3(NULL);
    AVCodecParameters *parameters = out_stream->codecpar;
    int ret = ERROR_OK;
    switch (stream_index) {
        case AVMEDIA_TYPE_AUDIO:
            parameters->codec_id = AV_CODEC_ID_AAC;
            parameters->codec_type = AVMEDIA_TYPE_AUDIO;
            parameters->sample_rate = 44100;
            parameters->channel_layout = AV_CH_LAYOUT_STEREO;
            parameters->channels = av_get_channel_layout_nb_channels(parameters->channel_layout);
            parameters->bit_rate = 64000;
            parameters->format = AV_SAMPLE_FMT_FLTP;
            avctx_out->time_base = (AVRational) {1, 44100};
            avcodec_parameters_to_context(avctx_out, parameters);
            break;
        case AVMEDIA_TYPE_VIDEO:
            parameters->codec_id = AV_CODEC_ID_H264;
            parameters->width = width;
            parameters->height = height;
            parameters->format = AV_PIX_FMT_YUV420P;
//            parameters->bit_rate = bit_rate;
            parameters->codec_type = AVMEDIA_TYPE_VIDEO;
            avcodec_parameters_to_context(avctx_out, parameters);
            avctx_out->time_base = (AVRational) {1, frame_rate};
            avctx_out->gop_size = 300;
            avctx_out->thread_count = 12;
            avctx_out->qmin = 10;
            avctx_out->qmax = 51;
            avctx_out->max_b_frames = 3;
            avctx_out->me_pre_cmp = 1;
            av_opt_set(avctx_out->priv_data, "preset", "superfast", 0);
            av_opt_set(avctx_out->priv_data, "tune", "zerolatency", 0);
//            av_dict_set(&param, "tune", "zero-latency", 0);
//            av_dict_set(&param, "preset", "superfast", 0);
            av_dict_set_int(&param, "qp", 18,0);
            break;
        default:
            break;
    }
    if (out->oformat->flags & AVFMT_GLOBALHEADER)
        avctx_out->flags |= CODEC_FLAG_GLOBAL_HEADER;
    codec_out = avcodec_find_encoder(parameters->codec_id);
    if (!codec_out) {
        av_log(NULL, AV_LOG_FATAL, "can not find codec");
        ret == AVMEDIA_TYPE_AUDIO ? ERROR_FIND_AUDIO_ENCODER:ERROR_FIND_VIDEO_ENCODER;
        goto error;
    }
    if (avcodec_open2(avctx_out, codec_out, &param) < 0) {
        av_log(NULL, AV_LOG_FATAL, "Fail to open codec");
        ret == AVMEDIA_TYPE_AUDIO ? ERROR_OPEN_AUDIO_ENCODER :ERROR_OPEN_VIDEO_ENCODER;
        goto error;
    }
    av_dict_free(&param);
    if (stream_index == AVMEDIA_TYPE_VIDEO) {
        parameters->extradata = avctx_out->extradata;
        parameters->extradata_size = avctx_out->extradata_size;
        if(videnc)
            delete(videnc);
        videnc = new VideoEncoder;
        videnc->init(avctx_out);
        if(out_stream->time_base.num>0){
            videnc->init_filter((char *) filter_descr, out_stream->time_base);
        } else
            videnc->init_filter((char *) filter_descr, avctx_out->time_base);
        videnc->start_encode_thread();
    } else {
        if(audenc)
            delete(audenc);
        audenc = new AudioEncoder;
        if (audenc->init(avctx_out) != 0) {
            avcodec_free_context(&avctx_out);
            return ERROR_INIT_SWR_CONTEXT;
        }
        audenc->start_encode_thread();
    }
    return 0;
    error:
    if(avctx_out){
        avcodec_free_context(&avctx_out);
    }
    av_dict_free(&param);
    return  ret;
}

void EasyRecorder::recorder_img(uint8_t *data,int len) {
    wait_state(RecorderState::READY);
    if (!data) {
        videnc->data_queue.push(nullptr);
        return;
    }
    uint8_t *_data = (uint8_t *) malloc(len);
    memcpy(_data, data, len);
    videnc->data_queue.push(_data);
}

void EasyRecorder::recorder_audio(uint8_t *data, int len) {
    wait_state(RecorderState::READY);
    if (!data) {
        audenc->data_queue.push(nullptr);
        return;
    }
    if (audenc->buf_size == 0) {
        audenc->buf_size = len;
    }
    uint8_t *_data = (uint8_t *) malloc(len);
    memcpy(_data, data, len);
    audenc->data_queue.push(_data);
}
