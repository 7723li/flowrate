#include "AcquireVideoInfo.h"

AcquireVideoInfo::AcquireVideoInfo(const QString &videopath, QVector<QImage>* imagelist, int* duration, double* fps, int analysisFrameCount) :
    mAVFormatContext(nullptr), mAVCodecContext(nullptr), mAVStream(nullptr),
    mVideoCodec(nullptr), mOrifmtFrame(nullptr), mSwsfmtFrame(nullptr),
    mImgConvertCtx(nullptr), mFrameBuffer(nullptr), mReadPackct(nullptr),
    mVideostreamIdx(-1), mFps(0.0), mTimeBase(0)
{
    if(init(videopath))
    {
        if(imagelist)
        {
            *imagelist = QVector<QImage>();
            int framecount = 0;
            QImage image(mAVCodecContext->width, mAVCodecContext->height, QImage::Format::Format_Grayscale8);
            while(av_read_frame(mAVFormatContext, mReadPackct) >= 0 && framecount < analysisFrameCount)
            {
                if(mReadPackct->stream_index != mVideostreamIdx)
                {
                    av_packet_unref(mReadPackct);
                    continue;
                }

                // 解码一帧
                int get_picture = 0;
                int ret = avcodec_decode_video2(mAVCodecContext, mOrifmtFrame, &get_picture, mReadPackct);
                // 如果 解码成功(ret > 0) 并且 该帧为关键帧(get_picture > 0) 才去转码
                if (ret > 0 && get_picture > 0)
                {
                    // 转码一帧格式
                    sws_scale(mImgConvertCtx, mOrifmtFrame->data, mOrifmtFrame->linesize, 0, mAVCodecContext->height, mSwsfmtFrame->data, mSwsfmtFrame->linesize);
                }

                memcpy(image.bits(), mFrameBuffer, mAVCodecContext->width * mAVCodecContext->height);
                if(imagelist)
                {
                    (*imagelist).push_back(image);
                }

                av_packet_unref(mReadPackct);

                ++framecount;
            }
        }

        if(duration)
        {
            *duration = mAVFormatContext->duration / AV_TIME_BASE;
        }

        if(fps)
        {
            *fps = mFps;
        }
    }

    release();
}

bool AcquireVideoInfo::init(const QString &videopath)
{
    av_register_all();	// 初始化ffmpeg环境

    // 打开一个格式内容 并右键查看"视频属性"
    mAVFormatContext = avformat_alloc_context();
    int open_format = avformat_open_input(&mAVFormatContext, videopath.toStdString().c_str(), nullptr, nullptr);
    if (open_format < 0)
    {
        return false;
    }

    // 根据 格式内容 寻找视频流、音频流之类的信息(TopV项目的视频一般都只有视频流)
    if (avformat_find_stream_info(mAVFormatContext, nullptr) < 0)
    {
        return false;
    }

    // 寻找视频流的位置
    mVideostreamIdx = -1;
    for (int i = 0; i < mAVFormatContext->nb_streams; ++i)
    {
        if (mAVFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            mVideostreamIdx = i;
            break;
        }
    }
    if (-1 == mVideostreamIdx)
    {
        return false;
    }

    // 找到视频流
    mAVStream = mAVFormatContext->streams[mVideostreamIdx];
    if (nullptr == mAVStream)
    {
        return false;
    }

    mFps = av_q2d(mAVStream->r_frame_rate);

    // 计算时间基
    mTimeBase = av_q2d(mAVStream->time_base);

    // 找到视频流的解码器内容
    mAVCodecContext = mAVStream->codec;
    if (nullptr == mAVCodecContext)
    {
        return false;
    }
    else if (mAVCodecContext->pix_fmt == AV_PIX_FMT_NONE)
    {
        return false;
    }

    // ffmpeg提供了一系列的解码器 这里就是根据id找要用的那个解码器 比如H264
    mVideoCodec = avcodec_find_decoder(mAVCodecContext->codec_id);
    if (nullptr == mVideoCodec)
    {
        return false;
    }

    // 打开解码器
    if (avcodec_open2(mAVCodecContext, mVideoCodec, nullptr) < 0)
    {
        return false;
    }

    // 申请帧结构
    mOrifmtFrame = av_frame_alloc();
    if (nullptr == mOrifmtFrame)
    {
        return false;
    }
    mSwsfmtFrame = av_frame_alloc();
    if (nullptr == mSwsfmtFrame)
    {
        return false;
    }

    // 图像格式转码器 YUV420 -> RGB24
    mImgConvertCtx = sws_getContext(
        mAVCodecContext->width,
        mAVCodecContext->height,
        mAVCodecContext->pix_fmt,
        mAVCodecContext->width,
        mAVCodecContext->height,
        AV_PIX_FMT_GRAY8,
        SWS_BICUBIC,
        NULL,
        NULL,
        NULL
        );
    if (nullptr == mImgConvertCtx)
    {
        return false;
    }

    // 根据转存格式计算 一帧需要的字节数 取决于 格式、图像大小
    int num_bytes = avpicture_get_size(
        AV_PIX_FMT_GRAY8,
        mAVCodecContext->width,
        mAVCodecContext->height);
    if (num_bytes <= 0)
    {
        return false;
    }

    // 根据一帧需要的字节数 给帧申请一个缓冲区 来装载一帧的内容
    mFrameBuffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
    if (nullptr == mFrameBuffer)
    {
        return false;
    }

    int fill_succ = avpicture_fill((AVPicture*)mSwsfmtFrame, mFrameBuffer, AV_PIX_FMT_GRAY8,
        mAVCodecContext->width, mAVCodecContext->height);
    if (fill_succ < 0)
    {
        return false;
    }

    // 图像面积
    int frame_area = mAVCodecContext->width * mAVCodecContext->height;

    // 分配一个数据包 读取一(视频)帧之后 用来存(视频)帧的数据
    mReadPackct = av_packet_alloc();
    if (av_new_packet(mReadPackct, frame_area) < 0)
    {
        return false;
    }

    return true;
}

void AcquireVideoInfo::release()
{
    if (nullptr != mFrameBuffer)
    {
        av_free(mFrameBuffer);					// 清空缓冲区
        mFrameBuffer = nullptr;
    }

    if (nullptr != mOrifmtFrame)
    {
        av_free(mOrifmtFrame);					// 清空原始帧格式
        mOrifmtFrame = nullptr;
    }

    if (nullptr != mSwsfmtFrame)
    {
        av_free(mSwsfmtFrame);					// 清空转换帧格式
        mSwsfmtFrame = nullptr;
    }

    if (nullptr != mReadPackct)
    {
        av_free_packet(mReadPackct);				// 释放读取包
        mReadPackct = nullptr;
    }

    if (nullptr != mAVCodecContext)
    {
        avcodec_close(mAVCodecContext);		// 关闭解码器
        mAVCodecContext = nullptr;
    }

    if (nullptr != mAVFormatContext)
    {
        avformat_close_input(&mAVFormatContext);	// 关闭视频属性
        mAVFormatContext = nullptr;
    }

    mVideoCodec = nullptr;
    mImgConvertCtx = nullptr;
}
