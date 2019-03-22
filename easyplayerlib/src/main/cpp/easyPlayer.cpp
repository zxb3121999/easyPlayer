// 视频播放以及边播放边保存
// Created by JasonXiao on 2016/11/10.
//
extern "C" {
#include "libavutil/channel_layout.h"
#include "libavutil/display.h"
}

#include "easyPlayer.h"

/**
 * /storage/emulated/0/ffmpeg/icon.jpg
 */
double get_rotation(AVStream *st) {
    uint8_t *displaymatrix = av_stream_get_side_data(st,
                                                     AV_PKT_DATA_DISPLAYMATRIX, NULL);
    double theta = 0;
    if (displaymatrix)
        theta = -av_display_rotation_get((int32_t *) displaymatrix);

    theta -= 360 * floor(theta / 360 + 0.9 / 360);

    if (fabs(theta - 90 * round(theta / 90)) > 2)
        av_log(NULL, AV_LOG_WARNING, "Odd rotation angle.\n"
                                     "If you want to help, upload a sample "
                                     "of this file to ftp://upload.ffmpeg.org/incoming/ "
                                     "and contact the ffmpeg-devel mailing list. (ffmpeg-devel@ffmpeg.org)");

    return theta;
}

void EasyPlayer::read() {
    if (!init_context()) {
        int err, i;
        is_open_input = true;
        AVDictionary *param = 0;
//        av_dict_set(&param, "user_agent",
//                    "Mozilla/5.0 (Linux; U; Android 26; zh-cn; HuaWei Build/FRF91) AppleWebKit/533.1 (KHTML, like Gecko) Version/4.0 Mobile Safari/533.1",
//                    0);
        err = avformat_open_input(&ic, filename, NULL, &param);
        av_dict_free(&param);
        if (err < 0) {
            is_open_input = false;
            av_log(NULL, AV_LOG_FATAL, "Could not open input file.\n");
            notify_message(MEDIA_ERROR, ERROR_OPEN_FILE, -1, "打开视频文件失败");
            return;
        }
        is_open_input = false;
        open_input_condition.notify_all();
        if (state != PlayerState::STOP && state != PlayerState::COMPLETED) {
            err = avformat_find_stream_info(ic, NULL);
            if (err < 0) {
                av_log(NULL, AV_LOG_WARNING, "%s: could not find codec parameters\n", filename);
                notify_message(MEDIA_ERROR, ERROR_FIND_STREAM_INFO, -1, "查找视频流信息失败");
                return;
            }
            av_dump_format(ic, 0, ic->url, 0);
//        realtime = is_realtime();//判断时间是否是视频的真实时间
            for (i = 0; i < ic->nb_streams; i++) {
                if (ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ||
                    ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {//音视频
                    stream_component_open(i);
                } else if (ic->streams[i]->codecpar->codec_tag == AVMEDIA_TYPE_SUBTITLE){
                    av_log(NULL,AV_LOG_FATAL,"找到字幕");
                }
            }
            if (video_stream < 0 && audio_stream < 0) {
                av_log(NULL, AV_LOG_FATAL, "Failed to open file '%s' or configure filtergraph\n",
                       filename);
                notify_message(MEDIA_ERROR, ERROR_FIND_STREAM_INFO, -1, "查找视频流信息失败");
                return;
            }
            on_state_change(PlayerState::READY);//更改播放器状态为准备就绪
            notify_message(MEDIA_PREPARED, 0, 0, "准备就绪");
            if (video_stream >= 0) {//回调界面尺寸
                notify_message(MEDIA_SET_VIDEO_SIZE, native_window_width, native_window_height,
                               "调整界面尺寸");
            }
            file_changed = false;
        } else {
            return;
        }
    }
    do_play();
}

bool EasyPlayer::init_context() {
    int ret = 0;
    if (ic && !file_changed && state != PlayerState::STOP) {
        //重新播放
        is_open_input = true;
        ret = av_seek_frame(ic, -1, 0, 0);
        is_open_input = false;
        open_input_condition.notify_all();
        if (ret >= 0 && state != PlayerState::STOP) {
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
            audio_clock = 0;
            on_state_change(PlayerState::READY);
            return true;
        }
    }
    if (ic) {
        if (viddec) {
            viddec->flush();
            viddec->wait_thread_stop();
        }
        if (auddec) {
            auddec->flush();
            auddec->wait_thread_stop();
        }
        if (viddec) {
            delete (viddec);
            viddec = nullptr;
        }
        if (auddec) {
            delete (auddec);
            auddec = nullptr;
        }

        if (ic) {
            avformat_close_input(&ic);
        }

        if (out) {
            avformat_free_context(out);
            out = NULL;
        }
        if (swr_ctx) {
            swr_free(&swr_ctx);
            swr_ctx = NULL;
        }
        if (img_convert_ctx) {
            sws_freeContext(img_convert_ctx);
            img_convert_ctx = NULL;
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
    on_state_change(PlayerState::BUFFERING);
    read_complete = false;
    if (play_when_ready) {
        play();
    }
    int re_try = 0;
    while (true) {
        if (abort_request) {
            if (video_stream >= 0) {
                viddec->flush();
            }
            if (audio_stream >= 0) {
                auddec->flush();
            }
            break;
        }
        if (paused) {
            av_read_pause(ic);//暂停
        } else
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
            notify_message(MEDIA_SEEK_COMPLETE, ret, ret, "seek end");
        }
        ret = av_read_frame(ic, pkt);//获取AVPacket
        if (!av_packet_ref(recorder, pkt)) {
            recorder_queue.put_packet(recorder);
        }
        if ((ret == EOF || ret == AVERROR_EOF) && re_try < 4) {//尝试3次
            re_try++;
            av_log(NULL, AV_LOG_FATAL, "重新尝试第%d次", re_try);
            continue;
        }
        re_try = 0;
        if (ret == EOF || ret == AVERROR_EOF) {//播放完成
            if (video_stream >= 0) {
                viddec->pkt_queue->put_nullpacket();
            }
            if (audio_stream >= 0) {
                auddec->pkt_queue->put_nullpacket();
            }
            on_state_change(PlayerState::BUFFERING_COMPLETED);
            break;
        } else {
            /* check if packet is in play range specified by user, then queue, otherwise discard */
            stream_start_time = ic->streams[pkt->stream_index]->start_time;
            pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
            pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                                (pkt_ts -
                                 (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
                                av_q2d(ic->streams[pkt->stream_index]->time_base) -
                                (double) (start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000
                                <= ((double) duration / 1000000);
            if (pkt->stream_index == audio_stream && pkt_in_play_range) {
                if (auddec->pkt_queue->put_packet(pkt) < 0)break;
            } else if (pkt->stream_index == video_stream && pkt_in_play_range
                       && !(ic->streams[video_stream]->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                if (viddec->pkt_queue->put_packet(pkt) < 0)break;
            } else {
                av_packet_unref(pkt);
                av_packet_unref(recorder);
            }
        }
    }
    av_packet_free(&recorder);
    av_packet_free(&pkt);
    av_log(NULL, AV_LOG_FATAL, "av_read_frame完成");
    read_complete = true;
    stop_condition.notify_all();
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
    uint8_t *displaymatrix = NULL;
    double theta = 0;
    char *s = new char[64];
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            swr_ctx = swr_alloc_set_opts(NULL, avctx->channels >= 2 ? AV_CH_LAYOUT_STEREO
                                                                    : AV_CH_FRONT_CENTER,
                                         AV_SAMPLE_FMT_S16,
                                         avctx->sample_rate,
                                         avctx->channel_layout, avctx->sample_fmt,
                                         avctx->sample_rate,
                                         0, NULL);
            if (!swr_ctx || swr_init(swr_ctx) < 0) {
                av_log(NULL, AV_LOG_FATAL,
                       "Cannot create sample rate converter for conversion channels!\n");
                avcodec_free_context(&avctx);
                swr_free(&swr_ctx);
                return -1;
            }
            audio_stream = stream_index;
            if (auddec) {
                delete (auddec);
            }
            auddec = new AudioDecoder;
            auddec->init(avctx);
            auddec->start_decode_thread();
            break;
        case AVMEDIA_TYPE_VIDEO:
            video_stream = stream_index;
            if (viddec)
                delete (viddec);
            viddec = new VideoDecoder;
            viddec->init(avctx);
            theta = get_rotation(ic->streams[stream_index]);
            native_window_width = avctx->width;
            native_window_height = avctx->height;
            if (fabs(theta - 90) < 1.0) {
                native_window_width = avctx->height;
                native_window_height = avctx->width;
                viddec->init_filter("transpose=clock", ic->streams[stream_index]->time_base);
                av_log(NULL, AV_LOG_INFO, "需要顺时针旋转90度");
            } else if (fabs(theta - 180) < 1.0) {
                viddec->init_filter("transpose=clock,transpose=clock",
                                    ic->streams[stream_index]->time_base);
                av_log(NULL, AV_LOG_INFO, "需要顺时针旋转180度");
            } else if (fabs(theta - 270) < 1.0) {
                native_window_width = avctx->height;
                native_window_height = avctx->width;
                viddec->init_filter("transpose=cclock", ic->streams[stream_index]->time_base);
                av_log(NULL, AV_LOG_INFO, "需要逆时针旋转90度");
            } else if(fabs(theta)>1.0){
                sprintf(s,"rotate=%.2f*PI/180",theta);
                viddec->init_filter(s,ic->streams[stream_index]->time_base);
                av_log(NULL, AV_LOG_INFO, "需要旋转%.2f度",theta);
            }
            viddec->start_decode_thread();
            break;
        case AVMEDIA_TYPE_SUBTITLE:
            break;
        default:
            break;
    }
    delete[] s;
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
    AVFrame *av_frame = viddec->frame_queue->get_frame();
    if (av_frame == NULL) {//文件结束
        if (recorder) {
            stop_recorder();
        }
        return false;
    }
    if (!recorder) {
        recorder_queue.remove_one();
    }
    if (!img_convert_ctx) {
        av_log(NULL, AV_LOG_FATAL, "frame width:%d,height:%d", av_frame->width, av_frame->height);
        img_convert_ctx = sws_getContext(av_frame->width, av_frame->height, viddec->avctx->pix_fmt,
                                         native_window_width, native_window_height, AV_PIX_FMT_RGBA,
                                         SWS_FAST_BILINEAR, NULL, NULL, NULL);
    }
    sws_scale(img_convert_ctx, (const uint8_t *const *) av_frame->data, av_frame->linesize, 0,
              av_frame->height, frame->data, frame->linesize);
    double timestamp =
            av_frame->best_effort_timestamp * av_q2d(ic->streams[video_stream]->time_base);
    if (audio_clock && !audio_complete && timestamp > audio_clock) {//保证视频同音频同步
        usleep((unsigned long) ((timestamp - audio_clock) * 1000000));
    }
    av_frame_free(&av_frame);
    return true;
}


//获取音频数据
bool EasyPlayer::get_aud_buffer() {
    if (!auddec) return false;
    AVFrame *frame = auddec->frame_queue->get_frame();
    if (!frame) {
        nextSize = 0;
        if (recorder) {
            stop_recorder();
        }
        return false;
    }
    if (!recorder) {
        recorder_queue.remove_one();
    }
    if (av_sample_fmt_is_planar(auddec->avctx->sample_fmt)) {
        nextSize = av_samples_get_buffer_size(frame->linesize, auddec->get_channels(),
                                              frame->nb_samples, auddec->avctx->sample_fmt, 1);
    } else {
        nextSize = av_samples_get_buffer_size(NULL, auddec->get_channels(),
                                              auddec->avctx->frame_size, auddec->avctx->sample_fmt,
                                              1);
    }
    if (!outputBuffer) {
        outputBufferSize = nextSize + 1;
        outputBuffer = (uint8_t *) malloc(sizeof(uint8_t) * outputBufferSize);
    }
    int ret = swr_convert(swr_ctx, &outputBuffer, frame->nb_samples,
                          (uint8_t const **) (frame->extended_data), frame->nb_samples);
    nextSize = ret * auddec->get_channels() * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    if (outputBufferSize < nextSize) {
        outputBufferSize = nextSize + 1;
        outputBuffer = (uint8_t *) realloc(outputBuffer, sizeof(uint8_t) * outputBufferSize);
        swr_convert(swr_ctx, &outputBuffer, frame->nb_samples,
                    (uint8_t const **) (frame->extended_data), frame->nb_samples);
    }
    audio_clock = frame->pts * av_q2d(ic->streams[audio_stream]->time_base);
    av_frame_free(&frame);
    return ret >= 0;
}


/*************open sles********************/
void EasyPlayer::createAudioEngine() {
    release_sles();
    SLresult result;

    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the environmental reverb interface
    // this could fail if the environmental reverb effect is not available,
    // either because the feature is not present, excessive CPU load, or
    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void) result;
    }
}

void EasyPlayer::createBufferQueueAudioPlayer(int sampleRate, int channel) {
    SLresult result;
    if (sampleRate >= 0) {
        bqPlayerSampleRate = sampleRate * 1000;
    }

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, (SLuint32) channel, SL_SAMPLINGRATE_44_1,
                                   SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                   SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
    /*
     * Enable Fast Audio when possible:  once we set the same rate to be the native, fast audio path
     * will be triggered
     */
    if (bqPlayerSampleRate) {
        format_pcm.samplesPerSec = bqPlayerSampleRate;       //sample rate in mili second
    }
    if (channel == 1) {
        format_pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
    } else {
        format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    }
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    /*
     * create audio player:
     *     fast audio does not support when SL_IID_EFFECTSEND is required, skip it
     *     for fast audio case
     */
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/ };
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
                                                3, ids, req);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // realize the player
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the play interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the buffer queue interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // register callback on the buffer queue
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, callback, &id);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // get the effect send interface
    bqPlayerEffectSend = NULL;
    if (0 == bqPlayerSampleRate) {
        result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
                                                 &bqPlayerEffectSend);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;
    }

#if 0   // mute/solo is not supported for sources that are known to be mono, as this is
    // get the mute/solo interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_MUTESOLO, &bqPlayerMuteSolo);
    assert(SL_RESULT_SUCCESS == result);
    (void)result;
#endif

    // get the volume interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;

    // set the player's state to playing
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    assert(SL_RESULT_SUCCESS == result);
    (void) result;
}

void EasyPlayer::releaseResampleBuf() {
    if (outputBuffer) {
        free(outputBuffer);
        outputBuffer = NULL;
    }
    if (0 == bqPlayerSampleRate) {
        /*
         * we are not using fast path, so we were not creating buffers, nothing to do
         */
        return;
    }
    if (resampleBuf)
        free(resampleBuf);
    resampleBuf = NULL;
}

void destory_object(SLObjectItf &obj) {
    if (obj) {
        (*obj)->Destroy(obj);
    }
    obj = NULL;
}

void EasyPlayer::release_sles() {
    releaseResampleBuf();
    destory_object(bqPlayerObject);
    bqPlayerObject = NULL;
    bqPlayerPlay = NULL;
    bqPlayerBufferQueue = NULL;
    bqPlayerEffectSend = NULL;
    bqPlayerMuteSolo = NULL;
    bqPlayerVolume = NULL;

    destory_object(outputMixObject);
    outputMixObject = NULL;
    outputMixEnvironmentalReverb = NULL;

    destory_object(engineObject);
    engineObject = NULL;
    engineEngine = NULL;
}
/*****************open sles end***************************/
//
void EasyPlayer::play_video() {
    wait_state(PlayerState::READY);
    if (!has_video() || !nativeWindow)
        return;
    ANativeWindow_acquire(nativeWindow);
    if (state == PlayerState::STOP || state == PlayerState::COMPLETED) {
        set_window(NULL);
        return;
    }
    if (0 >
        ANativeWindow_setBuffersGeometry(nativeWindow, native_window_width, native_window_height,
                                         WINDOW_FORMAT_RGBA_8888)) {
        av_log(NULL, AV_LOG_FATAL, "Couldn't set buffers geometry.\n");
        notify_message(MEDIA_ERROR, -10001, 1, "Couldn't set buffers geometry");
        set_window(NULL);
        return;
    }
    window_inited = true;
    AVFrame *frameRGBA = av_frame_alloc();
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, native_window_width,
                                            native_window_height, 1);
    uint8_t *vOutBuffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRGBA->data, frameRGBA->linesize, vOutBuffer, AV_PIX_FMT_RGBA,
                         native_window_width, native_window_height, 1);
    ANativeWindow_Buffer *windowBuffer = new ANativeWindow_Buffer;
    av_log(NULL, AV_LOG_FATAL, "start play video");
    set_play_state(false, false);
    while (get_img_frame(frameRGBA)) {
        if (get_paused()) {
            wait_paused();
        }
        if (is_in_background || !window_inited) {
            continue;
        }
        native_window_mutex.lock();
        if (nativeWindow&&ANativeWindow_lock(nativeWindow, windowBuffer, NULL) < 0) {
            av_log(NULL, AV_LOG_FATAL, "锁定window失败.\n");
            native_window_mutex.unlock();
            if(is_in_background || !window_inited)
                continue;
            notify_message(MEDIA_ERROR, -10002, 1, "锁定window失败");
            break;
        } else if(nativeWindow){
            uint8_t *dst = (uint8_t *) windowBuffer->bits;
            for (int h = 0; h < native_window_height; h++) {
                memcpy(dst + h * windowBuffer->stride * 4, vOutBuffer + h * frameRGBA->linesize[0],
                       frameRGBA->linesize[0]);
            }
            ANativeWindow_unlockAndPost(nativeWindow);
            native_window_mutex.unlock();
        }
    }
    if (windowBuffer) {
        free(windowBuffer);
    }
    av_free(vOutBuffer);
    av_frame_free(&frameRGBA);
    set_play_state(false, true);
    av_log(NULL, AV_LOG_FATAL, "end play video");
    return;
}

void EasyPlayer::play_audio() {
    wait_state(PlayerState::READY);
    if (!has_audio())
        return;
    if (state == PlayerState::STOP || state == PlayerState::COMPLETED) {
        return;
    }
    if (!file_changed && bqPlayerObject) {
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    } else {
        createAudioEngine();
        createBufferQueueAudioPlayer(auddec->get_sample_rate(), auddec->get_channels());
    }
    set_play_state(true, false);
    callback(bqPlayerBufferQueue, &id);
}

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
    if (this->state == PlayerState::COMPLETED) {
        notify_message(MEDIA_PLAYBACK_COMPLETE, 0, 0, "视频播放完成");
    } else if (this->state == PlayerState::PLAYING){
        notify_message(MEDIA_PLAYING,0,0,"开始播放");
    }
}

EasyPlayer::~EasyPlayer() {
    release();
}

void EasyPlayer::stop() {
    PlayerState old_state = state;
    on_state_change(PlayerState::STOP);
    if (is_open_input) {
        //等头文件读完过后再进行施放
        std::unique_lock<std::mutex> lock(open_input_mutex);
        open_input_condition.wait(lock, [this] {
            return !this->is_open_input;
        });
        av_log(NULL, AV_LOG_DEBUG, "open input 完成");
    }
    if (old_state >= PlayerState::READY) {
        abort_request = 1;
        if (old_state >= PlayerState::READY && get_paused()) {
            paused = false;
            pause_condition.notify_all();
        }
        if (is_recordering()) {
            stop_recorder();
        } else {
            recorder_queue.set_abort(1);
            recorder_queue.flush();
        }
        if (old_state <= PlayerState::BUFFERING || old_state == PlayerState::BUFFERING_COMPLETED) {
            if (viddec)
                viddec->flush();
            if (auddec)
                auddec->flush();
        }
        std::unique_lock<std::mutex> lock(stop_mutex);
        stop_condition.wait(lock, [this] {
            return read_complete && video_complete && audio_complete && !recorder;
        });
    }
}

void EasyPlayer::release() {
    recorder_queue.flush();
    event_listener = NULL;
    callback = NULL;
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = NULL;
    }
    release_sles();
    if (viddec) {
        viddec->flush();
        viddec->wait_thread_stop();
    }
    if (auddec) {
        auddec->flush();
        auddec->wait_thread_stop();
    }
    if (viddec) {
        delete (viddec);
        viddec = nullptr;
    }
    if (auddec) {
        delete (auddec);
        auddec = nullptr;
    }

    if (ic) {
        avformat_close_input(&ic);
    }

    if (out) {
        avformat_free_context(out);
        out = NULL;
    }

    if (filename) {
        delete (filename);
        filename = nullptr;
    }
    if (savename) {
        delete (savename);
        savename = nullptr;
    }
    if (swr_ctx) {
        swr_free(&swr_ctx);
        swr_ctx = NULL;
    }
    if (img_convert_ctx) {
        sws_freeContext(img_convert_ctx);
        img_convert_ctx = NULL;
    }
    avformat_network_deinit();
    av_log(NULL, AV_LOG_FATAL, "播放器清理完成");
    av_log_set_callback(NULL);
    id = -1;
}

void EasyPlayer::wait_paused() {
    std::unique_lock<std::mutex> lock(mutex);
    pause_condition.wait(lock, [this] {
        if (!this->paused) {
            av_log(NULL, AV_LOG_FATAL, "暂停结束");
        } else {
            av_log(NULL, AV_LOG_FATAL, "暂停继续");
        }
        return !this->paused;
    });
}


bool EasyPlayer::stream_seek(int64_t pos) {
    if (!ic->pb->seekable) {
        return false;
    }
    if (!seek_req) {
        seek_pos = pos;
        seek_req = true;
        return true;
    }
    return false;
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
        notify_message(REOCRDER_ERROR, ERROR_MALLOC_CONTEXT, err, "分配output context出错");
        return;
    }
    err = avio_open2(&out->pb, out->url, AVIO_FLAG_READ_WRITE, NULL, 0);
    if (err < 0) {
        av_log(NULL, AV_LOG_FATAL, "打开存储文件出错%s:%d", savename, err);
        notify_message(REOCRDER_ERROR, ERROR_OPEN_FILE, err, "打开输出视频位置失败");
        avformat_free_context(out);
        return;
    }
    int i;
    if (has_video() && recorder_component_open(video_stream) < 0) {
        avio_close(out->pb);
        avformat_free_context(out);
        notify_message(REOCRDER_ERROR, ERROR_OPEN_VIDEO_ENCODER, err, "打开视频编码器出错");
        return;
    }
    if (has_audio() && recorder_component_open(audio_stream) < 0) {
        avio_close(out->pb);
        avformat_free_context(out);
        notify_message(REOCRDER_ERROR, ERROR_OPEN_AUDIO_ENCODER, err, "打开音频编码器出错");
        return;
    }
    av_dump_format(out, 0, out->url, 1);
    for (i = 0; i < out->nb_streams; i++) {
        if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            last_audio_stream = i;
        } else if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            last_video_stream = i;
        }
    }
    AVDictionary *params = 0;
    av_dict_set(&params, "movflags", "faststart", 0);
    avformat_write_header(out, &params);
    av_dict_free(&params);
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
        index = packet->stream_index =
                packet->stream_index == audio_stream ? last_audio_stream : last_video_stream;
        if (!find_key_frame) {
            if (index == last_video_stream && (packet->flags & AV_PKT_FLAG_KEY))
                find_key_frame = true;
            else
                continue;
        }
        packet->pts = av_rescale_q_rnd(packet->pts, ic->streams[index]->time_base,
                                       out->streams[index]->time_base,
                                       (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet->dts = av_rescale_q_rnd(packet->dts, ic->streams[index]->time_base,
                                       out->streams[index]->time_base,
                                       (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet->duration = av_rescale_q(packet->duration, ic->streams[index]->time_base,
                                        out->streams[index]->time_base);
        if (index == last_video_stream &&
            out->streams[index]->codecpar->codec_id == AV_CODEC_ID_H264) {
            av_bitstream_filter_filter(h264bsfc, ic->streams[index]->codec, NULL, &packet->data,
                                       &packet->size, packet->data, packet->size, 0);
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
    out = NULL;
    notify_message(RECODER_COMPLETED, 0, 0, "录制视频结束");
    stop_condition.notify_all();
}

int EasyPlayer::recorder_component_open(int index) {
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
        avformat_network_init();
    }
    std::thread read_thread(&EasyPlayer::read, this);
    read_thread.join();
}

void EasyPlayer::prepare_async(bool play_when_ready) {
    if (!ic || file_changed) {
        avformat_network_init();
    }
    on_state_change(PlayerState::INIT);
    this->play_when_ready = play_when_ready;
    std::thread read_thread(&EasyPlayer::read, this);
    read_thread.detach();
}


#pragma clang diagnostic pop