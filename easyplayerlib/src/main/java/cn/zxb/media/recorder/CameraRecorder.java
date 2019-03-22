package cn.zxb.media.recorder;

public class CameraRecorder {
    static {
        System.loadLibrary("player-lib");
    }
    public native void recorder(String path);
}
