#include "AsyncProcesser.h"

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

/**********AsyncVesselDataCalculator**********/
AsyncVesselDataCalculator::AsyncVesselDataCalculator(QVector<QImage> &imagelist, QString absVideoPath, double fps, double pixelSize, int magnification) :
    mAbsVideoPath(absVideoPath), mFps(fps), mPixelSize(pixelSize), mMagnification(magnification), mFirstSharpImageIndex(0)
{
    mImagelist = std::move(imagelist);
}

AsyncVesselDataCalculator::~AsyncVesselDataCalculator()
{
    this->quit();
}

VesselData& AsyncVesselDataCalculator::getVesselData()
{
    return mVesselData;
}

int AsyncVesselDataCalculator::getFirstSharpImageIndex()
{
    return mFirstSharpImageIndex;
}

void AsyncVesselDataCalculator::run()
{
    VesselAlgorithm::calculateAll(mImagelist, mPixelSize, mMagnification, mFps, mVesselData, mFirstSharpImageIndex);
    mVesselData.absVideoPath = mAbsVideoPath;
    emit signalUpdateTrackArea(this);
}
/**********AsyncFlowTrackAreaCalculator**********/
