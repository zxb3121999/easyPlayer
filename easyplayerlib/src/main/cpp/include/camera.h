//
// Created by Administrator on 2018/12/19 0019.
//

#ifndef EASYPLAYER_MASTER_CAMERA_H
#define EASYPLAYER_MASTER_CAMERA_H
extern "C"{
#include <libavdevice/avdevice.h>
};
#include <string>
#include "coder.h"
class CameraDemo{
public:
    void recorder(const std::string path);
    int video_index = -1;
    int recorder_video_index = -1;
    int stream_component_open(int index);
    int recorder_component_open(int index);
    AVFormatContext *ic = NULL;
    AVFormatContext *out = NULL;
    void read();
    void write();
};
#endif //EASYPLAYER_MASTER_CAMERA_H
