#include <jni.h>
#include <string>
#include <thread>
#include <mutex>
#include <android/log.h>
#include <opensles.h>

#include <android/native_window.h>
#include <android/native_window_jni.h>


#define LOGD(format, ...)  __android_log_print(ANDROID_LOG_INFO,  "easyplayer-lib", format, ##__VA_ARGS__)

static JavaVM *gVm = NULL;
static jobject gObj = NULL;
static jobject gRecorder = NULL;
static jobject gTransform = NULL;
static const int MEDIA_NOP = 0; // interface test message
static const int MEDIA_PREPARED = 1;
static const int MEDIA_PLAYBACK_COMPLETE = 2;
static const int MEDIA_BUFFERING_UPDATE = 3;
static const int MEDIA_SEEK_COMPLETE = 4;
static const int MEDIA_SET_VIDEO_SIZE = 5;
static const int MEDIA_TIMED_TEXT = 99;
static const int MEDIA_ERROR = 100;
static const int MEDIA_INFO = 200;

ANativeWindow *nativeWindow;
ANativeWindow_Buffer windowBuffer;
jmethodID gOnResolutionChange = NULL;
jmethodID gPostEventFromNative = NULL;
jmethodID gPostEventFromNativeRecorder = NULL;
jmethodID gTransformFromaNative = NULL;
EasyPlayer *mPlayer = nullptr;
EasyRecorder *recorder = nullptr;
bool can_play_in_background = true;
bool is_in_background = false;
std::mutex mutex;
std::condition_variable state_condition;
enum class ReleaseState{
    UNKNOWN,
    RUNNING,
    START,
    COMPLETE,
};
ReleaseState state = ReleaseState ::COMPLETE;
//
void wait_state(ReleaseState need_state) {
    std::unique_lock<std::mutex> lock(mutex);
    state_condition.wait(lock, [need_state] {
        return state >= need_state;
    });
}
void on_state_change(ReleaseState _state) {
    std::unique_lock<std::mutex> lock(mutex);
    state = _state;
    state_condition.notify_all();
}
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    gVm = vm;
    return JNI_VERSION_1_4;
}

//注册播放回调
extern "C"
void
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1nativeInit
        (JNIEnv *env, jobject obj) {
    gObj = env->NewGlobalRef(obj);
    if (gObj == NULL) return;
    jclass clazz = env->GetObjectClass(obj);
    if (NULL == gPostEventFromNative) {
        gPostEventFromNative = env->GetMethodID(clazz, "postEventFromNative", "(IIILjava/lang/String;)V");
        if (NULL == gPostEventFromNative) {
            LOGD("Couldn't find postEventFromNative method.\n");
        }
    }
}


int picThread = 0;

//视频播放
void showPic() {
    picThread++;
    mPlayer->wait_state(PlayerState::READY);
    if (!mPlayer->has_video()) return;
    if (0 > ANativeWindow_setBuffersGeometry(nativeWindow, mPlayer->viddec->get_width(), mPlayer->viddec->get_height(), WINDOW_FORMAT_RGBA_8888)) {
        LOGD("Couldn't set buffers geometry.\n");
        ANativeWindow_release(nativeWindow);
        on_state_change(ReleaseState::COMPLETE);
        return;
    }
    AVFrame *frameRGBA = av_frame_alloc();
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGBA, mPlayer->viddec->get_width(), mPlayer->viddec->get_height(), 1);
    uint8_t *vOutBuffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRGBA->data, frameRGBA->linesize, vOutBuffer, AV_PIX_FMT_RGBA, mPlayer->viddec->get_width(), mPlayer->viddec->get_height(), 1);
    on_state_change(ReleaseState::RUNNING);
    while (state==ReleaseState::RUNNING&&mPlayer->get_img_frame(frameRGBA)) {
        if (mPlayer->get_paused()) {
            mPlayer->wait_paused();
        }
        if (is_in_background)
            continue;
        if (ANativeWindow_lock(nativeWindow, &windowBuffer, NULL) < 0) {
            LOGD("cannot lock window");
        } else {
            uint8_t *dst = (uint8_t *) windowBuffer.bits;
            for (int h = 0; h < mPlayer->viddec->get_height(); h++) {
                memcpy(dst + h * windowBuffer.stride * 4,
                       vOutBuffer + h * frameRGBA->linesize[0],
                       frameRGBA->linesize[0]);
            }
            ANativeWindow_unlockAndPost(nativeWindow);
        }
    }
    free(vOutBuffer);
    picThread--;
    av_frame_free(&frameRGBA);
    LOGD("图片播放完成");
    on_state_change(ReleaseState::COMPLETE);
    return;
}

//音频播放
void playAudio() {
    LOGD("start audio play thread");
    if (!mPlayer->has_audio())
        return;
    mPlayer->wait_state(PlayerState::READY);
    createAudioEngine();
    createBufferQueueAudioPlayer(mPlayer->auddec->get_sample_rate(), mPlayer->auddec->get_channels());
    audioStart();
}

void restart() {
    mPlayer->prepare_async();
    mPlayer->wait_state(PlayerState::READY);
    mPlayer->play();
    if (!picThread) {
        std::thread videoThread(showPic);
        std::thread audioThread(playAudio);
        audioThread.detach();
        videoThread.detach();
    }
}

void listener(EasyPlayer *player,int what, int arg1, int arg2, char *msg) {
    JNIEnv *env = NULL;
    if(what==-1&&player!= nullptr){
        delete(player);
        mPlayer = nullptr;
    }
    if (0 == gVm->AttachCurrentThread(&env, NULL)) {
        env->CallVoidMethod(gObj, gPostEventFromNative, what, arg1, arg2,env->NewStringUTF(msg));
        gVm->DetachCurrentThread();
    }
}


void log(void *ptr, int level, const char *fmt, va_list vl) {
    switch (level) {
        case AV_LOG_VERBOSE:
            __android_log_vprint(ANDROID_LOG_DEBUG, "native-lib", fmt, vl);
            break;
        case AV_LOG_INFO:
            __android_log_vprint(ANDROID_LOG_INFO, "native-lib", fmt, vl);
            break;
        case AV_LOG_WARNING:
            __android_log_vprint(ANDROID_LOG_WARN, "native-lib", fmt, vl);
            break;
        case AV_LOG_ERROR:
            __android_log_vprint(ANDROID_LOG_ERROR, "native-lib", fmt, vl);
            break;
        case AV_LOG_FATAL:
        case AV_LOG_PANIC:
            __android_log_vprint(ANDROID_LOG_FATAL, "native-lib", fmt, vl);
            break;
        case AV_LOG_QUIET:
            __android_log_vprint(ANDROID_LOG_SILENT, "native-lib", fmt, vl);
            break;
        default:
            break;
    }
}


extern "C"
void
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1setDataSource
        (JNIEnv *env, jobject obj, jstring path) {
    mPlayer = new EasyPlayer();
    char inputStr[500] = {0};
    sprintf(inputStr, "%s", env->GetStringUTFChars(path, NULL));
    av_log_set_callback(log);
    mPlayer->set_event_listener(listener);
    mPlayer->set_data_source(inputStr);
    init(mPlayer);
}


extern "C"
void
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1setVideoSurface
        (JNIEnv *env, jobject obj, jobject surface) {
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
    }
    nativeWindow = ANativeWindow_fromSurface(env, surface);
    if (0 == nativeWindow) {
        LOGD("Couldn't get native window from surface.\n");
        return;
    }
    if (mPlayer != nullptr&&mPlayer->has_video()) {
        if (0 > ANativeWindow_setBuffersGeometry(nativeWindow, mPlayer->viddec->get_width(), mPlayer->viddec->get_height(), WINDOW_FORMAT_RGBA_8888)) {
            LOGD("Couldn't set buffers geometry.\n");
            ANativeWindow_release(nativeWindow);
            return;
        }
    }
}
extern "C"
void
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1prepareAsync
        (JNIEnv *env, jobject obj) {
    if(mPlayer!= nullptr){
        mPlayer->prepare_async();
    }

}

extern "C"
void
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1start
        (JNIEnv *env, jobject obj) {
    if (mPlayer->state == PlayerState::COMPLETED) {
        std::thread restartThread(restart);
        restartThread.detach();
        return;
    }
    mPlayer->play();
    if (!picThread) {
        std::thread videoThread(showPic);
        std::thread audioThread(playAudio);
        audioThread.detach();
        videoThread.detach();
    }
}


extern "C"
void
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1pause
        (JNIEnv *env, jobject obj) {
    mPlayer->pause();

}


extern "C"
bool
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer_isPlaying
        (JNIEnv *env, jobject obj) {
    if (mPlayer != nullptr) {
        return mPlayer->is_playing();
    }
    return false;

}


extern "C"
long
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer_getDuration
        (JNIEnv *env, jobject obj) {
    if (mPlayer != nullptr) {
        return (long) mPlayer->get_duration();
    }
    return 0;

}


extern "C"
long
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer_getCurrentPosition
        (JNIEnv *env, jobject obj) {
    if (mPlayer != nullptr) {
        return mPlayer->get_curr_position();
    }
    return 0;

}


extern "C"
void
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer_seekTo
        (JNIEnv *env, jobject obj, jlong mSec) {
    if (mPlayer != nullptr) {
        if (mSec < 0) mSec = 0;
        if (mSec > mPlayer->get_duration()) mSec = mPlayer->get_duration() - 1000;
        mPlayer->stream_seek(mSec / 1000);
    }

}



extern "C"
void
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1release
        (JNIEnv *env, jobject obj, jlong mSec) {
    if (mPlayer != nullptr) {
        if(state == ReleaseState::RUNNING)
            on_state_change(ReleaseState::START);
        release();
        wait_state(ReleaseState::COMPLETE);
        mPlayer->release();
        delete(mPlayer);
        LOGD("清理完成");
    }
    mPlayer = nullptr;
}




extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1restart(JNIEnv *env, jobject instance) {
    if (mPlayer != nullptr) {
        mPlayer->prepare();
        if (!picThread) {
            std::thread videoThread(showPic);
            std::thread audioThread(playAudio);
            audioThread.detach();
            videoThread.detach();
        }
        mPlayer->play();
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1canPlayBackground(JNIEnv *env, jobject instance, jboolean canPlay) {
    can_play_in_background = canPlay;
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1setBackgroundState(JNIEnv *env, jobject instance, jboolean isBackground) {
    is_in_background = isBackground;
    if (mPlayer == nullptr)
        return;
    if (is_in_background) {
        if (!isBackground) {
            mPlayer->pause();
            return;
        }
    }
    mPlayer->play();
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1startRecorder(JNIEnv *env, jobject instance, jstring path_) {
    if (mPlayer != nullptr) {
        char inputStr[500] = {0};
        sprintf(inputStr, "%s", env->GetStringUTFChars(path_, NULL));
        mPlayer->start_recorder(inputStr);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_player_EasyMediaPlayer__1stopRecorder(JNIEnv *env, jobject instance) {

    // TODO
    if (mPlayer != nullptr) {
        mPlayer->stop_recorder();
    }
}

void event_listener(int what, int error_code, char *msg) {
    JNIEnv *env = NULL;
    if (0 == gVm->AttachCurrentThread(&env, NULL)) {
        env->CallVoidMethod(gRecorder, gPostEventFromNativeRecorder, what, error_code, env->NewStringUTF(msg));
        gVm->DetachCurrentThread();
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_recorder_EasyMediaRecorder__1startRecorder(JNIEnv *env, jobject instance, jstring path_, jint width, jint height, jint frame, jint bit) {
    if (gRecorder) {
        env->DeleteGlobalRef(gRecorder);
    }
    gRecorder = env->NewGlobalRef(instance);
    if (gRecorder == NULL) return;
    jclass clazz = env->GetObjectClass(instance);
    if (NULL == gPostEventFromNativeRecorder) {
        gPostEventFromNativeRecorder = env->GetMethodID(clazz, "postEventFromNative", "(IILjava/lang/String;)V");
        if (NULL == gPostEventFromNativeRecorder) {
            LOGD("Couldn't find postEventFromNative method.\n");
        }
    }
    if (recorder != nullptr) {
        delete (recorder);
    }
    av_log_set_callback(log);
    av_log(NULL, AV_LOG_FATAL, "test log");
    recorder = new EasyRecorder;
    char outputStr[500] = {0};
    sprintf(outputStr, "%s", env->GetStringUTFChars(path_, NULL));
    recorder->set_file_save_path(outputStr);
    recorder->set_height(height);
    recorder->set_width(width);
    recorder->set_frame_rate(frame);
    recorder->set_bit_rate(bit);
    recorder->prepare();
    recorder->set_event_listener(event_listener);
    // TODO
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_recorder_EasyMediaRecorder__1stopRecorder(JNIEnv *env, jobject instance) {

    // TODO
    if (recorder != nullptr) {
        recorder->recorder_audio(NULL, 0);
        recorder->recorder_img(NULL, 0);
    }
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_recorder_EasyMediaRecorder__1recorderVideo(JNIEnv *env, jobject instance, jbyteArray data_, jint len) {
    jbyte *data = env->GetByteArrayElements(data_, NULL);

    // TODO
    if (recorder != nullptr)
        recorder->recorder_img((uint8_t *) data, len);
    env->ReleaseByteArrayElements(data_, data, 0);
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_recorder_EasyMediaRecorder__1recorderAudio(JNIEnv *env, jobject instance, jbyteArray data_, jint len) {
    jbyte *data = env->GetByteArrayElements(data_, NULL);

    // TODO
    if (recorder != nullptr)
        recorder->recorder_audio((uint8_t *) data, len);
    env->ReleaseByteArrayElements(data_, data, 0);
}
extern "C"
JNIEXPORT void JNICALL
Java_cn_jx_easyplayerlib_recorder_EasyMediaRecorder__1release(JNIEnv *env, jobject instance) {

    // TODO
    if (recorder != nullptr) {
        delete (recorder);
        recorder = nullptr;
        gPostEventFromNativeRecorder = 0;
        env->DeleteGlobalRef(gRecorder);
        gRecorder = NULL;
    }
}
extern "C"
JNIEXPORT jint JNICALL
Java_cn_jx_easyplayerlib_recorder_EasyMediaRecorder__1getRecorderState(JNIEnv *env, jobject instance) {

    // TODO
    if (recorder != nullptr) {
        return static_cast<int>(recorder->get_recorder_state());
    }
    return static_cast<int>(RecorderState::UNKNOWN);
}
extern "C"
JNIEXPORT jint JNICALL
Java_cn_jx_easyplayerlib_cmd_CmdUtils_cmd(JNIEnv *env, jclass type, jobjectArray cmd) {

    // TODO
    return 1;
}

void transform_listener(EasyTransform *transform,int state,int error,int ffmpeg,char *msg){
    JNIEnv *env = NULL;
    if(state==4 || state == -1){
        delete(transform);
    }
    if (0 == gVm->AttachCurrentThread(&env, NULL)) {
        env->CallVoidMethod(gTransform, gTransformFromaNative,state,error,ffmpeg,env->NewStringUTF(msg));
        gVm->DetachCurrentThread();
    }
}
extern "C"
JNIEXPORT jint JNICALL
Java_cn_jx_easyplayerlib_transform_TransformUtils_transform_1native(JNIEnv *env, jobject instance, jstring srcFilePath_, jstring dstFilePath_, jlong startTime, jlong endTime, jlong duration, jint width, jint height, jstring filterDesc_) {
    const char *srcFilePath = env->GetStringUTFChars(srcFilePath_, 0);
    const char *dstFilePath = env->GetStringUTFChars(dstFilePath_, 0);
    const char *filterDesc = env->GetStringUTFChars(filterDesc_, 0);
    // TODO
    av_log_set_callback(log);
    if (gTransform) {
        env->DeleteGlobalRef(gTransform);
    }
    gTransform = env->NewGlobalRef(instance);
    if (gTransform == NULL) return -1;
    jclass clazz = env->GetObjectClass(instance);
    if (NULL == gTransformFromaNative) {
        gTransformFromaNative = env->GetMethodID(clazz, "postEventFromNative", "(IIILjava/lang/String;)V");
        if (NULL == gTransformFromaNative) {
            LOGD("Couldn't find postEventFromNative method.\n");
        }
    }
    EasyTransform *transform = new EasyTransform;
    transform->set_event_listener(transform_listener);
    transform->set_file_name(srcFilePath,dstFilePath);
    transform->video_crop(width,height);
    transform->interception(startTime,endTime);
    transform->set_duration(duration);
    transform->filter(filterDesc);
    transform->init();
    env->ReleaseStringUTFChars(srcFilePath_, srcFilePath);
    env->ReleaseStringUTFChars(dstFilePath_, dstFilePath);
    env->ReleaseStringUTFChars(filterDesc_, filterDesc);
    return transform->get_id();
}