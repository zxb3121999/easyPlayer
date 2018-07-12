//
// Created by Administrator on 2018/6/13 0013.
//

#ifndef EASYPLAYER_MASTER_CODER_H
#define EASYPLAYER_MASTER_CODER_H
#include "utils.h"
#include "threadsafe_queue.cpp"
class Encoder{
public:
    virtual int encoder_encode_frame() = 0;
    virtual void encode() = 0;
    virtual int init_swr() = 0;
    void loop();
    int flush_encoder();
    int init(AVCodecContext *ctx);
    void set_use_frame_queue(bool b){
        use_frame_queue = b;
    }
    void start_encode_thread();
    int buf_size = 0;
    PacketQueue *pkt_queue = NULL;
    threadsafe_queue<uint8_t *> data_queue;
    FrameQueue *frame_queue = NULL;
    bool is_finish = false;
    AVCodecContext *avctx = NULL;
    AVRational t;
    ~Encoder();
    int64_t start_time;
protected:
    bool use_frame_queue= false;
    AVFrame *frame = NULL;
    int frame_count = 0;
};
class VideoEncoder:public Encoder{
public:
    ~VideoEncoder();
    virtual int encoder_encode_frame() override ;
    virtual void encode() override ;
    virtual int init_swr() override ;
    void filter(uint8_t *picture_buf);
    int init_filter(char *filter_desc,AVRational time_base);
private:
private:
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterGraph *filter_graph = NULL;
    AVFrame *frame_out = NULL;
};
class AudioEncoder:public Encoder{
public:
    ~AudioEncoder();
    int init_swr() override ;
    virtual int encoder_encode_frame() override ;
    virtual void encode() override ;

private:
    struct SwrContext *swr;
};
class Decoder {
public:
    virtual int decoder_decode_frame() = 0;
    virtual void decode() = 0;
    void init(AVCodecContext *ctx);
    void start_decode_thread();
    PacketQueue *pkt_queue = NULL;
    FrameQueue *frame_queue = NULL;
    AVCodecContext *avctx = NULL;
    ~Decoder();
protected:
    AVPacket *pkt = nullptr;
    int pkt_serial;
    int finished;
    int packet_pending;
    std::condition_variable empty_queue_cond;
    int64_t start_pts;
    AVRational start_pts_tb;
    int64_t next_pts;
    AVRational next_pts_tb;
};


class VideoDecoder : public Decoder {
public:
    virtual int decoder_decode_frame() override ;
    virtual void decode() override ;
    int get_width();
    int get_height();
};

class AudioDecoder : public Decoder {
public:
    virtual int decoder_decode_frame() override ;
    virtual void decode() override ;
    int get_channels();
    int get_sample_rate();
private:

};
#endif //EASYPLAYER_MASTER_CODER_H
