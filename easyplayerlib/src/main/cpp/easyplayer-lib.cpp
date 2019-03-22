#include "camera.h"
#include <string>
#include <android/log.h>
#include "easyPlayer.h"
#include "jni.h"
#include <map>
#include "3rd/include/MemoryTrace.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>


#define LOGD(format, ...)  __android_log_print(ANDROID_LOG_INFO,  "easyplayer-lib", format, ##__VA_ARGS__)

static JavaVM *gVm = NULL;
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
jmethodID gOnResolutionChange = NULL;
jmethodID gPostEventFromNative = NULL;
jmethodID gPostEventFromNativeRecorder = NULL;
jmethodID gTransformFromNative = NULL;
EasyRecorder *recorder = nullptr;
std::map<int, EasyPlayer *> player_map;

void release_player(int player_id);

jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    gVm = vm;
    return JNI_VERSION_1_6;
}

void JNI_OnUnload(JavaVM *vm, void *reserved) {
    if (gRecorder) {
        delete gRecorder;
    }

    if (gTransform) {
        delete gTransform;
    }

    while (player_map.size() > 0) {
        std::map<int, EasyPlayer *>::iterator iterator = player_map.begin();
        release_player(iterator->first);
    }
}

EasyPlayer *find_player(int player_id) {
    std::map<int, EasyPlayer *>::iterator iterator = player_map.find(player_id);
    if (iterator == player_map.end()) {
        return nullptr;
    } else {
        return iterator->second;
    }
}

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    int id = *(int *) context;
    if (id >= 0) {

        std::map<int, EasyPlayer *>::iterator iterator = player_map.find(id);
        if (iterator != player_map.end()) {
            assert(iterator->second);
            assert(bq == iterator->second->bqPlayerBufferQueue);
            SLresult result;
            if (iterator->second->get_paused()) {
                result = (*iterator->second->bqPlayerPlay)->SetPlayState(
                        iterator->second->bqPlayerPlay, SL_PLAYSTATE_PAUSED);
                assert(result == SL_RESULT_SUCCESS);
                iterator->second->wait_paused();
                result = (*iterator->second->bqPlayerPlay)->SetPlayState(
                        iterator->second->bqPlayerPlay, SL_PLAYSTATE_PLAYING);
                assert(result == SL_RESULT_SUCCESS);
            }
//            av_log(NULL, AV_LOG_FATAL, "====play audio ");
            if (iterator->second->get_aud_buffer()) {
                result = (*bq)->Enqueue(bq, iterator->second->outputBuffer, iterator->second->nextSize);
                if (SL_RESULT_SUCCESS != result) {
                    (*iterator->second->bqPlayerPlay)->SetPlayState(iterator->second->bqPlayerPlay,
                                                                    SL_PLAYSTATE_STOPPED);
                    av_log(NULL, AV_LOG_FATAL, "播放音频结束,playerId:%d", id);
                    iterator->second->set_play_state(true, true);
                }
                (void) result;
            } else {
                (*iterator->second->bqPlayerPlay)->SetPlayState(iterator->second->bqPlayerPlay,
                                                                SL_PLAYSTATE_STOPPED);
                av_log(NULL, AV_LOG_FATAL, "播放音频结束,playerId:%d", id);
                iterator->second->set_play_state(true, true);
            }
        }
    } else {
        av_log(NULL, AV_LOG_FATAL, "播放器id出错:%d", id);
    }

}

//注册播放回调
extern "C"
jint
Java_cn_zxb_media_player_EasyMediaPlayer__1nativeInit
        (JNIEnv *env, jobject obj) {
    int id;
    std::map<int, EasyPlayer *>::reverse_iterator iter = player_map.rbegin();
    if (iter == player_map.rend()) {
        id = 0;
    } else {
        id = iter->first + 1;
    }
    EasyPlayer *player = new EasyPlayer();
    player->set_id(id);
    player->obj = env->NewGlobalRef(obj);
    if (player->obj == NULL) {
        delete player;
        return -1;
    }
    jclass clazz = env->GetObjectClass(obj);
    player->methodId = env->GetMethodID(clazz, "postEventFromNative", "(IIILjava/lang/String;)V");
    std::pair<std::map<int, EasyPlayer *>::iterator, bool> result = player_map.insert(
            std::pair<int, EasyPlayer *>(player->get_id(), player));
    if (result.second) {
        return player->get_id();
    } else {
        delete player;
        return -1;
    }
}


void listener(int player_id, int what, int arg1, int arg2, char *msg) {
    JNIEnv *env = NULL;
    EasyPlayer *player = find_player(player_id);
    if (player == nullptr)
        return;

    if (player->obj && player->methodId ) {
        int state = gVm->GetEnv((void **)(&env), JNI_VERSION_1_6);
        bool isAttach = false;
        if(state<0){
            if(gVm->AttachCurrentThread(&env, NULL)){
                return;
            }
            isAttach = true;
        }
        env->CallVoidMethod(player->obj, player->methodId, what, arg1, arg2,env->NewStringUTF(msg));
        if(isAttach)
            gVm->DetachCurrentThread();
    }
//    if (player->obj && player->methodId && what == -1 && player_id != -1) {
//        std::thread release([=] {
//            release_player(player_id);
//        });
//        release.detach();
//    }
}

void release_player(int player_id) {
    JNIEnv *env = NULL;
    std::map<int, EasyPlayer *>::iterator iterator = player_map.find(player_id);
    if (iterator != player_map.end()) {
        if (iterator->second->obj&&iterator->second->methodId) {
            int state = gVm->GetEnv((void **)(&env), JNI_VERSION_1_6);
            bool isAttach = false;
            if(state<0){
                if(gVm->AttachCurrentThread(&env, NULL)){
                    return;
                }
                isAttach = true;
            }
            env->CallVoidMethod(iterator->second->obj, iterator->second->methodId, 6, 0, 0,env->NewStringUTF("清除播放资源"));
            env->DeleteGlobalRef(iterator->second->obj);
            if(isAttach)
                gVm->DetachCurrentThread();
            iterator->second->obj = NULL;
            iterator->second->methodId = NULL;
            gOnResolutionChange = NULL;
        }
        iterator->second->stop();
        delete iterator->second;
        iterator->second = NULL;
        player_map.erase(player_id);
        LOGD("清理完成");
    }
}

void log_jni(void *ptr, int level, const char *fmt, va_list vl) {
    switch (level) {
        case AV_LOG_VERBOSE:
            __android_log_vprint(ANDROID_LOG_VERBOSE, "media-native", fmt, vl);
            break;
        case AV_LOG_INFO:
            __android_log_vprint(ANDROID_LOG_INFO, "media-native", fmt, vl);
            break;
        case AV_LOG_WARNING:
            __android_log_vprint(ANDROID_LOG_WARN, "media-native", fmt, vl);
            break;
        case AV_LOG_ERROR:
            __android_log_vprint(ANDROID_LOG_ERROR, "media-native", fmt, vl);
            break;
        case AV_LOG_FATAL:
        case AV_LOG_PANIC:
            __android_log_vprint(ANDROID_LOG_FATAL, "media-native", fmt, vl);
            break;
        case AV_LOG_QUIET:
            __android_log_vprint(ANDROID_LOG_SILENT, "media-native", fmt, vl);
            break;
        default:
//            __android_log_vprint(ANDROID_LOG_DEBUG, "media-native", fmt, vl);
            break;
    }
}

extern "C"
void
Java_cn_zxb_media_player_EasyMediaPlayer__1setDataSource
        (JNIEnv *env, jobject obj, jstring path, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        const char *path_ = env->GetStringUTFChars(path, NULL);
        av_log_set_callback(log_jni);
        mPlayer->set_event_listener(listener);
        mPlayer->set_audio_call_back(bqPlayerCallback);
        mPlayer->set_data_source(path_);
        env->ReleaseStringUTFChars(path,path_);
    }
}


extern "C"
void
Java_cn_zxb_media_player_EasyMediaPlayer__1setVideoSurface
        (JNIEnv *env, jobject obj, jobject surface, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        ANativeWindow *nativeWindow = ANativeWindow_fromSurface(env, surface);
        if (0 == nativeWindow) {
            LOGD("Couldn't get native window from surface.\n");
            return;
        }
        mPlayer->set_window(nativeWindow);
    }
}

extern "C"
void
Java_cn_zxb_media_player_EasyMediaPlayer__1destorySurface
        (JNIEnv *env, jobject obj, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        mPlayer->set_window(nullptr);
    }
}

extern "C"
void
Java_cn_zxb_media_player_EasyMediaPlayer__1prepareAsync
        (JNIEnv *env, jobject obj, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        mPlayer->prepare_async(false);
    }

}

extern "C"
void
Java_cn_zxb_media_player_EasyMediaPlayer__1start
        (JNIEnv *env, jobject obj, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer) {
        if (mPlayer->state == PlayerState::COMPLETED) {
            mPlayer->prepare_async(true);
            return;
        } else
            mPlayer->play();
    }
}


extern "C"
void
Java_cn_zxb_media_player_EasyMediaPlayer__1pause
        (JNIEnv *env, jobject obj, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer)
        mPlayer->pause();
}


extern "C"
bool
Java_cn_zxb_media_player_EasyMediaPlayer_getPlaying
        (JNIEnv *env, jobject obj, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        return mPlayer->is_playing();
    }
    return false;

}


extern "C"
long
Java_cn_zxb_media_player_EasyMediaPlayer_getDuration
        (JNIEnv *env, jobject obj, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        return (long) mPlayer->get_duration();
    }
    return 0;

}


extern "C"
long
Java_cn_zxb_media_player_EasyMediaPlayer_getCurrentPosition
        (JNIEnv *env, jobject obj, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        return mPlayer->get_curr_position();
    }
    return 0;

}

extern "C"
bool
Java_cn_zxb_media_player_EasyMediaPlayer__1seekTo
        (JNIEnv *env, jobject obj, jlong mSec, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        if (mSec < 0) mSec = 0;
        if (mSec > mPlayer->get_duration()) mSec = mPlayer->get_duration() - 1000;
        return mPlayer->stream_seek(mSec / 1000);
    }
    return false;
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_player_EasyMediaPlayer__1restart(JNIEnv *env, jobject instance, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        mPlayer->prepare();
        mPlayer->play();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_player_EasyMediaPlayer__1release(JNIEnv *env, jobject instance, jint player_id) {
    // TODO
    av_log(NULL, AV_LOG_FATAL, "release player", NULL);
//    std::thread release([=] {
        release_player(player_id);
//    });
//    release.detach();
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_player_EasyMediaPlayer__1canPlayBackground(JNIEnv *env, jobject instance,
                                                             jboolean canPlay, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        mPlayer->set_can_play_in_background(canPlay);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_player_EasyMediaPlayer__1setBackgroundState(JNIEnv *env, jobject instance,
                                                              jboolean isBackground,
                                                              jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        mPlayer->set_background_sate(isBackground);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_player_EasyMediaPlayer__1startRecorder(JNIEnv *env, jobject instance,
                                                         jstring path_, jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        const char *path = env->GetStringUTFChars(path_, NULL);
        mPlayer->start_recorder(path);
        env->ReleaseStringUTFChars(path_,path);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_player_EasyMediaPlayer__1stopRecorder(JNIEnv *env, jobject instance,
                                                        jint player_id) {
    EasyPlayer *mPlayer = find_player(player_id);
    if (mPlayer != nullptr) {
        mPlayer->stop_recorder();
    }
}


void event_listener(int what, int error_code, char *msg) {
    JNIEnv *env = NULL;
    if (0 == gVm->AttachCurrentThread(&env, NULL)) {
        env->CallVoidMethod(gRecorder, gPostEventFromNativeRecorder, what, error_code,env->NewStringUTF(msg));
        gVm->DetachCurrentThread();
    }
}


extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_recorder_EasyMediaRecorder__1startRecorder(JNIEnv *env, jobject instance,
                                                             jstring path_, jint width, jint height,
                                                             jint frame, jint bit) {
    if (gRecorder) {
        env->DeleteGlobalRef(gRecorder);
    }
    gRecorder = env->NewGlobalRef(instance);
    if (gRecorder == NULL) return;
    jclass clazz = env->GetObjectClass(instance);
    if (NULL == gPostEventFromNativeRecorder) {
        gPostEventFromNativeRecorder = env->GetMethodID(clazz, "postEventFromNative",
                                                        "(IILjava/lang/String;)V");
        if (NULL == gPostEventFromNativeRecorder) {
            LOGD("Couldn't find postEventFromNative method.\n");
        }
    }
    if (recorder != nullptr) {
        delete (recorder);
    }
    av_log_set_callback(log_jni);
    LOGD("start init recorder");
    recorder = new EasyRecorder;
    const char *path = env->GetStringUTFChars(path_, NULL);
    recorder->set_file_save_path(path);
    recorder->set_height(height);
    recorder->set_width(width);
    recorder->set_frame_rate(frame);
    recorder->set_bit_rate(bit);
    recorder->set_event_listener(event_listener);
    LOGD("start init prepare");
    recorder->prepare();
    env->ReleaseStringUTFChars(path_, path);
    // TODO
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_recorder_EasyMediaRecorder__1stopRecorder(JNIEnv *env, jobject instance,
                                                            jint flag) {

    // TODO
    if (recorder != nullptr) {
        if (flag == -2) {//立即停止
            recorder->stop_recorder();
        } else {
            recorder->recorder_audio(NULL, 0);
            recorder->recorder_img(NULL, 0);
        }
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_recorder_EasyMediaRecorder__1recorderVideo(JNIEnv *env, jobject instance,
                                                             jbyteArray data_, jint len) {
    jbyte *data = env->GetByteArrayElements(data_, NULL);

    // TODO
    if (recorder != nullptr && !recorder->is_video_stop())
        recorder->recorder_img((uint8_t *) data, len);
    env->ReleaseByteArrayElements(data_, data, 0);
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_recorder_EasyMediaRecorder__1recorderAudio(JNIEnv *env, jobject instance,
                                                             jbyteArray data_, jint len) {
    jbyte *data = env->GetByteArrayElements(data_, NULL);

    // TODO
    if (recorder != nullptr && !recorder->is_audio_stop())
        recorder->recorder_audio((uint8_t *) data, len);
    env->ReleaseByteArrayElements(data_, data, 0);
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_recorder_EasyMediaRecorder__1release(JNIEnv *env, jobject instance) {
    // TODO
    if (recorder != nullptr) {
        delete (recorder);
        recorder = nullptr;
    }
    if (gRecorder) {
        gPostEventFromNativeRecorder = 0;
        env->DeleteGlobalRef(gRecorder);
        gRecorder = NULL;
    }
    av_log(NULL, AV_LOG_FATAL, "清除gRecorder完成");
}

extern "C"
JNIEXPORT jint JNICALL
Java_cn_zxb_media_recorder_EasyMediaRecorder__1getRecorderState(JNIEnv *env, jobject instance) {

    // TODO
    av_log(NULL, AV_LOG_FATAL, "获取录制状态");
    if (recorder != nullptr) {
        return static_cast<int>(recorder->get_recorder_state());
    }
    return static_cast<int>(RecorderState::UNKNOWN);
}

extern "C"
JNIEXPORT jint JNICALL
Java_cn_zxb_media_cmd_CmdUtils_cmd(JNIEnv *env, jclass type, jobjectArray cmd) {

    // TODO
    return 1;
}

void transform_listener(EasyTransform *transform, int state, int error, int ffmpeg, char *msg) {
    JNIEnv *env = NULL;
    if (state == 4 || state == -1) {
        delete (transform);
    }
    if (0 == gVm->AttachCurrentThread(&env, NULL)) {
        env->CallVoidMethod(gTransform, gTransformFromNative, state, error, ffmpeg,env->NewStringUTF(msg));
        gVm->DetachCurrentThread();
    }
}

extern "C"
JNIEXPORT jint JNICALL
Java_cn_zxb_media_transform_TransformUtils_transform_1native(JNIEnv *env, jobject instance,
                                                             jstring srcFilePath_,
                                                             jstring dstFilePath_, jlong startTime,
                                                             jlong endTime, jlong duration,
                                                             jint width, jint height,
                                                             jstring filterDesc_) {
    const char *srcFilePath = env->GetStringUTFChars(srcFilePath_, 0);
    const char *dstFilePath = env->GetStringUTFChars(dstFilePath_, 0);
    const char *filterDesc = env->GetStringUTFChars(filterDesc_, 0);
    // TODO
    av_log_set_callback(log_jni);
    if (gTransform) {
        env->DeleteGlobalRef(gTransform);
    }
    gTransform = env->NewGlobalRef(instance);
    if (gTransform == NULL) return -1;
    jclass clazz = env->GetObjectClass(instance);
    if (NULL == gTransformFromNative) {
        gTransformFromNative = env->GetMethodID(clazz, "postEventFromNative",
                                                "(IIILjava/lang/String;)V");
        if (NULL == gTransformFromNative) {
            LOGD("Couldn't find postEventFromNative method.\n");
        }
    }
    EasyTransform *transform = new EasyTransform;
    transform->set_event_listener(transform_listener);
    transform->set_file_name(srcFilePath, dstFilePath);
    transform->video_crop(width, height);
    transform->interception(startTime, endTime);
    transform->set_duration(duration);
    transform->filter(filterDesc);
    transform->init();
    env->ReleaseStringUTFChars(srcFilePath_, srcFilePath);
    env->ReleaseStringUTFChars(dstFilePath_, dstFilePath);
    env->ReleaseStringUTFChars(filterDesc_, filterDesc);
    return transform->get_id();
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_recorder_CameraRecorder_recorder(JNIEnv *env, jobject instance, jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);

    // TODO
    av_log_set_callback(log_jni);
    CameraDemo demo;
    demo.recorder(path);
    env->ReleaseStringUTFChars(path_, path);
}

extern "C"
JNIEXPORT void JNICALL
Java_cn_zxb_media_OpenSSlDemo_testOpenssl(JNIEnv *env, jobject instance) {

    // TODO

}extern "C"
JNIEXPORT jbyteArray JNICALL
Java_cn_zxb_media_OpenSSlDemo_hashKey(JNIEnv *env, jobject instance, jbyteArray pass,
                                      jbyteArray salt, jint count) {
    jbyte* pJbytePass = env->GetByteArrayElements(pass, NULL);
    char* szBytePass = (char *)pJbytePass;
    int iLenPass = env->GetArrayLength(pass);
    jbyte* pJbyteSalt = env->GetByteArrayElements( salt, NULL);
    char* szByteSalt = (char *)pJbyteSalt;
    int iLenSalt = env->GetArrayLength( salt);
    int OUTSIZE = 64;
    char buf[64];
    memset( buf, 0, sizeof(buf) );
    PKCS5_PBKDF2_HMAC(
            szBytePass,
            iLenPass,
            reinterpret_cast<const unsigned char *>(szByteSalt),
            iLenSalt,
            count, EVP_sha512(), OUTSIZE, reinterpret_cast<unsigned char *>(buf));
    jbyteArray jarray = env->NewByteArray(OUTSIZE);
    env->SetByteArrayRegion(jarray, 0, OUTSIZE, reinterpret_cast<const jbyte *>(buf));
    env->ReleaseByteArrayElements(pass, pJbytePass, 0);
    env->ReleaseByteArrayElements( salt, pJbyteSalt, 0);
    return jarray;
}