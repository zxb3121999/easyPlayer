package cn.jx.easyplayer;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Message;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.support.v7.app.AppCompatActivity;
import android.util.SparseArray;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;
import cn.jx.easyplayerlib.recorder.EasyMediaRecorder;
import cn.jx.easyplayerlib.recorder.IMediaRecorder;
import java.io.IOException;
import java.util.List;

public class RecorderActivity extends AppCompatActivity {
  private String TAG = getClass().getSimpleName();
  private SurfaceHolder mHolder;
  private android.hardware.Camera mCamera;
  private android.hardware.Camera.Parameters mParameters;
  private HandlerThread mBackgroundThread;
  private Handler mBackgroundHandler;
  private AudioRecord mRecord;
  private boolean isRecording = false;
  private int bufferSize = 0;
  private EasyMediaRecorder mRecorder;
  private static SparseArray ORIENTATIONS = new SparseArray();
  private TextView tv;

  static {
    ORIENTATIONS.append(Surface.ROTATION_0, 90);
    ORIENTATIONS.append(Surface.ROTATION_90, 0);
    ORIENTATIONS.append(Surface.ROTATION_180, 270);
    ORIENTATIONS.append(Surface.ROTATION_270, 180);
  }

  @Override protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_recorder);
    SurfaceView surfaceView = (SurfaceView) findViewById(R.id.surface);
    tv = (TextView) findViewById(R.id.recorder);
    mHolder = surfaceView.getHolder();
    mHolder.addCallback(new SurfaceHolder.Callback() {
      @Override public void surfaceCreated(SurfaceHolder holder) {
        if (mCamera == null) {
          initCamera();
        }else {
          startPreviewDisplay();
          mCamera.cancelAutoFocus();
        }
      }

      @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (mCamera != null) {
          stopPreviewDisplay();
          startPreviewDisplay();
          mCamera.cancelAutoFocus();
        }
      }

      @Override public void surfaceDestroyed(SurfaceHolder holder) {
        stopPreviewDisplay();
      }
    });
  }

  @Override protected void onPause() {
    super.onPause();
  }

  @Override public void onResume() {
    super.onResume();

  }

  @Override protected void onDestroy() {
    if (mCamera != null) mCamera.release();
    if (isRecording) {
      isRecording = false;
      mBackgroundHandler.sendEmptyMessage(IMediaRecorder.STOP_NOW);
    }
    super.onDestroy();
  }

  private void initAudioRecorder() {
    bufferSize = AudioRecord.getMinBufferSize(44100, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT);
    mRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, 44100, AudioFormat.CHANNEL_IN_STEREO, AudioFormat.ENCODING_PCM_16BIT, bufferSize);
  }

  private void initCamera() {
    if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
      ActivityCompat.requestPermissions(this, new String[] { Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO }, 0);
      return;
    }
    mRecorder = new EasyMediaRecorder();
    mRecorder.setCompletedListener(new IMediaRecorder.OnRecorderCompletedListener() {
      @Override public void onCompleted() {
        tv.setText("开始");
      }
    });
    mRecorder.setStartListener(new IMediaRecorder.OnStartListener() {
      @Override public void onStart() {
        tv.setText("停止");
      }
    });
    mRecorder.setErrorListener(new IMediaRecorder.OnErrorListener() {
      @Override public void onError(int errorCode, String msg) {
        tv.setText("停止");
      }
    });
    mCamera = android.hardware.Camera.open();
    if (mCamera == null) {
      Toast.makeText(this, "打开相机失败", Toast.LENGTH_SHORT).show();
      return;
    }
    mParameters = mCamera.getParameters();
    mParameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
    if (mParameters.isVideoStabilizationSupported()) mParameters.setVideoStabilization(true);
    List<Camera.Size> list = mParameters.getSupportedPreviewSizes();
    Camera.Size maxSize = list.get(0);
    for (Camera.Size size : list) {
      if ((size.width * size.height) >= (maxSize.width * maxSize.height)) {
        maxSize = size;
      }
    }
    mParameters.setPreviewSize(640, 480);
    mParameters.setPreviewFormat(ImageFormat.YV12);
    List<Integer> frameList = mParameters.getSupportedPreviewFrameRates();
    if (frameList.contains(30)) {
      mParameters.setPreviewFrameRate(30);
    } else {
      mParameters.setPreviewFrameRate(frameList.get(0));
    }
    mCamera.setParameters(mParameters);
    followScreenOrientation();
    startPreviewDisplay();
    mCamera.cancelAutoFocus();
  }

  /**
   * 检测是否支持指定特性
   */
  private boolean isSupported(List<String> list, String key) {
    return list != null && list.contains(key);
  }

  long time;

  public void Recorder(View view) {
    if (mBackgroundThread == null) {
      mBackgroundThread = new HandlerThread("start_recorder");
      mBackgroundThread.start();
      mBackgroundHandler = new Handler(mBackgroundThread.getLooper(), new Handler.Callback() {
        @Override public boolean handleMessage(Message msg) {
          if (msg.obj != null && msg.what == 0) {
            byte[] data = (byte[]) msg.obj;
            mRecorder.recorderVideo(data, data.length);
          } else if (msg.what == IMediaRecorder.STOP_NOW||msg.what == IMediaRecorder.STOP_WAIT_FOR_COMPLETED) {
            mBackgroundHandler.removeMessages(0);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
              mBackgroundThread.quitSafely();
            } else {
              mBackgroundThread.quit();
            }
            mRecorder.stopRecorder(msg.what);
            mBackgroundThread = null;
          }
          return false;
        }
      });
    }
    TextView tv = (TextView) view;
    if (isRecording) {
      tv.setText("开始");
      isRecording = false;
      mBackgroundHandler.sendEmptyMessage(-1);
    } else {
      initAudioRecorder();
      //mRecorder.startRecorder("rtmp://192.168.232.22:1935/live/12345", mParameters);
      mRecorder.startRecorder(Environment.getExternalStorageDirectory().getAbsolutePath() + "/ffmpeg/test.mp4", mParameters);
      isRecording = true;
      new Thread(AudioRunnable).start();
    }
  }

  public void getState(View v) {
    startActivity(new Intent(this,MainActivity.class));
  }

  private void followScreenOrientation() {
    final int orientation = getResources().getConfiguration().orientation;
    if (orientation == Configuration.ORIENTATION_LANDSCAPE) {
      mCamera.setDisplayOrientation(180);
    } else if (orientation == Configuration.ORIENTATION_PORTRAIT) {
      mCamera.setDisplayOrientation(90);
    }
  }

  private void startPreviewDisplay() {
    if (mCamera != null) {
      try {
        followScreenOrientation();
        mCamera.setPreviewDisplay(mHolder);
        int size = ((mParameters.getPreviewSize().width * mParameters.getPreviewSize().height) * ImageFormat.getBitsPerPixel(mParameters.getPreviewFormat())) / 8;
        mCamera.addCallbackBuffer(new byte[size]);
        mCamera.addCallbackBuffer(new byte[size]);
        mCamera.addCallbackBuffer(new byte[size]);
        mCamera.setPreviewCallbackWithBuffer(mCallback);
        mCamera.startPreview();
      } catch (IOException e) {
        e.printStackTrace();
      }
    }
  }

  private void stopPreviewDisplay() {
    if (mCamera != null) {
      mCamera.stopPreview();
    }
  }

  private Camera.PreviewCallback mCallback = new Camera.PreviewCallback() {
    @Override public void onPreviewFrame(byte[] data, Camera camera) {
      if (data != null) {
        if (isRecording && mBackgroundHandler != null) {
          mBackgroundHandler.sendMessage(Message.obtain(mBackgroundHandler, 0, data));
        }
        mCamera.addCallbackBuffer(data);
      }
    }
  };

  @Override public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
    if (requestCode == 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
      initCamera();
    }
  }

  private Runnable AudioRunnable = new Runnable() {
    @Override public void run() {
      byte[] buffer = new byte[4096];
      try {
        mRecord.startRecording();
        int result = 0;
        while (isRecording) {
          result = mRecord.read(buffer, 0, 4096);
          if (result == AudioRecord.ERROR_INVALID_OPERATION || result == AudioRecord.ERROR_BAD_VALUE) {
            continue;
          }
          if (result > 0) {
            mRecorder.recorderAudio(buffer, result);
          }
        }
        mRecord.stop();
        mRecord.release();
      } catch (Exception e) {
        e.printStackTrace();
      }
    }
  };
}
