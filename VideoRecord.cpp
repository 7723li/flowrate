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
    connect(mUI.begin_record_btn, &QPushButton::clicked, this, &VideoRecord::beginRecord);
    connect(mUI.stop_record_btn, &QPushButton::clicked, this, &VideoRecord::stopRecord);
    connect(mUI.checkBoxAutoRecord, &QCheckBox::stateChanged, this, &VideoRecord::slotAutoRecordChecked);
    connect(mUI.continueRecordNum, &QComboBox::currentTextChanged, this, &VideoRecord::slotContinueRecordNumChanged);
    connect(mUI.videorecord, &QTableWidget::doubleClicked, this, &VideoRecord::slotVideoRecordDoubleClick);
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

    mRecordTimeLimitTimer.setInterval(60 * 60 * 1000);
    mRecordTimeLimitTimer.setSingleShot(true);
    connect(&mRecordTimeLimitTimer, &QTimer::timeout, this, &VideoRecord::stopRecord);

    mAutoRecordTimeLimitTimer.setInterval(1000);
    mAutoRecordTimeLimitTimer.setSingleShot(true);
    connect(&mAutoRecordTimeLimitTimer, &QTimer::timeout, this, &VideoRecord::stopRecord);

    mLoopCalcFlowTrackTimer.setInterval(1000);
    connect(&mLoopCalcFlowTrackTimer, &QTimer::timeout, this, &VideoRecord::slotLoopCalcFlowTrack);
    mLoopCalcFlowTrackTimer.start();

    QTimer::singleShot(1000, this, &VideoRecord::slotInitOpenCamera);
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
    static const QString tips[] = {QStringLiteral("是"), QStringLiteral("否")};

    QImage image = mRecvImage;
    double sharpness = 0.0;
    bool isSharp = false;
    VesselAlgorithm::getImageSharpness(image, sharpness, isSharp);

    mUI.sharpness->setText(QString::number(sharpness, 'g', 3));
    if(isSharp)
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

void VideoRecord::slotVideoRecordDoubleClick(const QModelIndex &index)
{
    int inRow = index.row();

    QString videoPath = mUI.videorecord->item(inRow, 0)->text();
    if(videoPath.isEmpty() || !QFile::exists(videoPath))
    {
        return;
    }

    QVector<QImage> imagelist;
    AcquireVideoInfo(videoPath, &imagelist);
    if(imagelist.empty())
    {
        return;
    }

    double fps = mUI.videorecord->item(inRow, 2)->text().toDouble();
    DataAnalysis* videoFramePlayer = nullptr;
    if(!mDataAnalysisList[inRow])
    {
        videoFramePlayer = new DataAnalysis(fps);
        mDataAnalysisList[inRow] = videoFramePlayer;
        connect(videoFramePlayer, &DataAnalysis::signalExit, this, &VideoRecord::slotCloseVideoFramePlayer);

        auto iter = mMapVideoRecordRowToFlowtrackArea.find(inRow);
        if(iter != mMapVideoRecordRowToFlowtrackArea.end())
        {
            videoFramePlayer->setFirstSharpImageIndex(mFirstSharpImageIndexList[inRow]);
            videoFramePlayer->updateVesselData(mMapVideoRecordRowToFlowtrackArea[inRow]);
        }

        videoFramePlayer->setVideoAbsPath(videoPath);
        videoFramePlayer->setWindowTitle(QStringLiteral("流速--图像解析 ") + videoPath);
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

    for(const QFileInfo& fileinfo : fileinfolist)
    {
        if(fileinfo.fileName().endsWith(".avi"))
        {
            insertOneVideoRecord(fileinfo);
        }
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

void VideoRecord::slotLoopCalcFlowTrack()
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
                    continue;
                }

                double fps = mUI.videorecord->item(rowIndex, 2)->text().toDouble();
                double pixelSize = mUI.videorecord->item(rowIndex, 3)->text().toDouble();
                int magnification = mUI.videorecord->item(rowIndex, 4)->text().toInt();

                ++mFlowTrackCalculatingNumer;
                mUI.videorecord->item(rowIndex, 5)->setText(QStringLiteral("计算中"));

                AsyncVesselDataCalculator* asyncVesselDataCalculator = new AsyncVesselDataCalculator(imagelist, path, fps, pixelSize, magnification);
                mMapFlowTrackThreadToRowIndex[asyncVesselDataCalculator] = rowIndex;
                connect(asyncVesselDataCalculator, &AsyncVesselDataCalculator::signalUpdateTrackArea, this, &VideoRecord::slotUpdateVesselData);

                asyncVesselDataCalculator->start();
            }
        }
        else
        {
            break;
        }
    }
}

void VideoRecord::slotUpdateVesselData(AsyncVesselDataCalculator *asyncVesselDataCalculator)
{
    if(!asyncVesselDataCalculator)
    {
        return;
    }
    else
    {
        auto iter = mMapFlowTrackThreadToRowIndex.find(asyncVesselDataCalculator);
        if(iter == mMapFlowTrackThreadToRowIndex.end())
        {
            return;
        }
    }

    VesselData tVesselData = std::move(asyncVesselDataCalculator->getVesselData());

    int inRow = mMapFlowTrackThreadToRowIndex[asyncVesselDataCalculator];
    mMapFlowTrackThreadToRowIndex.remove(asyncVesselDataCalculator);
    mFirstSharpImageIndexList[inRow] = asyncVesselDataCalculator->getFirstSharpImageIndex();

    delete asyncVesselDataCalculator;
    asyncVesselDataCalculator = nullptr;

    --mFlowTrackCalculatingNumer;

    mMapVideoRecordRowToFlowtrackArea[inRow] = tVesselData;

    if(mDataAnalysisList[inRow])
    {
        mDataAnalysisList[inRow]->setFirstSharpImageIndex(mFirstSharpImageIndexList[inRow]);
        mDataAnalysisList[inRow]->updateVesselData(mMapVideoRecordRowToFlowtrackArea[inRow]);
    }

    mUI.videorecord->item(inRow, 5)->setText(QStringLiteral("已完成"));
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
    const QList<VesselData> vesselDataList = mMapVideoRecordRowToFlowtrackArea.values();
    for(const VesselData& vesselData: vesselDataList)
    {
        double diametersSum = 0.0, lengthSum = 0.0, flowrateSum = 0.0, glycocalyxSum = 0.0;

        excelFile.write(QString("%1\n").arg(vesselData.absVideoPath).toUtf8());
        excelFile.write(QStringLiteral(", 直径, 长度, 流速, 糖萼\n").toUtf8());

        for(int i = 0; i < vesselData.vesselNumber; ++i)
        {
            QString writeRow(", %1, %2, %3, %4\n");
            writeRow = writeRow.arg(vesselData.diameters[i]).arg(vesselData.lengths[i]).arg(vesselData.flowrates[i]).arg(vesselData.glycocalyx[i]);
            diametersSum += vesselData.diameters[i];
            lengthSum += vesselData.lengths[i];
            flowrateSum += vesselData.flowrates[i];
            glycocalyxSum += vesselData.glycocalyx[i];
            excelFile.write(writeRow.toUtf8());
        }

        excelFile.write(QStringLiteral("平均值：, %1, %2, %3, %4\n")
                        .arg(diametersSum / vesselData.vesselNumber)
                        .arg(lengthSum / vesselData.vesselNumber)
                        .arg(flowrateSum / vesselData.vesselNumber)
                        .arg(glycocalyxSum / vesselData.vesselNumber)
                        .toUtf8());
        excelFile.write("\n");
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

void VideoRecord::insertOneVideoRecord()
{
    int currentRowCount = mUI.videorecord->rowCount();
    mUI.videorecord->setRowCount(currentRowCount + 1);

    QString videoAbsPath = mRecordVideoAbsFileInfo.absoluteFilePath();

    QTableWidgetItem* pathItem = new QTableWidgetItem;
    pathItem->setText(videoAbsPath);
    pathItem->setToolTip(videoAbsPath);

    QTableWidgetItem* durationItem = new QTableWidgetItem;
    durationItem->setText(mUI.video_time_display->text());

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
}

void VideoRecord::insertOneVideoRecord(const QFileInfo &videoFileinfo)
{
    int currentRowCount = mUI.videorecord->rowCount();
    mUI.videorecord->setRowCount(currentRowCount + 1);

    QString videoAbsPath = videoFileinfo.absoluteFilePath();

    int duration = 0;
    double fps = 0.0;
    AcquireVideoInfo(videoAbsPath, nullptr, &duration, &fps);

    QTableWidgetItem* pathItem = new QTableWidgetItem;
    pathItem->setText(videoFileinfo.absoluteFilePath());
    pathItem->setToolTip(videoFileinfo.absoluteFilePath());

    QTableWidgetItem* durationItem = new QTableWidgetItem;
    durationItem->setText(QString::number(duration));

    QTableWidgetItem* framerateItem = new QTableWidgetItem;
    framerateItem->setText(QString::number(fps));

    QTableWidgetItem* pixelSizeItem = new QTableWidgetItem;
    pixelSizeItem->setText("5.6");

    QTableWidgetItem* magnificationItem = new QTableWidgetItem;
    magnificationItem->setText("5");

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
}
