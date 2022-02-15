#pragma once

#include <QWidget>
#include <QLibrary>
#include <QPaintEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QDir>

#include <string>
#include <mutex>

#include "opencv2/opencv.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/core/mat.hpp"
#include "opencv2/video/video.hpp"

#include "CameraParamWidget.h"
#include "flowrate.h"

#include "ui_widget.h"

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
    virtual ~AsyncVideoRecorder() override;

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

private:
    inline void asyncUpdateAvaliableCameras();

    inline void updateRecordTime();

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

    QImage mDisplayImage;                           // 用来显示的图像
    QImage mRecvImage;                              // 接收到的图像
    int mImageSize;                                 // 图像尺寸

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
};
