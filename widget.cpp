#include "widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent),
    mUsingCamera(nullptr), mIsCameraOpen(false), mIsCameraCapturing(false),
    mLib("GenericCameraModule.dll"), mCommonFuncPtr(nullptr), mRuntimeFramerateFuncPtr(nullptr), mCameraParamWidget(new CameraParamWidget(mLib)),
    mFrameWidth(0), mFrameHeight(0), mPixelByteCount(0), mImageSize(0), mVideoRecorder(new AsyncVideoRecorder(mVideoWriter)), mConfigFramerate(0.0), mRunningFramerate(0.0),
    mLastSecondRecvFrameCount(0), mLastSecondRecvFrameCountDisplay(0), mIsUpdatingCameraList(false), mShowDebugMessage(true), mSharpness(0.0),
    mContinueSharpFrameCount(0), mContinueRecordVideoCount(1), mFlowTrackCalculatingNumer(0)
{
    qDebug() << "lib.load GenericCameraModule.dll" << mLib.load();

    mUI.setupUi(this);
    mUI.begin_stop_record_stack->setCurrentWidget(mUI.page_begin_record_btn);
    mUI.begin_record_btn->setEnabled(false);
    connect(mUI.begin_record_btn, &QPushButton::clicked, this, &Widget::beginRecord);
    connect(mUI.stop_record_btn, &QPushButton::clicked, this, &Widget::stopRecord);
    connect(mUI.checkBoxAutoRecord, &QCheckBox::stateChanged, this, &Widget::slotAutoRecordChecked);
    connect(mUI.continueRecordNum, &QComboBox::currentTextChanged, this, &Widget::slotContinueRecordNumChanged);
    connect(mUI.videorecord, &QTableWidget::doubleClicked, this, &Widget::slotVideoRecordDoubleClick);
    connect(mUI.importButton, &QPushButton::clicked, this, &Widget::slotImportDir);

    connect(mCameraParamWidget, &CameraParamWidget::signalOpenCamera, this, &Widget::openCamera);
    connect(mCameraParamWidget, &CameraParamWidget::signalCloseCamera, this, &Widget::closeCamera);
    connect(mCameraParamWidget, &CameraParamWidget::signalBeginCapture, this, &Widget::beginCapture);
    connect(mCameraParamWidget, &CameraParamWidget::signalStopCapture, this, &Widget::stopCaptutre);
    connect(mCameraParamWidget, &CameraParamWidget::signalUpdateCameraListFinish, this, &Widget::slotAfterUpdateAvaliableCameras);

    mRecordDuratiom.setHMS(0, 0, 0, 0);

    mDisplayTimer.setInterval(40);
    connect(&mDisplayTimer, SIGNAL(timeout()), this, SLOT(update()));
    mDisplayTimer.start();

    mRuntimeFramerateFuncPtr = mLib.resolve("getRuntimeFramerate");

//    mGetRuntimeFramerateTimer.setInterval(1000);
//    connect(&mGetRuntimeFramerateTimer, &QTimer::timeout, this, &Widget::slotRefreshFramerate);

    connect(&mAsyncFlowrateCalculator, &AsyncSharpnessCalculator::signalUpdateSharpness,this, &Widget::slotCalcSharpness, Qt::QueuedConnection);

    mRecordTimeLimitTimer.setInterval(60 * 60 * 1000);
    mRecordTimeLimitTimer.setSingleShot(true);
    connect(&mRecordTimeLimitTimer, &QTimer::timeout, this, &Widget::stopRecord);

    mAutoRecordTimeLimitTimer.setInterval(1000);
    mAutoRecordTimeLimitTimer.setSingleShot(true);
    connect(&mAutoRecordTimeLimitTimer, &QTimer::timeout, this, &Widget::stopRecord);

    mLoopCalcFlowTrackTimer.setInterval(1000);
    connect(&mLoopCalcFlowTrackTimer, &QTimer::timeout, this, &Widget::slotLoopCalcFlowTrack);
    mLoopCalcFlowTrackTimer.start();

    //QTimer::singleShot(1000, this, &Widget::slotInitOpenCamera);
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
            mAsyncFlowrateCalculator.setImageFormat(mRecvImage, mImageSize);
            mMat = cv::Mat(mFrameHeight, mFrameWidth, CV_8UC1);
        }
        else if (mPixelByteCount == 3)
        {
            mRecvImage = QImage(mFrameWidth, mFrameHeight, QImage::Format::Format_RGB888);
            mAsyncFlowrateCalculator.setImageFormat(mRecvImage, mImageSize);
            mMat = cv::Mat(mFrameHeight, mFrameWidth, CV_8UC3);
        }

        mCommonFuncPtr = mLib.resolve("beginCapture");
        ((captureFunc*)mCommonFuncPtr)(mUsingCamera);

        mUI.begin_record_btn->setEnabled(true);

        mCameraParamWidget->switchCameraCapturingStyle();

        mUsingCameraCaptureBeginDateTime = QDateTime::currentDateTime();

        mGetRuntimeFramerateTimer.start();

        mAsyncFlowrateCalculator.startCalculate();

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

        mAsyncFlowrateCalculator.stopCalculate();

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
    qDebug() << mRecordVideoAbsFileInfo.absoluteFilePath() << "_mVideoWriter.isOpened()" << opened << mConfigFramerate << mFrameWidth << mFrameHeight << mPixelByteCount;

    if (mVideoWriter.isOpened())
    {
        mVideoRecorder->startRecord();

        mRecordBeginDateTime = QDateTime::currentDateTime();

        if(mContinueRecordVideoCount <= 1)
        {
            if(!mUI.checkBoxAutoRecord->isChecked())
            {
                mUI.begin_stop_record_stack->setCurrentWidget(mUI.page_stop_record_btn);
            }

            mUI.checkBoxAutoRecord->setEnabled(false);
            mUI.continueRecordNum->setEnabled(false);

            mRecordTimeLimitTimer.start();
        }
    }
}

void Widget::stopRecord()
{
    if (!mVideoWriter.isOpened() || !mVideoRecorder->recording())
    {
        return;
    }

    mVideoRecorder->stopRecord();

    if(mContinueRecordVideoCount <= 1)
    {
        mRecordTimeLimitTimer.stop();

        mUI.checkBoxAutoRecord->setEnabled(true);
        mUI.continueRecordNum->setEnabled(true);

        if(!mUI.checkBoxAutoRecord->isChecked())
        {
            mUI.begin_stop_record_stack->setCurrentWidget(mUI.page_begin_record_btn);
        }
    }

    insertOneVideoRecord();
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

        if(mAsyncFlowrateCalculator.calculating())
        {
            mAsyncFlowrateCalculator.cache(mMat);
        }
    }
}

void Widget::paintEvent(QPaintEvent *e)
{
    e->accept();

    if (!this->isVisible() || !mPainter.begin(mUI.frame_displayer))
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

    mUI.video_time_display->setText(mRecordDuratiom.toString("hh:mm:ss"));
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
    mCameraParamWidget->close();
    this->hide();

    closeCamera();

    QEventLoop loop;
    QTimer timer;
    auto lambdaCheckUpdateCameraFinish = [&](){
        if(!mIsUpdatingCameraList)
        {
            loop.quit();
        }
    };

    if(mIsUpdatingCameraList)
    {
        connect(&timer, &QTimer::timeout, lambdaCheckUpdateCameraFinish);
        timer.start(10);
        loop.exec();
    }

    qDebug() << "lib.unload CameraObject.dll" << mLib.unload();

    delete mCameraParamWidget;
    mCameraParamWidget = nullptr;

    for(VideoFramePlayer* videoAnalyser : mVideoFramePlayerList)
    {
        delete videoAnalyser;
        videoAnalyser = nullptr;
    }
    mVideoFramePlayerList.clear();
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
    if (!this->isVisible())
    {
        mIsUpdatingCameraList = false;
        return;
    }
    else if (cameraCount > 0 && mIsUpdatingCameraList)
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

void Widget::slotCalcSharpness(double sharpness)
{
    static const QString tips[] = {QStringLiteral("是"), QStringLiteral("否")};

    mSharpness = sharpness;
    mUI.sharpness->setText(QString::number(mSharpness, 'g', 3));

    if(Flowrate::isSharp(mSharpness))
    {
        mUI.isSharp->setText(tips[0]);

        if(!mVideoRecorder->recording() && mUI.checkBoxAutoRecord->isChecked())
        {
            ++mContinueSharpFrameCount;
            if(mContinueSharpFrameCount >= 10)
            {
                mContinueSharpFrameCount = 0;
                if(mContinueRecordVideoCount > 0)
                {
                    --mContinueRecordVideoCount;
                    beginRecord();
                    mAutoRecordTimeLimitTimer.start();
                }
                else
                {
                    mContinueRecordVideoCount = 1;
                    mUI.checkBoxAutoRecord->setChecked(false);
                }
            }
        }
        else
        {
            mContinueSharpFrameCount = 0;
        }
    }
    else
    {
        mUI.isSharp->setText(tips[1]);
        mContinueSharpFrameCount = 0;
    }
}

void Widget::slotAutoRecordChecked(int status)
{
    if(status == Qt::CheckState::Checked)
    {
        mUI.continueRecordNum->setEnabled(false);
        mUI.begin_stop_record_stack->setEnabled(false);
    }
    else
    {
        mUI.continueRecordNum->setEnabled(true);
        mUI.begin_stop_record_stack->setEnabled(true);
    }
}

void Widget::slotContinueRecordNumChanged(const QString& number)
{
    mContinueRecordVideoCount = number.toInt();
}

void Widget::slotVideoRecordDoubleClick(const QModelIndex &index)
{
    int inRow = index.row();

    QTableWidgetItem* clickedItem = mUI.videorecord->item(inRow, 0);
    if(!clickedItem)
    {
        return;
    }

    QString videoPath = clickedItem->text();
    if(videoPath.isEmpty() || !QFile::exists(videoPath))
    {
        return;
    }

    QVector<QImage> imagelist;
    VideoAnalysier(videoPath, imagelist);
    if(imagelist.empty())
    {
        return;
    }

    VideoFramePlayer* videoFramePlayer = new VideoFramePlayer(inRow);
    mVideoFramePlayerList.push_back(videoFramePlayer);

    auto iter = mMapVideoRecordRowToFlowtrackArea.find(inRow);
    if(iter != mMapVideoRecordRowToFlowtrackArea.end())
    {
        videoFramePlayer->updateFlowTrackAreas(mMapVideoRecordRowToFlowtrackArea[inRow]);
        videoFramePlayer->updateFlowTrackPoints(mMapVideoRecordRowToFlowtrackRegionPoints[inRow]);
    }

    videoFramePlayer->setWindowTitle(videoPath);
    videoFramePlayer->setImageList(imagelist);
    videoFramePlayer->show();
}

void Widget::slotImportDir()
{
    QString inputdir = mUI.importDir->text();
    QDir dir(inputdir);
    if(!dir.exists())
    {
        return;
    }

    QFileInfoList fileinfolist = dir.entryInfoList(QDir::Filter::Files, QDir::SortFlag::Name);
    for(const QFileInfo& fileinfo : fileinfolist)
    {
        if(fileinfo.fileName().endsWith(".avi"))
        {
            insertOneVideoRecord(fileinfo);
        }
    }
}

void Widget::slotCloseVideoFramePlayer(VideoFramePlayer *videoFramePlayer)
{
    if(!videoFramePlayer)
    {
        return;
    }
    else if(mVideoFramePlayerList.removeOne(videoFramePlayer))
    {
        delete videoFramePlayer;
        videoFramePlayer = nullptr;
    }
}

void Widget::slotLoopCalcFlowTrack()
{
    QTableWidget* videorecord = mUI.videorecord;
    int rowCount = videorecord->rowCount();

    for(int i = 0; i < rowCount; ++i)
    {
        QTableWidgetItem* flowrateItem = videorecord->item(i, 2);
        if(mFlowTrackCalculatingNumer < mUI.flowrateProcessNumber->currentText().toInt() && flowrateItem->text() == QStringLiteral("等待中"))
        {
            QVector<QImage> imagelist;
            VideoAnalysier(videorecord->item(i, 0)->text(), imagelist);
            if(imagelist.size() < 2)
            {
                continue;
            }

            ++mFlowTrackCalculatingNumer;
            flowrateItem->setText(QStringLiteral("计算中"));

            AsyncFlowTrackAreaCalculator* asyncFlowTrackAreaCalculator = new AsyncFlowTrackAreaCalculator(imagelist);
            mMapFlowTrackThreadToFlowrateItem[asyncFlowTrackAreaCalculator] = flowrateItem;
            connect(asyncFlowTrackAreaCalculator, &AsyncFlowTrackAreaCalculator::signalUpdateTrackArea, this, &Widget::slotUpdateTrackArea);

            asyncFlowTrackAreaCalculator->start();
        }
    }
}

void Widget::slotUpdateTrackArea(AsyncFlowTrackAreaCalculator *asyncFlowTrackAreaCalculator)
{
    if(!asyncFlowTrackAreaCalculator)
    {
        return;
    }
    else
    {
        auto iter = mMapFlowTrackThreadToFlowrateItem.find(asyncFlowTrackAreaCalculator);
        if(iter == mMapFlowTrackThreadToFlowrateItem.end())
        {
            return;
        }
    }

    QVector<double> trackareas = std::move(asyncFlowTrackAreaCalculator->getFlowtrackArea());
    QVector<RegionPoints> regionPoints = std::move(asyncFlowTrackAreaCalculator->getFlowtrackRegionPoints());

    QTableWidgetItem* flowrateItem = mMapFlowTrackThreadToFlowrateItem[asyncFlowTrackAreaCalculator];
    mMapFlowTrackThreadToFlowrateItem.remove(asyncFlowTrackAreaCalculator);

    delete asyncFlowTrackAreaCalculator;
    asyncFlowTrackAreaCalculator = nullptr;

    --mFlowTrackCalculatingNumer;

    int inRow = flowrateItem->row();

    mMapVideoRecordRowToFlowtrackArea[inRow] = trackareas;
    mMapVideoRecordRowToFlowtrackRegionPoints[inRow] = regionPoints;

    for(VideoFramePlayer* videoFramePlayer : mVideoFramePlayerList)
    {
        if(videoFramePlayer->inRow() == inRow)
        {
            videoFramePlayer->updateFlowTrackAreas(trackareas);
            videoFramePlayer->updateFlowTrackPoints(regionPoints);
        }
    }

    double sumFlowtrackArea = 0.0, flowrate = 0.0;
    for(double trackarea : trackareas)
    {
        sumFlowtrackArea += trackarea;
    }
    flowrate = sumFlowtrackArea * 5.6 * 30 / 1000 / 5 / 1000 / trackareas.size();

    flowrateItem->setText(QString::number(flowrate));
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

void Widget::insertOneVideoRecord()
{
    int currentRowCount = mUI.videorecord->rowCount();
    mUI.videorecord->setRowCount(currentRowCount + 1);

    QString videoAbsPath = mRecordVideoAbsFileInfo.absoluteFilePath();

    QTableWidgetItem* pathItem = new QTableWidgetItem;
    pathItem->setText(videoAbsPath);
    pathItem->setToolTip(videoAbsPath);

    QTableWidgetItem* durationItem = new QTableWidgetItem;
    durationItem->setText(mUI.video_time_display->text());

    QTableWidgetItem* flowrateItem = new QTableWidgetItem;
    flowrateItem->setText(QStringLiteral("等待中"));

    mUI.videorecord->setItem(currentRowCount, 0, pathItem);
    mUI.videorecord->setItem(currentRowCount, 1, durationItem);
    mUI.videorecord->setItem(currentRowCount, 2, flowrateItem);
}

void Widget::insertOneVideoRecord(const QFileInfo &videoFileinfo)
{
    int currentRowCount = mUI.videorecord->rowCount();
    mUI.videorecord->setRowCount(currentRowCount + 1);

    QString videoAbsPath = videoFileinfo.absoluteFilePath();

    QTableWidgetItem* pathItem = new QTableWidgetItem;
    pathItem->setText(videoFileinfo.absoluteFilePath());
    pathItem->setToolTip(videoFileinfo.absoluteFilePath());

    QTableWidgetItem* durationItem = new QTableWidgetItem;
    durationItem->setText("/");

    QTableWidgetItem* flowrateItem = new QTableWidgetItem;
    flowrateItem->setText(QStringLiteral("等待中"));

    mUI.videorecord->setItem(currentRowCount, 0, pathItem);
    mUI.videorecord->setItem(currentRowCount, 1, durationItem);
    mUI.videorecord->setItem(currentRowCount, 2, flowrateItem);
}

/**********AsyncVideoRecorder**********/
AsyncVideoRecorder::AsyncVideoRecorder(cv::VideoWriter& videoWriter):
    mRecoring(false), mVideoWriter(videoWriter), mCacheIndex(0), mWriteIndex(0)
{
    mCacheQueue.resize(2048);
    mOccupied.resize(2048, false);
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
/**********AsyncVideoRecorder**********/

/**********AsyncSharpnessCalculator**********/
AsyncSharpnessCalculator::AsyncSharpnessCalculator() :
    mImageSize(0), mCalculating(false), mOccupied(false)
{

}

void AsyncSharpnessCalculator::setImageFormat(const QImage &image, int imageSize)
{
    mCalculateImage = image;
    mImageSize = imageSize;
}

void AsyncSharpnessCalculator::startCalculate()
{
    auto lambdaSlotCheckRecordFinish = [&](){
        if (this->isRunning() && mCalculating)
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

bool AsyncSharpnessCalculator::calculating()
{
    return mCalculating;
}

void AsyncSharpnessCalculator::stopCalculate()
{
    auto lambdaSlotCheckRecordFinish = [&](){
        if (!this->isRunning())
        {
            this->quit();
            QTimer::singleShot(0, &mEventLoop, &QEventLoop::quit);
        }
    };

    QTimer timerCheckRecordFinish;
    connect(&timerCheckRecordFinish, &QTimer::timeout, lambdaSlotCheckRecordFinish);

    mCalculating = false;
    timerCheckRecordFinish.start(10);
    mEventLoop.exec();
}

void AsyncSharpnessCalculator::cache(const cv::Mat &mat)
{
    lock _lock;

    mMat = mat;
    mOccupied = true;
}

void AsyncSharpnessCalculator::run()
{
    QImage image;

    mCalculating = true;
    while (true)
    {
        if (mOccupied)
        {
            lock _lock;

            memcpy(mCalculateImage.bits(), mMat.data, mImageSize);
            mOccupied = false;

            double shaprness = Flowrate::getImageSharpness(mCalculateImage);
            emit signalUpdateSharpness(shaprness);

            QThread::msleep(10);
        }
        else if (!mCalculating)
        {
            break;
        }
    }
}
/**********AsyncSharpnessCalculator**********/

/**********AsyncFlowTrackAreaCalculator**********/
AsyncFlowTrackAreaCalculator::AsyncFlowTrackAreaCalculator(QVector<QImage>& imagelist)
{
    mImagelist = std::move(imagelist);
}

AsyncFlowTrackAreaCalculator::~AsyncFlowTrackAreaCalculator()
{
    this->quit();
}

QVector<double>& AsyncFlowTrackAreaCalculator::getFlowtrackArea()
{
    return mFlowtrackArea;
}

QVector<RegionPoints>& AsyncFlowTrackAreaCalculator::getFlowtrackRegionPoints()
{
    return mFlowtrackRegionPoints;
}

void AsyncFlowTrackAreaCalculator::run()
{
    Flowrate::calFlowTrackAreas(mImagelist, mFlowtrackArea, mFlowtrackRegionPoints);
    emit signalUpdateTrackArea(this);
}
/**********AsyncFlowTrackAreaCalculator**********/
