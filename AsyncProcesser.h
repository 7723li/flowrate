#pragma once

#include <QThread>
#include <QEventLoop>
#include <QImage>
#include <QTimer>
#include <QDebug>
#include <QDateTime>

#include <mutex>

#include "opencv2/opencv.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/core/mat.hpp"
#include "opencv2/video/video.hpp"

#include "VesselAlgorithm.h"

typedef struct _Fake_Mat
{
    _Fake_Mat() :
        w(0), h(0), pixelByteCount(1), data(nullptr)
    {

    }

    int w;                  // 宽度
    int h;                  // 高度
    int pixelByteCount;     // 位深
    unsigned char* data;    // 数据
}_Fake_Mat;

using RegionPoints = QVector<QPoint>;

/*!
* @brief
* 异步帧写入
*/
class AsyncVideoRecorder : public QThread
{
    struct lock
    {
        lock(){ _mutex.lock(); }
        ~lock(){ _mutex.unlock(); }
        std::mutex _mutex;
    };

public:
    explicit AsyncVideoRecorder(cv::VideoWriter& videoWriter);

    void startRecord();

    bool recording();

    void stopRecord();

    void cache(const cv::Mat& mat);

protected:
    virtual void run() override;

private:
    bool mRecoring;

    cv::VideoWriter& mVideoWriter;

    std::vector<cv::Mat> mCacheQueue;
    std::vector<bool> mOccupied;
    int mCacheIndex;
    int mWriteIndex;

    QEventLoop mEventLoop;
};

/*!
* @brief
* 异步帧流动轨迹计算
*/
class AsyncDataAnalyser : public QThread
{
    Q_OBJECT

public:
    explicit AsyncDataAnalyser(QVector<QImage>& imagelist, QString absVideoPath, double fps, double pixelSize, int magnification);
    virtual ~AsyncDataAnalyser() override;

    VesselInfo& getVesselInfo();

    int getFirstSharpImageIndex();

protected:
    virtual void run() override;

signals:
    void signalUpdateTrackArea(AsyncDataAnalyser*);

private:
    QVector<QImage> mImagelist;
    QString mAbsVideoPath;
    double mFps;
    double mPixelSize;
    int mMagnification;

    VesselInfo mVesselInfo;
    int mFirstSharpImageIndex;

    bool mCalculating;

    QEventLoop mEventLoop;
};

/*!
 * @brief
 * 异步清晰度计算线程
 */
class AsyncSharpnessCalculator : public QThread
{
    Q_OBJECT

public:
    explicit AsyncSharpnessCalculator(QObject* p = nullptr);
    virtual ~AsyncSharpnessCalculator() override;

    void cache(QImage& image);

protected:
    virtual void run() override;

signals:
    void signalCalcSharpnessFinish(double sharpness, bool isSharp);

private:
    QImage mImage;
};

/*!
 * @brief
 * 数据重新计算线程
 */
class AsyncDataReanalyser : public QThread
{
    Q_OBJECT

public:
    explicit AsyncDataReanalyser(const QVector<QImage>& imagelist, double fps, double pixelSize, int magnification, const QVector<int>& erasedOriVesselIndex, const VesselInfo &oriVesselInfo);
    virtual ~AsyncDataReanalyser() override;

    VesselInfo& getNewVesselInfo();

protected:
    virtual void run() override;

signals:
    void signalReanalysisFinished();

private:
    const QVector<QImage>& mImagelist;
    double mFps;
    double mPixelSize;
    int mMagnification;
    const QVector<int>& mErasedOriVesselIndex;
    const VesselInfo& mOriVesselInfo;
    VesselInfo mNewVesselInfo;
};
