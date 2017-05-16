#include "native-lib.h"

//Java调用剪切
JNIEXPORT void JNICALL
Java_com_wzjing_videoclipper_MainActivity_clipVideo(JNIEnv *env, jobject instance,
                                                    jdouble startTime, jdouble endTime,
                                                    jstring inFileName_, jstring outFileName_) {

    const char *inFileName = env->GetStringUTFChars(inFileName_, 0);
    const char *outFileName = env->GetStringUTFChars(outFileName_, 0);

    int result = cut_video((float) startTime, (float) endTime, inFileName, outFileName);
    if (result == 0) {
        toast(env, "Clip video finished");
    } else {
        error(result);
        toast(env, "Clip video failed");
    }
    env->ReleaseStringUTFChars(inFileName_, inFileName);
    env->ReleaseStringUTFChars(outFileName_, outFileName);
}

//Java调用转码
JNIEXPORT void JNICALL
Java_com_wzjing_videoclipper_MainActivity_remuxVideo(JNIEnv *env, jobject instance,
                                                     jstring inFileName_, jstring outFileName_) {
    const char *inFileName = env->GetStringUTFChars(inFileName_, 0);
    const char *outFileName = env->GetStringUTFChars(outFileName_, 0);

    start_time = clock();


    if (remuxing(inFileName, outFileName) == 0) {
        toast(env, "Remux video finished");
    } else {
        toast(env, "Remux video failed");
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
static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag) {
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "%s: pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
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
int cut_video(double starttime, double endtime, const char *in_filename, const char *out_filename) {
    LOGI("开始执行裁剪");
    AVOutputFormat *outputFormat = NULL;
    AVFormatContext *inFormatContext = NULL, *outFormatContext = NULL;
    AVPacket packet;
    int ret;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;

    //1、注册所有组件
    LOGI("1、注册所有组件");
    av_register_all();

    //2、开始读取输入视频文件
    LOGI("2、开始读取输入视频文件");
    if ((ret = avformat_open_input(&inFormatContext, in_filename, 0, 0)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open input file %s", in_filename);
        return ret;
    }

    //3、获取视频流媒体信息
    LOGI("3、获取视频流媒体信息");
    if ((ret = avformat_find_stream_info(inFormatContext, 0)) < 0) {
        LOGE("\tFailed to retrieve input stream information");
        return ret;
    }

    __android_log_print(ANDROID_LOG_INFO, TAG,
                        "bit_rate:%lld\n"
                                "duration:%lld\n"
                                "nb_streams:%d\n"
                                "fps_prob_size:%d\n"
                                "format_probesize:%d\n"
                                "max_picture_buffer:%d\n"
                                "max_chunk_size:%d\n"
                                "format_name:%s",
                        inFormatContext->bit_rate,
                        inFormatContext->duration,
                        inFormatContext->nb_streams,
                        inFormatContext->fps_probe_size,
                        inFormatContext->format_probesize,
                        inFormatContext->max_picture_buffer,
                        inFormatContext->max_chunk_size,
                        inFormatContext->iformat->name);

    //4、创建输出的AVFormatContext对象
    LOGI("4、创建输出的AVFormatContext对象");
    avformat_alloc_output_context2(&outFormatContext, NULL, NULL, out_filename);
    if (!outFormatContext) {
        LOGE("\tCould not create output context\n");
        ret = AVERROR_UNKNOWN;
        return ret;
    }

    //设置stream_mapping
    LOGI("5、设置stream_mapping");
    stream_mapping_size = inFormatContext->nb_streams;
    stream_mapping = (int*)av_mallocz_array((size_t)stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping){
        ret=AVERROR(ENOMEM);
        LOGE("Error while set stream_mapping");
        return ret;
    }

    outputFormat = outFormatContext->oformat;
    //6、根据输入流设置相应的输出流参数
    LOGI("6、根据输入流设置相应的输出流参数");
    for (int i = 0; i < inFormatContext->nb_streams; i++) {
        AVStream *in_stream = inFormatContext->streams[i];
        AVStream *out_stream = avformat_new_stream(outFormatContext, NULL);
        AVCodecParameters *in_codecpar = in_stream->codecpar;
        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE){
            stream_mapping[i] = -1;
            continue;
        }
        stream_mapping[i] = stream_index++;

        if (!out_stream) {
            LOGE("\tFailed to create output stream");
            ret = AVERROR_UNKNOWN;
            return ret;
        }

        if ((ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar)) < 0) {
            LOGE("\tFailed to copy codec parameters");
            return ret;
        }

        out_stream->codecpar->codec_tag = 0;

    }


    //7、检查输出文件是否正确配置完成
    LOGI("7、检查输出文件是否正确配置完成");
    if (!(outputFormat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outFormatContext->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("\tCould not open output file %s", out_filename);
            return ret;
        }
    }

    //8、写入Stream头部信息
    LOGI("8、写入Stream头部信息");
    ret = avformat_write_header(outFormatContext, NULL);
    if (ret < 0) {
        LOGE("\tError occurred while write header");
        return ret;
    }



    //9、设置dts和pts变量的内存
    LOGI("9、设置dts和pts变量的内存");
    int64_t *dts_start_from = (int64_t *) malloc(sizeof(int64_t) * inFormatContext->nb_streams);
    memset(dts_start_from, 0, sizeof(int64_t) * inFormatContext->nb_streams);
    int64_t *pts_start_from = (int64_t *) malloc(sizeof(int64_t) * inFormatContext->nb_streams);
    memset(pts_start_from, 0, sizeof(int64_t) * inFormatContext->nb_streams);

    //10、定位当前位置到裁剪的起始位置 from_seconds
    LOGI("10、定位当前位置到裁剪的起始位置:%lld, stream: %d, ", (int64_t) (starttime * AV_TIME_BASE), stream_index);

    ret = av_seek_frame(inFormatContext, -1, (int64_t) (starttime * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LOGE("\tError seek to the start");
        return ret;
    }

    //11、开始写入视频信息
    LOGI("11、开始写入视频信息");
    int k = 0;
    while (1) {
        k++;
        LOGD("<1> -----------------------------< Loop：%d >-------------------------------", k);
        AVStream *in_stream, *out_stream;

        LOGD("<2> Read frame");
        ret = av_read_frame(inFormatContext, &packet);
        if (ret < 0) {
            break;
        }


        LOGI("\tPacket stream_index：%d", packet.stream_index);
        in_stream = inFormatContext->streams[packet.stream_index];
        if (packet.stream_index >= stream_mapping_size ||
            stream_mapping[packet.stream_index] < 0) {
            LOGE("reach end");
            av_packet_unref(&packet);
            continue;
        }

        //convert coding
//        avcodec_send_frame(in_stream->codec, frame);
//        AVCodecContext *outCodecContext = avcodec_alloc_context3(in_stream->codec->codec);
//        avcodec_parameters_to_context(outCodecContext, in_stream->codecpar);
//        outCodecContext->bit_rate = 40000;
//        avcodec_receive_packet(outCodecContext, &packet);

        packet.stream_index= stream_mapping[packet.stream_index];

        out_stream = outFormatContext->streams[packet.stream_index];

        av_dict_copy(&(out_stream->metadata), in_stream->metadata, AV_DICT_IGNORE_SUFFIX);

        LOGI("\tin_steam bit_rate:%lld", in_stream->codecpar->bit_rate);
        LOGI("\tin_steam bits_codec_sample:%d", in_stream->codecpar->bits_per_coded_sample);
        LOGI("\tin_steam bits_per_raw_sample:%d", in_stream->codecpar->bits_per_raw_sample);


//        log_packet(inFormatContext, &packet, "in");

        //av_q2d转换AVRational(包含分子分母的结构)类型为double,此过程有损
        LOGI("\tin_stream time_base: %d/%d", in_stream->time_base.num, in_stream->time_base.den);
        if (av_q2d(in_stream->time_base) * packet.pts > endtime) {
            LOGI("Reach End");
            av_packet_unref(&packet);
            break;
        }

        if (dts_start_from[packet.stream_index] == 0) {
            dts_start_from[packet.stream_index] = packet.dts;
            LOGI("\tdts_start_from: %lld", dts_start_from[packet.stream_index]);
        }

        if (pts_start_from[packet.stream_index] == 0) {
            pts_start_from[packet.stream_index] = packet.pts;
            LOGI("\tpts_start_from: %lld", pts_start_from[packet.stream_index]);
        }

        //判断dts和pts的关系，如果 dts < pts 那么当调用 av_write_frame() 时会导致 Invalid Argument
        if (dts_start_from[packet.stream_index] < pts_start_from[packet.stream_index]){
            LOGW("pts is smaller than dts, resting pts to equal dts");
            pts_start_from[packet.stream_index] = dts_start_from[packet.stream_index];
        }

        LOGD("<3> Packet timestamp");
        packet.pts = av_rescale_q_rnd(packet.pts - pts_start_from[packet.stream_index],
                                      in_stream->time_base, out_stream->time_base,
                                      AV_ROUND_INF );
        packet.dts = av_rescale_q_rnd(packet.dts - dts_start_from[packet.stream_index],
                                      in_stream->time_base, out_stream->time_base,
                                      AV_ROUND_ZERO);
        if (packet.pts < 0) {
            packet.pts = 0;
        }
        if (packet.dts < 0) {
            packet.dts = 0;
        }
        LOGI("PTS:%lld\tDTS:%lld", packet.dts, packet.dts);
        packet.duration = av_rescale_q((int64_t) packet.duration, in_stream->time_base,
                                       out_stream->time_base);
        packet.pos = -1;

        //将当前Packet写入输出文件
        LOGD("<4> Write Packet");
        LOGI("\tAVFormatContext State:%d", outFormatContext != NULL);
        LOGI("\tPacket State:%d", &packet != NULL);
        if ((ret = av_interleaved_write_frame(outFormatContext, &packet)) < 0) {
            LOGE("\tError write packet\n");
            return ret;
        }
        //重置Packet对象
        LOGD("<5> Unref Packet");
        av_packet_unref(&packet);
    }
    free(dts_start_from);
    free(pts_start_from);

    //12、写入stream尾部信息
    LOGD("12、写入stream尾部信息");
    av_write_trailer(outFormatContext);

    //13、收尾：回收内存，关闭流...
    LOGD("13、收尾：回收内存，关闭流...");
    avformat_close_input(&inFormatContext);

    if (outFormatContext && !(outputFormat->flags & AVFMT_NOFILE)) {
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
int convert_and_cut(float starttime, float endtime, const char *in_filename, const char *out_filename) {
    AVCodecContext *inCodecContext = NULL;
    AVCodecContext *outCodecContext = NULL;

    AVFormatContext *inFormatContext = NULL;
    AVFormatContext *outFormatContext = NULL;

    //int inVideoStreamIndex = -1;
    AVStream *inVideoStream = NULL;
    int ret = 0;

    //1、读取视频文件
    LOGI("%s", "读取视频文件");
    av_register_all();
    if ((ret = avformat_open_input(&inFormatContext, in_filename, 0, 0)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, TAG, "Could not open input file '%s'", in_filename);
        return ret;
    }

    //2、创建输出对象
    LOGI("%s", "创建输出对象");
    avformat_alloc_output_context2(&outFormatContext, NULL, NULL, out_filename);

    //3、根据输入流设置相应的输出流参数
    LOGI("%s", "根据输入流设置相应的输出流参数");
    for (int i = 0; i < inFormatContext->nb_streams; i++) {
        inVideoStream = inFormatContext->streams[i];
        AVStream *out_stream = avformat_new_stream(outFormatContext, NULL);
        if (!out_stream) {
            LOGE("%s", "Failed to create output stream\n");
            ret = AVERROR_UNKNOWN;
            return ret;
        }

        if((ret = avcodec_copy_context(out_stream->codec, inVideoStream->codec)) < 0){
            LOGE("%s", "Failed to codecpar from input to output stream codecpar\n");
            return ret;
        }

        out_stream->codecpar->codec_tag = 0;

    }

    AVFrame *frame;
    AVPacket inPacket, outPacket;

    //5、打开输出文件
    LOGI("%s", "打开输出文件");
    if((ret = avio_open(&outFormatContext->pb, out_filename, AVIO_FLAG_WRITE)) < 0) {
        fprintf(stderr, "convert(): cannot open out file\n");
        return ret;
    }

    //6、获取视频流索引
    LOGI("%s", "获取视频流索引");
    int inVideoStreamIndex;
    for (int i = 0; i < inFormatContext->nb_streams; i++) {
        if (inFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            inVideoStreamIndex = i;
            break;
        }
    }


    AVRational default_timebase;
    default_timebase.num = 1;
    default_timebase.den = AV_TIME_BASE;

    // 7、设置起始点时间
    LOGI("%s", "设置起始点时间");
    int64_t starttime_int64 = av_rescale_q((int64_t)( starttime * AV_TIME_BASE ), default_timebase, inVideoStream->time_base);
    int64_t endtime_int64 = av_rescale_q((int64_t)( endtime * AV_TIME_BASE ), default_timebase, inVideoStream->time_base);

    //8、移动到裁剪的起点
    LOGI("%s", "移动到裁剪的起点");
    if(avformat_seek_file(inFormatContext, inVideoStreamIndex, INT64_MIN, starttime_int64, INT64_MAX, 0) < 0) {
        ret = AVERROR_UNKNOWN;
        return ret;
    }

    //LOGI("%s", "avcodec_flush_buffers");
    //avcodec_flush_buffers( inVideoStream->codec );
    // END

    LOGI("%s", "avformat_write_header");
    avformat_write_header(outFormatContext, NULL);
    frame = av_frame_alloc();
    av_init_packet(&inPacket);

    int frameFinished;
    int outputed;

    //9、开始进行写入
    LOGI("%s", "开始进行写入");
    // you used avformat_seek_file() to seek CLOSE to the point you want... in order to give precision to your seek,
    // just go on reading the packets and checking the packets PTS (presentation timestamp)
    int k = 0;
    while(av_read_frame(inFormatContext, &inPacket) >= 0) {
        k++;
        LOGE("循环:%d", k);
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

    return 1;
}

/**
 * 无损压缩视频
 * @param in_filename   原视频路径(包含文件名)
 * @param out_filename  压缩结果存放路径(包含文件名)
 * @return  如果成功则返回0，否则返回其他
 */
int remuxing(const char *in_filename, const char *out_filename) {
    LOGI("1、开始无损提取");
    AVOutputFormat *ofmt = NULL;
    AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int stream_index = 0;
    int *stream_mapping = NULL;
    int stream_mapping_size = 0;
    LOGI("2、初始化所有组件");
    av_register_all();
    LOGI("3、读取输入文件");
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        LOGE("Could not open input file %s", in_filename);
        return ret;
    }
    LOGI("4、获取输入流信息");
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        LOGE("Failed to retrieve input stream information");
        return ret;
    }
    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    LOGI("5、创建输出AVFormatContext");
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        LOGE("Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        return ret;
    }
    LOGI("6、设置stream_mapping");
    stream_mapping_size = ifmt_ctx->nb_streams;
    stream_mapping = (int *) av_mallocz_array((size_t) stream_mapping_size,
                                              sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        LOGE("Error set stream_mapping");
        return ret;
    }
    LOGI("7、开始循环读取流");
    ofmt = ofmt_ctx->oformat;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        LOGI("(1)、-----------------<流编号：%d>-----------------", i);
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
        LOGI("(2)、创建新的输出流");
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            LOGE("Failed allocating output stream");
            ret = AVERROR_UNKNOWN;
            return ret;
        }
        if ((ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar)) < 0) {
            LOGE("Failed to copy codec parameters\n");
            return ret;
        }
        out_stream->codecpar->codec_tag = 0;
    }
    av_dump_format(ofmt_ctx, 0, out_filename, 1);
    LOGI("8、检测输出文件配置");
    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("Could not open output file %s", out_filename);
            return ret;
        }
    }
    LOGI("9、写入文件头信息");
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Error occurred when opening output file");
        return ret;
    }
    int k = 0;
    LOGI("10、开始写入到输出流");
    while (1) {
        k++;
        LOGE("(1) -----------------------<循环：%d>--------------------", k);
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx, &pkt);
        if (ret < 0)
            break;
        in_stream = ifmt_ctx->streams[pkt.stream_index];
        if (pkt.stream_index >= stream_mapping_size ||
            stream_mapping[pkt.stream_index] < 0) {
            LOGE("reach end");
            av_packet_unref(&pkt);
            continue;
        }
        pkt.stream_index = stream_mapping[pkt.stream_index];
        out_stream = ofmt_ctx->streams[pkt.stream_index];
        log_packet(ifmt_ctx, &pkt, "in");
        /* copy packet */
        LOGE("(2) 设置timestamp");
        pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base,
                                   AV_ROUND_PASS_MINMAX);//AV_ROUND_NEAR_INF|
        pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base,
                                   AV_ROUND_PASS_MINMAX);
        pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
        pkt.pos = -1;
        log_packet(ofmt_ctx, &pkt, "out");
        LOGE("(3) 写入frame");
        ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
        if (ret < 0) {
            LOGE("Error muxing packet");
            break;
        }
        av_packet_unref(&pkt);
    }
    LOGE("11、写入文件尾部信息");
    av_write_trailer(ofmt_ctx);
    LOGE("12、收尾：回收内存，关闭流...");
    avformat_close_input(&ifmt_ctx);
    /* close output */
    if (ofmt != NULL && ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    av_freep(&stream_mapping);
    if (ret < 0 && ret != AVERROR_EOF) {
        error(ret);
        return 1;
    }
    return 0;
}

void error(int error_code) {
    LOGE("Error detail: %s", av_err2str(error_code));
}

void toast(JNIEnv *env, const char *message) {

    LOGI("%s, total use time: %ld", message, clock() - start_time);

//    jclass jc = (*env).FindClass("com/wzjing/videoclipper/MainActivity");
//
//    jmethodID jmID = (*env).GetMethodID(jc, "toast", "(Ljava/lang/String;)V");
//    if (jmID == 0){
//        __android_log_print(ANDROID_LOG_ERROR, TAG, "Can not find java method");
//        return;
//    }
//    (*env).CallVoidMethod(jc, jmID, (*env).NewStringUTF(message));

}

void dump_meta(AVDictionary *m, const char *indent)
{
    if (m && !(av_dict_count(m) == 1 && av_dict_get(m, "language", NULL, 0))) {
        AVDictionaryEntry *tag = NULL;

        LOGE("%sMetadata:\n", indent);
        while ((tag = av_dict_get(m, "", tag, AV_DICT_IGNORE_SUFFIX)))
            if (strcmp("language", tag->key)) {
                const char *p = tag->value;
                LOGE("%s  %-16s: ", indent, tag->key);
                while (*p) {
                    char tmp[256];
                    size_t len = strcspn(p, "\x8\xa\xb\xc\xd");

                    av_strlcpy(tmp, p, FFMIN(sizeof(tmp), len+1));
                    LOGE("%s", tmp);
                    p += len;
                    if (*p == 0xd) LOGE(" ");
                    if (*p == 0xa) LOGE("\n%s  %-16s: ", indent, "");
                    if (*p) p++;
                }
                LOGE("\n");
            }
    }
}

