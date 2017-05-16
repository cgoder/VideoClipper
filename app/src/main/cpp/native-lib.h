#ifndef VIDEOCLIPPER_NATIVE_LIB_H
#define VIDEOCLIPPER_NATIVE_LIB_H

#include <jni.h>
#include <string>
#include <Android/log.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
};
#endif //VIDEOCLIPPER_NATIVE_LIB_H
#define TAG "native_tag"
#define LOGD(format, ...) __android_log_print(ANDROID_LOG_INFO, TAG, format, ## __VA_ARGS__)
#define LOGI(format, ...) __android_log_print(ANDROID_LOG_INFO, TAG, format, ## __VA_ARGS__)
#define LOGW(format, ...) __android_log_print(ANDROID_LOG_WARN, TAG, format, ## __VA_ARGS__)
#define LOGE(format, ...) __android_log_print(ANDROID_LOG_ERROR, TAG, format, ## __VA_ARGS__)

long start_time;

extern "C"
{

JNIEXPORT void JNICALL
Java_com_wzjing_videoclipper_MainActivity_clipVideo(JNIEnv *env, jobject instance,
                                                    jdouble startTime, jdouble endTime,
                                                    jstring inFileName_, jstring outFileName_);

JNIEXPORT void JNICALL
Java_com_wzjing_videoclipper_MainActivity_remuxVideo(JNIEnv *env, jobject instance,
                                                     jstring inFileName_, jstring outFileName_);
};

int cut_video(double starttime, double endtime, const char *in_filename, const char *out_filename);

int convert_and_cut(float starttime, float endtime, const char *in_filename, const char *out_filename);

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag);

int remuxing(const char *in_filename, const char *out_filename);

void error(int error_code);

void toast(JNIEnv* env, const char* message);