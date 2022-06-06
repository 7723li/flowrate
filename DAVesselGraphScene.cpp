#include "DAVesselGraphScene.h"

DAVesselGraphScene::DAVesselGraphScene(Ui_DataAnalysis &ui, const QVector<QImage> &imagelist, QObject *p) :
    QGraphicsScene(p), mUI(ui), mImageList(imagelist), mPresseChooseDiameterRecordItem(nullptr), mUserMotionStatus(UserMotionStatus::None),
    mIsDrawing(false), mIsErasing(false)
{
    connect(mUI.drawVesselGraph, &QPushButton::clicked, this, &DAVesselGraphScene::slotSwitchDrawStatus);
    connect(mUI.eraseVesselGraph, &QPushButton::clicked, this, &DAVesselGraphScene::slotSwitchEraseStatus);
}

DAVesselGraphScene::~DAVesselGraphScene()
{
    this->clear();

    mPathItems.clear();
    mVesselIndex.clear();
}

void DAVesselGraphScene::refresh()
{
    if(mRegionPoints.empty())
    {
        CommonFunc::splitVesselRegion(mImageList[0], mRegionPoints, "Union", mVesselDiameters);
    }

    qreal scalex = mCurrentVisualRect.width() / mImageList[0].width(), scaley = mCurrentVisualRect.height() / mImageList[0].height();

    if(mRegionPoints.size() > mUI.vesselDiameterRecord->columnCount())
    {
        mUI.vesselDiameterRecord->setColumnCount(mRegionPoints.size());
    }

    int vesselIndex = 0;
    for(const RegionPoints& regionPoints : mRegionPoints)
    {
        QPainterPath path;
        for(const QPoint& regionPoint : regionPoints)
        {
            path.addEllipse(QRectF(regionPoint.x() * scalex, regionPoint.y() * scaley, 1.5, 1.5));
        }

        if(vesselIndex < mPathItems.size())
        {
            mPathItems[vesselIndex]->setPath(path);
        }
        else
        {
            mPathItems.push_back(this->addPath(path, QPen(Qt::red), QBrush(Qt::red)));
            mVesselIndex.push_back(vesselIndex);
        }

        QTableWidgetItem* item = mUI.vesselDiameterRecord->item(0, vesselIndex);
        if(!item)
        {
            item = new QTableWidgetItem;
            mUI.vesselDiameterRecord->setItem(0, vesselIndex, item);
        }

        item->setText(QString::number(mVesselDiameters[vesselIndex]));

        ++vesselIndex;
    }
}

void DAVesselGraphScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    if(mIsDrawing || mIsErasing)
    {
        painter->drawImage(mCurrentVisualRect, mImageList[0] , QRect(0, 0, mImageList[0].width(), mImageList[0].height()));
        return;
    }

    painter->drawImage(rect, mImageList[0] , QRect(0, 0, mImageList[0].width(), mImageList[0].height()));

    if(rect != mCurrentVisualRect)
    {
        mCurrentVisualRect = rect;
        this->refresh();
    }
}

void DAVesselGraphScene::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    e->accept();

    if(mUserMotionStatus == UserMotionStatus::None)
    {
        pressChooseVessel(e->scenePos());
    }
    else if(mUserMotionStatus == UserMotionStatus::Draw)
    {
        mDrawBeginPoint = e->scenePos();
        mIsDrawing = true;
    }
    else if(mUserMotionStatus == UserMotionStatus::Erase)
    {
        mIsErasing = true;
    }
}

void DAVesselGraphScene::mouseMoveEvent(QGraphicsSceneMouseEvent *e)
{
    e->accept();

    if(mIsDrawing)
    {
        qreal scalex = mCurrentVisualRect.width() / mImageList[0].width(), scaley = mCurrentVisualRect.height() / mImageList[0].height();
        QLineF line(mDrawBeginPoint.x() * scalex, mDrawBeginPoint.y() * scaley, e->scenePos().x() * scalex, e->scenePos().y() * scaley);

        mDrawBeginPoint = e->scenePos();

        mDrawingPoints.push_back(QPoint(static_cast<int>(e->scenePos().x() / scalex), static_cast<int>(e->scenePos().y() / scaley)));
    }
}

void DAVesselGraphScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *e)
{
    e->accept();

    if(mUserMotionStatus == UserMotionStatus::Draw)
    {
        mIsDrawing = false;
        mRegionPoints.push_back(mDrawingPoints);
        mDrawingPoints.clear();
    }
    else if(mUserMotionStatus == UserMotionStatus::Erase)
    {
        mIsErasing = false;
    }
}

void DAVesselGraphScene::wheelEvent(QGraphicsSceneWheelEvent *e)
{
    e->accept();
    return;
}

void DAVesselGraphScene::slotSwitchDrawStatus()
{
    if(mUserMotionStatus == UserMotionStatus::Draw)
    {
        mUserMotionStatus = UserMotionStatus::None;
    }
    else
    {
        mUserMotionStatus = UserMotionStatus::Draw;
        mIsDrawing = false;
    }
}

void DAVesselGraphScene::slotSwitchEraseStatus()
{
    if(mUserMotionStatus == UserMotionStatus::Erase)
    {
        mUserMotionStatus = UserMotionStatus::None;
    }
    else
    {
        mUserMotionStatus = UserMotionStatus::Erase;
        mIsErasing = false;
    }
}

void DAVesselGraphScene::pressChooseVessel(QPointF presspos)
{
    QTransform transform;
    QList<QGraphicsItem*> pressedItems = this->items(QRectF(presspos, QSizeF(10, 10)));

    QVector<int> mostVesselIndex;
    for(QGraphicsItem* item : pressedItems)
    {
        int pointIndex = mPathItems.indexOf(static_cast<QGraphicsPathItem*>(item));
        if(-1 != pointIndex)
        {
            mostVesselIndex.push_back(mVesselIndex[pointIndex]);
        }
    }
    qSort(mostVesselIndex.begin(), mostVesselIndex.end());

    if(mPresseChooseDiameterRecordItem && mPresseChooseDiameterRecordItem->isSelected())
    {
        mPresseChooseDiameterRecordItem->setSelected(false);
    }
    if(mostVesselIndex.empty())
    {
        return;
    }

    int vesselIndex = mostVesselIndex.back();
    mPresseChooseDiameterRecordItem = mUI.vesselDiameterRecord->item(0, vesselIndex);
    mUI.vesselDiameterRecord->scrollToItem(mPresseChooseDiameterRecordItem);
    mPresseChooseDiameterRecordItem->setSelected(true);
}
