#include "DADataDisplaySceneWidget.h"

DADataDisplaySceneWidget::DADataDisplaySceneWidget(Ui_DataAnalysis& ui, const QVector<QImage>& imageList, QWidget *p):
    QWidget(p), mUI(ui), mImageList(imageList), mCurrentFrameIndex(0), mTotalFrameCount(0), mCtrlPressing(false), mScale(1.0), mPtrVesselInfo(nullptr),
    mSharpness(0.0), mIsSharp(false), mIsShowingRegionSkeleton(true), mIsShowingRegionBorder(false), mIsShowingRegionUnion(false),
    mChoosenVesselInfoRowItem(nullptr), mCurrentChoosenPathItem(nullptr)
{
    connect(mUI.showRegionCombobox, &QComboBox::currentTextChanged, this, &DADataDisplaySceneWidget::slotChangeShowRegion);
    connect(mUI.exportCurrentImage, &QPushButton::clicked, this, &DADataDisplaySceneWidget::slotExportCurrenImage);
    connect(mUI.exportImageList, &QPushButton::clicked, this, &DADataDisplaySceneWidget::slotExportImageList);
}

DADataDisplaySceneWidget::~DADataDisplaySceneWidget()
{

}

void DADataDisplaySceneWidget::setVideoAbsPath(const QString &videoAbsPath)
{
    mVideoAbsPath = videoAbsPath;
}

void DADataDisplaySceneWidget::updateFrameCount(int frameCount)
{
    mTotalFrameCount = frameCount;

    mUI.totalPageCount->setNum(mTotalFrameCount);

    this->update();
}

void DADataDisplaySceneWidget::updateVesselInfo(const VesselInfo &vesselInfo)
{
    mPtrVesselInfo = &vesselInfo;

    this->update();
}

void DADataDisplaySceneWidget::setFirstSharpImageIndex(int firstSharpImageIndex)
{
    mCurrentFrameIndex = firstSharpImageIndex;
    mUI.currentPageIndex->setNum(mCurrentFrameIndex + 1);
    this->update();
}

void DADataDisplaySceneWidget::keyPressEvent(QKeyEvent *e)
{
    e->accept();

    if(e->key() == Qt::Key::Key_Left)
    {
        prevFrame();
    }
    else if(e->key() == Qt::Key::Key_Right)
    {
        nextFrame();
    }
    else if(e->key() == Qt::Key_Control)
    {
        mCtrlPressing = true;
    }
}

void DADataDisplaySceneWidget::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();

    if(event->key() == Qt::Key_Control)
    {
        mCtrlPressing = false;
    }
}

void DADataDisplaySceneWidget::wheelEvent(QWheelEvent *e)
{
    e->accept();
    if(mCtrlPressing)
    {
        if(e->delta() > 0)
        {
            if(mScale < 2.0)
            {
                mScale += 0.05;
            }
        }
        else if(e->delta() < 0)
        {
            if(mScale > 0.1)
            {
                mScale -= 0.05;
            }
        }
        this->resize(QSizeF(mShowImage.width() * mScale, mShowImage.height() * mScale).toSize());
        this->update();
    }
    else
    {
        if(e->delta() > 0)
        {
            prevFrame();
        }
        else if(e->delta() < 0)
        {
            nextFrame();
        }
    }
}

void DADataDisplaySceneWidget::paintEvent(QPaintEvent *event)
{
    if(mImageList.empty())
    {
        event->ignore();
        return;
    }

    event->accept();

    QImage currentImage = mImageList[mCurrentFrameIndex];
    mShowImage = currentImage;

    VesselAlgorithm::getImageSharpness(mShowImage, mSharpness, mIsSharp);
    mUI.sharpness->setText(QStringLiteral("清晰度：%1").arg(QString::number(mSharpness, 'g', 3)));
    mUI.isSharp->setText(QStringLiteral("是否清晰：%1").arg(mIsSharp ? QStringLiteral("是") : QStringLiteral("否")));
    mUI.percent->setText(QString("%1%").arg(mScale * 100));

    QPainter painter;
    painter.begin(this);
    painter.drawImage(QRectF(0, 0, this->width(), this->height()), mShowImage, QRectF(0, 0, mShowImage.width(), mShowImage.height()));

    painter.scale(mScale, mScale);

    if(mPtrVesselInfo && (mIsShowingRegionSkeleton || mIsShowingRegionBorder || mIsShowingRegionUnion))
    {
        if(mIsShowingRegionSkeleton)
        {
            painter.setPen(Qt::red);
            painter.setBrush(QBrush(Qt::red));

            for(const RegionPoints& regionPoints : mPtrVesselInfo->regionsSkeletonPoints)
            {
                painter.drawPoints(regionPoints);
            }
        }

        if(mIsShowingRegionBorder)
        {
            painter.setPen(Qt::red);
            painter.setBrush(QBrush(Qt::red));

            for(const RegionPoints& regionPoints : mPtrVesselInfo->regionsBorderPoints)
            {
                painter.drawPoints(regionPoints);
            }
        }

        if(mIsShowingRegionUnion)
        {
            int vesselIndex = 0;
            Qt::GlobalColor color;

            for(const RegionPoints& regionPoints : mPtrVesselInfo->regionsUnionPoints)
            {
                color = Qt::GlobalColor(Qt::red + vesselIndex % 12);
                painter.setPen(color);
                painter.setBrush(color);
                ++vesselIndex;
                painter.drawPoints(regionPoints);
            }
        }

        if(mCurrentChoosenPathItem)
        {
            painter.setPen(QPen(Qt::green));
            painter.setBrush(QBrush(Qt::green));
            painter.drawPoints(*mCurrentChoosenPathItem);
        }
    }
}

void DADataDisplaySceneWidget::resizeEvent(QResizeEvent *event)
{
    event->accept();

    this->update();
}

void DADataDisplaySceneWidget::mousePressEvent(QMouseEvent *event)
{
    event->accept();

    mCurrentChoosenPathItem = nullptr;

    QRectF intersectRect(static_cast<qreal>(event->x()) / mScale - 2, static_cast<qreal>(event->y()) / mScale - 2, 4, 4);

    int vesselIndex = -1, searchVesselIndex = 0;
    if(mIsShowingRegionSkeleton)
    {
        for(const RegionPoints& regionPoints : mPtrVesselInfo->regionsSkeletonPoints)
        {
            QPolygon intersectPolygon(regionPoints);
            QPainterPath intersectPath;
            intersectPath.addPolygon(intersectPolygon);
            if(intersectPath.intersects(intersectRect))
            {
                mCurrentChoosenPathItem = &regionPoints;
                vesselIndex = searchVesselIndex;
                break;
            }
            if(vesselIndex != -1)
            {
                break;
            }
            ++searchVesselIndex;
        }
    }

    if(mIsShowingRegionUnion)
    {
        for(const RegionPoints& regionPoints : mPtrVesselInfo->regionsUnionPoints)
        {
            QPolygon intersectPolygon(regionPoints);
            QPainterPath intersectPath;
            intersectPath.addPolygon(intersectPolygon);
            if(intersectPath.intersects(intersectRect))
            {
                vesselIndex = searchVesselIndex;
                break;
            }
            if(vesselIndex != -1)
            {
                break;
            }
            ++searchVesselIndex;
        }
    }

    if(mChoosenVesselInfoRowItem && mChoosenVesselInfoRowItem->isSelected())
    {
        mChoosenVesselInfoRowItem->setSelected(false);
        mChoosenVesselInfoRowItem = nullptr;
    }

    if(vesselIndex == -1 || vesselIndex >= mUI.dataDisplayList->rowCount())
    {
        return;
    }

    mChoosenVesselInfoRowItem = mUI.dataDisplayList->item(vesselIndex, 0);
    mUI.dataDisplayList->scrollToItem(mChoosenVesselInfoRowItem);
    mChoosenVesselInfoRowItem->setSelected(true);

    this->update();
}

void DADataDisplaySceneWidget::showEvent(QShowEvent *event)
{
    connect(mUI.dataDisplayList, &QTableWidget::clicked, this, &DADataDisplaySceneWidget::slotDataListClicked);

    event->accept();

    this->update();
}

void DADataDisplaySceneWidget::hideEvent(QHideEvent *event)
{
    event->accept();

    disconnect(mUI.dataDisplayList, &QTableWidget::clicked, this, &DADataDisplaySceneWidget::slotDataListClicked);
}

void DADataDisplaySceneWidget::slotChangeShowRegion(const QString &regionName)
{
    int index = 0, regionNumber = mUI.showRegionCombobox->count();
    for(int i = 0; i < regionNumber; ++i)
    {
        if(mUI.showRegionCombobox->itemText(i) == regionName)
        {
            index = i;
            break;
        }
    }

    mCurrentChoosenPathItem = nullptr;

    (index == 0 || index == 2) ? (mIsShowingRegionSkeleton = true) : (mIsShowingRegionSkeleton = false);
    (index == 1 || index == 2) ? (mIsShowingRegionBorder = true) : (mIsShowingRegionBorder = false);
    (index == 3) ? (mIsShowingRegionUnion = true) : (mIsShowingRegionUnion = false);
    this->update();
}

void DADataDisplaySceneWidget::slotExportCurrenImage()
{
    QFileInfo videoFileInfo(mVideoAbsPath);
    QDir currentDir(QDir::current());

    if(currentDir.exists("data"))
    {
        currentDir.cd("data");
    }
    else if(currentDir.mkdir("data"))
    {
        currentDir.cd("data");
    }

    QString foldername = videoFileInfo.fileName().remove(".avi");
    if(!currentDir.exists(foldername))
    {
        currentDir.mkdir(foldername);
        if(!currentDir.cd(foldername))
        {
            return;
        }
    }

    mShowImage.save(QString("%1.bmp").arg(mCurrentFrameIndex + 1));
}

void DADataDisplaySceneWidget::slotExportImageList()
{
    QFileInfo videoFileInfo(mVideoAbsPath);
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
        mImageList[i - 1].save(currentDir.absoluteFilePath(QString("%1.bmp").arg(i)));
    }
}

void DADataDisplaySceneWidget::slotDataListClicked(const QModelIndex &index)
{
    int rowindex = index.row();

    if(mChoosenVesselInfoRowItem && mChoosenVesselInfoRowItem->isSelected())
    {
        mChoosenVesselInfoRowItem->setSelected(false);
    }
    mChoosenVesselInfoRowItem = mUI.dataDisplayList->item(rowindex, 0);
    mChoosenVesselInfoRowItem->setSelected(true);

    if(mIsShowingRegionSkeleton)
    {
        mCurrentChoosenPathItem = &(mPtrVesselInfo->regionsSkeletonPoints[rowindex]);
    }
    else
    {
        return;
    }

    this->update();
}

void DADataDisplaySceneWidget::prevFrame()
{
    if(mCurrentFrameIndex == 0)
    {
        return;
    }
    else
    {
        --mCurrentFrameIndex;
    }

    mUI.currentPageIndex->setNum(mCurrentFrameIndex + 1);
    this->update();
}

void DADataDisplaySceneWidget::nextFrame()
{
    if(mCurrentFrameIndex == mTotalFrameCount - 1)
    {
        return;
    }
    else
    {
        ++mCurrentFrameIndex;
    }

    mUI.currentPageIndex->setNum(mCurrentFrameIndex + 1);
    this->update();
}
