#pragma once

#include <QWidget>
#include <QVector>
#include <QImage>
#include <QDebug>
#include <QPaintEvent>
#include <QPainter>
#include <QCloseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QPainterPath>
#include <QTime>
#include <QDir>
#include <QFileInfo>

#include "UI_VideoAnalyser.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/pixfmt.h"
}

#include "flowrate.h"

class VideoAnalysier
{
public:
    VideoAnalysier(const QString& videopath, QVector<QImage>& imagelist, int analysisFrameCount = 100);

private:
    bool init(const QString& videopath);
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
    double mTimeBase;                       // 用于换算成 -> 秒 <- 数的时间基
};

class VideoFramePlayer : public QWidget
{
    Q_OBJECT

public:
    explicit VideoFramePlayer(int inRow, QWidget* p = nullptr);
    virtual ~VideoFramePlayer() override;

    int inRow();

    void setImageList(QVector<QImage>& imagelist);

    void updateFlowTrackPoints(const QVector<RegionPoints>& regionPoints);

    void updateFlowTrackAreas(const QVector<double>& flowTrackAreas);

protected:
    virtual void paintEvent(QPaintEvent* e) override;

    virtual void closeEvent(QCloseEvent* e) override;

    virtual void keyPressEvent(QKeyEvent* e) override;

    virtual void wheelEvent(QWheelEvent* e) override;

    virtual void resizeEvent(QResizeEvent* e) override;

private slots:
    void slotPrevFrame();

    void slotNextFrame();

    void slotPaintBoundaryVesselRegion();

    void slotPaintSplitVesselRegion();

    void slotPaintFlowTrackRegion();

    void slotSaveImage();

    void slotExportImageList();

    void slotRedraw();

signals:
    void signalExit(VideoFramePlayer*);

private:
    Ui_VideoAnalyser mUI;

    int mInRow;

    QVector<QImage> mImageList;

    QImage mCurrentShowImage;               // 当前显示的图像
    int mTotalFrameCount;                   // 总帧数
    int mCurrentFrameIdnex;                 // 当前帧下标

    RegionPoints mRegionBoundaryPoints;             // 单张图片的血管外边缘区域的坐标
    QVector<RegionPoints> mRegionBranchPoints;      // 单张图片的血管分支区域的坐标
    QVector<RegionPoints> mRegionNodesPoints;       // 单张图片的血管结点区域的坐标
    QVector<RegionPoints> mRegionFlowTrackPoints;   // 全部图片的流动轨迹的坐标
    QVector<double> mFlowTrackAreas;                // 全部图片的流动轨迹面积

    bool mIsShowingTracks;                  // 是否正在显示某个区域
};
