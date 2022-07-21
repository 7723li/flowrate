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

/*!
 * @brief 图像参数质量
 */
enum ImageParamQuality
{
    High = 0,
    Middle = 1,
    Low = 2
};

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
 * 自动录制线程
 *
 * @attention
 * 由于自动录制的帧数有限 且存在连续并行录制的情况 所以设计成临时线程
 */
class AsyncVideoAutoRecorder : public QThread
{
public:
    explicit AsyncVideoAutoRecorder(cv::VideoWriter& videoWriter);

    virtual ~AsyncVideoAutoRecorder() override;

    void startRecord(QList<cv::Mat>& autorecordCacheQueue);

    bool recording();

protected:
    virtual void run() override;

private:
    cv::VideoWriter& mVideoWriter;

    QList<cv::Mat> mCacheQueue;

    bool mRecording;
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

    struct lock
    {
        lock(){ _mutex.lock(); }
        ~lock(){ _mutex.unlock(); }
        std::mutex _mutex;
    };

public:
    explicit AsyncSharpnessCalculator(QObject* p = nullptr);
    virtual ~AsyncSharpnessCalculator() override;

    /*!
     * @attention
     * 不要迷惑为什么要多出个imageSize参数 因为不是图像面积的意思 要考虑位深
     */
    void setImageFormat(int width, int height, int imageSize, QImage::Format format);

    void cache(const cv::Mat& mat);

    void begin();

    void stop();

protected:
    virtual void run() override;

signals:
    void signalCalcSharpnessFinish(cv::Mat calcMat, ImageParamQuality sharpnessQuality);

private:
    bool mRunning;
    QEventLoop mEventLoop;

    int mImageSize;
    QImage mCalcImage;

    std::vector<cv::Mat> mCacheQueue;
    std::vector<bool> mOccupied;
    int mCacheIndex;
    int mWriteIndex;
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
    void signalReanalysisFinished(AsyncDataReanalyser*);

private:
    QVector<QImage> mImagelist;
    double mFps;
    double mPixelSize;
    int mMagnification;
    QVector<int> mErasedOriVesselIndex;
    VesselInfo mOriVesselInfo;
    VesselInfo mNewVesselInfo;
};
