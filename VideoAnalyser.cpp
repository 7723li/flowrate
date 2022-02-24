#include "VideoAnalyser.h"

VideoFramePlayer::VideoFramePlayer(int inRow, QWidget *p):
    QWidget(p), mInRow(inRow), mTotalFrameCount(0), mCurrentFrameIdnex(1)
{
    mUI.setupUi(this);

    connect(mUI.prevFrame, &QPushButton::clicked, this, &VideoFramePlayer::slotPrevFrame);
    connect(mUI.nextFrame, &QPushButton::clicked, this, &VideoFramePlayer::slotNextFrame);
    connect(mUI.boundaryVesselRegion, &QPushButton::clicked, this, &VideoFramePlayer::slotPaintBoundaryVesselRegion);
    connect(mUI.splitVesselRegion, &QPushButton::clicked, this, &VideoFramePlayer::slotPaintSplitVesselRegion);
    connect(mUI.saveImageButton, &QPushButton::clicked, this, &VideoFramePlayer::slotSaveImage);
    connect(mUI.exportImageList, &QPushButton::clicked, this, &VideoFramePlayer::slotExportImageList);

    mUI.showFlowTrack->setEnabled(false);

    this->setFocus(Qt::FocusReason::NoFocusReason);
}

VideoFramePlayer::~VideoFramePlayer()
{

}

int VideoFramePlayer::inRow()
{
    return mInRow;
}

void VideoFramePlayer::setImageList(QVector<QImage>& imagelist)
{
    mImageList = std::move(imagelist);

    mCurrentShowImage = mImageList.front();
    mTotalFrameCount = mImageList.size();

    mUI.currentPageIndex->setNum(mCurrentFrameIdnex);
    mUI.totalPageCount->setNum(mTotalFrameCount);
}

void VideoFramePlayer::updateFlowTrackPoints(const QVector<RegionPoints> &regionPoints)
{
    mUI.showFlowTrack->setEnabled(true);

    mRegionFlowTrackPoints = regionPoints;
}

void VideoFramePlayer::updateFlowTrackAreas(const QVector<double> &flowTrackAreas)
{
    mFlowTrackAreas = flowTrackAreas;

    mUI.flowTrackArea->setText(QStringLiteral("流动轨迹面积：%1").arg(mFlowTrackAreas[mCurrentFrameIdnex]));
}

void VideoFramePlayer::paintEvent(QPaintEvent *e)
{
    e->accept();

    QPainter painter;
    painter.begin(mUI.frame);

    const QImage& image = mCurrentShowImage;
    if(!image.isNull() && image.width() > 0 && image.height() > 0)
    {
        painter.drawImage(QRect(0, 0, mUI.frame->width(), mUI.frame->height()), image, QRect(0, 0, image.width(), image.height()));

        double sharpness = Flowrate::getImageSharpness(image);
        bool isSharp = Flowrate::isSharp(sharpness);
        mUI.sharpness->setText(QStringLiteral("清晰度：%1").arg(QString::number(sharpness, 'g', 3)));
        mUI.isSharp->setText(QStringLiteral("是否清晰：%1").arg(isSharp ? QStringLiteral("是") : QStringLiteral("否")));
    }

//    for(const QPainterPath& path : mRegionPainterPaths)
//    {
//        painter.setBrush(QBrush(Qt::red));
//        painter.drawPath(path);
//    }

    painter.end();
}

void VideoFramePlayer::closeEvent(QCloseEvent *e)
{
    e->accept();

    this->hide();

    emit signalExit(this);
}

void VideoFramePlayer::keyPressEvent(QKeyEvent *e)
{
    if(e->key() == Qt::Key::Key_Left)
    {
        e->accept();
        slotPrevFrame();
    }
    else if(e->key() == Qt::Key::Key_Right)
    {
        e->accept();
        slotNextFrame();
    }
    else
    {
        e->ignore();
    }
}

void VideoFramePlayer::wheelEvent(QWheelEvent *e)
{
    e->accept();
    if(e->delta() > 0)
    {
        slotPrevFrame();
    }
    else if(e->delta() < 0)
    {
        slotNextFrame();
    }
}

void VideoFramePlayer::resizeEvent(QResizeEvent *e)
{
    e->accept();

    this->update();
}

void VideoFramePlayer::slotPrevFrame()
{
    if(mCurrentFrameIdnex == 1)
    {
        return;
    }
    else
    {
        --mCurrentFrameIdnex;
    }

    mUI.currentPageIndex->setNum(mCurrentFrameIdnex);
    mCurrentShowImage = mImageList[mCurrentFrameIdnex - 1];
    mUI.flowTrackArea->setText(QStringLiteral("流动轨迹面积：%1").arg(mFlowTrackAreas[mCurrentFrameIdnex]));
    this->update();
}

void VideoFramePlayer::slotNextFrame()
{
    if(mCurrentFrameIdnex == mTotalFrameCount)
    {
        return;
    }
    else
    {
        ++mCurrentFrameIdnex;
    }

    mUI.currentPageIndex->setNum(mCurrentFrameIdnex);
    mCurrentShowImage = mImageList[mCurrentFrameIdnex - 1];
    mUI.flowTrackArea->setText(QStringLiteral("流动轨迹面积：%1").arg(mFlowTrackAreas[mCurrentFrameIdnex]));
    this->update();
}

void VideoFramePlayer::slotPaintBoundaryVesselRegion()
{
    QTime time;
    time.start();

    Flowrate::getBoundaryVesselRegion(mCurrentShowImage, mRegionBoundaryPoints);

    qDebug() << QString("VideoAnalyser getBoundaryVesselRegion %1ms %2").arg(time.elapsed()).arg(mRegionNodesPoints.size());

//    mRegionPainterPaths.clear();
//    QPainterPath path;
//    for(const QPoint& point : mRegionBoundaryPoints)
//    {
//        path.addEllipse(point, 1, 1);
//    }
//    mRegionPainterPaths.push_back(path);
}

void VideoFramePlayer::slotPaintSplitVesselRegion()
{
    QTime time;
    time.start();

    Flowrate::getSplitVesselRegion(mCurrentShowImage, mRegionBranchPoints, mRegionNodesPoints);

    qDebug() << QString("VideoAnalyser splitVesselRegion %1ms %2Branchs %3Nodes").arg(time.elapsed()).arg(mRegionBranchPoints.size()).arg(mRegionNodesPoints.size());

//    mRegionPainterPaths.clear();
//    for(const RegionPoints& rp : mRegionBranchPoints)
//    {
//        QPainterPath path;
//        for(const QPoint& point : rp)
//        {
//            path.addEllipse(point, 1, 1);
//        }
//        mRegionPainterPaths.push_back(path);
//    }

//    for(const RegionPoints& rp : mRegionNodesPoints)
//    {
//        QPainterPath path;
//        for(const QPoint& point : rp)
//        {
//            path.addEllipse(point, 1, 1);
//        }
//        mRegionPainterPaths.push_back(path);
//    }

//    qDebug() << "VideoAnalyser transferPointsToPaths" << time.elapsed() << "ms";

    this->update();
}

void VideoFramePlayer::slotSaveImage()
{
    QFileInfo videoFileInfo(this->windowTitle());
    QDir currentDir(QDir::current());
    currentDir.cd("../data/");

    QString foldername = videoFileInfo.fileName().remove(".avi");
    if(!currentDir.exists(foldername))
    {
        currentDir.mkdir(foldername);
        if(!currentDir.cd(foldername))
        {
            return;
        }
    }

    mCurrentShowImage.save(QString("%1.png").arg(mCurrentFrameIdnex));
}

void VideoFramePlayer::slotExportImageList()
{
    QFileInfo videoFileInfo(this->windowTitle());
    QDir currentDir(QDir::current());
    currentDir.cd("../data/");

    QString foldername = videoFileInfo.fileName().remove(".avi");
    if(!currentDir.exists(foldername))
    {
        currentDir.mkdir(foldername);
        if(!currentDir.cd(foldername))
        {
            return;
        }
    }
    else if(!currentDir.cd(foldername))
    {
        return;
    }

    for(int i = 1; i <= mTotalFrameCount; ++i)
    {
        mImageList[i-1].save(currentDir.absoluteFilePath(QString("%1.png").arg(i)));
    }
}

VideoAnalysier::VideoAnalysier() :
    mAVFormatContext(nullptr), mAVCodecContext(nullptr), mAVStream(nullptr),
    mVideoCodec(nullptr), mOrifmtFrame(nullptr), mSwsfmtFrame(nullptr),
    mImgConvertCtx(nullptr), mFrameBuffer(nullptr), mReadPackct(nullptr),
    mVideostreamIdx(-1), mTimeBase(0)
{

}

QVector<QImage>& VideoAnalysier::analysisFirst100Frame(const QString &videopath)
{
    if(!init(videopath))
    {
        return mImagelist;
    }

    int framecount = 0;
    QImage image(mAVCodecContext->width, mAVCodecContext->height, QImage::Format::Format_Grayscale8);
    while(av_read_frame(mAVFormatContext, mReadPackct) >= 0 && framecount < 100)
    {
        if(mReadPackct->stream_index != mVideostreamIdx)
        {
            av_packet_unref(mReadPackct);
            continue;
        }

        ++framecount;

        // 解码一帧
        int get_picture = 0;
        int ret = avcodec_decode_video2(mAVCodecContext, mOrifmtFrame, &get_picture, mReadPackct);
        // 如果 解码成功(ret > 0) 并且 该帧为关键帧(get_picture > 0) 才去转码
        if (ret > 0 && get_picture > 0)
        {
            // 转码一帧格式
            sws_scale(mImgConvertCtx, mOrifmtFrame->data, mOrifmtFrame->linesize, 0, mAVCodecContext->height, mSwsfmtFrame->data, mSwsfmtFrame->linesize);
        }

        memcpy(image.bits(), mFrameBuffer, mAVCodecContext->width * mAVCodecContext->height);
        mImagelist.push_back(image);

        av_packet_unref(mReadPackct);
    }

    release();

    return mImagelist;
}

bool VideoAnalysier::init(const QString &videopath)
{
    av_register_all();	// 初始化ffmpeg环境

    // 打开一个格式内容 并右键查看"视频属性"
    mAVFormatContext = avformat_alloc_context();
    int open_format = avformat_open_input(&mAVFormatContext, videopath.toStdString().c_str(), nullptr, nullptr);
    if (open_format < 0)
    {
        return false;
    }

    // 根据 格式内容 寻找视频流、音频流之类的信息(TopV项目的视频一般都只有视频流)
    if (avformat_find_stream_info(mAVFormatContext, nullptr) < 0)
    {
        return false;
    }

    // 寻找视频流的位置
    mVideostreamIdx = -1;
    for (int i = 0; i < mAVFormatContext->nb_streams; ++i)
    {
        if (mAVFormatContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            mVideostreamIdx = i;
            break;
        }
    }
    if (-1 == mVideostreamIdx)
    {
        return false;
    }

    // 找到视频流
    mAVStream = mAVFormatContext->streams[mVideostreamIdx];
    if (nullptr == mAVStream)
    {
        return false;
    }

    // 计算时间基
    mTimeBase = av_q2d(mAVStream->time_base);

    // 找到视频流的解码器内容
    mAVCodecContext = mAVStream->codec;
    if (nullptr == mAVCodecContext)
    {
        return false;
    }
    else if (mAVCodecContext->pix_fmt == AV_PIX_FMT_NONE)
    {
        return false;
    }

    // ffmpeg提供了一系列的解码器 这里就是根据id找要用的那个解码器 比如H264
    mVideoCodec = avcodec_find_decoder(mAVCodecContext->codec_id);
    if (nullptr == mVideoCodec)
    {
        return false;
    }

    // 打开解码器
    if (avcodec_open2(mAVCodecContext, mVideoCodec, nullptr) < 0)
    {
        return false;
    }

    // 申请帧结构
    mOrifmtFrame = av_frame_alloc();
    if (nullptr == mOrifmtFrame)
    {
        return false;
    }
    mSwsfmtFrame = av_frame_alloc();
    if (nullptr == mSwsfmtFrame)
    {
        return false;
    }

    // 图像格式转码器 YUV420 -> RGB24
    mImgConvertCtx = sws_getContext(
        mAVCodecContext->width,
        mAVCodecContext->height,
        mAVCodecContext->pix_fmt,
        mAVCodecContext->width,
        mAVCodecContext->height,
        AV_PIX_FMT_GRAY8,
        SWS_BICUBIC,
        NULL,
        NULL,
        NULL
        );
    if (nullptr == mImgConvertCtx)
    {
        return false;
    }

    // 根据转存格式计算 一帧需要的字节数 取决于 格式、图像大小
    int num_bytes = avpicture_get_size(
        AV_PIX_FMT_GRAY8,
        mAVCodecContext->width,
        mAVCodecContext->height);
    if (num_bytes <= 0)
    {
        return false;
    }

    // 根据一帧需要的字节数 给帧申请一个缓冲区 来装载一帧的内容
    mFrameBuffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
    if (nullptr == mFrameBuffer)
    {
        return false;
    }

    int fill_succ = avpicture_fill((AVPicture*)mSwsfmtFrame, mFrameBuffer, AV_PIX_FMT_GRAY8,
        mAVCodecContext->width, mAVCodecContext->height);
    if (fill_succ < 0)
    {
        return false;
    }

    // 图像面积
    int frame_area = mAVCodecContext->width * mAVCodecContext->height;

    // 分配一个数据包 读取一(视频)帧之后 用来存(视频)帧的数据
    mReadPackct = av_packet_alloc();
    if (av_new_packet(mReadPackct, frame_area) < 0)
    {
        return false;
    }

    return true;
}

void VideoAnalysier::release()
{
    if (nullptr != mFrameBuffer)
    {
        av_free(mFrameBuffer);					// 清空缓冲区
        mFrameBuffer = nullptr;
    }

    if (nullptr != mOrifmtFrame)
    {
        av_free(mOrifmtFrame);					// 清空原始帧格式
        mOrifmtFrame = nullptr;
    }

    if (nullptr != mSwsfmtFrame)
    {
        av_free(mSwsfmtFrame);					// 清空转换帧格式
        mSwsfmtFrame = nullptr;
    }

    if (nullptr != mReadPackct)
    {
        av_free_packet(mReadPackct);				// 释放读取包
        mReadPackct = nullptr;
    }

    if (nullptr != mAVCodecContext)
    {
        avcodec_close(mAVCodecContext);		// 关闭解码器
        mAVCodecContext = nullptr;
    }

    if (nullptr != mAVFormatContext)
    {
        avformat_close_input(&mAVFormatContext);	// 关闭视频属性
        mAVFormatContext = nullptr;
    }

    mVideoCodec = nullptr;
    mImgConvertCtx = nullptr;
}
