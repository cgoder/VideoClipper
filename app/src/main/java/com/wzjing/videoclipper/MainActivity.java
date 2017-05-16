package com.wzjing.videoclipper;

import android.content.Intent;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.provider.MediaStore;
import android.support.design.widget.FloatingActionButton;
import android.support.v7.app.AppCompatActivity;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.MediaController;
import android.widget.RelativeLayout;
import android.widget.Toast;
import android.widget.VideoView;

import java.io.File;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.OnClick;

public class MainActivity extends AppCompatActivity {

    private static String TAG = "mainActivity";

    @BindView(R.id.saveButton)
    Button saveButton;
    @BindView(R.id.videoView)
    VideoView videoView;
    @BindView(R.id.openFileFloatingActionButton)
    FloatingActionButton openFab;


    private File inputVideo, outputVideo;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        ButterKnife.bind(this);
//        videoView.setMediaController(controller);

        String state = Environment.getExternalStorageState();
        if (!Environment.MEDIA_MOUNTED.equals(state)) {
            Log.e(TAG, "Storage unavailable");
            return;
        }
        inputVideo = new File(Environment.getExternalStorageDirectory(), "simple.mp4");
        if (!inputVideo.exists()) {
            Log.e(TAG, "simple.mp4 not exists");
            return;
        }
        outputVideo = new File(inputVideo.getParent(), "simple_clip.mp4");
        MediaMetadataRetriever retriever = new MediaMetadataRetriever();
        retriever.setDataSource(inputVideo.getPath());
        int vw = Integer.valueOf(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH));
        int vh = Integer.valueOf(retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT));
        Log.i(TAG, String.format("Video Spec:%d x %d", vw, vh));
        ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) videoView.getLayoutParams();
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        if ((metrics.heightPixels / 2 * vw) / vh > metrics.widthPixels) {
            params.width = metrics.widthPixels;
            params.height = metrics.widthPixels * vh / vw;
        } else {
            params.height = metrics.heightPixels / 2;
            params.width = (metrics.heightPixels / 2) * vw / vh;
        }
        Log.i(TAG, String.format("params:%d x %d", params.width, params.height));
        videoView.setLayoutParams(params);

        videoView.setVideoPath(inputVideo.getPath());
        videoView.start();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 100 && resultCode == RESULT_OK) {
            Uri uri = data.getData();
            if (uri != null) {
                videoView.setVideoURI(uri);
                videoView.start();
            }
        }
    }

    @OnClick(R.id.saveButton)
    public void saveClip() {

    }

    @OnClick(R.id.openFileFloatingActionButton)
    public void openFile() {
        Intent intent = new Intent(Intent.ACTION_PICK, MediaStore.Video.Media.EXTERNAL_CONTENT_URI);
        startActivityForResult(intent, 100);
    }

    @OnClick(R.id.cutVideoFloatingActionButton)
    public void cutVideo() {
        if (inputVideo != null && outputVideo != null)
            clipVideo(10d, 20d, inputVideo.getAbsolutePath(), outputVideo.getAbsolutePath());
    }

    @OnClick(R.id.remuxingFloatingActionButton)
    public void remuxing() {
        if (inputVideo != null && outputVideo != null)
            remuxVideo(inputVideo.getAbsolutePath(), outputVideo.getAbsolutePath());
    }

    /* Native Calls Methods*/

    @SuppressWarnings("unused")
    public void toast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }

    /* Native Medthods */

    @SuppressWarnings("SameParameterValue")
    public native void clipVideo(double startTime, double endTime, String inFileName, String outFileName);

    public native void remuxVideo(String inFileName, String outFileName);

    static {
        System.loadLibrary("ffmpeg");
        System.loadLibrary("native-lib");
    }
}
