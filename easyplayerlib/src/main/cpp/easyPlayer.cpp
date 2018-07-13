// 视频播放以及边播放边保存
// Created by JasonXiao on 2016/11/10.
//
extern "C"{
    #include "libavutil/channel_layout.h"
}
#include "easyPlayer.h"
/**
 * /storage/emulated/0/ffmpeg/icon.jpg
 */

void EasyPlayer::read() {
    if (!init_context()) {
        int err, i;
        ic = avformat_alloc_context();
        if (!ic) {
            notify_message(MEDIA_ERROR,ERROR_MALLOC_CONTEXT,-1,"分配input context出错");
            return;
        }
        err = avformat_open_input(&ic, filename, NULL, NULL);
        if (err < 0) {
            av_log(NULL, AV_LOG_FATAL, "Could not open input file.\n");
            notify_message(MEDIA_ERROR,ERROR_OPEN_FILE,-1,"打开视频文件失败");
            return;
        }

        err = avformat_find_stream_info(ic, NULL);
        if (err < 0) {
            av_log(NULL, AV_LOG_WARNING, "%s: could not find codec parameters\n", filename);
            notify_message(MEDIA_ERROR,ERROR_FIND_STREAM_INFO,-1,"查找视频流信息失败");
            return;
        }
        av_dump_format(ic, 0, ic->filename, 0);
        realtime = is_realtime();//判断时间是否是视频的真实时间
        for (i = 0; i < ic->nb_streams; i++) {
            if (ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO||ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {//音视频
                stream_component_open(i);
            }
        }
        if (video_stream < 0 && audio_stream < 0) {
            av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n", filename);
            notify_message(MEDIA_ERROR,ERROR_FIND_STREAM_INFO,-1,"查找视频流信息失败");
            return;
        }
        on_state_change(PlayerState::READY);//更改播放器状态为准备就绪
        notify_message(MEDIA_PREPARED,0,0,"准备就绪");
        if (video_stream >= 0) {//回调界面尺寸
            notify_message(MEDIA_SET_VIDEO_SIZE, viddec->get_width(), viddec->get_height(),"调整界面尺寸");
        }
        file_changed = false;
    }
    do_play();
}

bool EasyPlayer::init_context() {
    int ret;
    if (ic && !file_changed) {
        //重新播放
        ret = av_seek_frame(ic, -1, 0, 0);
        if (ret >= 0) {
            //进度条拖动成功
            if (video_stream >= 0) {
                viddec->pkt_queue->flush();
                viddec->frame_queue->flush();
                viddec->start_decode_thread();
            }
            if (audio_stream >= 0) {
                auddec->pkt_queue->flush();
                auddec->frame_queue->flush();
                auddec->start_decode_thread();
            }
            queue_attachments_req = 1;
            eof = 0;
            on_state_change(PlayerState::READY);
            return true;
        }
    }
    return false;
}

void EasyPlayer::do_play() {
    int ret;
    AVPacket *pkt = av_packet_alloc();
    AVPacket *recorder = av_packet_alloc();
    int64_t stream_start_time;
    int64_t pkt_ts;
    int pkt_in_play_range = 0;
    recorder_queue.set_abort(0);
    abort_request = 0;
    while (true) {
        if (abort_request){
            if (video_stream >= 0) {
                viddec->pkt_queue->put_nullpacket();
            }
            if (audio_stream >= 0) {
                auddec->pkt_queue->put_nullpacket();
            }
            break;
        }
        if (paused)
            av_read_pause(ic);//暂停
        else
            av_read_play(ic);//播放
        if (seek_req) {//拉动进度条
            ret = av_seek_frame(ic, -1, seek_pos * AV_TIME_BASE, 0);
            if (ret < 0) {
                av_log(NULL, AV_LOG_FATAL, "%s: error while seeking\n", filename);
            } else {
                if (audio_stream >= 0) {
                    auddec->pkt_queue->flush();//清空音频队列
                    auddec->frame_queue->flush();
                }
                if (video_stream >= 0) {
                    viddec->pkt_queue->flush();//清空视频队列
                    viddec->frame_queue->flush();
                }
            }
            seek_req = false;
            queue_attachments_req = 1;
            eof = 0;
            notify_message(MEDIA_SEEK_COMPLETE,ret,ret,"seek end");
            av_log(NULL, AV_LOG_FATAL, "seek end");
        }
        ret = av_read_frame(ic, pkt);//获取AVPacket
        if(!av_packet_ref(recorder,pkt)){
            recorder_queue.put_packet(recorder);
        }
        if (ret == EOF || ret == AVERROR_EOF) {//播放完成
            if (video_stream >= 0) {
                viddec->pkt_queue->put_nullpacket();
            }
            if (audio_stream >= 0) {
                auddec->pkt_queue->put_nullpacket();
            }
            break;
        } else {
            /* check if packet is in play range specified by user, then queue, otherwise discard */
            stream_start_time = ic->streams[pkt->stream_index]->start_time;
            pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
            pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                                (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                                (double) (start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
                                <= ((double) duration / 1000000);
            if (pkt->stream_index == audio_stream && pkt_in_play_range) {
                if (auddec->pkt_queue->put_packet(pkt) < 0)break;
            } else if (pkt->stream_index == video_stream && pkt_in_play_range
                       && !(video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                if (viddec->pkt_queue->put_packet(pkt) < 0)break;
            } else {
                av_packet_unref(pkt);
                av_packet_unref(recorder);
            }
        }
    }
    av_packet_free(&recorder);
    av_packet_free(&pkt);
    if(state == PlayerState::STOP){
        release();
        notify_message(MEDIA_STOP,0,0,"停止播放");
    }
}

bool EasyPlayer::is_realtime() {
    if (ic == nullptr) return false;
    if (!strcmp(ic->iformat->name, "rtp")
        || !strcmp(ic->iformat->name, "rtsp")
        || !strcmp(ic->iformat->name, "sdp"))
        return true;

    if (ic->pb && (!strncmp(ic->filename, "rtp:", 4)
                   || !strncmp(ic->filename, "udp:", 4)))
        return 1;
    return 0;
}

//开启解码器组件
int EasyPlayer::stream_component_open(int stream_index) {
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
    av_codec_set_pkt_timebase(avctx, ic->streams[stream_index]->time_base);
    codec = avcodec_find_decoder(avctx->codec_id);
    if (!codec)
        return AVERROR(ENOMEM);;
    eof = 0;
//    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_FATAL, "Fail to open codec on stream %d\n", stream_index);
        return ret;
    }
    //根据音视频类型，获取SwrContext，用于转换数据
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            swr_ctx = swr_alloc();
            swr_ctx = swr_alloc_set_opts(NULL, avctx->channel_layout, AV_SAMPLE_FMT_S16, avctx->sample_rate,
                                         avctx->channel_layout, avctx->sample_fmt, avctx->sample_rate,
                                         0, NULL);
            if (!swr_ctx || swr_init(swr_ctx) < 0) {
                av_log(NULL, AV_LOG_FATAL, "Cannot create sample rate converter for conversion channels!\n");
                avcodec_free_context(&avctx);
                swr_free(&swr_ctx);
                return -1;
            }
            audio_stream = stream_index;
            audio_st = ic->streams[stream_index];
            if(auddec){
                delete(auddec);
            }
            auddec = new AudioDecoder;
            auddec->init(avctx);
            auddec->start_decode_thread();
            break;
        case AVMEDIA_TYPE_VIDEO:
            video_stream = stream_index;
            video_st = ic->streams[stream_index];
            img_convert_ctx = sws_getContext(avctx->width, avctx->height, avctx->pix_fmt,
                                             avctx->width, avctx->height, AV_PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);
            if(viddec)
                delete(viddec);
            viddec = new VideoDecoder;
            viddec->init(avctx);
            viddec->start_decode_thread();
            break;
        default:
            break;
    }
    return ret;
}


//是否有视频数据
bool EasyPlayer::has_video() {
    if (ic) {
        return video_stream >= 0;
    }
    return false;
}

bool EasyPlayer::has_audio() {
    if (ic) {
        return audio_stream >= 0;
    }
    return false;
}

//获取可显示的图片
bool EasyPlayer::get_img_frame(AVFrame *frame) {
    if (!frame || !viddec) return false;
    auto av_frame = viddec->frame_queue->get_frame();
    if (av_frame == nullptr) {//文件结束
        if (recorder) {
            stop_recorder();
        }
        if(state != PlayerState::STOP&&state!=PlayerState::COMPLETED)
            on_state_change(PlayerState::COMPLETED);
        return false;
    }
    if (!recorder) {
        recorder_queue.remove_one();
    }
    sws_scale(img_convert_ctx, (const uint8_t *const *) av_frame->frame->data, av_frame->frame->linesize, 0, viddec->avctx->height,
              frame->data, frame->linesize);
    double timestamp = av_frame_get_best_effort_timestamp(av_frame->frame) * av_q2d(video_st->time_base);
    if (audio_clock && timestamp > audio_clock) {//保证视频同音频同步
        usleep((unsigned long) ((timestamp - audio_clock) * 1000000));
    }
    av_frame_free(&av_frame->frame);
    return true;
}

//获取音频数据
bool EasyPlayer::get_aud_buffer(int &nextSize, uint8_t *outputBuffer) {
    if (!outputBuffer||!auddec) return false;
    auto av_frame = auddec->frame_queue->get_frame();
    if (av_frame == nullptr) {
        nextSize = 0;
        if (recorder) {
            stop_recorder();
        }
        if(state != PlayerState::STOP&&state!=PlayerState::COMPLETED)
            on_state_change(PlayerState::COMPLETED);
        return false;
    }
    if (!recorder) {
        recorder_queue.remove_one();
    }
    if (av_sample_fmt_is_planar(auddec->avctx->sample_fmt)) {
        nextSize = av_samples_get_buffer_size(av_frame->frame->linesize, auddec->avctx->channels, av_frame->frame->nb_samples, auddec->avctx->sample_fmt, 1);
    } else {
        nextSize = av_samples_get_buffer_size(NULL, auddec->avctx->channels, auddec->avctx->frame_size, auddec->avctx->sample_fmt, 1);
    }
    int ret = swr_convert(swr_ctx, &outputBuffer, av_frame->frame->nb_samples,
                          (uint8_t const **) (av_frame->frame->extended_data),
                          av_frame->frame->nb_samples);
    nextSize = ret * auddec->avctx->channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    audio_clock = av_frame->frame->pkt_pts * av_q2d(audio_st->time_base);
    av_frame_free(&av_frame->frame);
    return ret >= 0;
}


//
void EasyPlayer::wait_state(PlayerState need_state) {
    std::unique_lock<std::mutex> lock(mutex);
    state_condition.wait(lock, [this, need_state] {
        return this->state >= need_state;
    });
}

void EasyPlayer::on_state_change(PlayerState state) {
    std::unique_lock<std::mutex> lock(mutex);
    this->state = state;
    state_condition.notify_all();
    if(this->state == PlayerState::COMPLETED){
        notify_message(MEDIA_PLAYBACK_COMPLETE,0,0,"视频播放完成");
    }
}

EasyPlayer::~EasyPlayer() {
    release();
}
void EasyPlayer::stop() {
    if(state >= PlayerState::READY){
        abort_request = 1;
        on_state_change(PlayerState::STOP);
    }
}
void EasyPlayer::release() {
    if(viddec){
        viddec->flush();
        viddec->wait_thread_stop();
    }
    if(auddec){
        auddec->flush();
        auddec->wait_thread_stop();
    }
    if (ic) {
        avformat_free_context(ic);
        ic = NULL;
    }
    if (out) {
        avformat_free_context(out);
        out = NULL;
    }
    if(viddec){
        delete(viddec);
        viddec = nullptr;
    }
    if(auddec){
        delete(auddec);
        auddec = nullptr;
    }
    if(filename){
        delete(filename);
        filename = nullptr;
    }
    if(savename){
        delete(savename);
        savename = nullptr;
    }
    if(video_st){
        video_st = NULL;
    }
    if(audio_st){
        audio_st = NULL;
    }
    if(swr_ctx){
        swr_free(&swr_ctx);
        swr_ctx = NULL;
    }
    if(img_convert_ctx){
        sws_freeContext(img_convert_ctx);
        img_convert_ctx = NULL;
    }
    av_log(NULL,AV_LOG_FATAL,"播放器清理完成");
}

void EasyPlayer::wait_paused() {
    std::unique_lock<std::mutex> lock(mutex);
    pause_condition.wait(lock, [this] {
        return !this->paused;
    });
}


void EasyPlayer::stream_seek(int64_t pos) {
    if (!seek_req) {
        seek_pos = pos;
        seek_req = true;
    }
}

void EasyPlayer::start_recorder(const std::string save_filename) {
    if (out) {
        if (recorder) {
            stop_recorder();
        }
        avformat_free_context(out);
    }
    if (save_filename.empty()) {
        av_log(nullptr, AV_LOG_FATAL, "保存文件路径不能为空\n");
        return;
    }
    savename = av_strdup(save_filename.c_str());
    std::thread thread(&EasyPlayer::recorder_run, this);
    thread.detach();
}

void EasyPlayer::recorder_run() {
    int err;
    err = avformat_alloc_output_context2(&out, NULL, NULL, savename);
    if (out == NULL) {
        av_log(NULL, AV_LOG_FATAL, "分配context出错:%d", err);
        notify_message(REOCRDER_ERROR,ERROR_MALLOC_CONTEXT,err,"分配output context出错");
        return;
    }
    err = avio_open2(&out->pb, out->filename, AVIO_FLAG_READ_WRITE, NULL, 0);
    if (err < 0) {
        av_log(NULL, AV_LOG_FATAL, "打开存储文件出错%s:%d", savename, err);
        notify_message(REOCRDER_ERROR,ERROR_OPEN_FILE,err,"打开输出视频位置失败");
        avformat_free_context(out);
        return;
    }
    int i;
    if (has_video() && recorder_compenent_open(video_stream) < 0) {
        avio_close(out->pb);
        avformat_free_context(out);
        notify_message(REOCRDER_ERROR,ERROR_OPEN_VIDEO_ENCODER,err,"打开视频编码器出错");
        return;
    }
    if (has_audio() && recorder_compenent_open(audio_stream) < 0) {
        avio_close(out->pb);
        avformat_free_context(out);
        notify_message(REOCRDER_ERROR,ERROR_OPEN_AUDIO_ENCODER,err,"打开音频编码器出错");
        return;
    }
    av_dump_format(out, 0, out->filename, 1);
    for (i = 0; i < out->nb_streams; i++) {
        if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            last_audio_stream = i;
        } else if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            last_video_stream = i;
        }
    }
    avformat_write_header(out, NULL);
    AVPacket *packet = av_packet_alloc();
    recorder = true;

    int index;
    AVBitStreamFilterContext *h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
    recorder_queue.set_abort(0);
    bool find_key_frame = false;
    while (recorder) {
        recorder_queue.get_packet(packet);
        if (packet->size == 0) {
            break;
        }
        index = packet->stream_index = packet->stream_index == audio_stream ? last_audio_stream : last_video_stream;
        if (!find_key_frame) {
            if (index == last_video_stream && (packet->flags & AV_PKT_FLAG_KEY))
                find_key_frame = true;
            else
                continue;
        }
        packet->pts = av_rescale_q_rnd(packet->pts, ic->streams[index]->time_base, out->streams[index]->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet->dts = av_rescale_q_rnd(packet->dts, ic->streams[index]->time_base, out->streams[index]->time_base, (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet->duration = av_rescale_q(packet->duration, ic->streams[index]->time_base, out->streams[index]->time_base);
        if (index == last_video_stream && out->streams[index]->codec->codec_id == AV_CODEC_ID_H264) {
            av_bitstream_filter_filter(h264bsfc, ic->streams[index]->codec, NULL, &packet->data, &packet->size, packet->data, packet->size, 0);
        }
        av_interleaved_write_frame(out, packet);
        av_packet_unref(packet);
    }
    av_bitstream_filter_close(h264bsfc);
    av_write_trailer(out);
    av_packet_free(&packet);
    if (out && !(out->oformat->flags & AVFMT_NOFILE))
        avio_close(out->pb);
    avformat_free_context(out);
    notify_message(RECODER_COMPLETED,0,0,"录制视频结束");
}

int EasyPlayer::recorder_compenent_open(int index) {
    AVCodecContext *avctx_out;
    AVCodec *codec_out;
    AVStream *out_stream;
    AVDictionary *param = 0;
    AVCodecParameters *parameters;
    AVCodecContext *avctx = ic->streams[index]->codec;
    out_stream = avformat_new_stream(out, 0);
    avctx_out = avcodec_alloc_context3(NULL);
    parameters = out_stream->codecpar;
    switch (ic->streams[index]->codecpar->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            parameters->codec_id = audio_st->codecpar->codec_id;
            parameters->codec_type = audio_st->codecpar->codec_type;
            parameters->sample_rate = audio_st->codecpar->sample_rate;
            parameters->channel_layout = audio_st->codecpar->channel_layout;
            parameters->channels = audio_st->codecpar->channels;
            parameters->bit_rate = audio_st->codecpar->bit_rate;
            parameters->format = audio_st->codecpar->format;
            avcodec_parameters_to_context(avctx_out, parameters);
            avctx_out->max_b_frames = ic->streams[index]->codec->max_b_frames;
            break;
        case AVMEDIA_TYPE_VIDEO:
            avcodec_parameters_copy(parameters,ic->streams[index]->codecpar);
            avcodec_parameters_to_context(avctx_out, parameters);
            avctx_out->gop_size = avctx->gop_size;
            avctx_out->thread_count = avctx->thread_count;
            avctx_out->qmin = 10;
            avctx_out->qmax = 51;
            avctx_out->time_base = ic->streams[index]->time_base;
            if (out->oformat->flags & AVFMT_GLOBALHEADER)
                avctx_out->flags |= CODEC_FLAG_GLOBAL_HEADER;
            avctx_out->max_b_frames = ic->streams[index]->codec->max_b_frames;
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
    av_codec_set_pkt_timebase(avctx_out, ic->streams[index]->time_base);
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
    return 0;
}

void EasyPlayer::set_data_source(const std::string input_filename) {
    if (input_filename.empty()) {
        av_log(nullptr, AV_LOG_FATAL, "An input file must be specified\n");
        return;
    }
    if (filename != nullptr && strcmp(input_filename.c_str(), filename) == 0) {
        file_changed = false;
        return;
    }
    file_changed = true;
    filename = av_strdup(input_filename.c_str());
}


void EasyPlayer::prepare() {
    if (!ic || file_changed) {
        av_register_all();
        avformat_network_init();
    }
    std::thread read_thread(&EasyPlayer::read, this);
    read_thread.join();
}

void EasyPlayer::prepare_async() {
    if (!ic || file_changed) {
        av_register_all();
        avformat_network_init();
    }
    on_state_change(PlayerState::INIT);
    std::thread read_thread(&EasyPlayer::read, this);
    read_thread.detach();
}

