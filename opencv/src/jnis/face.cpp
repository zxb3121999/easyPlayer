//
// Created by Administrator on 2018/4/9 0009.
//

#include <jni.h>
#include <opencv2/core.hpp>
#include <opencv2/objdetect.hpp>

#include <string>
#include <vector>

#include <android/log.h>

#define LOG_TAG "FaceDetection/DetectionBasedTracker"
#define LOGD(...) ((void)__android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__))

using namespace std;
using namespace cv;

inline void vector_Rect_to_Mat(vector<Rect>& v_rect, Mat& mat)
{
    mat = Mat(v_rect, true);
}

class CascadeDetectorAdapter: public DetectionBasedTracker::IDetector
{
public:
    CascadeDetectorAdapter(cv::Ptr<cv::CascadeClassifier> detector):
            IDetector(),
            Detector(detector)
    {
        LOGD("CascadeDetectorAdapter::Detect::Detect");
        CV_Assert(detector);
    }

    void detect(const cv::Mat &Image, std::vector<cv::Rect> &objects)
    {
        LOGD("CascadeDetectorAdapter::Detect: begin");
        LOGD("CascadeDetectorAdapter::Detect: scaleFactor=%.2f, minNeighbours=%d, minObjSize=(%dx%d), maxObjSize=(%dx%d)", scaleFactor, minNeighbours, minObjSize.width, minObjSize.height, maxObjSize.width, maxObjSize.height);
        Detector->detectMultiScale(Image, objects, scaleFactor, minNeighbours, 0, minObjSize, maxObjSize);
        LOGD("CascadeDetectorAdapter::Detect: end");
    }

    virtual ~CascadeDetectorAdapter()
    {
        LOGD("CascadeDetectorAdapter::Detect::~Detect");
    }

private:
    CascadeDetectorAdapter();
    cv::Ptr<cv::CascadeClassifier> Detector;
};

struct DetectorAgregator
{
    cv::Ptr<CascadeDetectorAdapter> mainDetector;
    cv::Ptr<CascadeDetectorAdapter> trackingDetector;

    cv::Ptr<DetectionBasedTracker> tracker;
    DetectorAgregator(cv::Ptr<CascadeDetectorAdapter>& _mainDetector, cv::Ptr<CascadeDetectorAdapter>& _trackingDetector):
            mainDetector(_mainDetector),
            trackingDetector(_trackingDetector)
    {
        CV_Assert(_mainDetector);
        CV_Assert(_trackingDetector);

        DetectionBasedTracker::Parameters DetectorParams;
        tracker = makePtr<DetectionBasedTracker>(mainDetector, trackingDetector, DetectorParams);
    }
};

extern "C"
JNIEXPORT jlong JNICALL
Java_org_opencv_DetectionBasedTracker_nativeCreateObject(JNIEnv *env, jclass type, jstring cascadeName_, jint minFaceSize) {
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeCreateObject enter");
    const char* jnamestr = env->GetStringUTFChars(cascadeName_, NULL);
    string stdFileName(jnamestr);
    jlong result = 0;

    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeCreateObject");

    try
    {
        cv::Ptr<CascadeDetectorAdapter> mainDetector = makePtr<CascadeDetectorAdapter>(
                makePtr<CascadeClassifier>(stdFileName));
        cv::Ptr<CascadeDetectorAdapter> trackingDetector = makePtr<CascadeDetectorAdapter>(
                makePtr<CascadeClassifier>(stdFileName));
        result = (jlong)new DetectorAgregator(mainDetector, trackingDetector);
        if (minFaceSize > 0)
        {
            mainDetector->setMinObjectSize(Size(minFaceSize, minFaceSize));
            //trackingDetector->setMinObjectSize(Size(faceSize, faceSize));
        }
    }
    catch(cv::Exception& e)
    {
        LOGD("nativeCreateObject caught cv::Exception: %s", e.what());
        jclass je = env->FindClass("org/opencv/core/CvException");
        if(!je)
            je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
    catch (...)
    {
        LOGD("nativeCreateObject caught unknown exception");
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code of DetectionBasedTracker.nativeCreateObject()");
        result = 0;
    }
    env->ReleaseStringUTFChars(cascadeName_, jnamestr);
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeCreateObject exit");
    return result;
}
extern "C"
JNIEXPORT void JNICALL
Java_org_opencv_DetectionBasedTracker_nativeDestroyObject(JNIEnv *env, jclass type, jlong thiz) {

// TODO
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeDestroyObject");

    try
    {
        if(thiz != 0)
        {
            ((DetectorAgregator*)thiz)->tracker->stop();
            delete (DetectorAgregator*)thiz;
        }
    }
    catch(cv::Exception& e)
    {
        LOGD("nativeestroyObject caught cv::Exception: %s", e.what());
        jclass je = env->FindClass("org/opencv/core/CvException");
        if(!je)
            je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
    catch (...)
    {
        LOGD("nativeDestroyObject caught unknown exception");
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code of DetectionBasedTracker.nativeDestroyObject()");
    }
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeDestroyObject exit");

}
extern "C"
JNIEXPORT void JNICALL
Java_org_opencv_DetectionBasedTracker_nativeStart(JNIEnv *env, jclass type, jlong thiz) {

// TODO
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeStart");

    try
    {
        ((DetectorAgregator*)thiz)->tracker->run();
    }
    catch(cv::Exception& e)
    {
        LOGD("nativeStart caught cv::Exception: %s", e.what());
        jclass je = env->FindClass("org/opencv/core/CvException");
        if(!je)
            je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
    catch (...)
    {
        LOGD("nativeStart caught unknown exception");
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code of DetectionBasedTracker.nativeStart()");
    }
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeStart exit");

}
extern "C"
JNIEXPORT void JNICALL
Java_org_opencv_DetectionBasedTracker_nativeStop(JNIEnv *env, jclass type, jlong thiz) {

// TODO
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeStop");

    try
    {
        ((DetectorAgregator*)thiz)->tracker->stop();
    }
    catch(cv::Exception& e)
    {
        LOGD("nativeStop caught cv::Exception: %s", e.what());
        jclass je = env->FindClass("org/opencv/core/CvException");
        if(!je)
            je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
    catch (...)
    {
        LOGD("nativeStop caught unknown exception");
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code of DetectionBasedTracker.nativeStop()");
    }
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeStop exit");
}
extern "C"
JNIEXPORT void JNICALL
Java_org_opencv_DetectionBasedTracker_nativeSetFaceSize(JNIEnv *env, jclass type, jlong thiz, jint size) {

// TODO
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeSetFaceSize -- BEGIN");

    try
    {
        if (size > 0)
        {
            ((DetectorAgregator*)thiz)->mainDetector->setMinObjectSize(Size(size, size));
            //((DetectorAgregator*)thiz)->trackingDetector->setMinObjectSize(Size(faceSize, faceSize));
        }
    }
    catch(cv::Exception& e)
    {
        LOGD("nativeStop caught cv::Exception: %s", e.what());
        jclass je = env->FindClass("org/opencv/core/CvException");
        if(!je)
            je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
    catch (...)
    {
        LOGD("nativeSetFaceSize caught unknown exception");
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code of DetectionBasedTracker.nativeSetFaceSize()");
    }
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeSetFaceSize -- END");

}
extern "C"
JNIEXPORT void JNICALL
Java_org_opencv_DetectionBasedTracker_nativeDetect(JNIEnv *env, jclass type, jlong thiz, jlong inputImage, jlong faces) {

// TODO
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeDetect");

    try
    {
        vector<Rect> RectFaces;
        ((DetectorAgregator*)thiz)->tracker->process(*((Mat*)inputImage));
        ((DetectorAgregator*)thiz)->tracker->getObjects(RectFaces);
        *((Mat*)faces) = Mat(RectFaces, true);
    }
    catch(cv::Exception& e)
    {
        LOGD("nativeCreateObject caught cv::Exception: %s", e.what());
        jclass je = env->FindClass("org/opencv/core/CvException");
        if(!je)
            je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, e.what());
    }
    catch (...)
    {
        LOGD("nativeDetect caught unknown exception");
        jclass je = env->FindClass("java/lang/Exception");
        env->ThrowNew(je, "Unknown exception in JNI code DetectionBasedTracker.nativeDetect()");
    }
    LOGD("Java_org_opencv_samples_facedetect_DetectionBasedTracker_nativeDetect END");
}