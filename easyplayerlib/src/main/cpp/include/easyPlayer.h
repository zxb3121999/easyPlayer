//
// Created by Administrator on 2016/11/10.
//

#ifndef EASYPLAYER_EASYPLAYER_H
#define EASYPLAYER_EASYPLAYER_H


#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include "coder.h"
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <jni.h>
#include <pthread.h>
#include <assert.h>

#define  ERROR_OK 0
#define  ERROR_MALLOC_CONTEXT -3001
#define  ERROR_WRITE_HEADER -3002
#define  ERROR_OPEN_FILE -4001
#define  ERROR_OPEN_VIDEO_ENCODER -1001
#define  ERROR_OPEN_AUDIO_ENCODER -1002
#define  ERROR_FIND_VIDEO_ENCODER -1003
#define  ERROR_FIND_AUDIO_ENCODER -1004
#define  ERROR_FIND_STREAM_INFO -1005
#define  ERROR_FIND_VIDEO_DECODER -1006
#define  ERROR_FIND_AUDIO_DECODER -1007
#define  ERROR_OPEN_VIDEO_DECODER -1008
#define  ERROR_OPEN_AUDIO_DECODER -1009
#define ERROR_INIT_SWR_CONTEXT -2001




enum class PlayerState {
    UNKNOWN,
    STOP,
    INIT,
    READY,
    BUFFERING,
    PLAYING,
    BUFFERING_COMPLETED,
    COMPLETED,
};

enum class RecorderState {
    UNKNOWN = -2,
    ERROR,
    READY,
    RECORDING,
    PAUSE,
    COMPLETED,
};
enum class TransformState{
    ERROR =-1,
    READY,
    TRANSFORMING,
    PAUSE,
    COMPLETED
};
class EasyPlayer {
public:
    EasyPlayer() = default;
    ~EasyPlayer();
    void set_data_source(const std::string input_filename);
    void prepare();
    void prepare_async();
    bool has_video();
    bool has_audio();
    bool get_img_frame(AVFrame *frame);
    bool get_aud_buffer(int &nextSize, uint8_t *outputBuffer);
    void wait_state(PlayerState need_state);
    void wait_paused();
    void stop();
    void release();
    bool is_playing() {
        return state == PlayerState::PLAYING && !paused;
    }
    void play() {
        if (state == PlayerState::READY||state == PlayerState ::BUFFERING||state == PlayerState::COMPLETED) {
            std::unique_lock<std::mutex> lock(mutex);
            state = PlayerState::PLAYING;
            paused = false;
            pause_condition.notify_all();
        }
        if(audio_complete&&has_audio()){
            std::thread play_audio(&EasyPlayer::play_audio,this);
            play_audio.detach();
        }
        if(video_complete&&has_video()){
            std::thread play_video(&EasyPlayer::play_video,this);
            play_video.detach();
        }
        play_when_ready = true;
    }
    void pause() {
        av_log(NULL, AV_LOG_VERBOSE, "pause called,current state %d\n", state);
        if (state == PlayerState::PLAYING) {
            std::unique_lock<std::mutex> lock(mutex);
            paused = true;
            pause_condition.notify_all();
            state = PlayerState::READY;
        }

    }
    bool get_paused() {
        return paused;
    }
    void set_window(ANativeWindow *window){
        if(nativeWindow){
            ANativeWindow_release(nativeWindow);
        }
        nativeWindow = window;
        if(state == PlayerState::PLAYING&&has_video()){
            if (0 > ANativeWindow_setBuffersGeometry(nativeWindow, viddec->get_width(), viddec->get_height(), WINDOW_FORMAT_RGBA_8888)) {
                ANativeWindow_release(nativeWindow);
                return;
            }
        }
    }
    void set_play_state(bool is_audio, bool state){
        if(is_audio){
            audio_complete = state;
        } else{
            video_complete = state;
        }

        if(audio_complete&&video_complete){
            if(this->state != PlayerState::STOP){
                on_state_change(PlayerState::COMPLETED);
            } else{
                stop_condition.notify_all();
            }
        }
    }
    void set_audio_call_back( slAndroidSimpleBufferQueueCallback callback){
        this->callback = callback;
    }
    int64_t get_duration() {
        if (ic != nullptr) {
            return (ic->duration)/1000;
        }
    };
    long get_curr_position() {
        return audio_clock?(long)(audio_clock * 1000-ic->start_time/1000):audio_clock;
    };
    void stream_seek(int64_t pos);
    void set_event_listener(void (*cb)(EasyPlayer *player,int, int, int,char *)) {
        event_listener = cb;
    }
    void set_background_listener(bool (*cb)()){
        is_in_background_listener = cb;
    }
    void recorder_run();
    void start_recorder(const std::string save_filename);
    void stop_recorder(){
        if(recorder){
            std::unique_lock<std::mutex> lock(recorder_mutex);
            recorder = false;
            recorder_condition.notify_all();
        }
    }
    bool is_recordering(){
        return recorder;
    }
    AVFormatContext *ic = NULL;
    AVFormatContext *out = NULL;
    char *filename = nullptr;
    char *savename = nullptr;
    int abort_request = 0;

    bool seek_req = false;
    int seek_flags;
    int64_t seek_pos;
    AudioDecoder *auddec = nullptr;
    VideoDecoder *viddec = nullptr;
    PlayerState state = PlayerState::UNKNOWN;
private:
    void notify_message(int state,int error_code,int ffmpeg_code,char *err_msg){
        if(event_listener!= nullptr){
            event_listener(this,state,error_code,ffmpeg_code,err_msg);
        }
    }
    ANativeWindow *nativeWindow = NULL;
    void play_video();
    void play_audio();
    void read();
    void do_play();
    bool init_context();
    bool is_realtime();
    int stream_component_open(int stream_index);
    void on_state_change(PlayerState state);
    int last_video_stream, last_audio_stream;
    int video_stream = -1;
    bool paused = false;
    bool play_when_ready = false;
    bool file_changed = false;
    void (*event_listener)(EasyPlayer *,int, int, int,char *);
    bool (*is_in_background_listener)();
    struct SwsContext *img_convert_ctx = NULL;

    double audio_clock = 0;

    struct SwrContext *swr_ctx = NULL;

    int audio_stream = -1;

    bool audio_complete = true;
    bool video_complete = true;
    int64_t start_time = AV_NOPTS_VALUE;
    int64_t duration = AV_NOPTS_VALUE;
    std::mutex mutex;
    std::condition_variable state_condition;
    std::condition_variable pause_condition;
    std::mutex audio_mutex;
    std::condition_variable audio_condition;
    std::mutex stop_mutex;
    std::condition_variable stop_condition;
    //open sles start
    SLObjectItf engineObject = NULL;
    SLEngineItf engineEngine = NULL;

// output mix interfaces
    SLObjectItf outputMixObject = NULL;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;

// buffer queue player interfaces
    SLObjectItf bqPlayerObject = NULL;
    SLPlayItf bqPlayerPlay = NULL;
    SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue = NULL;
    SLEffectSendItf bqPlayerEffectSend = NULL;
    SLMuteSoloItf bqPlayerMuteSolo = NULL;
    SLVolumeItf bqPlayerVolume = NULL;
    SLmilliHertz bqPlayerSampleRate = 0;
    short *resampleBuf = NULL;
// pointer and size of the next player buffer to enqueue, and number of remaining buffers
    uint8_t *outputBuffer = NULL;
    int nextSize =0;

// aux effect on the output mix, used by the buffer queue player
    const SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;
    const int outputBufferSize = 8196;
    void createAudioEngine();
    void createBufferQueueAudioPlayer(int sampleRate, int channel);
    friend void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
    slAndroidSimpleBufferQueueCallback callback;
    void releaseResampleBuf(void);
    void release_sles();
    void wait_for_audio_completed(){
        std::unique_lock<std::mutex> lock(audio_mutex);
        audio_condition.wait(lock);
    }
    //open sles end

    bool recorder = false;
    PacketQueue recorder_queue;
    std::mutex recorder_mutex;
    std::condition_variable recorder_condition;
    int recorder_compenent_open(int index);

    static const int MEDIA_NOP = 0; // interface test message
    static const int MEDIA_PREPARED = 1;
    static const int MEDIA_PLAYBACK_COMPLETE = 2;
    static const int MEDIA_BUFFERING_UPDATE = 3;
    static const int MEDIA_SEEK_COMPLETE = 4;
    static const int MEDIA_SET_VIDEO_SIZE = 5;
    static const int MEDIA_STOP = 6;
    static const int MEDIA_TIMED_TEXT = 99;
    static const int MEDIA_ERROR = -1;
    static const int REOCRDER_ERROR = -1000;
    static const int RECODER_COMPLETED = 1002;
    static const int MEDIA_INFO = 200;

};

class EasyRecorder{
public:
    EasyRecorder() = default;
    ~EasyRecorder();
    void prepare();
    void set_file_save_path(const std::string output_file_name);
    void recorder_img(uint8_t *data,int len);
    void on_state_change(RecorderState state);
    void wait_state(RecorderState state);
    void recorder_audio(uint8_t *data,int len);
    RecorderState get_recorder_state(){
        return state;
    }
    void set_event_listener(void (*cb)(int,int,char *)) {
        event_listener = cb;
    }
    void stop_recorder(){
        if(videnc){
            videnc->is_finish = true;
            videnc->flush_queue();
        }
        if(audenc){
            audenc->is_finish = true;
            audenc->flush_queue();
        }
    }
    void set_width(int w){
        width = w;
    }
    void set_height(int h){
        height = h;
    }
    void set_frame_rate(int fps){
        frame_rate = fps;
    }
    void set_bit_rate(int _bit_rate){
        bit_rate = _bit_rate;
    }
    AVFormatContext *out = NULL;
    AudioEncoder *audenc = nullptr;
    VideoEncoder *videnc = nullptr;
    int get_audio_buf_size(){
        return this->audio_buf_size;
    }
    std::shared_ptr<uint8_t *> get_video_data(){
        return this->video_data_queue.wait_and_pop();
    }
    std::shared_ptr<uint8_t *> get_audio_data(){
        return this->audio_data_queue.wait_and_pop();
    }
private:
    void notify_message(int state,int error_code,char *errMsg){
        if(event_listener){
            event_listener(state,error_code,errMsg);
        }
    }
    RecorderState state = RecorderState ::UNKNOWN;
    void release();
    void write();
    void do_write();
    char *file_name = nullptr;
    int width = 0;
    int height = 0;
    std::mutex mutex;
    std::condition_variable state_condition;
    int frame_rate = 30;
    int bit_rate = 40000;
    int img_frame_count =0;
    int audio_frame_count = 0;
    bool init_context();
    int video_stream = -1;
    int audio_stream = -1;
    int audio_buf_size = 0;
    threadsafe_queue video_data_queue,audio_data_queue;
    int stream_component_open(int stream_index);
    void (*event_listener)(int,int,char *);
    static const int RECORDER_STATE_READY = 1;
    static const int RECORDER_STATE_RECORDING = 2;
    static const int RECORDER_STATE_PAUSE = 3;
    static const int RECORDER_STATE_COMPLETED = 4;
    static const int RECORDER_STATE_ERROR = -1;
};

class EasyTransform{
private:
    bool pause = false;
    int id = 0;
    char *filter_desc= nullptr;
    int video_index =-1;
    int video_out_index = -1;
    int audio_out_index = -1;
    int audio_index = -1;
    int width = -1;
    int height= -1;
    int start_time = AV_NOPTS_VALUE;
    int end_time = -1;
    int64_t duration = AV_NOPTS_VALUE;
    char* input_file_name = nullptr;
    char* save_file_name = nullptr;
    void (*event_listener)(EasyTransform *,int, int,int,char *);
    char* file_type = NULL;
    AVFormatContext *out=NULL,*in=NULL;
    AudioEncoder *audenc = nullptr;
    VideoEncoder *videnc = nullptr;
    AudioDecoder *auddec = nullptr;
    VideoDecoder *viddec = nullptr;
    struct SwsContext *img_convert_ctx = NULL;
    static const int STATE_READY = 1;
    static const int STATE_RECORDING = 2;
    static const int STATE_PAUSE = 3;
    static const int STATE_COMPLETED = 4;
    static const int STATE_ERROR = -1;
    void notify_message(int state,int error_code,int ffmpeg_code,char *err_msg){
        if(event_listener!= nullptr){
            event_listener(this,state,error_code,ffmpeg_code,err_msg);
        }
    }
public:
    ~EasyTransform();
    void set_file_name(const char *input_file_name,const char *save_file_name);
    void set_file_type(const char *type){
        if(file_type&&strlen(type)>0){
            file_type = av_strdup(type);
        }
    }
    void set_event_listener(void (*cb)(EasyTransform *,int,int,int,char *)){
        event_listener = cb;
    }
    int get_id(){
        return id;
    }
    void init();
    void init_context();
    void start();
    void read();
    void read_audio();
    void read_video();
    void do_pause(bool p){
        pause = p;
    }
    void write();
    void filter(const char *filter){
        if(filter&&strlen(filter)>0){
            filter_desc = av_strdup(filter);
        }
    }
    int stream_component_open(int stream_index);
    int stream_out_component_open(int stream_index);
    void video_crop(int w,int h){
        width = w;
        height = h;
    }
    void set_duration(int64_t dur){
        if(dur>0){
            duration = dur * AV_TIME_BASE;
        }
    }
    void interception(int64_t start,int64_t end){
        if(start >0){
            start_time = start * AV_TIME_BASE;
        }
        if(end>0&&end>start){
            end_time = end * AV_TIME_BASE;
            duration = (end - (start>0?start:0))*AV_TIME_BASE;
        }
    }
};
#endif //EASYPLAYER_EASYPLAYER_H
