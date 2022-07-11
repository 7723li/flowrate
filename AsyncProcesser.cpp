﻿#include "AsyncProcesser.h"

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

/**********AsyncVesselInfoCalculator**********/
AsyncDataAnalyser::AsyncDataAnalyser(QVector<QImage> &imagelist, QString absVideoPath, double fps, double pixelSize, int magnification) :
    mAbsVideoPath(absVideoPath), mFps(fps), mPixelSize(pixelSize), mMagnification(magnification), mFirstSharpImageIndex(0)
{
    mImagelist = std::move(imagelist);
}

AsyncDataAnalyser::~AsyncDataAnalyser()
{
    this->quit();
}

VesselInfo& AsyncDataAnalyser::getVesselInfo()
{
    return mVesselInfo;
}

int AsyncDataAnalyser::getFirstSharpImageIndex()
{
    return mFirstSharpImageIndex;
}

void AsyncDataAnalyser::run()
{
    VesselAlgorithm::calculateAll(mImagelist, mPixelSize, mMagnification, mFps, mVesselInfo, mFirstSharpImageIndex);
    emit signalUpdateTrackArea(this);
}
/**********AsyncFlowTrackAreaCalculator**********/

/**********AsyncSharpnessCalculator**********/
AsyncSharpnessCalculator::AsyncSharpnessCalculator(QObject *p):
    QThread(p)
{

}

AsyncSharpnessCalculator::~AsyncSharpnessCalculator()
{
    QEventLoop eventLoop;

    auto lambdaSlotCheckRecordFinish = [&](){
        if (!this->isRunning())
        {
            this->quit();
            QTimer::singleShot(0, &eventLoop, &QEventLoop::quit);
        }
    };

    QTimer timerCheckRecordFinish;
    connect(&timerCheckRecordFinish, &QTimer::timeout, lambdaSlotCheckRecordFinish);

    timerCheckRecordFinish.start(10);
    eventLoop.exec();
}

void AsyncSharpnessCalculator::cache(QImage &image)
{
    if(this->isRunning())
    {
        return;
    }

    mImage = std::move(image);
    this->start();
}

void AsyncSharpnessCalculator::run()
{
    double sharpness = 0.0;
    bool isSharp = false;

    VesselAlgorithm::getImageSharpness(mImage, sharpness, isSharp);

    emit signalCalcSharpnessFinish(sharpness, isSharp);
}
/**********AsyncSharpnessCalculator**********/

/**********AsyncDataReanalyser**********/
AsyncDataReanalyser::AsyncDataReanalyser(const QVector<QImage> &imagelist, double fps, double pixelSize, int magnification, const QVector<int>& erasedOriVesselIndex, const VesselInfo &oriVesselInfo):
    mImagelist(imagelist), mFps(fps), mPixelSize(pixelSize), mMagnification(magnification), mErasedOriVesselIndex(erasedOriVesselIndex), mOriVesselInfo(oriVesselInfo)
{

}

AsyncDataReanalyser::~AsyncDataReanalyser()
{

}

VesselInfo &AsyncDataReanalyser::getNewVesselInfo()
{
    return mNewVesselInfo;
}

void AsyncDataReanalyser::run()
{
    VesselAlgorithm::reCalculateAll(mImagelist, mPixelSize, mMagnification, mFps, mErasedOriVesselIndex, mOriVesselInfo, mNewVesselInfo);

    emit signalReanalysisFinished(this);
}
/**********AsyncDataReanalyser**********/
