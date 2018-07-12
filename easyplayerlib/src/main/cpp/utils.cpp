//
// Created by Administrator on 2018/6/13 0013.
//
#include "utils.h"

//将AVPacket压入PacketQueue
int PacketQueue::put_packet(AVPacket *pkt) {
    if (abort_request) {
        av_log(NULL, AV_LOG_INFO, "put_packet abort.\n");
        return -1;
    }
    if (!pkt)
        return 0;
    int ret;
    std::unique_lock<std::mutex> lock(mutex);
    while (true) {
        if (queue.size() < MAX_SIZE) {
            AVPacket *packet = av_packet_alloc();
            if (packet == NULL) {
                av_log(NULL, AV_LOG_FATAL, "Could not create new AVPacket.\n");
                return -1;
            }
            if(pkt->size!=0){
                ret = av_packet_ref(packet, pkt);
                av_packet_unref(pkt);
                if (ret != 0) {
                    av_packet_free(&packet);
                    av_log(NULL, AV_LOG_FATAL, "Could not copy AVPacket.\n");
                    return -1;
                }
            } else{
                packet->data = NULL;
                packet->size = 0;
            }
            queue.push(packet);
            duration += packet->duration;
            cond.notify_one();
            break;
        } else {
            full.wait(lock);
        }
    }
    return 0;
}

//从PacketQueue中获取AVPacket
int PacketQueue::get_packet(AVPacket *pkt) {
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
        if (abort_request) {
            return -1;
        }
        if (queue.size() > 0) {
            AVPacket *tmp = queue.front();
            av_packet_ref(pkt, tmp);
            duration -= tmp->duration;
            queue.pop();
            av_packet_free(&tmp);
            full.notify_one();
            return 0;
        } else {
            cond.wait(lock);
        }
    }
}

int PacketQueue::remove_one() {
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
        if (abort_request) {
            return -1;
        }
        if (queue.size() > 0) {
            AVPacket *tmp = queue.front();
            queue.pop();
            av_packet_free(&tmp);
            full.notify_all();
            return 0;
        } else {
            return -1;
        }
    }
}

//清空PacketQueue
void PacketQueue::flush() {
    std::unique_lock<std::mutex> lock(mutex);
    while (queue.size() > 0) {
        AVPacket *tmp = queue.front();
        queue.pop();
        av_packet_free(&tmp);
    }
    duration = 0;
    full.notify_one();
}

//压入一个空的PacketQueue
int PacketQueue::put_nullpacket() {
    AVPacket *pkt = new AVPacket();
    av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    put_packet(pkt);
    av_packet_free(&pkt);
    return 0;
}

//暂停
void PacketQueue::set_abort(int abort) {
    abort_request = abort;
    if (abort_request) {
        cond.notify_all();
        full.notify_all();
    }
}

//获取是不处于暂停
int PacketQueue::get_abort() {
    return abort_request;
}

int PacketQueue::get_serial() {
    return serial;
}

//获取PacketQueue长度
size_t PacketQueue::get_queue_size() {
    return queue.size();
}

//从FrameQueue中获取AVFrame数据
std::shared_ptr<Frame> FrameQueue::get_frame() {
    std::unique_lock<std::mutex> lock(mutex);
    for (;;) {
        if (queue.size() > 0) {
            auto tmp = queue.front();
            queue.pop();
            full.notify_one();
            return tmp;
        } else {
            empty.wait(lock);
        }
    }
}

//将AVFrame压入FrameQueue
void FrameQueue::put_frame(AVFrame *frame) {
    std::unique_lock<std::mutex> lock(mutex);

    while (true) {
        if (queue.size() < MAX_SIZE) {
            if(frame == nullptr){
                queue.push(nullptr);
                empty.notify_one();
                return;
            }
            AVFrame *avFrame = av_frame_alloc();
            if (av_frame_ref(avFrame, frame) == 0) {
                auto m_frame = std::make_shared<Frame>(avFrame);
                queue.push(m_frame);
                empty.notify_one();
            } else {
                av_frame_free(&avFrame);
                av_log(NULL, AV_LOG_FATAL, "copy frame failed");
            }
            return;
        } else {
            full.wait(lock);;
        }
    }

}


//获取最后一个的位置
int64_t FrameQueue::frame_queue_last_pos() {
    auto frame = queue.back();
    return frame->frame->pkt_pos;
}


//获取FrameQueue长度
size_t FrameQueue::get_size() {
    return queue.size();
}

int FrameQueue::put_null_frame() {
    put_frame(nullptr);
    return 0;
}

void FrameQueue::flush() {
    std::unique_lock<std::mutex> lock(mutex);
    while (queue.size() > 0) {
        auto tmp = queue.front();
        queue.pop();
        if(tmp!= nullptr)
            av_frame_free(&tmp->frame);
    }
    full.notify_one();
}
