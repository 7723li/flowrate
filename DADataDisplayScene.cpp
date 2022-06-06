#include "DADataDisplayScene.h"

DADataDisplayScene::DADataDisplayScene(Ui_DataAnalysis& ui, const QVector<QImage>& imageList, QObject *p):
    QGraphicsScene(p), mUI(ui), mImageList(imageList), mCurrentFrameIndex(0), mTotalFrameCount(0), mVesselData(nullptr),
    mSharpness(0.0), mIsSharp(false), mIsShowingRegionSkeleton(true), mIsShowingRegionBorder(false), mIsShowingRegionUnion(false),
    mChoosenVesselDataRowItem(nullptr), mCurrentChoosenPathItem(nullptr), mLastChoosenPathItem(nullptr)
{
    connect(mUI.showRegionCombobox, &QComboBox::currentTextChanged, this, &DADataDisplayScene::slotChangeShowRegion);
    connect(mUI.exportCurrentImage, &QPushButton::clicked, this, &DADataDisplayScene::slotExportCurrenImage);
    connect(mUI.exportImageList, &QPushButton::clicked, this, &DADataDisplayScene::slotExportImageList);
    connect(mUI.dataDisplayList, &QTableWidget::clicked, this, &DADataDisplayScene::slotDataListClicked);
}

DADataDisplayScene::~DADataDisplayScene()
{
    this->clear();
}

void DADataDisplayScene::setVideoAbsPath(const QString &videoAbsPath)
{
    mVideoAbsPath = videoAbsPath;
}

void DADataDisplayScene::updateFrameCount(int frameCount)
{
    mTotalFrameCount = frameCount;

    mUI.totalPageCount->setNum(mTotalFrameCount);

    this->update();
}

void DADataDisplayScene::updateVesselData(const VesselData &vesselData)
{
    mVesselData = &vesselData;

    this->update();
}

void DADataDisplayScene::setFirstSharpImageIndex(int firstSharpImageIndex)
{
    mCurrentFrameIndex = firstSharpImageIndex;
    mUI.currentPageIndex->setNum(mCurrentFrameIndex + 1);
    this->update();
}

void DADataDisplayScene::keyPressEvent(QKeyEvent *e)
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
}

void DADataDisplayScene::wheelEvent(QGraphicsSceneWheelEvent *e)
{
    e->accept();
    if(e->delta() > 0)
    {
        prevFrame();
    }
    else if(e->delta() < 0)
    {
        nextFrame();
    }
}

void DADataDisplayScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QImage currentImage = mImageList[mCurrentFrameIndex];
    mShowImage = currentImage;

    painter->drawImage(QRectF(0, 0, rect.width(), rect.height()), mShowImage, QRectF(rect.x(), rect.y(), mShowImage.width(), mShowImage.height()));

    VesselAlgorithm::getImageSharpness(mShowImage, mSharpness, mIsSharp);
    mUI.sharpness->setText(QStringLiteral("清晰度：%1").arg(QString::number(mSharpness, 'g', 3)));
    mUI.isSharp->setText(QStringLiteral("是否清晰：%1").arg(mIsSharp ? QStringLiteral("是") : QStringLiteral("否")));

    qreal scalex = rect.width() / mShowImage.width(), scaley = rect.height() / mShowImage.height();

    if(mVesselData && (mIsShowingRegionSkeleton || mIsShowingRegionBorder || mIsShowingRegionUnion))
    {
        if(mIsShowingRegionSkeleton)
        {
            int vesselIndex = 0;
            for(const RegionPoints& regionPoints : mVesselData->regionsSkeletonPoints)
            {
                QPainterPath path;
                for(const QPoint& regionPoint : regionPoints)
                {
                    path.addEllipse(QRectF(static_cast<qreal>(regionPoint.x()) * scalex, static_cast<qreal>(regionPoint.y()) * scaley, 1.5, 1.5));
                }

                if(vesselIndex < mSkeletonPathItems.size())
                {
                    mSkeletonPathItems[vesselIndex]->setPath(path);
                    mSkeletonPathItems[vesselIndex]->show();
                }
                else
                {
                    mSkeletonPathItems.push_back(this->addPath(path, QPen(Qt::red), QBrush(Qt::red)));
                }

                ++vesselIndex;
            }
        }

        if(mIsShowingRegionBorder)
        {
            int vesselIndex = 0;
            for(const RegionPoints& regionPoints : mVesselData->regionsBorderPoints)
            {
                QPainterPath path;
                for(const QPoint& regionPoint : regionPoints)
                {
                    path.addEllipse(QRectF(regionPoint.x() * scalex, regionPoint.y() * scaley, 1.5, 1.5));
                }

                if(vesselIndex < mBorderPathItems.size())
                {
                    mBorderPathItems[vesselIndex]->setPath(path);
                    mBorderPathItems[vesselIndex]->show();
                }
                else
                {
                    mBorderPathItems.push_back(this->addPath(path, QPen(Qt::red), QBrush(Qt::red)));
                }

                ++vesselIndex;
            }
        }

        if(mIsShowingRegionUnion)
        {
            int vesselIndex = 0;
            Qt::GlobalColor color;
            for(const RegionPoints& regionPoints : mVesselData->regionsUnionPoints)
            {
                QPainterPath path;
                for(const QPoint& regionPoint : regionPoints)
                {
                    path.addEllipse(QRectF(regionPoint.x() * scalex, regionPoint.y() * scaley, 1.5, 1.5));
                }

                if(vesselIndex < mRegionUnionPathItems.size())
                {
                    mRegionUnionPathItems[vesselIndex]->setPath(path);
                    mRegionUnionPathItems[vesselIndex]->show();
                }
                else
                {
                    color = Qt::GlobalColor(Qt::red + vesselIndex % 12);
                    mRegionUnionPathItems.push_back(this->addPath(path, QPen(color), QBrush(color)));
                }

                ++vesselIndex;
            }
        }

        if(mCurrentChoosenPathItem)
        {
            mCurrentChoosenPathItem->setPen(QPen(Qt::green));
            mCurrentChoosenPathItem->setBrush(QBrush(Qt::green));
        }
        if(mLastChoosenPathItem)
        {
            mLastChoosenPathItem->setPen(QPen(Qt::red));
            mLastChoosenPathItem->setBrush(QBrush(Qt::red));
        }
    }
}

void DADataDisplayScene::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    QList<QGraphicsItem*> pressedItems = this->items(QRectF(e->scenePos(), QSizeF(10, 10)));
    if(pressedItems.empty())
    {
        e->ignore();
        return;
    }
    else
    {
        e->accept();
    }

    QVector<QGraphicsPathItem*>&& showingRegion = QVector<QGraphicsPathItem*>();
    if(mIsShowingRegionSkeleton)
    {
        showingRegion = mSkeletonPathItems;
    }
    else if(mIsShowingRegionBorder)
    {
        showingRegion = mBorderPathItems;
    }
    else if(mIsShowingRegionUnion)
    {
        showingRegion = mRegionUnionPathItems;
    }
    else
    {
        return;
    }

    if(mChoosenVesselDataRowItem && mChoosenVesselDataRowItem->isSelected())
    {
        mChoosenVesselDataRowItem->setSelected(false);
    }

    int index = showingRegion.indexOf(static_cast<QGraphicsPathItem*>(pressedItems.front()));
    mChoosenVesselDataRowItem = mUI.dataDisplayList->item(index, 0);
    mUI.dataDisplayList->scrollToItem(mChoosenVesselDataRowItem);
    mChoosenVesselDataRowItem->setSelected(true);
}

void DADataDisplayScene::slotChangeShowRegion(const QString &regionName)
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

    mLastChoosenPathItem = mCurrentChoosenPathItem;
    mCurrentChoosenPathItem = nullptr;

    for(QGraphicsPathItem* item : mSkeletonPathItems)
    {
        item->hide();
    }
    for(QGraphicsPathItem* item : mBorderPathItems)
    {
        item->hide();
    }
    for(QGraphicsPathItem* item : mRegionUnionPathItems)
    {
        item->hide();
    }

    (index == 0 || index == 2) ? (mIsShowingRegionSkeleton = true) : (mIsShowingRegionSkeleton = false);
    (index == 1 || index == 2) ? (mIsShowingRegionBorder = true) : (mIsShowingRegionBorder = false);
    (index == 3) ? (mIsShowingRegionUnion = true) : (mIsShowingRegionUnion = false);
    this->update();
}

void DADataDisplayScene::slotExportCurrenImage()
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

    mShowImage.save(QString("%1.bmp").arg(mCurrentFrameIndex + 1));
}

void DADataDisplayScene::slotExportImageList()
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

void DADataDisplayScene::slotDataListClicked(const QModelIndex &index)
{
    int rowindex = index.row();

    if(mChoosenVesselDataRowItem && mChoosenVesselDataRowItem->isSelected())
    {
        mChoosenVesselDataRowItem->setSelected(false);
    }
    mChoosenVesselDataRowItem = mUI.dataDisplayList->item(rowindex, 0);
    mChoosenVesselDataRowItem->setSelected(true);

    QVector<QGraphicsPathItem*>&& showingRegion = QVector<QGraphicsPathItem*>();
    if(mIsShowingRegionSkeleton)
    {
        showingRegion = mSkeletonPathItems;
    }
    else if(mIsShowingRegionBorder)
    {
        showingRegion = mBorderPathItems;
    }
    else
    {
        return;
    }

    mLastChoosenPathItem = mCurrentChoosenPathItem;
    mCurrentChoosenPathItem = showingRegion[rowindex];

    this->update();
}

void DADataDisplayScene::prevFrame()
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

void DADataDisplayScene::nextFrame()
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
