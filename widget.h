#pragma once

#include <QWidget>
#include <QLibrary>
#include <QPaintEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>

#include <string>
#include <mutex>

#include "ui_widget.h"

#include "opencv2/opencv.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/core/mat.hpp"
#include "opencv2/video/video.hpp"

#include "CameraParamWidget.h"
#include "flowrate.h"
#include "VideoAnalyser.h"

class GenericCameraModule;

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
* 异步帧清晰度计算
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
    explicit AsyncSharpnessCalculator();

    void setImageFormat(const QImage& image, int imageSize);

    void startCalculate();

    bool calculating();

    void stopCalculate();

    void cache(const cv::Mat& mat);

protected:
    virtual void run() override;

signals:
    void signalUpdateSharpness(double);

private:
    QImage mCalculateImage;
    int mImageSize;

    bool mCalculating;

    cv::Mat mMat;
    bool mOccupied;

    QEventLoop mEventLoop;
};

/*!
* @brief
* 异步帧流动轨迹计算
*/
class AsyncFlowTrackAreaCalculator : public QThread
{
    Q_OBJECT

public:
    explicit AsyncFlowTrackAreaCalculator(QVector<QImage>& imagelist);
    virtual ~AsyncFlowTrackAreaCalculator() override;

    QVector<double>& getFlowtrackArea();

    QVector<RegionPoints>& getFlowtrackRegionPoints();

protected:
    virtual void run() override;

signals:
    void signalUpdateTrackArea(AsyncFlowTrackAreaCalculator*);

private:
    QVector<QImage> mImagelist;
    QVector<double> mFlowtrackArea;
    QVector<RegionPoints> mFlowtrackRegionPoints;

    bool mCalculating;

    QEventLoop mEventLoop;
};

class Widget : public QWidget
{
    Q_OBJECT

    using registProcessCallbackFuncFunc = void*(GenericCameraModule*, std::function<void(const _Fake_Mat&)>);
    using openCameraBySNFunc = void*(GenericCameraModule*, bool&, std::string, std::vector<CameraParamStruct>*);
    using closeCameraFunc = void*(GenericCameraModule*, bool&);
    using framerateFunc = void*(GenericCameraModule*, double&);
    using frameSizeFunc = void*(GenericCameraModule*, int&);
    using captureFunc = void*(GenericCameraModule*);

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

public slots:
    bool openCamera();

    bool closeCamera();

    void beginCapture();

    void stopCaptutre();

    void beginRecord();

    void stopRecord();

protected:
    void processOneFrame(const _Fake_Mat& m);

    virtual void paintEvent(QPaintEvent* e) override;

    virtual void keyPressEvent(QKeyEvent* e) override;

    virtual void closeEvent(QCloseEvent* e) override;

private slots:
    void slotInitOpenCamera();

    void slotAfterUpdateAvaliableCameras(int cameraCount);

    void slotRefreshFramerate();

    void slotCalcSharpness(double sharpness);

    void slotAutoRecordChecked(int status);

    void slotContinueRecordNumChanged(const QString& number);

    void slotVideoRecordDoubleClick(const QModelIndex &index);

    void slotImportDir();

    void slotCloseVideoFramePlayer(VideoFramePlayer* videoFramePlayer);

    void slotLoopCalcFlowTrack();

    void slotUpdateTrackArea(AsyncFlowTrackAreaCalculator* asyncFlowTrackAreaCalculator);

private:
    inline void asyncUpdateAvaliableCameras();

    inline void updateRecordTime();

    inline void insertOneVideoRecord();

    inline void insertOneVideoRecord(const QFileInfo& videoFileinfo);

private:
    Ui_Widget mUI;

    GenericCameraModule* mUsingCamera;              // 在用的相机
    std::string mUsingCameraSN;
    bool mIsCameraOpen;                             // 相机打开了没
    bool mIsCameraCapturing;                        // 相机有没有在采集

    QLibrary mLib;                                  // 动态库
    QFunctionPointer mCommonFuncPtr;                // 通用动态库函数指针
    QFunctionPointer mRuntimeFramerateFuncPtr;      // 获取相机采集帧率的特定函数指针

    CameraParamWidget* mCameraParamWidget;          // 相机参数设置控件

    QDateTime mUsingCameraCaptureBeginDateTime;     // 相机开始采集的日期时间
    QDateTime mUsingCameraCaptureFinishDateTime;    // 相机停止采集的日期时间
    QTime mCameraRunTime;							// 相机运行时长

    int mFrameWidth;                                // 宽
    int mFrameHeight;                               // 高
    int mPixelByteCount;                            // 一像素占用的字节数

    int mImageSize;                                 // 图像尺寸
    QImage mRecvImage;                              // 接收到的图像
    QImage mDisplayImage;                           // 用来显示的图像

    QPainter mPainter;

    cv::VideoWriter mVideoWriter;
    cv::Mat mMat;
    AsyncVideoRecorder* mVideoRecorder;

    double mConfigFramerate;                        // 相机配置的帧率
    double mRunningFramerate;                       // 相机实际采集帧率
    int mLastSecondRecvFrameCount;                  // 计算实际接收帧率 若接收帧率不等于相机采集帧率 会导致数据丢失
    int mLastSecondRecvFrameCountDisplay;

    QTimer mGetRuntimeFramerateTimer;               // 定时获取相机的实际采集帧率 1000ms
    QTimer mDisplayTimer;                           // 显示定时器 固定显示帧率为25fps 40ms

    QTime mRecordDuratiom;                          // 录制时长
    QDateTime mRecordBeginDateTime;                 // 录制开始日期时间
    QTimer mRecordTimeLimitTimer;                   // 录制时长显示定时器 60 * 60 * 1000ms 即1个钟
    QFileInfo mRecordVideoAbsFileInfo;				// 录制的视频绝对路径

    bool mIsUpdatingCameraList;                     // 是否正在更新相机列表
    bool mShowDebugMessage;                         // 显示调试信息

    AsyncSharpnessCalculator mAsyncFlowrateCalculator;   // 异步计算图像清晰度 后续拓展成异步计算流速
    double mSharpness;                              // 清晰度
    int mContinueSharpFrameCount;                   // 连续清晰的帧数
    int mContinueRecordVideoCount;                  // 连续录制的视频数

    QTimer mAutoRecordTimeLimitTimer;               // 智能录制时长显示定时器 1000ms

    QList<VideoFramePlayer*> mVideoFramePlayerList; // 图片查看控件列表

    int mFlowTrackCalculatingNumer;                 // 正在计算流速的线程数量
    QTimer mLoopCalcFlowTrackTimer;                 // 循环检查流速计算的定时去 1000ms
    QMap<AsyncFlowTrackAreaCalculator*, QTableWidgetItem*> mMapFlowTrackThreadToFlowrateItem;   // 计算流速的线程 对应到 流速显示控件
    QMap<int, QVector<double>> mMapVideoRecordRowToFlowtrackArea;                               // 视频记录行号 对应到 流动轨迹面积
    QMap<int, QVector<RegionPoints>> mMapVideoRecordRowToFlowtrackRegionPoints;                 // 视频记录行号 对应到 流动轨迹坐标
};
