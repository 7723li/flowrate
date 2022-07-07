#pragma once

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/pixfmt.h"
}

#include <QString>
#include <QVector>
#include <QImage>

/*!
 * @brief
 * 基于ffpmeg的视频文件解析类
 */
class AcquireVideoInfo
{
public:
    /*!
     * @brief AcquireVideoInfo
     * 仅开放构造函数模拟接口 可确保数据封装的独立与安全
     *
     * @param videopath             输入的视频路径
     * @param imagelist             输出的解析图形序列 若输入空指针 则不会解析图形序列
     * @param duration              输出的视频时长 若输入空指针 同上
     * @param fps                   输出的视频fps 若输入空指针 同上
     * @param analysisFrameCount    输入的需要解析的图像序列大小 默认为100
     */
    AcquireVideoInfo(const QString& videopath, QVector<QImage>* imagelist = nullptr, int* duration = nullptr, double* fps = nullptr, int* width = nullptr, int* height = nullptr, int analysisFrameCount = 100);

private:
    /*!
     * @brief
     * 解析视频信息 若成功 返回true 否则返回false
     */
    bool init(const QString& videopath);
    /*!
     * @brief
     * 释放资源 无论解析成功还是失败 都进行过资源申请 因此都必须一次释放操作
     */
    void release();

private:
    AVFormatContext * mAVFormatContext;     // 视频属性
    AVCodecContext * mAVCodecContext;       // 解码器属性
    AVStream *mAVStream;                    // 视频流
    AVCodec* mVideoCodec;                   // 视频解码器
    AVFrame* mOrifmtFrame;                  // 用于从视频读取的原始帧结构
    AVFrame* mSwsfmtFrame;                  // 用于将 原始帧结构 转换成显示格式的 帧结构
    SwsContext* mImgConvertCtx;             // 用于转换图像格式的转换器

    uint8_t* mFrameBuffer;                  //  帧缓冲区
    AVPacket* mReadPackct;                  //  用于读取帧的数据包

    int mVideostreamIdx;                    // 视频流位置
    double mFps;
    double mTimeBase;                       // 用于换算成 -> 秒 <- 数的时间基
};
