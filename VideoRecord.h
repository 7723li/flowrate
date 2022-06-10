#pragma once

#include <QWidget>
#include <QLibrary>
#include <QPaintEvent>
#include <QPainter>
#include <QKeyEvent>
#include <QDir>
#include <QFileInfo>

#include "ui_VideoRecord.h"

#include "CameraParamWidget.h"
#include "AsyncProcesser.h"
#include "AcquireVideoInfo.h"
#include "DataAnalysis.h"

#include "DataBase.hpp"

class GenericCameraModule;

class VideoRecord : public QWidget
{
    Q_OBJECT

    using registProcessCallbackFuncFunc = void*(GenericCameraModule*, std::function<void(const _Fake_Mat&)>);
    using openCameraBySNFunc = void*(GenericCameraModule*, bool&, std::string, std::vector<CameraParamStruct>*);
    using closeCameraFunc = void*(GenericCameraModule*, bool&);
    using framerateFunc = void*(GenericCameraModule*, double&);
    using frameSizeFunc = void*(GenericCameraModule*, int&);
    using captureFunc = void*(GenericCameraModule*);
    using magnificationFunc = void*(GenericCameraModule*, int&);
    using pixelSizeFunc = void*(GenericCameraModule*, double&);

public:
    VideoRecord(QWidget *parent = nullptr);
    ~VideoRecord();

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

    virtual bool eventFilter(QObject* obj, QEvent* e) override;

private slots:
    void slotInitOpenCamera();

    void slotAfterUpdateAvaliableCameras(int cameraCount);

    void slotRefreshFramerate();

    void slotCalcSharpness();

    void slotAutoRecordChecked(int status);

    void slotContinueRecordNumChanged(const QString& number);

    void slotEnterDataAnalysis(QTableWidgetItem* videorecordItem);

    void slotImportDir();

    void slotCloseVideoFramePlayer(DataAnalysis* videoFramePlayer);

    void slotLoopCalcFlowTrack();

    void slotDataAnalysisFinish(AsyncDataAnalyser* asyncDataAnalyser);

    void slotExportAllData();

private:
    inline void asyncUpdateAvaliableCameras();

    inline void updateRecordTime();

    /*!
     * @brief
     * 录制插入
     *
     * @attention
     * 显示一条视频信息并插入一条到数据库
     */
    inline bool insertOneVideoRecord();
    /*!
     * @brief
     * 导入插入
     *
     * @attention
     * 和录制插入相似 有可能导入若干条记录
     */
    inline bool insertOneVideoRecord(const QFileInfo& videoFileinfo, VideoInfo& insertVideoInfo);
    /*!
     * @brief
     * 数据库记录插入
     *
     * @attention
     * 除了表格显示无需任何多余操作
     */
    inline bool insertOneVideoRecord(const VideoInfo& videoInfo);

private:
    Ui_VideoRecord mUI;

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
    QTime mCameraRunTime;                           // 相机运行时长

    int mFrameWidth;                                // 宽
    int mFrameHeight;                               // 高
    int mPixelByteCount;                            // 一像素占用的字节数
    int mMagnification;                             // 放大倍率
    double mPixelSize;                              // 像素尺寸(um/px)

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
    QFileInfo mRecordVideoAbsFileInfo;              // 录制的视频绝对路径

    bool mIsUpdatingCameraList;                     // 是否正在更新相机列表
    bool mShowDebugMessage;                         // 显示调试信息

    QTimer mSharpnessTimer;                         // 清晰度计算定时器 25ms
    int mContinueSharpFrameCount;                   // 连续清晰的帧数
    int mContinueRecordVideoCount;                  // 连续录制的视频数

    QTimer mAutoRecordTimeLimitTimer;               // 智能录制时长显示定时器 1000ms

    QList<QString> mAbsVideoPathList;               // 绝对视频路径列表
    QList<QString> mPkVideoInfoIDList;              // 视频信息的主键列表
    QList<DataAnalysis*> mDataAnalysisList;         // 数据分析界面列表
    QList<int> mFirstSharpImageIndexList;           // 数据分析界面对应的清晰首帧

    int mFlowTrackCalculatingNumer;                 // 正在计算流速的线程数量
    QTimer mLoopCalcFlowTrackTimer;                 // 循环检查流速计算的定时去 1000ms
    QMap<AsyncDataAnalyser*, int> mMapDataAnalyserToRowIndex;   // 计算流速的线程 对应到 视频记录行号
    QMap<int, QString> mMapVideoRecordRowToPkVesselInfoID;      // 视频记录行号 对应到 血管信息主键
};
