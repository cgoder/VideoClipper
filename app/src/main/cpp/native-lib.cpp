#include "native-lib.h"

//Java调用剪切
JNIEXPORT void JNICALL
Java_com_wzjing_videoclipper_MainActivity_clipVideo(JNIEnv *env, jobject instance,
                                                    jdouble startTime, jdouble endTime,
                                                    jstring inFileName_, jstring outFileName_)
{
    const char *inFileName = env->GetStringUTFChars(inFileName_, 0);
    const char *outFileName = env->GetStringUTFChars(outFileName_, 0);

    if(cut_video(startTime, endTime, inFileName, outFileName) == 0)
    {
        toast(env, instance, "Clip video finished");
    }
    else
    {
        toast(env, instance, "Clip video failed");
    }

    env->ReleaseStringUTFChars(inFileName_, inFileName);
    env->ReleaseStringUTFChars(outFileName_, outFileName);
}


//Java调用转码
JNIEXPORT void JNICALL
Java_com_wzjing_videoclipper_MainActivity_remuxVideo(JNIEnv *env, jobject instance,
                                                     jstring inFileName_, jstring outFileName_)
{
    const char *inFileName = env->GetStringUTFChars(inFileName_, 0);
    const char *outFileName = env->GetStringUTFChars(outFileName_, 0);

    if(remuxing(inFileName, outFileName) == 0)
    {
        toast(env, instance, "Remux video finished");
    }
    else
    {
        toast(env, instance, "Remux video failed");
    }

    env->ReleaseStringUTFChars(inFileName_, inFileName);
    env->ReleaseStringUTFChars(outFileName_, outFileName);
}


/**
 * 打印 Packet 对象详细信息
 * @param fmt_ctx   输出的AVFormatContext对象
 * @param pkt       AVPacket指针
 * @param tag       标签(输入in或者输出out)
 */
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
                        tag,
                        av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
                        av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
                        av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
                        pkt->stream_index);
}

/**
 * 根据起始时间裁剪一段视频
 * @param starttime  开始时间
 * @param endtime   结束时间
 * @param in_filename   原视频的路径(包含文件名)
 * @param out_filename  裁剪结果的存放路径(包含文件名)
 * @return  如果成功则返回0，否则返回其他
 */
int cut_video(double starttime, double endtime, const char* in_filename, const char* out_filename)
{
    LOGI("%s", "Start cut video");
    AVOutputFormat *outputFormat = NULL;
    AVFormatContext *inFormatContext = NULL, *outFormatContext = NULL;
    AVPacket packet;
    int return_code, i;

    //1、注册所有组件
    LOGI("%s", "注册所有组件");
    av_register_all();

    //2、开始读取输入视频文件
    if ((return_code = avformat_open_input(&inFormatContext, in_filename, 0, 0)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open input file %s", in_filename);
        error(return_code);
        return -1;
    }

    //3、获取视频流媒体信息
    LOGI("%s", "获取视频流媒体信息");
    if ((return_code = avformat_find_stream_info(inFormatContext, 0)) < 0) {
        LOGE("%s", "Failed to retrieve input stream information");
        error(return_code);
        return -1;
    }

    //4、创建输出的AVFormatContext对象
    LOGI("%s", "创建输出的AVFormatContext对象");
    avformat_alloc_output_context2(&outFormatContext, NULL, NULL, out_filename);
    if (!outFormatContext) {
        LOGE("%s", "Could not create output context\n");
        return_code = AVERROR_UNKNOWN;
        error(return_code);
        return -1;
    }
    outputFormat = outFormatContext->oformat;
    //5、根据输入流设置相应的输出流参数（不发生转码）
    LOGI("%s", "根据输入流设置相应的输出流参数（不发生转码）");
    for (i = 0; i < inFormatContext->nb_streams; i++) {
        AVStream *in_stream = inFormatContext->streams[i];
//        AVCodecParameters *inCodecParameters = in_stream->codecpar;
        AVStream *out_stream = avformat_new_stream(outFormatContext, NULL);

        if (!out_stream) {
            LOGE("%s", "Failed to create output stream\n");
            return_code = AVERROR_UNKNOWN;
            error(return_code);
            return -1;
        }

        if((return_code = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar)) < 0){
            LOGE("%s", "Failed to codecpar from input to output stream codecpar\n");
            error(return_code);
            return -1;
        }

        out_stream->codecpar->codec_tag = 0;

    }

    //6、检查输出文件是否正确配置完成
    LOGI("%s", "检查输出文件是否正确配置完成");
    if (!(outputFormat->flags & AVFMT_NOFILE)) {
        return_code = avio_open(&outFormatContext->pb, out_filename, AVIO_FLAG_WRITE);
        if (return_code < 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open output file %s", out_filename);
            error(return_code);
            return -1;
        }
    }

    //7、写入Stream头部信息
    LOGI("%s", "写入Stream头部信息");
    return_code = avformat_write_header(outFormatContext, NULL);
    if (return_code < 0) {
        LOGE("%s", "Error occurred when opening output file\n");
        error(return_code);
        return -1;
    }

    //8、定位当前位置到裁剪的起始位置 from_seconds
    LOGI("%s", "定位当前位置到裁剪的起始位置 from_seconds");
    return_code = av_seek_frame(inFormatContext, -1, (int64_t)(starttime*AV_TIME_BASE), AVSEEK_FLAG_ANY);
    if (return_code < 0) {
        LOGE("%s", "Error seek to the start\n");
        error(return_code);
        return -1;
    }

    //9、计算起始时间戳
    LOGI("%s", "计算起始时间戳");
    int64_t *dts_start_from = (int64_t *) malloc(sizeof(int64_t) * inFormatContext->nb_streams);
    memset(dts_start_from, 0, sizeof(int64_t) * inFormatContext->nb_streams);
    int64_t *pts_start_from = (int64_t *) malloc(sizeof(int64_t) * inFormatContext->nb_streams);
    memset(pts_start_from, 0, sizeof(int64_t) * inFormatContext->nb_streams);

    //10、开始写入视频信息
    LOGI("%s", "开始写入视频信息");
    while (1) {
        AVStream *in_stream, *out_stream;

        return_code = av_read_frame(inFormatContext, &packet);
        if (return_code < 0){
            break;
        }

        in_stream  = inFormatContext->streams[packet.stream_index];
        out_stream = outFormatContext->streams[packet.stream_index];
        LOGI("当前Packet索引：%d",packet.stream_index);

        log_packet(inFormatContext, &packet, "in");

        //av_q2d转换AVRational(包含分子分母的结构)类型为double,此过程有损
        if (av_q2d(in_stream->time_base) * packet.pts > endtime) {
            //当前的时间大于转换时间，则转换完成
            LOGI("%s", "到达截止时间点");
            av_packet_unref(&packet);
            break;
        }

        if (dts_start_from[packet.stream_index] == 0) {
            dts_start_from[packet.stream_index] = packet.dts;
            __android_log_print(ANDROID_LOG_INFO, TAG, "dts_start_from: %d", av_ts2str(dts_start_from[packet.stream_index]));
        }
        if (pts_start_from[packet.stream_index] == 0) {
            pts_start_from[packet.stream_index] = packet.pts;
            __android_log_print(ANDROID_LOG_INFO, TAG, "pts_start_from: %d", av_ts2str(pts_start_from[packet.stream_index]));
        }

        //拷贝数据包Packet对象(视频存储的单元)
        LOGI("%s", "拷贝数据包Packet对象(视频存储的单元)");
        packet.pts = av_rescale_q_rnd(packet.pts - pts_start_from[packet.stream_index], in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF); //AV_ROUND_PASS_MINMAX
        packet.dts = av_rescale_q_rnd(packet.dts - dts_start_from[packet.stream_index], in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
        if (packet.pts < 0) {
            packet.pts = 0;
        }
        if (packet.dts < 0) {
            packet.dts = 0;
        }
        packet.duration = (int)av_rescale_q((int64_t)packet.duration, in_stream->time_base, out_stream->time_base);
        packet.pos = -1;
        log_packet(outFormatContext, &packet, "out");

        //将当前Packet写入输出文件
        LOGI("%s", "将当前Packet写入输出文件");

        if ((return_code = av_write_frame(outFormatContext, &packet)) < 0) {
            LOGE("%s", "Error muxing packet\n");
            error(return_code);
            break;
        }
        //重置Packet对象
        av_packet_unref(&packet);
    }
    free(dts_start_from);
    free(pts_start_from);

    //11、写入stream尾部信息
    av_write_trailer(outFormatContext);

    //12、收尾：回收内存，关闭流...
    avformat_close_input(&inFormatContext);

    if (outFormatContext && !(outputFormat->flags & AVFMT_NOFILE)){
        avio_closep(&outFormatContext->pb);
    }
    avformat_free_context(outFormatContext);

    return 0;
}

/**
 * 剪切视频并转码
 * @param starttime  开始时间
 * @param endtime   结束时间
 * @param in_filename   原视频的路径(包含文件名)
 * @param out_filename  裁剪结果的存放路径(包含文件名)
 * @return  如果成功则返回0，否则返回其他
 */
int convert_and_cut(float starttime, float endtime, char *in_filename, char *out_filename) {
    AVCodecContext *inCodecContext = NULL;
    AVCodecContext *outCodecContext = NULL;

    AVFormatContext *inFormatContext = NULL;
    AVFormatContext *outFormatContext = NULL;

    //int inVideoStreamIndex = -1;
    AVStream *inVideoStream = NULL;
    int ret = 0;

    //1、读取视频文件
    av_register_all();
    if ((ret = avformat_open_input(&inFormatContext, in_filename, 0, 0)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open input file '%s'", in_filename);
        goto error;
    }

    //2、创建输出对象
    avformat_alloc_output_context2(&outFormatContext, NULL, NULL, out_filename);

    //3、根据输入流设置相应的输出流参数（不发生转码）
    LOGI("%s", "根据输入流设置相应的输出流参数（不发生转码）");
    for (int i = 0; i < inFormatContext->nb_streams; i++) {
        inVideoStream = inFormatContext->streams[i];
        AVStream *out_stream = avformat_new_stream(outFormatContext, NULL);
        if (!out_stream) {
            LOGE("%s", "Failed to create output stream\n");
            ret = AVERROR_UNKNOWN;
            goto error;
        }

        if((ret = avcodec_parameters_copy(out_stream->codecpar, inVideoStream->codecpar)) < 0){
            LOGE("%s", "Failed to codecpar from input to output stream codecpar\n");
            goto error;
        }

        out_stream->codecpar->codec_tag = 0;

    }

    AVFrame *frame;
    AVPacket inPacket, outPacket;

    if((ret = avio_open(&outFormatContext->pb, in_filename, AVIO_FLAG_WRITE)) < 0) {
        fprintf(stderr, "convert(): cannot open out file\n");
        goto error;
    }

    //4、移动到裁剪的起点
    AVRational default_timebase;
    default_timebase.num = 1;
    default_timebase.den = AV_TIME_BASE;

    // suppose you have access to the "inVideoStream" of course
    int64_t starttime_int64 = av_rescale_q((int64_t)( starttime * AV_TIME_BASE ), default_timebase, inVideoStream->time_base);
    int64_t endtime_int64 = av_rescale_q((int64_t)( endtime * AV_TIME_BASE ), default_timebase, inVideoStream->time_base);

    if(avformat_seek_file(inFormatContext, inVideoStreamIndex, INT64_MIN, starttime_int64, INT64_MAX, 0) < 0) {
        ret = AVERROR_UNKNOWN;
        goto error;
    }

    avcodec_flush_buffers( inVideoStream->codec );
    // END

    avformat_write_header(outFormatContext, NULL);
    frame = av_frame_alloc();
    av_init_packet(&inPacket);

    int frameFinished = -1;
    int outputed = -1;

    // you used avformat_seek_file() to seek CLOSE to the point you want... in order to give precision to your seek,
    // just go on reading the packets and checking the packets PTS (presentation timestamp)
    while(av_read_frame(inFormatContext, &inPacket) >= 0) {
        if(inPacket.stream_index == inVideoStreamIndex) {
            avcodec_decode_video2(inCodecContext, frame, &frameFinished, &inPacket);
            // this line guarantees you are getting what you really want.
            if(frameFinished && frame->pts >= starttime_int64 && frame->pts <= endtime_int64) {
                av_init_packet(&outPacket);
                avcodec_encode_video2(outCodecContext, &outPacket, frame, &outputed);
                if(outputed) {
                    if (av_write_frame(outFormatContext, &outPacket) != 0) {
                        fprintf(stderr, "convert(): error while writing video frame\n");
                        return 0;
                    }
                }
                av_packet_unref(&outPacket);
            }

            // exit the loop if you got the frames you want.
            if(frame->pts > endtime_int64) {
                break;
            }
        }
    }

    av_write_trailer(outFormatContext);
    av_packet_unref(&inPacket);
    goto end;
    error:
    error(ret);
    return -1;
    end:
    return 1;
}

/**
 * 无损压缩视频
 * @param in_filename   原视频路径(包含文件名)
 * @param out_filename  压缩结果存放路径(包含文件名)
 * @return  如果成功则返回0，否则返回其他
 */
int remuxing(const char *in_filename, const char *out_filename)
{
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
    av_register_all();
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open input file '%s'", in_filename);
        goto end;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to retrieve input stream information");
        goto end;
    }
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    stream_mapping_size = ifmt_ctx->nb_streams;
    stream_mapping = (int *) av_mallocz_array((size_t) stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }
        stream_mapping[i] = stream_index++;
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        if ((ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar)) < 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Failed to copy codec parameters\n");
            goto end;
        }
        out_stream->codecpar->codec_tag = 0;
    }
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open output file '%s'", out_filename);
            goto end;
        }
    }
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Error occurred when opening output file\n");
        goto end;
    }
    while (1) {
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;
        in_stream  = ifmt_ctx->streams[pkt.stream_index];
        if (pkt.stream_index >= stream_mapping_size ||
            stream_mapping[pkt.stream_index] < 0) {
            av_packet_unref(&pkt);
            continue;
        }
        pkt.stream_index = stream_mapping[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        log_packet(ifmt_ctx, &pkt, "in");
        /* copy packet */
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_PASS_MINMAX);//AV_ROUND_NEAR_INF|
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_PASS_MINMAX);
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        log_packet(ofmt_ctx, &pkt, "out");
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "Error muxing packet\n");
            break;
        }
        av_packet_unref(&pkt);
    }
    av_write_trailer(ofmt_ctx);
    end:
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt!=NULL && ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    av_freep(&stream_mapping);
    if (ret < 0 && ret != AVERROR_EOF) {
        error(ret);
        return 1;
    }
    return 0;
}

void error(int error_code)
{
    __android_log_print(ANDROID_LOG_ERROR, TAG, "Error detail: %s", av_err2str(error_code));
}

void toast(JNIEnv* env, jobject instance, const char* message)
{
    jclass jc = env->GetObjectClass(instance);
    jmethodID jmID = env->GetMethodID(jc, "toast", "(Ljava/lang/String;)V");
    jstring _message = env->NewStringUTF(message);
    env->CallVoidMethod(jc, jmID, _message);
}

