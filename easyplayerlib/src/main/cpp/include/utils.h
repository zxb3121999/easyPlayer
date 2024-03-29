//
// Created by Administrator on 2018/6/13 0013.
//

#ifndef EASYPLAYER_MASTER_UTILS_H
#define EASYPLAYER_MASTER_UTILS_H

extern "C"{
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
#include "libavutil/mathematics.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavcodec/avfft.h"
#include "libavutil/timestamp.h"
#include "libavfilter/buffersrc.h"
#include "libavfilter/buffersink.h"
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavutil/avutil.h"
};


#include <queue>
#include <mutex>
#include <condition_variable>
#include <stdio.h>
class PacketQueue {
public:
    ~PacketQueue();
    int put_packet(AVPacket *pkt);
    int get_packet(AVPacket *pkt);
    int remove_one();
    int put_nullpacket();
    void set_abort(int abort);
    int get_abort();
    int get_serial();
    void flush();
    int64_t get_duration(){
        return duration;
    }
    size_t get_queue_size();
private:
    std::queue<AVPacket*> queue;
    int64_t duration = 0;
    int abort_request = 1;
    int serial;
    std::mutex mutex;
    std::condition_variable cond;
    std::condition_variable full;
    const size_t MAX_SIZE = 1000;

};


class FrameQueue {
public:
    ~FrameQueue();
    void put_frame(AVFrame *frame);
    int put_null_frame();
    AVFrame* get_frame();
    size_t get_size();
    int64_t frame_queue_last_pos();
    void flush();
    void set_stop_state(bool stop){
        is_stop = stop;
        if(is_stop){
            empty.notify_all();
            full.notify_all();
        }
    }
private:
    bool is_stop = false;
    std::queue<AVFrame*> queue;
    std::mutex mutex;
    std::condition_variable empty;
    std::condition_variable full;
    const size_t MAX_SIZE = 100;
};

#endif //EASYPLAYER_MASTER_UTILS_H
