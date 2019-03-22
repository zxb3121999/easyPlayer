//
// Created by Administrator on 2018/6/13 0013.
//

#ifndef EASYPLAYER_MASTER_CODER_H
#define EASYPLAYER_MASTER_CODER_H
#include "utils.h"
#include "threadsafe_queue.cpp"
#ifndef RESULT_FAIL
#define RESULT_FAIL -1
#endif
#ifndef RESULT_OK
#define RESULT_OK 0
#endif
class Coder{
public:
    ~Coder();
    int init_filter(char *filter_desc,AVRational time_base);
    int filter_frame(AVFrame *frame);
    AVCodecContext *avctx = NULL;
    PacketQueue *pkt_queue = NULL;
    FrameQueue *frame_queue = NULL;
    AVFrame *frame_out = NULL;
    virtual char* get_tag(){
        return "Coder";
    }
protected:
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterGraph *filter_graph = NULL;

};
class Encoder:public Coder{
public:
    virtual int encoder_encode_frame(AVPacket *pkt) = 0;
    virtual void encode() = 0;
    virtual int init_swr() = 0;
    void loop();
    void flush_queue();
    int flush_encoder();
    int init(AVCodecContext *ctx);
    void set_use_frame_queue(bool b){
        use_frame_queue = b;
    }
    void start_encode_thread();
    int buf_size = 0;
    AVRational t;
    ~Encoder();
    int64_t start_time;
    void set_get_data_fun(std::function<uint8_t *()> cb){
        get_data_fun = cb;
    }
    void set_buf_size_listener(std::function<int()> cb){
        get_buf_size = cb;
    }
protected:
    uint8_t* get_data(){
        if(get_data_fun){
            return get_data_fun();
        }
        return nullptr;
    }
    bool use_frame_queue= false;
    AVFrame *frame = NULL;
    int frame_count = 0;
    std::function<int()> get_buf_size;
    std::function<uint8_t *()> get_data_fun;
};
class VideoEncoder:public Encoder{
public:
    ~VideoEncoder();
    virtual int encoder_encode_frame(AVPacket *pkt) override ;
    virtual void encode() override ;
    virtual int init_swr() override ;
    void filter(uint8_t *picture_buf);
    char *get_tag(){
        return "视频编码器";
    }
protected:
};
class AudioEncoder:public Encoder{
public:
    ~AudioEncoder();
    virtual int init_swr() override ;
    virtual int encoder_encode_frame(AVPacket *pkt) override ;
    virtual void encode() override ;
    char *get_tag(){
        return "音频编码器";
    }
private:
    struct SwrContext *swr = NULL;
};
class Decoder:public Coder{
public:
    int decoder_decode_frame();
    void decode();
    void init(AVCodecContext *ctx);
    void start_decode_thread();
    void flush();
    void wait_thread_stop(){
        while(is_thread_running){}
    }
    void flush_codec();
    int decode_frame();
    ~Decoder();
protected:
    bool is_thread_running = false;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
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
    int get_width();
    int get_height();
    void set_rotate(double d){
        rotate = d;
    }
    char *get_tag(){
        return "视频解码器";
    }
private:
    double rotate = 0;
};

class AudioDecoder : public Decoder {
public:
    int get_channels();
    int get_sample_rate();
    char *get_tag(){
        return "音频编码器";
    }
private:

};
#endif //EASYPLAYER_MASTER_CODER_H
