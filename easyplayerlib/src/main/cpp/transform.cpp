// 视频转换
// Created by Administrator on 2018/6/13 0013.
//

#include "easyPlayer.h"
/**
 * 设置文件名
 * @param input_file_name 输入视频url或者地址
 * @param save_file_name 输出视频url或者地址
 * @return
 */
int _thread_id = 0;
EasyTransform::~EasyTransform() {
    if(input_file_name){
        delete(input_file_name);
        input_file_name = nullptr;
    }
    if(save_file_name){
        delete(save_file_name);
        save_file_name = nullptr;
    }
    if(file_type){
        delete(file_type);
        file_type = nullptr;
    }
    if(filter_desc){
        delete(filter_desc);
        filter_desc = nullptr;
    }
    if(out!=NULL){
        avformat_free_context(out);
        out = NULL;
    }
    if(in!=NULL){
        avformat_close_input(&in);
        in = NULL;
    }
    if(videnc){
        videnc->avctx = NULL;
        delete(videnc);
        videnc = nullptr;
    }
    if(audenc){
        audenc->avctx = NULL;
        delete(audenc);
        audenc = nullptr;
    }
    if(viddec){
        delete(viddec);
        viddec = nullptr;
    }
    if(auddec){
        delete(auddec);
        auddec = nullptr;
    }
    av_log(NULL,AV_LOG_FATAL,"释放资源完成");
}
void EasyTransform::set_file_name( const char *input_file,const char *save_file) {
    if(input_file_name){
        delete(input_file_name);
    }
    this->input_file_name = av_strdup(input_file);
    if(save_file_name){
        delete(save_file_name);
    }
    this->save_file_name = av_strdup(save_file);
}
/**
 * 初始化ffmpeg
 */
void EasyTransform::init() {
    avformat_network_init();
    id = _thread_id++;
    std::thread thread(&EasyTransform::init_context,this);
    thread.detach();
}
/**
 * 读取视频数据
 */
void EasyTransform::read() {
    int ret;
    AVPacket *pkt = (AVPacket *) av_malloc(sizeof(AVPacket));
    int64_t stream_start_time;
    int64_t pkt_ts;
    int pkt_in_play_range = 0;
    if(start_time>0){
       av_seek_frame(in,-1,start_time,AVSEEK_FLAG_BACKWARD);
    }
    while (true) {
        if (pause)
            av_read_pause(in);//暂停
        else
            av_read_play(in);//播放
        ret = av_read_frame(in, pkt);//获取AVPacket
        if (ret == EOF || ret == AVERROR_EOF) {//播放完成
            goto complete;
        } else {
            /* check if packet is in play range specified by user, then queue, otherwise discard */
            stream_start_time = in->streams[pkt->stream_index]->start_time;
            pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;
            pkt_in_play_range = duration == AV_NOPTS_VALUE ||
                                (pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) * av_q2d(in->streams[pkt->stream_index]->time_base)                                -(double) (start_time != AV_NOPTS_VALUE ? start_time : 0) / 1000000 <= ((double) duration / 1000000);
            if(!pkt_in_play_range){
                av_packet_unref(pkt);
                goto complete;
            }if (pkt->stream_index == audio_index) {
                if (auddec->pkt_queue->put_packet(pkt) < 0) goto complete;
            } else if (pkt->stream_index == video_index && !(in->streams[video_index]->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
                if (viddec->pkt_queue->put_packet(pkt) < 0)goto complete;
            } else {
                av_packet_unref(pkt);
            }
        }
    }
    complete:
        if (viddec) {
            viddec->pkt_queue->put_nullpacket();
        }
        if (audenc) {
            auddec->pkt_queue->put_nullpacket();
        }
        av_packet_free(&pkt);
}

void EasyTransform::read_audio() {
    double frame_ts = 0;
    while(true){
        AVFrame *frame = auddec->frame_queue->get_frame();
        if (frame == nullptr) {//文件结束
            audenc->frame_queue->put_null_frame();
            return;
        }
        if(start_time!=AV_NOPTS_VALUE&&start_time>0&&frame_ts<start_time){
            frame_ts = frame->pkt_pts * av_q2d(in->streams[audio_index]->time_base);
            if(frame_ts<((double) start_time / 1000000)){
                av_frame_free(&frame);
                continue;
            }
        }
        audenc->frame_queue->put_frame(frame);
    }
}
void EasyTransform::read_video() {
    double frame_ts = 0;
    if(height>0||width>0){

    }
    while(true){
        AVFrame *frame = viddec->frame_queue->get_frame();
        if (frame == nullptr) {//文件结束
            videnc->frame_queue->put_null_frame();
            break;
        }
        if(start_time!=AV_NOPTS_VALUE&&start_time>0&&frame_ts<start_time){
            frame_ts = av_frame_get_best_effort_timestamp(frame) * av_q2d(in->streams[video_index]->time_base);
            if(frame_ts<((double) start_time / 1000000)){
                av_frame_free(&frame);
                continue;
            }
        }
        if(width>0||height>0){
            AVFrame *temp = av_frame_alloc();
            temp->width = videnc->avctx->width;
            temp->height = videnc->avctx->height;
            temp->format = videnc->avctx->pix_fmt;
            int numBytes = av_image_get_buffer_size(videnc->avctx->pix_fmt, videnc->avctx->width,videnc->avctx->height, 1);
            uint8_t *vOutBuffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
            av_image_fill_arrays(temp->data, temp->linesize, vOutBuffer, videnc->avctx->pix_fmt, videnc->avctx->width, videnc->avctx->height, 1);
            sws_scale(img_convert_ctx, (const uint8_t *const *) frame->data, frame->linesize, 0,viddec->avctx->height,
                      temp->data, temp->linesize);
            videnc->frame_queue->put_frame(temp);
            av_frame_free(&frame);
            av_free(vOutBuffer);
        } else{
            videnc->frame_queue->put_frame(frame);
        }
    }
}
/**
 * 写入视频数据
 */
void EasyTransform::write() {
    if (video_index>=0&&out->streams[video_out_index]->time_base.den) {
        videnc->t = out->streams[video_out_index]->time_base;
    }
    AVPacket *packet = av_packet_alloc();
    int cur_pts_v = 0;
    int cur_pts_a = 0;
    int frame_video_index = 0;
    int frame_audio_index = 0;
    AVStream *st;
    AVCodecContext *actx;
    int64_t video_calc_duration = (double) AV_TIME_BASE / av_q2d(in->streams[video_index]->r_frame_rate);
    int64_t video_duration = (double) video_calc_duration / (double) (av_q2d(in->streams[video_out_index]->time_base) * AV_TIME_BASE);
    AVBitStreamFilterContext *h264bsfc = NULL;
    AVBitStreamFilterContext *aacbsfc = NULL;
    if(strcmp("mp4",out->oformat->name)==0){
        if(out->streams[video_out_index]->codecpar->codec_id == AV_CODEC_ID_H264){
             h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
        }
        if(out->streams[audio_out_index]->codecpar->codec_id == AV_CODEC_ID_AAC){
             aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
        }
    }
    bool is_video_complete = false;
    bool is_audio_complete = false;
    int num = duration == AV_NOPTS_VALUE?in->duration:duration;
    while (!(is_audio_complete&&is_video_complete)) {
        av_packet_unref(packet);
        if (av_compare_ts(cur_pts_v, out->streams[video_out_index]->time_base, cur_pts_a, out->streams[video_out_index]->time_base) <= 0) {
            //写视频数据
            if(is_video_complete){
                cur_pts_v = cur_pts_a*1000;
                continue;
            }
            st = in->streams[video_index];
            actx = videnc->avctx;
            videnc->pkt_queue->get_packet(packet);
            if (packet->size == 0) {
                is_video_complete = true;
                av_log(NULL,AV_LOG_FATAL,"video recorder completed");
                continue;
            }
            packet->stream_index = video_out_index;
            packet->pts=(double)(frame_video_index*video_calc_duration)/(av_q2d(st->time_base)*AV_TIME_BASE);
            packet->dts=packet->pts;
            packet->duration=video_duration;
            cur_pts_v = packet->pts;
            if(h264bsfc)
                av_bitstream_filter_filter(h264bsfc,actx, NULL, &packet->data, &packet->size, packet->data, packet->size, 0);
            notify_message(STATE_PROGRESS,frame_video_index*video_calc_duration*100/num,0,"更新进度条");
            frame_video_index++;
        } else {
            //写音频数据
            if(is_audio_complete){
                cur_pts_a = cur_pts_v*1000;
                continue;
            }
            st = out->streams[audio_out_index];
            actx = audenc->avctx;
            audenc->pkt_queue->get_packet(packet);
            if (packet->size == 0) {
                is_audio_complete = true;
                av_log(NULL,AV_LOG_FATAL,"audio recorder completed");
                continue;
            }
            packet->stream_index = audio_out_index;
            av_packet_rescale_ts(packet, actx->time_base, st->time_base);
            frame_audio_index++;
            cur_pts_a = packet->pts;
            if(aacbsfc)
                av_bitstream_filter_filter(aacbsfc,actx, NULL, &packet->data, &packet->size, packet->data, packet->size, 0);
        }
        av_interleaved_write_frame(out, packet);
    }
    if(h264bsfc)
        av_bitstream_filter_close(h264bsfc);
    if(aacbsfc)
        av_bitstream_filter_close(aacbsfc);
    av_write_trailer(out);
    av_packet_unref(packet);
    av_packet_free(&packet);
    av_log(NULL,AV_LOG_FATAL,"录制完成");
    if (out && !(out->oformat->flags & AVFMT_NOFILE))
        avio_close(out->pb);
    notify_message(STATE_COMPLETED,0,0,"转换完成");
}
/**
 * 打开音视频解码器
 * @param stream_index
 * @return
 */
int EasyTransform::stream_component_open(int stream_index) {
    AVCodecContext *avctx = NULL;
    AVCodec *codec = NULL;
    int ret = 0;
    if (stream_index < 0 || stream_index >= in->nb_streams)
        return -1;
    avctx = avcodec_alloc_context3(NULL);
    if (!avctx){
        notify_message(STATE_ERROR,ERROR_MALLOC_CONTEXT,-1,"分配解码器context失败");
        return AVERROR(ENOMEM);
    }

    ret = avcodec_parameters_to_context(avctx, in->streams[stream_index]->codecpar);
    if (ret < 0) {
        avcodec_free_context(&avctx);
        notify_message(STATE_ERROR,ERROR_MALLOC_CONTEXT,ret,"解码器context设置参数失败");
        return AVERROR(ENOMEM);
    }
    av_codec_set_pkt_timebase(avctx, in->streams[stream_index]->time_base);
    codec = avcodec_find_decoder(avctx->codec_id);
    if (!codec){
        notify_message(STATE_ERROR,avctx->codec_type==AVMEDIA_TYPE_AUDIO?ERROR_FIND_AUDIO_DECODER:ERROR_FIND_VIDEO_DECODER,ret,"未找到音视频解码器");
        return AVERROR(ENOMEM);;
    }
    ret = avcodec_open2(avctx, codec, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_FATAL, "Fail to open codec on stream %d\n", stream_index);
        notify_message(STATE_ERROR,avctx->codec_type==AVMEDIA_TYPE_AUDIO?ERROR_OPEN_AUDIO_DECODER:ERROR_OPEN_VIDEO_DECODER,ret,"打开音视频解码器失败");
        return ret;
    }
    //根据音视频类型，获取SwrContext，用于转换数据
    switch (avctx->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            audio_index = stream_index;
            if(auddec)
                delete(auddec);
            auddec = new AudioDecoder;
            auddec->init(avctx);
            auddec->start_decode_thread();
            break;
        case AVMEDIA_TYPE_VIDEO:
            video_index = stream_index;
            img_convert_ctx = sws_getContext(avctx->width, avctx->height, avctx->pix_fmt,
                                             width>0?width:avctx->width, height>0?height:avctx->height, avctx->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
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
/**
 * 打开音视频编码器
 * @param stream_index
 * @return
 */
int EasyTransform::stream_out_component_open(int stream_index) {
    AVStream *stream = NULL;
    AVCodec *codec = NULL;
    AVDictionary *param = 0;
    int ret = 0;
    codec = avcodec_find_encoder(in->streams[stream_index]->codecpar->codec_id);
    if (!codec){
        av_log(NULL,AV_LOG_ERROR,"查找编码器失败");
        notify_message(STATE_ERROR,in->streams[stream_index]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO?ERROR_FIND_AUDIO_ENCODER:ERROR_FIND_VIDEO_ENCODER,ret,"未找到音视频解码器");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    stream = avformat_new_stream(out,codec);
    if(!stream){
        notify_message(STATE_ERROR,AVERROR_STREAM_NOT_FOUND,-1,"建立输出音视频流失败");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    ret = avcodec_copy_context(stream->codec, in->streams[stream_index]->codec);
    if (ret < 0) {
        notify_message(STATE_ERROR,ERROR_MALLOC_CONTEXT,ret,"编码器context设置参数失败");
        ret = AVERROR(ENOMEM);
        goto fail;
    }
    stream->time_base = in->streams[stream_index]->time_base;
    if (out->oformat->flags & AVFMT_GLOBALHEADER)
        stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    switch(stream->codec->codec_type){
        case AVMEDIA_TYPE_AUDIO:
            if((ret = avcodec_open2(stream->codec,codec,&param))<0){
                av_log(NULL,AV_LOG_ERROR,"打开音频编码器失败:%d",ret);
                notify_message(STATE_ERROR,ERROR_OPEN_AUDIO_ENCODER,ret,"打开音频编码器失败");
                goto fail;
            }
            if(audenc)
                delete(audenc);
            audenc = new AudioEncoder;
            audenc->set_use_frame_queue(true);
            audenc->init(stream->codec);
            audenc->start_encode_thread();
            break;
        case AVMEDIA_TYPE_VIDEO:
            if(width>0){
                stream->codec->width = width;
            }
            if(height>0){
                stream->codec->height = height;
            }
            if( stream->codec->codec_id==AV_CODEC_ID_H264){
                stream->codec->me_pre_cmp = 1;
                stream->codec->me_range = 16;
                stream->codec->max_qdiff = 4;
                stream->codec->qmin = 10;
                stream->codec->qmax = 51;
                stream->codec->qcompress = 0.6;
                av_opt_set( stream->codec->priv_data, "tune", "zerolatency", 0);
                av_opt_set( stream->codec->priv_data, "preset", "ultrafast", 0);
                av_opt_set_int(stream->codec->priv_data,"qp",18,0);
                av_dict_set_int(&param, "qp", 18,0);
            }
            if((ret = avcodec_open2(stream->codec,codec,&param))<0){
                av_log(NULL,AV_LOG_ERROR,"打开视频编码器失败:%d",ret);
                notify_message(STATE_ERROR, stream->codec->codec_type==AVMEDIA_TYPE_VIDEO?ERROR_OPEN_VIDEO_ENCODER:ERROR_OPEN_AUDIO_ENCODER,ret,"打开视频编码器失败");
                goto fail;
            }
            if(videnc)
                delete(videnc);
            videnc = new VideoEncoder;
            videnc->set_use_frame_queue(true);
            videnc->init(stream->codec);
            if(filter_desc){
                videnc->init_filter(filter_desc,stream->time_base.num>0?stream->time_base: stream->codec->time_base);
            }
            videnc->start_encode_thread();
            break;
    }
    avcodec_parameters_from_context(stream->codecpar, stream->codec);
    av_dict_free(&param);
    return 0;
    fail:
    av_dict_free(&param);
    avio_close(out->pb);
    avformat_free_context(out);
    return ret;
}
/**
 * 初始化context
 */
void EasyTransform::init_context() {
    if (in!=NULL) {
        avformat_free_context(in);
    }
    int ret = 0;
    in = avformat_alloc_context();
    if(!in){
        av_log(NULL, AV_LOG_ERROR, "Could not allocate context.\n");
        notify_message(STATE_ERROR,ERROR_MALLOC_CONTEXT,-1,"分配input context出错");
        return;
    }
    AVInputFormat *iformat = NULL;
    if(file_type){
        iformat = av_find_input_format(file_type);
    }
    if((ret = avformat_open_input(&in,input_file_name,iformat,NULL))<0){
        av_log(NULL, AV_LOG_ERROR, "Could not open input file with error code:%d\n",ret);
        notify_message(STATE_ERROR,ERROR_OPEN_FILE,ret,"打开输入视频文件失败");
        return;
    }
    if((ret = avformat_find_stream_info(in,NULL))<0){
        av_log(NULL, AV_LOG_ERROR, "查找音视频流信息失败,错误码:%d\n",ret);
        notify_message(STATE_ERROR,ERROR_FIND_STREAM_INFO,ret,"查找音视频流信息失败");
        return;
    }
    av_dump_format(in, 0, in->filename, 0);
    if((ret = avformat_alloc_output_context2(&out, NULL,NULL,save_file_name))<0){
        av_log(NULL,AV_LOG_ERROR,"分配output context出错:%d",ret);
        notify_message(STATE_ERROR,ERROR_MALLOC_CONTEXT,ret,"分配output context出错");
        return;
    }
    if((ret = avio_open2(&out->pb,save_file_name,AVIO_FLAG_READ_WRITE,NULL,NULL))<0){
        av_log(NULL,AV_LOG_ERROR,"打开输出视频位置失败:%d",ret);
        notify_message(STATE_ERROR,ERROR_OPEN_FILE,ret,"打开输出视频位置失败");
        return;
    }
    for (int i = 0; i < in->nb_streams; i++) {
        if (in->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO||in->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {//音视频流
            if( stream_component_open(i)<0||stream_out_component_open(i)<0){
                return;
            }
        }
    }
    for (int i = 0; i < out->nb_streams; i++) {
        if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_out_index = i;
        } else if (out->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_out_index = i;
        }
    }
    av_dump_format(out, 0, out->filename, 1);
    if( (ret = avformat_write_header(out, NULL))< 0){
        notify_message(STATE_ERROR,ERROR_WRITE_HEADER,ret,"写入头文件出错");
        avio_close(out->pb);
        avformat_free_context(out);
        out = NULL;
        return;
    }
    std::thread read(&EasyTransform::read, this);
    read.detach();
    std::thread write(&EasyTransform::write, this);
    write.detach();
    std::thread read_audio(&EasyTransform::read_audio,this);
    read_audio.detach();
    std::thread read_video(&EasyTransform::read_video,this);
    read_video.detach();
}
void EasyTransform::start() {
    std::thread thread(&EasyTransform::init_context, this);
    thread.detach();
}