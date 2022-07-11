#include "VideoRecord.h"

VideoRecord::VideoRecord(QWidget *parent)
    : QWidget(parent),
    mUsingCamera(nullptr), mIsCameraOpen(false), mIsCameraCapturing(false),
    mLib("GenericCameraModule.dll"), mCommonFuncPtr(nullptr), mRuntimeFramerateFuncPtr(nullptr), mCameraParamWidget(new CameraParamWidget(mLib)),
    mFrameWidth(0), mFrameHeight(0), mPixelByteCount(0), mMagnification(0), mPixelSize(0.0), mImageSize(0), mVideoRecorder(new AsyncVideoRecorder(mVideoWriter)),
    mConfigFramerate(0.0), mRunningFramerate(0.0), mLastSecondRecvFrameCount(0), mLastSecondRecvFrameCountDisplay(0), mIsUpdatingCameraList(false),
    mShowDebugMessage(true), mContinueSharpFrameCount(0), mContinueRecordVideoCount(1), mFlowTrackCalculatingNumer(0)
{
    qDebug() << "lib.load GenericCameraModule.dll" << mLib.load();

    mUI.setupUi(this);
    mUI.begin_stop_record_stack->setCurrentWidget(mUI.page_begin_record_btn);
    mUI.begin_record_btn->setEnabled(false);
    mUI.videorecord->setVerticalScrollMode(QTableWidget::ScrollMode::ScrollPerPixel);
    mUI.videorecord->setHorizontalScrollMode(QTableWidget::ScrollMode::ScrollPerPixel);
    mUI.videorecord->installEventFilter(this);

    connect(mUI.begin_record_btn, &QPushButton::clicked, this, &VideoRecord::beginRecord);
    connect(mUI.stop_record_btn, &QPushButton::clicked, this, &VideoRecord::stopRecord);
    connect(mUI.checkBoxAutoRecord, &QCheckBox::stateChanged, this, &VideoRecord::slotAutoRecordChecked);
    connect(mUI.continueRecordNum, &QComboBox::currentTextChanged, this, &VideoRecord::slotContinueRecordNumChanged);
    connect(mUI.importButton, &QPushButton::clicked, this, &VideoRecord::slotImportDir);
    connect(mUI.exportAllData, &QPushButton::clicked, this, &VideoRecord::slotExportAllData);

    connect(mCameraParamWidget, &CameraParamWidget::signalOpenCamera, this, &VideoRecord::openCamera);
    connect(mCameraParamWidget, &CameraParamWidget::signalCloseCamera, this, &VideoRecord::closeCamera);
    connect(mCameraParamWidget, &CameraParamWidget::signalBeginCapture, this, &VideoRecord::beginCapture);
    connect(mCameraParamWidget, &CameraParamWidget::signalStopCapture, this, &VideoRecord::stopCaptutre);
    connect(mCameraParamWidget, &CameraParamWidget::signalUpdateCameraListFinish, this, &VideoRecord::slotAfterUpdateAvaliableCameras);

    mRecordDuratiom.setHMS(0, 0, 0, 0);

    mDisplayTimer.setInterval(40);
    connect(&mDisplayTimer, SIGNAL(timeout()), this, SLOT(update()));
    mDisplayTimer.start();

    mRuntimeFramerateFuncPtr = mLib.resolve("getRuntimeFramerate");

    mGetRuntimeFramerateTimer.setInterval(1000);
    connect(&mGetRuntimeFramerateTimer, &QTimer::timeout, this, &VideoRecord::slotRefreshFramerate);

    mSharpnessTimer.setInterval(25);
    connect(&mSharpnessTimer, &QTimer::timeout, this, &VideoRecord::slotCalcSharpness);
    connect(&mSharpnseeCalculator, &AsyncSharpnessCalculator::signalCalcSharpnessFinish, this, &VideoRecord::slotDisplaySharpnessAndAutoRecord);

    mRecordTimeLimitTimer.setInterval(60 * 60 * 1000);
    mRecordTimeLimitTimer.setSingleShot(true);
    connect(&mRecordTimeLimitTimer, &QTimer::timeout, this, &VideoRecord::stopRecord);

    mAutoRecordTimeLimitTimer.setInterval(1100);
    mAutoRecordTimeLimitTimer.setSingleShot(true);
    connect(&mAutoRecordTimeLimitTimer, &QTimer::timeout, this, &VideoRecord::stopRecord);

    mLoopCalcFlowTrackTimer.setInterval(1000);
    connect(&mLoopCalcFlowTrackTimer, &QTimer::timeout, this, &VideoRecord::slotLoopCheckDataAnalysis);
    mLoopCalcFlowTrackTimer.start();

    QTimer::singleShot(1000, this, &VideoRecord::slotInitOpenCamera);
    QTimer::singleShot(1000, this, &VideoRecord::slotLoadVideoInfos);
}

VideoRecord::~VideoRecord()
{

}

bool VideoRecord::openCamera()
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

        std::function<void(const _Fake_Mat&)> func = std::bind(&VideoRecord::processOneFrame, this, std::placeholders::_1);

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

bool VideoRecord::closeCamera()
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

void VideoRecord::beginCapture()
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

        mCommonFuncPtr = mLib.resolve("magnification");
        ((magnificationFunc*)mCommonFuncPtr)(mUsingCamera, mMagnification);

        mCommonFuncPtr = mLib.resolve("pixelSize");
        ((pixelSizeFunc*)mCommonFuncPtr)(mUsingCamera, mPixelSize);

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

        mGetRuntimeFramerateTimer.start();

        mSharpnessTimer.start();

        mIsCameraCapturing = true;
    }
}

void VideoRecord::stopCaptutre()
{
    if (mIsCameraOpen && mUsingCamera)
    {
        mGetRuntimeFramerateTimer.stop();

        mCommonFuncPtr = mLib.resolve("stopCapture");
        ((captureFunc*)mCommonFuncPtr)(mUsingCamera);

        mSharpnessTimer.stop();

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

void VideoRecord::beginRecord()
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

void VideoRecord::stopRecord()
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

void VideoRecord::processOneFrame(const _Fake_Mat &m)
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

void VideoRecord::paintEvent(QPaintEvent *e)
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

void VideoRecord::keyPressEvent(QKeyEvent *e)
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

void VideoRecord::closeEvent(QCloseEvent *e)
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

    delete mVideoRecorder;
    mVideoRecorder = nullptr;

    delete mCameraParamWidget;
    mCameraParamWidget = nullptr;

    for(DataAnalysis* videoFramePlayer : mDataAnalysisList)
    {
        if(videoFramePlayer)
        {
            delete videoFramePlayer;
            videoFramePlayer = nullptr;
        }
    }
    mDataAnalysisList.clear();

    QList<AsyncDataAnalyser*> tAsyncDataAnalysers = mMapDataAnalyserToRowIndex.keys();
    for(AsyncDataAnalyser* asyncDataAnalyser : tAsyncDataAnalysers)
    {
        disconnect(asyncDataAnalyser, &AsyncDataAnalyser::signalUpdateTrackArea, this, &VideoRecord::slotDataAnalysisFinish);
        connect(asyncDataAnalyser, &AsyncDataAnalyser::signalUpdateTrackArea, [&](){
            delete asyncDataAnalyser;
            asyncDataAnalyser = nullptr;
            mMapDataAnalyserToRowIndex.remove(asyncDataAnalyser);
        });
    }
}

bool VideoRecord::eventFilter(QObject *obj, QEvent *e)
{
    if(obj == mUI.videorecord)
    {
        if(e->type() == QEvent::KeyPress)
        {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(e);
            if(keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return)
            {
                keyEvent->accept();

                QList<QTableWidgetItem*> selectedItems = mUI.videorecord->selectedItems();
                for(QTableWidgetItem* videorecordItem : selectedItems)
                {
                    slotEnterDataAnalysis(videorecordItem);
                }

                return true;
            }
        }
    }

    e->ignore();
    return false;
}

void VideoRecord::slotInitOpenCamera()
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

void VideoRecord::slotLoadVideoInfos()
{
    QVector<VideoInfo> savedVideoInfo;
    if(TableVideoInfo::getTableVideoInfo().selectVideoInfo(savedVideoInfo))
    {
        for(int i = 0; i < savedVideoInfo.size(); ++i)
        {
            mMapVideoRecordRowToPkVesselInfoID[i] = savedVideoInfo[i].fkVesselInfoID;
            insertOneVideoRecord(savedVideoInfo[i]);
        }
    }
}

void VideoRecord::slotAfterUpdateAvaliableCameras(int cameraCount)
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

void VideoRecord::slotRefreshFramerate()
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

void VideoRecord::slotCalcSharpness()
{
    QImage image = mRecvImage;
    mSharpnseeCalculator.cache(image);
}

void VideoRecord::slotDisplaySharpnessAndAutoRecord(double sharpness, bool isSharp)
{
    static const QString tips[] = {QStringLiteral("是"), QStringLiteral("否")};

    mUI.sharpness->setText(QString::number(sharpness, 'g', 3));
    if(isSharp)
    {
        mUI.isSharp->setText(tips[0]);

        if(!mVideoWriter.isOpened() && !mVideoRecorder->recording() && mUI.checkBoxAutoRecord->isChecked())
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

void VideoRecord::slotAutoRecordChecked(int status)
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

void VideoRecord::slotContinueRecordNumChanged(const QString& number)
{
    mContinueRecordVideoCount = number.toInt();
}

void VideoRecord::slotEnterDataAnalysis(QTableWidgetItem *videorecordItem)
{
    int inRow = videorecordItem->row();

    QString videoPath = mUI.videorecord->item(inRow, 0)->text();
    if(videoPath.isEmpty() || !QFile::exists(videoPath))
    {
        QMessageBox::information(nullptr, QStringLiteral("提示"), QStringLiteral("视频文件不存在"));
        return;
    }

    QVector<QImage> imagelist;
    AcquireVideoInfo(videoPath, &imagelist);
    if(imagelist.empty())
    {
        QMessageBox::information(nullptr, QStringLiteral("提示"), QStringLiteral("视频文件已损坏"));
        return;
    }

    double fps = mUI.videorecord->item(inRow, 2)->text().toDouble();
    double pixelSize = mUI.videorecord->item(inRow, 3)->text().toDouble();
    int magnification = mUI.videorecord->item(inRow, 4)->text().toInt();
    DataAnalysis* videoFramePlayer = nullptr;
    if(!mDataAnalysisList[inRow])
    {
        videoFramePlayer = new DataAnalysis(fps);
        mDataAnalysisList[inRow] = videoFramePlayer;
        connect(videoFramePlayer, &DataAnalysis::signalExit, this, &VideoRecord::slotCloseVideoFramePlayer);

        auto iter = mMapVideoRecordRowToPkVesselInfoID.find(inRow);
        if(iter != mMapVideoRecordRowToPkVesselInfoID.end())
        {
            VesselInfo vesselInfo;
            if(TableVesselInfo::getTableVesselInfo().selectVesselInfo(vesselInfo, iter.value()))
            {
                videoFramePlayer->setFirstSharpImageIndex(mFirstSharpImageIndexList[inRow]);
                videoFramePlayer->updateVesselInfo(vesselInfo);
            }
        }

        videoFramePlayer->setVideoInfoMation(videoPath, pixelSize, magnification, fps);
        videoFramePlayer->setImageList(imagelist);
    }
    else
    {
        videoFramePlayer = mDataAnalysisList[inRow];
    }

    videoFramePlayer->show();
    videoFramePlayer->raise();
}

void VideoRecord::slotImportDir()
{
    QString inputdir = mUI.importDir->text();

    VideoInfo videoInfo;
    QList<VideoInfo> videoInfoList;

    QFileInfo inputdirInfo(inputdir);
    if(inputdirInfo.isFile())
    {
        if(inputdirInfo.fileName().endsWith(".avi"))
        {
            if(insertOneVideoRecord(inputdirInfo, videoInfo))
            {
                TableVideoInfo::getTableVideoInfo().insertVideoInfo(videoInfo);
                mPkVideoInfoIDList.push_back(videoInfo.pkVideoInfoID);
            }
        }
        return;
    }

    QDir dir(inputdir);
    if(!dir.exists())
    {
        return;
    }

    QFileInfoList fileinfolist = dir.entryInfoList(QDir::Filter::Files, QDir::SortFlag::Name);

    if(mUI.recursivePathCheck->isChecked())
    {
        QFileInfoList dirlist = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        while(!dirlist.empty())
        {
            QFileInfoList recursiveFileInfoList = QDir(dirlist.front().absoluteFilePath()).entryInfoList(QDir::Filter::Files, QDir::SortFlag::Name);
            fileinfolist.append(recursiveFileInfoList);
            QFileInfoList recursiveDirList = QDir(dirlist.front().absoluteFilePath()).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            if(!recursiveDirList.empty())
            {
                dirlist.append(recursiveDirList);
            }
            dirlist.pop_front();
        }
    }

    QFileInfoList importFileinfolist;
    for(const QFileInfo& fileinfo : fileinfolist)
    {
        if(fileinfo.fileName().endsWith(".avi"))
        {
            importFileinfolist.push_back(fileinfo);
        }
    }
    fileinfolist.clear();

    for(const QFileInfo& fileinfo : importFileinfolist)
    {
        if(insertOneVideoRecord(fileinfo, videoInfo))
        {
            videoInfoList.push_back(videoInfo);
        }
    }
    TableVideoInfo::getTableVideoInfo().insertVideoInfo(videoInfoList);

    for(const VideoInfo& videoInfo : videoInfoList)
    {
        mPkVideoInfoIDList.push_back(videoInfo.pkVideoInfoID);
    }
}

void VideoRecord::slotCloseVideoFramePlayer(DataAnalysis *videoFramePlayer)
{
    if(!videoFramePlayer)
    {
        return;
    }
    else
    {
        int index = mDataAnalysisList.indexOf(videoFramePlayer);

        delete videoFramePlayer;
        videoFramePlayer = nullptr;

        mDataAnalysisList[index] = nullptr;
    }
}

void VideoRecord::slotLoopCheckDataAnalysis()
{
    int rowCount = mUI.videorecord->rowCount();
    for(int rowIndex = 0; rowIndex < rowCount; ++rowIndex)
    {
        if(mFlowTrackCalculatingNumer < mUI.flowrateProcessNumber->currentText().toInt() && mFlowTrackCalculatingNumer < rowCount)
        {
            if(mUI.videorecord->item(rowIndex, 5)->text() == QStringLiteral("等待中"))
            {
                QVector<QImage> imagelist;
                QString path = mUI.videorecord->item(rowIndex, 0)->text();
                AcquireVideoInfo(path, &imagelist);
                if(imagelist.size() < 2)
                {
                    mUI.videorecord->item(rowIndex, 5)->setText(QStringLiteral("视频不存在或已损坏"));
                    continue;
                }

                double fps = mUI.videorecord->item(rowIndex, 2)->text().toDouble();
                double pixelSize = mUI.videorecord->item(rowIndex, 3)->text().toDouble();
                int magnification = mUI.videorecord->item(rowIndex, 4)->text().toInt();

                ++mFlowTrackCalculatingNumer;
                mUI.videorecord->item(rowIndex, 5)->setText(QStringLiteral("计算中"));

                AsyncDataAnalyser* asyncDataAnalyser = new AsyncDataAnalyser(imagelist, path, fps, pixelSize, magnification);
                mMapDataAnalyserToRowIndex[asyncDataAnalyser] = rowIndex;
                connect(asyncDataAnalyser, &AsyncDataAnalyser::signalUpdateTrackArea, this, &VideoRecord::slotDataAnalysisFinish);

                asyncDataAnalyser->start();
            }
        }
        else
        {
            break;
        }
    }
}

void VideoRecord::slotDataAnalysisFinish(AsyncDataAnalyser *asyncDataAnalyser)
{
    if(!asyncDataAnalyser)
    {
        return;
    }
    else
    {
        auto iter = mMapDataAnalyserToRowIndex.find(asyncDataAnalyser);
        if(iter == mMapDataAnalyserToRowIndex.end())
        {
            return;
        }
    }

    VesselInfo tVesselInfo = std::move(asyncDataAnalyser->getVesselInfo());

    TableVesselInfo::getTableVesselInfo().insertVesselInfo(tVesselInfo);

    int inRow = mMapDataAnalyserToRowIndex[asyncDataAnalyser];
    int firstSharpFrameIndex = asyncDataAnalyser->getFirstSharpImageIndex();;
    mMapDataAnalyserToRowIndex.remove(asyncDataAnalyser);
    mFirstSharpImageIndexList[inRow] = firstSharpFrameIndex;

    delete asyncDataAnalyser;
    asyncDataAnalyser = nullptr;

    --mFlowTrackCalculatingNumer;

    mMapVideoRecordRowToPkVesselInfoID[inRow] = tVesselInfo.pkVesselInfoID;

    VideoInfo videoInfo;
    if(TableVideoInfo::getTableVideoInfo().selectVideoInfo(videoInfo, mPkVideoInfoIDList[inRow]))
    {
        videoInfo.fkVesselInfoID = tVesselInfo.pkVesselInfoID;
        videoInfo.firstSharpFrameIndex = firstSharpFrameIndex;
        videoInfo.analysisFinishTime = QDateTime::currentDateTime().toTime_t();
        TableVideoInfo::getTableVideoInfo().updateVideoInfo(videoInfo);
    }

    if(mDataAnalysisList[inRow])
    {
        mDataAnalysisList[inRow]->setFirstSharpImageIndex(mFirstSharpImageIndexList[inRow]);
        mDataAnalysisList[inRow]->updateVesselInfo(tVesselInfo);
    }

    mUI.videorecord->item(inRow, 5)->setText(QStringLiteral("已完成 %1").arg(QDateTime::fromTime_t(videoInfo.analysisFinishTime).toString("yy-MM-dd hh:mm:ss")));
}

void VideoRecord::slotExportAllData()
{
    QDir currentDir(QDir::current());
    currentDir.cdUp();
    if(currentDir.exists("data"))
    {
        currentDir.cd("data");
    }
    else if(currentDir.mkdir("data"))
    {
        currentDir.cd("data");
    }

    QFileInfo excelPath = currentDir.absoluteFilePath(QString("%1.csv").arg(QDateTime::currentDateTime().toTime_t()));

    QFile excelFile(excelPath.absoluteFilePath());
    if(!excelFile.open(QIODevice::OpenModeFlag::WriteOnly))
    {
        QMessageBox::information(nullptr, QStringLiteral("导出失败"), QStringLiteral("文件已存在或创建文件失败"));
    }

    //设置表格数据
    VesselInfo vesselInfo;
    for(auto iter = mMapVideoRecordRowToPkVesselInfoID.begin(); iter != mMapVideoRecordRowToPkVesselInfoID.end(); ++iter)
    {
        if(TableVesselInfo::getTableVesselInfo().selectVesselInfo(vesselInfo, iter.value()))
        {
            double diametersSum = 0.0, lengthSum = 0.0, flowrateSum = 0.0, glycocalyxSum = 0.0;
            int glycocalyxCount = 0;

            excelFile.write(QString("%1\n").arg(mAbsVideoPathList[iter.key()]).toUtf8());
            excelFile.write(QStringLiteral(", 直径, 长度, 流速, 糖萼\n").toUtf8());

            for(int i = 0; i < vesselInfo.vesselNumber; ++i)
            {
                QString writeRow(", %1, %2, %3, %4\n");
                writeRow = writeRow.arg(vesselInfo.diameters[i]).arg(vesselInfo.lengths[i]).arg(vesselInfo.flowrates[i]).arg(vesselInfo.glycocalyx[i]);
                diametersSum += vesselInfo.diameters[i];
                lengthSum += vesselInfo.lengths[i];
                flowrateSum += vesselInfo.flowrates[i];
                if(-vesselInfo.glycocalyx[i] != 1.0)
                {
                    glycocalyxSum += vesselInfo.glycocalyx[i];
                    ++glycocalyxCount;
                }
                excelFile.write(writeRow.toUtf8());
            }

            excelFile.write(QStringLiteral("平均值：, %1, %2, %3, %4\n")
                            .arg(diametersSum / vesselInfo.vesselNumber)
                            .arg(lengthSum / vesselInfo.vesselNumber)
                            .arg(flowrateSum / vesselInfo.vesselNumber)
                            .arg(glycocalyxSum / glycocalyxCount)
                            .toUtf8());
            excelFile.write("\n");
        }
    }

    excelFile.close();

    QMessageBox::information(nullptr, QStringLiteral("导出"), QStringLiteral("导出完成"));
}

void VideoRecord::asyncUpdateAvaliableCameras()
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

void VideoRecord::updateRecordTime()
{
    mRecordDuratiom.setHMS(0, 0, 0, 0);
    mRecordDuratiom = mRecordDuratiom.addMSecs(mRecordBeginDateTime.msecsTo(QDateTime::currentDateTime()));
}

bool VideoRecord::insertOneVideoRecord()
{
    QString videoAbsPath = mRecordVideoAbsFileInfo.absoluteFilePath();
    if(-1 != mAbsVideoPathList.indexOf(videoAbsPath))
    {
        return false;
    }
    mAbsVideoPathList.push_back(videoAbsPath);

    int currentRowCount = mUI.videorecord->rowCount();
    mUI.videorecord->setRowCount(currentRowCount + 1);

    QTableWidgetItem* pathItem = new QTableWidgetItem;
    pathItem->setText(videoAbsPath);
    pathItem->setToolTip(videoAbsPath);

    QTableWidgetItem* durationItem = new QTableWidgetItem;
    durationItem->setText(QString::number(mRecordDuratiom.hour() * 3600 + mRecordDuratiom.minute() * 60 + mRecordDuratiom.second()));

    QTableWidgetItem* framerateItem = new QTableWidgetItem;
    framerateItem->setText(QString::number(mConfigFramerate));

    QTableWidgetItem* pixelSizeItem = new QTableWidgetItem;
    pixelSizeItem->setText(QString::number(mPixelSize));

    QTableWidgetItem* magnificationItem = new QTableWidgetItem;
    magnificationItem->setText(QString::number(mMagnification));

    QTableWidgetItem* flowrateItem = new QTableWidgetItem;
    flowrateItem->setText(QStringLiteral("等待中"));

    mUI.videorecord->setItem(currentRowCount, 0, pathItem);
    mUI.videorecord->setItem(currentRowCount, 1, durationItem);
    mUI.videorecord->setItem(currentRowCount, 2, framerateItem);
    mUI.videorecord->setItem(currentRowCount, 3, pixelSizeItem);
    mUI.videorecord->setItem(currentRowCount, 4, magnificationItem);
    mUI.videorecord->setItem(currentRowCount, 5, flowrateItem);

    mDataAnalysisList.push_back(nullptr);
    mFirstSharpImageIndexList.push_back(0);

    VideoInfo videoInfo;
    videoInfo.collectTime = QFileInfo(videoAbsPath).created().toTime_t();
    videoInfo.videoDuration = mRecordDuratiom.hour() * 3600 + mRecordDuratiom.minute() * 60 + mRecordDuratiom.second();
    videoInfo.absVideoPath = videoAbsPath;
    videoInfo.pixelSize = mPixelSize;
    videoInfo.magnification = mMagnification;
    videoInfo.fps = mConfigFramerate;
    TableVideoInfo::getTableVideoInfo().insertVideoInfo(videoInfo);
    mPkVideoInfoIDList.push_back(videoInfo.pkVideoInfoID);

    return true;
}

bool VideoRecord::insertOneVideoRecord(const QFileInfo &videoFileinfo, VideoInfo& insertVideoInfo)
{
    QString videoAbsPath = videoFileinfo.absoluteFilePath();
    if(-1 != mAbsVideoPathList.indexOf(videoAbsPath))
    {
        return false;
    }
    mAbsVideoPathList.push_back(videoAbsPath);

    int currentRowCount = mUI.videorecord->rowCount();
    mUI.videorecord->setRowCount(currentRowCount + 1);

    int duration = 0;
    double fps = 0.0;
    int width = 0;
    int height = 0;
    AcquireVideoInfo(videoAbsPath, nullptr, &duration, &fps, &width, &height);

    if(0 == width || 0 == height)
    {
        return false;
    }

    QTableWidgetItem* pathItem = new QTableWidgetItem;
    pathItem->setText(videoFileinfo.absoluteFilePath());
    pathItem->setToolTip(videoFileinfo.absoluteFilePath());

    QTableWidgetItem* durationItem = new QTableWidgetItem;
    durationItem->setText(QString::number(duration));

    QTableWidgetItem* framerateItem = new QTableWidgetItem;
    framerateItem->setText(QString::number(fps));

    QTableWidgetItem* pixelSizeItem = new QTableWidgetItem;
    QTableWidgetItem* magnificationItem = new QTableWidgetItem;

    double pixelsize = 5.6;
    int magnification = 5;
    if(width == 656 && height == 492)
    {
        pixelsize = 5.6;
        magnification = 5;
    }
    else if((width == 1456 && height == 1088) || (width == 1936 && height == 1216))
    {
        pixelsize = 3.45;
        magnification = 4;
    }

    pixelSizeItem->setText(QString::number(pixelsize));
    magnificationItem->setText(QString::number(magnification));

    QTableWidgetItem* flowrateItem = new QTableWidgetItem;
    flowrateItem->setText(QStringLiteral("等待中"));

    mUI.videorecord->setItem(currentRowCount, 0, pathItem);
    mUI.videorecord->setItem(currentRowCount, 1, durationItem);
    mUI.videorecord->setItem(currentRowCount, 2, framerateItem);
    mUI.videorecord->setItem(currentRowCount, 3, pixelSizeItem);
    mUI.videorecord->setItem(currentRowCount, 4, magnificationItem);
    mUI.videorecord->setItem(currentRowCount, 5, flowrateItem);

    mDataAnalysisList.push_back(nullptr);
    mFirstSharpImageIndexList.push_back(0);

    insertVideoInfo.collectTime = videoFileinfo.created().toTime_t();
    insertVideoInfo.videoDuration = duration;
    insertVideoInfo.absVideoPath = videoAbsPath;
    insertVideoInfo.pixelSize = 5.6;
    insertVideoInfo.magnification = 5;
    insertVideoInfo.fps = fps;

    return true;
}

bool VideoRecord::insertOneVideoRecord(const VideoInfo &videoInfo)
{
    QString videoAbsPath = videoInfo.absVideoPath;
    if(-1 != mAbsVideoPathList.indexOf(videoAbsPath))
    {
        return false;
    }
    mAbsVideoPathList.push_back(videoAbsPath);

    int currentRowCount = mUI.videorecord->rowCount();
    mUI.videorecord->setRowCount(currentRowCount + 1);

    int duration = 0;
    AcquireVideoInfo(videoAbsPath, nullptr, &duration);

    QTableWidgetItem* pathItem = new QTableWidgetItem;
    pathItem->setText(videoAbsPath);
    pathItem->setToolTip(videoAbsPath);

    QTableWidgetItem* durationItem = new QTableWidgetItem;
    durationItem->setText(QString::number(duration));

    QTableWidgetItem* framerateItem = new QTableWidgetItem;
    framerateItem->setText(QString::number(videoInfo.fps));

    QTableWidgetItem* pixelSizeItem = new QTableWidgetItem;
    pixelSizeItem->setText(QString::number(videoInfo.pixelSize));

    QTableWidgetItem* magnificationItem = new QTableWidgetItem;
    magnificationItem->setText(QString::number(videoInfo.magnification));

    QTableWidgetItem* flowrateItem = new QTableWidgetItem;
    flowrateItem->setText(videoInfo.fkVesselInfoID.isEmpty() ? QStringLiteral("等待中") : QStringLiteral("历史记录 %1").arg(QDateTime::fromTime_t(videoInfo.analysisFinishTime).toString("yy-MM-dd hh:mm:ss")));

    mUI.videorecord->setItem(currentRowCount, 0, pathItem);
    mUI.videorecord->setItem(currentRowCount, 1, durationItem);
    mUI.videorecord->setItem(currentRowCount, 2, framerateItem);
    mUI.videorecord->setItem(currentRowCount, 3, pixelSizeItem);
    mUI.videorecord->setItem(currentRowCount, 4, magnificationItem);
    mUI.videorecord->setItem(currentRowCount, 5, flowrateItem);

    mDataAnalysisList.push_back(nullptr);
    mFirstSharpImageIndexList.push_back(videoInfo.firstSharpFrameIndex);
    mPkVideoInfoIDList.push_back(videoInfo.pkVideoInfoID);

    return true;
}
