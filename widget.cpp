#include "widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent),
    mUsingCamera(nullptr), mIsCameraOpen(false), mIsCameraCapturing(false),
    mLib("GenericCameraModule.dll"), mCommonFuncPtr(nullptr), mRuntimeFramerateFuncPtr(nullptr), mCameraParamWidget(new CameraParamWidget(mLib)),
    mFrameWidth(0), mFrameHeight(0), mPixelByteCount(0), mImageSize(0), mVideoRecorder(new AsyncVideoRecorder(mVideoWriter)), mConfigFramerate(0.0), mRunningFramerate(0.0),
    mLastSecondRecvFrameCount(0), mLastSecondRecvFrameCountDisplay(0), mIsUpdatingCameraList(false), mShowDebugMessage(false)
{
    qDebug() << "lib.load GenericCameraModule.dll" << mLib.load();

    mUI.setupUi(this);
    mUI.begin_stop_record_stack->setCurrentWidget(mUI.page_begin_record_btn);
    mUI.begin_record_btn->setEnabled(false);
    connect(mUI.begin_record_btn, &QPushButton::clicked, this, &Widget::beginRecord);
    connect(mUI.stop_record_btn, &QPushButton::clicked, this, &Widget::stopRecord);

    connect(mCameraParamWidget, &CameraParamWidget::signalOpenCamera, this, &Widget::openCamera);
    connect(mCameraParamWidget, &CameraParamWidget::signalCloseCamera, this, &Widget::closeCamera);
    connect(mCameraParamWidget, &CameraParamWidget::signalBeginCapture, this, &Widget::beginCapture);
    connect(mCameraParamWidget, &CameraParamWidget::signalStopCapture, this, &Widget::stopCaptutre);
    connect(mCameraParamWidget, &CameraParamWidget::signalUpdateCameraListFinish, this, &Widget::slotAfterUpdateAvaliableCameras);

    mDisplayTimer.setInterval(40);
    connect(&mDisplayTimer, SIGNAL(timeout()), this, SLOT(update()));

    mRuntimeFramerateFuncPtr = mLib.resolve("getRuntimeFramerate");

    mGetRuntimeFramerateTimer.setInterval(1000);
    connect(&mGetRuntimeFramerateTimer, &QTimer::timeout, this, &Widget::slotRefreshFramerate);

    mRecordTimeLimitTimer.setInterval(60 * 60 * 1000);
    mRecordTimeLimitTimer.setSingleShot(true);
    connect(&mRecordTimeLimitTimer, &QTimer::timeout, this, &Widget::stopRecord);

    QTimer::singleShot(1000, this, &Widget::slotInitOpenCamera);
}

Widget::~Widget()
{

}

bool Widget::openCamera()
{
    bool openSuccess = false;

    if (!mIsCameraOpen)
    {
        mUsingCamera = mCameraParamWidget->getUsingCamera();
        mUsingCameraSN = mCameraParamWidget->getUsingCameraSN();

        if (!mUsingCamera || mUsingCameraSN.empty())
        {
            return false;
        }

        std::function<void(const _Fake_Mat&)> func = std::bind(&Widget::processOneFrame, this, std::placeholders::_1);

        mCommonFuncPtr = mLib.resolve("registProcessCallbackFunc");
        ((registProcessCallbackFuncFunc*)mCommonFuncPtr)(mUsingCamera, func);

        std::vector<CameraParamStruct> tCameraParams;
        mCommonFuncPtr = mLib.resolve("openCameraBySN");
        ((openCameraBySNFunc*)mCommonFuncPtr)(mUsingCamera, openSuccess, mUsingCameraSN, &tCameraParams);

        qDebug() << "openSuccess" << openSuccess;

        mCameraParamWidget->updateCameraParam(tCameraParams);

        if (openSuccess)
        {
            mCameraParamWidget->switchCameraOpeningStyle();

            mIsCameraOpen = true;
        }
    }

    return openSuccess;
}

bool Widget::closeCamera()
{
    bool closeSuccess = true;

    if (mIsCameraCapturing)
    {
        stopCaptutre();
    }

    if (mIsCameraOpen && mUsingCamera)
    {
        mCommonFuncPtr = mLib.resolve("closeCamera");
        ((closeCameraFunc*)mCommonFuncPtr)(mUsingCamera, closeSuccess);

        qDebug() << "closeSuccess" << closeSuccess;
    }

    if (closeSuccess)
    {
        mIsCameraOpen = false;

        mCameraParamWidget->switchCameraClosingStyle();
    }

    return closeSuccess;
}

void Widget::beginCapture()
{
    if (mIsCameraOpen && mUsingCamera)
    {
        mCommonFuncPtr = mLib.resolve("getConfigFramerate");
        ((framerateFunc*)mCommonFuncPtr)(mUsingCamera, mConfigFramerate);

        mCommonFuncPtr = mLib.resolve("frameWidth");
        ((frameSizeFunc *)mCommonFuncPtr)(mUsingCamera, mFrameWidth);

        mCommonFuncPtr = mLib.resolve("frameHeight");
        ((frameSizeFunc *)mCommonFuncPtr)(mUsingCamera, mFrameHeight);

        mCommonFuncPtr = mLib.resolve("pixelByteCount");
        ((frameSizeFunc *)mCommonFuncPtr)(mUsingCamera, mPixelByteCount);

        mImageSize = mFrameWidth * mFrameHeight * mPixelByteCount;

        if (mPixelByteCount == 1)
        {
            mRecvImage = QImage(mFrameWidth, mFrameHeight, QImage::Format::Format_Grayscale8);
            mMat = cv::Mat(mFrameHeight, mFrameWidth, CV_8UC1);
        }
        else if (mPixelByteCount == 3)
        {
            mRecvImage = QImage(mFrameWidth, mFrameHeight, QImage::Format::Format_RGB888);
            mMat = cv::Mat(mFrameHeight, mFrameWidth, CV_8UC3);
        }

        mCommonFuncPtr = mLib.resolve("beginCapture");
        ((captureFunc*)mCommonFuncPtr)(mUsingCamera);

        mUI.begin_record_btn->setEnabled(true);

        mCameraParamWidget->switchCameraCapturingStyle();

        mUsingCameraCaptureBeginDateTime = QDateTime::currentDateTime();

        mDisplayTimer.start();
        mGetRuntimeFramerateTimer.start();

        mIsCameraCapturing = true;
    }
}

void Widget::stopCaptutre()
{
    if (mIsCameraOpen && mUsingCamera)
    {
        mGetRuntimeFramerateTimer.stop();

        mCommonFuncPtr = mLib.resolve("stopCapture");
        ((captureFunc*)mCommonFuncPtr)(mUsingCamera);

        mCameraParamWidget->switchCameraUncapturingStyle();

        mRunningFramerate = 0;
        mLastSecondRecvFrameCount = 0;
        mLastSecondRecvFrameCountDisplay = 0;
        mUsingCameraCaptureFinishDateTime = QDateTime::currentDateTime();

        this->update();

        mIsCameraCapturing = false;

        stopRecord();
    }
}

void Widget::beginRecord()
{
    if (mVideoWriter.isOpened() || !mIsCameraCapturing)
    {
        return;
    }

    mRecordDuratiom.setHMS(0, 0, 0, 0);

    QString tFormatVideoname = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");

    QDir tDir = QDir::currentPath();
    if (!tDir.cdUp() || !tDir.cd("data"))
    {
        return;
    }

    if (!tDir.exists(tFormatVideoname))
    {
        if (tDir.mkdir(tFormatVideoname))
        {
            if (!tDir.cd(tFormatVideoname))
            {
                return;
            }
        }
        else
        {
            return;
        }
    }
    else if (!tDir.cd(tFormatVideoname))
    {
        return;
    }

    mRecordVideoAbsFileInfo.setFile(tDir, tFormatVideoname + ".avi");

    bool opened = mVideoWriter.open(mRecordVideoAbsFileInfo.absoluteFilePath().toStdString().c_str(), CV_FOURCC('M', 'J', 'P', 'G'), mConfigFramerate, cv::Size(mFrameWidth, mFrameHeight), (mPixelByteCount != 1));
    qDebug() << mRecordVideoAbsFileInfo.absoluteFilePath() << "_mVideoWriter.isOpened()" << opened;

    if (mVideoWriter.isOpened())
    {
        mVideoRecorder->startRecord();

        mRecordBeginDateTime = QDateTime::currentDateTime();

        mUI.begin_stop_record_stack->setCurrentWidget(mUI.page_stop_record_btn);

        mRecordTimeLimitTimer.start();
    }
}

void Widget::stopRecord()
{
    int videoDuration = mRecordDuratiom.hour() * 3600 + mRecordDuratiom.minute() * 60 + mRecordDuratiom.second();
    if (!mVideoWriter.isOpened() || !mVideoRecorder->recording())
    {
        return;
    }

    mVideoRecorder->stopRecord();

    mRecordTimeLimitTimer.stop();

    mUI.begin_stop_record_stack->setCurrentWidget(mUI.page_begin_record_btn);
}

void Widget::processOneFrame(const _Fake_Mat &m)
{
    if (mIsCameraCapturing && m.data)
    {
        ++mLastSecondRecvFrameCount;

        memcpy(mMat.data, m.data, mImageSize);

        if (mVideoRecorder->recording())
        {
            mVideoRecorder->cache(mMat);

            updateRecordTime();
        }
    }
}

void Widget::paintEvent(QPaintEvent *e)
{
    e->accept();

    if (!mPainter.begin(mUI.frame_displayer))
    {
        e->ignore();
        return;
    }
    else
    {
        e->accept();
    }

    if (mLastSecondRecvFrameCountDisplay > 0)
    {
        memcpy(mRecvImage.bits(), mMat.data, mImageSize);
        mDisplayImage = mRecvImage;
        mPainter.drawImage(QRect(0, 0, mUI.frame_displayer->width(), mUI.frame_displayer->height()), mDisplayImage, QRect(0, 0, mDisplayImage.width(), mDisplayImage.height()));
    }
    else if (mLastSecondRecvFrameCountDisplay == 0)
    {
        mPainter.fillRect(QRect(0, 0, mUI.frame_displayer->width(), mUI.frame_displayer->height()), Qt::black);
    }

    if (mShowDebugMessage)
    {
        QDateTime tNow = QDateTime::currentDateTime();
        mCameraRunTime.setHMS(0, 0, 0, 0);
        if (mIsCameraCapturing)
        {
            mCameraRunTime = mCameraRunTime.addMSecs(mUsingCameraCaptureBeginDateTime.msecsTo(tNow));
        }
        else
        {
            mCameraRunTime = mCameraRunTime.addMSecs(mUsingCameraCaptureBeginDateTime.msecsTo(mUsingCameraCaptureFinishDateTime));
        }

        qint64 DAY = mCameraRunTime.hour() / 24;
        mCameraRunTime.setHMS(mCameraRunTime.hour() % 24, mCameraRunTime.minute(), mCameraRunTime.second(), mCameraRunTime.msec());

        mPainter.setPen(QPen(Qt::red));
        mPainter.drawText(QPointF(20, 20), QString("cameraFramerate : %1").arg(mRunningFramerate));
        mPainter.drawText(QPointF(20, 40), QString("recvFramerate : %1").arg(mLastSecondRecvFrameCountDisplay));
        mPainter.drawText(QPointF(20, 60), tNow.toString("yyyy-MM-dd hh:mm:ss"));
        mPainter.drawText(QPointF(20, 80), QString("camera run %1day %2:%3:%4:%5").arg(DAY).
            arg(mCameraRunTime.hour(), 2, 10, QLatin1Char('0')).
            arg(mCameraRunTime.minute(), 2, 10, QLatin1Char('0')).
            arg(mCameraRunTime.second(), 2, 10, QLatin1Char('0')).arg(mCameraRunTime.msec(), 3, 10, QLatin1Char('0')));
    }

    mPainter.end();

    if (mVideoWriter.isOpened())
    {
        mUI.video_time_display->setText(mRecordDuratiom.toString("hh:mm:ss"));
    }
    else if (!mUI.video_time_display->text().isEmpty())
    {
        mUI.video_time_display->clear();
    }
}

void Widget::keyPressEvent(QKeyEvent *e)
{
    e->accept();

    if (e->modifiers() == Qt::KeyboardModifier::ControlModifier && e->key() == Qt::Key_B)
    {
        mShowDebugMessage = !mShowDebugMessage;
    }
    else if (e->modifiers() == Qt::KeyboardModifier::ControlModifier && e->key() == Qt::Key_D)
    {
        mCameraParamWidget->show();
    }
}

void Widget::closeEvent(QCloseEvent *e)
{
    closeCamera();

    qDebug() << "lib.unload CameraObject.dll" << mLib.unload();

    delete mCameraParamWidget;
    mCameraParamWidget = nullptr;
}

void Widget::slotInitOpenCamera()
{
    if (openCamera())
    {
        beginCapture();
    }
    else
    {
        asyncUpdateAvaliableCameras();
    }
}

void Widget::slotAfterUpdateAvaliableCameras(int cameraCount)
{
    if (cameraCount > 0 && mIsUpdatingCameraList)
    {
        if (openCamera())
        {
            beginCapture();

            mIsUpdatingCameraList = false;

            return;
        }
    }
    else
    {
        mIsUpdatingCameraList = false;

        asyncUpdateAvaliableCameras();
    }
}

void Widget::slotRefreshFramerate()
{
    if (mUsingCamera)
    {
        ((framerateFunc*)mRuntimeFramerateFuncPtr)(mUsingCamera, mRunningFramerate);

        if (mIsCameraCapturing && mLastSecondRecvFrameCount == 0 && !mCameraParamWidget->isVisible())
        {
            closeCamera();

            asyncUpdateAvaliableCameras();
        }

        mLastSecondRecvFrameCountDisplay = mLastSecondRecvFrameCount;
        mLastSecondRecvFrameCount = 0;
    }
}

void Widget::asyncUpdateAvaliableCameras()
{
    if (mIsUpdatingCameraList)
    {
        return;
    }
    mIsUpdatingCameraList = true;

    if (mUI.begin_record_btn->isEnabled())
    {
        mUI.begin_record_btn->setEnabled(false);
    }

    mCameraParamWidget->asyncUpdateAvaliableCameras();
}

void Widget::updateRecordTime()
{
    mRecordDuratiom.setHMS(0, 0, 0, 0);
    mRecordDuratiom = mRecordDuratiom.addMSecs(mRecordBeginDateTime.msecsTo(QDateTime::currentDateTime()));
}

AsyncVideoRecorder::AsyncVideoRecorder(cv::VideoWriter& videoWriter):
    mRecoring(false), mVideoWriter(videoWriter), mCacheIndex(0), mWriteIndex(0)
{
    mCacheQueue.resize(2048);
    mOccupied.resize(2048, false);
}

AsyncVideoRecorder::~AsyncVideoRecorder()
{

}

void AsyncVideoRecorder::startRecord()
{
    auto lambdaSlotCheckRecordFinish = [&](){
        if (this->isRunning() && mRecoring)
        {
            QTimer::singleShot(0, &mEventLoop, &QEventLoop::quit);
        }
    };

    QTimer timerCheckRecordFinish;
    connect(&timerCheckRecordFinish, &QTimer::timeout, lambdaSlotCheckRecordFinish);

    this->start();
    timerCheckRecordFinish.start(10);
    mEventLoop.exec();
}

bool AsyncVideoRecorder::recording()
{
    return mRecoring;
}

void AsyncVideoRecorder::stopRecord()
{
    auto lambdaSlotCheckRecordFinish = [&](){
        if (!mVideoWriter.isOpened())
        {
            mCacheIndex = 0;
            mWriteIndex = 0;
            mOccupied.resize(2048, false);
            this->quit();
            QTimer::singleShot(0, &mEventLoop, &QEventLoop::quit);
        }
    };

    QTimer timerCheckRecordFinish;
    connect(&timerCheckRecordFinish, &QTimer::timeout, lambdaSlotCheckRecordFinish);

    mRecoring = false;
    timerCheckRecordFinish.start(10);
    mEventLoop.exec();
}

void AsyncVideoRecorder::cache(const cv::Mat& mat)
{
    lock _lock;

    if (mOccupied[mCacheIndex])
    {
        return;
    }

    mCacheQueue[mCacheIndex] = mat;
    mOccupied[mCacheIndex++] = true;

    if (mCacheIndex == 2048)
    {
        mCacheIndex = 0;
    }
}

void AsyncVideoRecorder::run()
{
    mRecoring = true;
    while (true)
    {
        if (mOccupied[mWriteIndex])
        {
            lock _lock;
            const cv::Mat& writeMat = mCacheQueue[mWriteIndex];
            mVideoWriter.write(writeMat);

            mOccupied[mWriteIndex++] = false;

            if (mWriteIndex == 2048)
            {
                mWriteIndex = 0;
            }
        }
        else if (!mRecoring)
        {
            break;
        }
    }

    mVideoWriter.release();
    qDebug() << "mVideoWtirer.release()";
}
