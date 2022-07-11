#include "DAVesselGraphSceneWidget.h"

DAVesselGraphSceneWidget::DAVesselGraphSceneWidget(Ui_DataAnalysis &ui, const QVector<QImage> &imagelist, QWidget *parent) :
    QWidget(parent), mUI(ui), mImageList(imagelist), mCtrlPressing(false), mScale(1.0), mChoosenVesselIndex(-1),
    mChoosenVesselInfoRowItem(nullptr), mUserMotionStatus(UserMotionStatus::None), mErasedSomething(false), mAsyncDataReanalyser(nullptr), mIsSaving(false)
{
    mVesselPanel = new QWidget(nullptr);
    mVesselPanel->setWindowFlags(Qt::WindowType::FramelessWindowHint);
    mVesselPanel->hide();
    mUIVesselPanel.setupUi(mVesselPanel);
    connect(mUIVesselPanel.listWidget, &QListWidget::clicked, this, &DAVesselGraphSceneWidget::slotVesselPanelConfirm);
    connect(mUIVesselPanel.largeVessel, &QPushButton::clicked, this, &DAVesselGraphSceneWidget::slotVesselPanelSetDiameter);
    connect(mUIVesselPanel.middleVessel, &QPushButton::clicked, this, &DAVesselGraphSceneWidget::slotVesselPanelSetDiameter);
    connect(mUIVesselPanel.smallVessel, &QPushButton::clicked, this, &DAVesselGraphSceneWidget::slotVesselPanelSetDiameter);
    connect(mUIVesselPanel.vessleEdit, &QLineEdit::returnPressed, this, &DAVesselGraphSceneWidget::slotVesselPanelSetDiameter);

    connect(mUI.drawVessel, &QPushButton::clicked, this, &DAVesselGraphSceneWidget::slotSwitchDrawStatus);
    connect(mUI.eraseVessel, &QPushButton::clicked, this, &DAVesselGraphSceneWidget::slotSwitchEraseStatus);
    connect(mUI.revert, &QPushButton::clicked, this, &DAVesselGraphSceneWidget::slotRevert);
    connect(mUI.recover, &QPushButton::clicked, this, &DAVesselGraphSceneWidget::slotRecover);
    connect(mUI.vesselGraphSave, &QPushButton::clicked, this, &DAVesselGraphSceneWidget::slotSave);
}

DAVesselGraphSceneWidget::~DAVesselGraphSceneWidget()
{
    if(mAsyncDataReanalyser)
    {
        disconnect(mAsyncDataReanalyser, &AsyncDataReanalyser::signalReanalysisFinished, this, &DAVesselGraphSceneWidget::slotReanalysisFinish);
        connect(mAsyncDataReanalyser, &AsyncDataReanalyser::signalReanalysisFinished, &(AsyncDataReanalyserRecycleBin::getInstange()), &AsyncDataReanalyserRecycleBin::slotRecycle);
    }

    delete mVesselPanel;
    mVesselPanel = nullptr;
}

void DAVesselGraphSceneWidget::updateVesselInfo(VesselInfo &vesselInfo)
{
    mPtrVesselInfo = &vesselInfo;
    mOriVesselInfo = vesselInfo;

    this->update();
}

void DAVesselGraphSceneWidget::setFirstSharpImageIndex(int firstSharpImageIndex)
{
    mFirstSharpImageIndex = firstSharpImageIndex;

    this->update();
}

bool DAVesselGraphSceneWidget::isSaving()
{
    return ((mAsyncDataReanalyser && mAsyncDataReanalyser->isRunning()) || mIsSaving);
}

void DAVesselGraphSceneWidget::paintEvent(QPaintEvent *event)
{
    event->accept();

    if(!mPtrVesselInfo || mImageList.empty())
    {
        return;
    }

    mShowImage = mImageList[mFirstSharpImageIndex];

    mUI.percent->setText(QString("%1%").arg(mScale * 100));

    QPainter painter;
    painter.begin(this);
    painter.drawImage(QRectF(0, 0, this->width(), this->height()), mShowImage, QRectF(0, 0, mShowImage.width(), mShowImage.height()));

    painter.scale(mScale, mScale);

    painter.setPen(Qt::red);
    painter.setBrush(QBrush(Qt::red));

    for(const RegionPoints& regionPoints : mPtrVesselInfo->regionsSkeletonPoints)
    {
        painter.drawPoints(regionPoints);
    }

    if(mChoosenVesselIndex != -1)
    {
        painter.setPen(QPen(Qt::green));
        painter.setBrush(QBrush(Qt::green));
        painter.drawPoints((*mPtrVesselInfo).regionsSkeletonPoints[mChoosenVesselIndex]);
    }

    if(!mDrawingPoints.isEmpty())
    {
        painter.setPen(Qt::red);
        painter.setBrush(QBrush(Qt::red));
        painter.drawPoints(mDrawingPoints);
    }

    if(mUserMotionStatus == UserMotionStatus::Erase)
    {
        painter.setPen(Qt::yellow);
        painter.setBrush(QBrush(Qt::yellow));
        painter.drawRect(mEraseRect);
    }

    painter.end();
}

void DAVesselGraphSceneWidget::resizeEvent(QResizeEvent *event)
{
    event->accept();

    this->update();
}

void DAVesselGraphSceneWidget::showEvent(QShowEvent *event)
{
    connect(mUI.dataDisplayList, &QTableWidget::clicked, this, &DAVesselGraphSceneWidget::slotDataListClicked);

    event->accept();

    this->update();
}

void DAVesselGraphSceneWidget::hideEvent(QHideEvent *event)
{
    disconnect(mUI.dataDisplayList, &QTableWidget::clicked, this, &DAVesselGraphSceneWidget::slotDataListClicked);
}

void DAVesselGraphSceneWidget::mousePressEvent(QMouseEvent *e)
{
    mVesselPanel->hide();
    mChoosenVesselIndex = -1;
    if(mChoosenVesselInfoRowItem && mChoosenVesselInfoRowItem->isSelected())
    {
        mChoosenVesselInfoRowItem->setSelected(false);
        mChoosenVesselInfoRowItem = nullptr;
    }
    this->update();

    if(mUserMotionStatus == UserMotionStatus::None)
    {
        e->accept();

        QRectF intersectRect(static_cast<qreal>(e->x()) / mScale - 2, static_cast<qreal>(e->y()) / mScale - 2, 4, 4);
        int searchVesselIndex = 0;
        for(RegionPoints& regionPoints : mPtrVesselInfo->regionsSkeletonPoints)
        {
            QPainterPath intersectPath;
            for(const QPoint& regionPoint : regionPoints)
            {
                intersectPath.addEllipse(QRect(regionPoint, QSize(1, 1)));
            }
            if(intersectPath.intersects(intersectRect))
            {
                mChoosenVesselIndex = searchVesselIndex;
                break;
            }
            if(mChoosenVesselIndex != -1)
            {
                break;
            }
            ++searchVesselIndex;
        }

        this->update();

        if(mChoosenVesselIndex != -1)
        {
            if(e->button() == Qt::MouseButton::LeftButton)
            {
                if(mChoosenVesselIndex < mUI.dataDisplayList->rowCount())
                {
                    mChoosenVesselInfoRowItem = mUI.dataDisplayList->item(mChoosenVesselIndex, 0);
                    mUI.dataDisplayList->scrollToItem(mChoosenVesselInfoRowItem);
                    mChoosenVesselInfoRowItem->setSelected(true);
                }
            }
            else if(e->button() == Qt::MouseButton::RightButton)
            {
                mUIVesselPanel.listWidget->show();
                mUIVesselPanel.vesselSize->hide();
                mUIVesselPanel.vessleEdit->hide();
                mVesselPanel->move(this->mapToGlobal(e->pos()));
                mVesselPanel->show();
            }
        }
    }
    else
    {
        if(e->button() != Qt::MouseButton::LeftButton)
        {
            mIsDrawing = false;
            mIsErasing = false;
            e->ignore();
            return;
        }

        if(mUserMotionStatus == UserMotionStatus::Draw)
        {
            mIsDrawing = true;
            mDrawingPoint = e->pos();
            mDrawingPoints.push_back(mDrawingPoint);

            this->update();
        }
        else if(mUserMotionStatus == UserMotionStatus::Erase)
        {
            mIsErasing = true;
            mErasedSomething = false;

            mVesselInfoRevertStack.push_back(*mPtrVesselInfo);

            erasing(e->pos());

            this->update();
        }
    }
}

void DAVesselGraphSceneWidget::mouseMoveEvent(QMouseEvent *e)
{
    if(!mIsDrawing && !mIsErasing)
    {
        e->ignore();
        return;
    }
    else if(e->button() != Qt::MouseButton::NoButton && e->button() != Qt::MouseButton::LeftButton)
    {
        e->ignore();
        return;
    }

    e->accept();
    if(mUserMotionStatus == UserMotionStatus::Draw)
    {
        QPoint drawingPoint = e->pos();
        QLineF drawingLine(mDrawingPoint, drawingPoint);
        qreal length = drawingLine.length(), split = 1.0 / length;
        if(length > sqrt(2))
        {
            QList<QPoint> fillRegionPoints;
            for(qreal i = split; i <= 1.0; i += split)
            {
                fillRegionPoints.push_back(drawingLine.pointAt(i).toPoint());
            }
            fillRegionPoints.push_back(drawingPoint);
            std::unique(fillRegionPoints.begin(), fillRegionPoints.end());
            mDrawingPoints.append(fillRegionPoints.toVector());
        }
        else
        {
            mDrawingPoints.push_back(drawingPoint);
        }

        mDrawingPoint = drawingPoint;
        this->update();
    }
    else if(mUserMotionStatus == UserMotionStatus::Erase)
    {
        erasing(e->pos());
        this->update();
    }
}

void DAVesselGraphSceneWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if(!mIsDrawing && !mIsErasing)
    {
        e->ignore();
        return;
    }
    else if(e->button() != Qt::MouseButton::NoButton && e->button() != Qt::MouseButton::LeftButton)
    {
        e->ignore();
        return;
    }

    e->accept();
    if(mUserMotionStatus == UserMotionStatus::Draw)
    {
        mVesselInfoRevertStack.push_back(*mPtrVesselInfo);
        mVesselInfoRecoverStack.clear();
        mUI.revert->setEnabled(true);
        mUI.recover->setEnabled(false);
        mUI.vesselGraphSave->setEnabled(true);

        mPtrVesselInfo->regionsSkeletonPoints.push_back(mDrawingPoints);
        mPtrVesselInfo->lengths.push_back(0);
        mPtrVesselInfo->diameters.push_back(20);
        mPtrVesselInfo->flowrates.push_back(0);
        mPtrVesselInfo->glycocalyx.push_back(0);

        mChoosenVesselIndex = mPtrVesselInfo->regionsSkeletonPoints.size() - 1;

        mUIVesselPanel.listWidget->hide();
        mUIVesselPanel.vesselSize->show();
        mUIVesselPanel.vessleEdit->show();
        mUIVesselPanel.vessleEdit->clear();
        mVesselPanel->move(this->mapToGlobal(mDrawingPoints[mDrawingPoints.size() / 2]));
        mVesselPanel->show();

        mDrawingPoints.clear();

        mIsDrawing = false;

        this->update();
    }
    else if(mUserMotionStatus == UserMotionStatus::Erase)
    {
        if(mErasedSomething)
        {
            mVesselInfoRecoverStack.clear();
            mUI.revert->setEnabled(true);
            mUI.recover->setEnabled(false);
            mUI.vesselGraphSave->setEnabled(true);
        }
        else
        {
            mVesselInfoRevertStack.pop_back();
        }

        mIsErasing = false;
    }
}

void DAVesselGraphSceneWidget::keyPressEvent(QKeyEvent *event)
{
    event->accept();
    if(event->key() == Qt::Key_Control)
    {
        mCtrlPressing = true;
    }
}

void DAVesselGraphSceneWidget::keyReleaseEvent(QKeyEvent *event)
{
    event->accept();
    if(event->key() == Qt::Key_Control)
    {
        mCtrlPressing = false;
    }
}

void DAVesselGraphSceneWidget::wheelEvent(QWheelEvent *event)
{
    if(mCtrlPressing)
    {
        event->accept();
        if(event->delta() > 0)
        {
            if(mScale < 2.0)
            {
                mScale += 0.05;
            }
        }
        else if(event->delta() < 0)
        {
            if(mScale > 0.1)
            {
                mScale -= 0.05;
            }
        }
        this->resize(QSizeF(mShowImage.width() * mScale, mShowImage.height() * mScale).toSize());
        this->update();

        if(mScale != 1.0)
        {
            mUI.drawVessel->setEnabled(false);
            mUI.eraseVessel->setEnabled(false);
            mUserMotionStatus = UserMotionStatus::None;
            mUI.drawVessel->setText(QStringLiteral("画"));
            mUI.eraseVessel->setText(QStringLiteral("擦"));
            this->setCursor(QCursor());
        }
        else
        {
            mUI.drawVessel->setEnabled(true);
            mUI.eraseVessel->setEnabled(true);
        }
    }
    else
    {
        event->ignore();
    }
}

void DAVesselGraphSceneWidget::slotSwitchDrawStatus()
{
    if(mUserMotionStatus == UserMotionStatus::Draw)
    {
        mUserMotionStatus = UserMotionStatus::None;
        mUI.drawVessel->setText(QStringLiteral("画"));
    }
    else
    {
        mUserMotionStatus = UserMotionStatus::Draw;
        mIsDrawing = false;
        mUI.drawVessel->setText(QStringLiteral("画*"));
        mUI.eraseVessel->setText(QStringLiteral("擦"));
    }
}

void DAVesselGraphSceneWidget::slotSwitchEraseStatus()
{
    if(mUserMotionStatus == UserMotionStatus::Erase)
    {
        mUserMotionStatus = UserMotionStatus::None;
        mUI.eraseVessel->setText(QStringLiteral("擦"));
    }
    else
    {
        mUserMotionStatus = UserMotionStatus::Erase;
        mIsErasing = false;
        mUI.drawVessel->setText(QStringLiteral("画"));
        mUI.eraseVessel->setText(QStringLiteral("擦*"));
    }
}

void DAVesselGraphSceneWidget::slotRevert()
{
    if(mVesselInfoRevertStack.empty())
    {
        return;
    }

    mChoosenVesselIndex = -1;
    mVesselPanel->hide();
    if(mChoosenVesselInfoRowItem)
    {
        mChoosenVesselInfoRowItem->setSelected(false);
        mChoosenVesselInfoRowItem = nullptr;
    }

    mVesselInfoRecoverStack.push_back(*mPtrVesselInfo);
    *mPtrVesselInfo = mVesselInfoRevertStack.back();
    mVesselInfoRevertStack.pop_back();

    mUI.revert->setEnabled(!mVesselInfoRevertStack.isEmpty());
    mUI.recover->setEnabled(true);
    mUI.vesselGraphSave->setEnabled(!mVesselInfoRevertStack.isEmpty());

    this->update();
}

void DAVesselGraphSceneWidget::slotRecover()
{
    if(mVesselInfoRecoverStack.empty())
    {
        return;
    }

    mChoosenVesselIndex = -1;
    mVesselPanel->hide();
    if(mChoosenVesselInfoRowItem)
    {
        mChoosenVesselInfoRowItem->setSelected(false);
        mChoosenVesselInfoRowItem = nullptr;
    }

    mVesselInfoRevertStack.push_back(*mPtrVesselInfo);
    *mPtrVesselInfo = mVesselInfoRecoverStack.back();
    mVesselInfoRecoverStack.pop_back();

    mUI.revert->setEnabled(true);
    mUI.recover->setEnabled(!mVesselInfoRecoverStack.isEmpty());
    mUI.vesselGraphSave->setEnabled(true);

    this->update();
}

void DAVesselGraphSceneWidget::slotSave()
{
    QVector<int> erasedOriVesselIndex;
    int oriVesselNumber = mOriVesselInfo.vesselNumber;
    for(int i = 0; i < oriVesselNumber; ++i)
    {
        if(mPtrVesselInfo->regionsSkeletonPoints[i] != mOriVesselInfo.regionsSkeletonPoints[i])
        {
            erasedOriVesselIndex.push_back(i);
        }
    }

    double fps = mUI.fps->text().toDouble();
    double pixelSize = mUI.pixelsize->text().toDouble();
    int magnification = mUI.magnification->text().toInt();

    mChoosenVesselIndex = -1;
    mUserMotionStatus = UserMotionStatus::None;
    this->setCursor(QCursor());
    mUI.drawVessel->setText(QStringLiteral("画"));
    mUI.eraseVessel->setText(QStringLiteral("擦"));
    mUI.drawVessel->setEnabled(false);
    mUI.eraseVessel->setEnabled(false);
    mUI.revert->setEnabled(false);
    mUI.recover->setEnabled(false);
    mUI.vesselGraphSave->setEnabled(false);
    mUI.saveTip->setEnabled(true);
    mUI.saveTip->setText(QStringLiteral("重新计算中..."));
    this->update();

    mAsyncDataReanalyser = new AsyncDataReanalyser(mImageList, fps, pixelSize, magnification, erasedOriVesselIndex, *mPtrVesselInfo);
    connect(mAsyncDataReanalyser, &AsyncDataReanalyser::signalReanalysisFinished, this, &DAVesselGraphSceneWidget::slotReanalysisFinish);
    mAsyncDataReanalyser->start();
}

void DAVesselGraphSceneWidget::slotReanalysisFinish(AsyncDataReanalyser* t)
{
    *mPtrVesselInfo = std::move(t->getNewVesselInfo());
    delete t;
    t = nullptr;
    mAsyncDataReanalyser = t;

    QEventLoop little_trick_to_little_prick;//一些给"好伙伴(们)"的小窍门 嘻嘻
    bool saveSuccess = true;
    mIsSaving = true;
    mUI.saveTip->setText(QStringLiteral("保存中..."));
    QTimer::singleShot(2000, [&](){
        saveSuccess = TableVesselInfo::getTableVesselInfo().updateVesselInfo(*mPtrVesselInfo);
        little_trick_to_little_prick.quit();
    });
    little_trick_to_little_prick.exec();
    mIsSaving = false;
    if(saveSuccess)
    {
        mUI.saveTip->setText(QStringLiteral("保存成功"));
    }
    else
    {
        mUI.vesselGraphSave->setEnabled(true);
        mUI.saveTip->setText(QStringLiteral("保存失败"));
    }
    mUI.saveTip->setEnabled(false);

    mUI.drawVessel->setEnabled(true);
    mUI.eraseVessel->setEnabled(true);

    emit signalUpdateVesselInfo(*mPtrVesselInfo);
}

void DAVesselGraphSceneWidget::slotVesselPanelConfirm(const QModelIndex &confirmIndex)
{
    mVesselPanel->hide();
    if(confirmIndex.row() == 0)
    {
        mVesselInfoRevertStack.push_back(*mPtrVesselInfo);
        mVesselInfoRecoverStack.clear();
        (*mPtrVesselInfo).regionsSkeletonPoints[mChoosenVesselIndex].clear();
        mUI.revert->setEnabled(true);
        mUI.vesselGraphSave->setEnabled(true);
        this->update();
    }
    else if(confirmIndex.row() == 1)
    {
        if(mChoosenVesselIndex < mUI.dataDisplayList->rowCount())
        {
            mChoosenVesselInfoRowItem = mUI.dataDisplayList->item(mChoosenVesselIndex, 0);
            mUI.dataDisplayList->scrollToItem(mChoosenVesselInfoRowItem);
            mChoosenVesselInfoRowItem->setSelected(true);
        }
    }
}

void DAVesselGraphSceneWidget::slotDataListClicked(const QModelIndex &index)
{
    int rowindex = index.row();

    if(mChoosenVesselInfoRowItem && mChoosenVesselInfoRowItem->isSelected())
    {
        mChoosenVesselInfoRowItem->setSelected(false);
    }
    mChoosenVesselInfoRowItem = mUI.dataDisplayList->item(rowindex, 0);
    mChoosenVesselInfoRowItem->setSelected(true);

    mChoosenVesselIndex = rowindex;

    this->update();
}

void DAVesselGraphSceneWidget::slotVesselPanelSetDiameter()
{
    QObject* sender = this->sender();

    if(sender == mUIVesselPanel.largeVessel)
    {
        mPtrVesselInfo->diameters[mChoosenVesselIndex] = 100.0;
    }
    else if(sender == mUIVesselPanel.middleVessel)
    {
        mPtrVesselInfo->diameters[mChoosenVesselIndex] = 50.0;
    }
    else if(sender == mUIVesselPanel.smallVessel)
    {
        mPtrVesselInfo->diameters[mChoosenVesselIndex] = 20.0;
    }
    else if(sender == mUIVesselPanel.vessleEdit)
    {
        bool ok = false;
        double diameter = mUIVesselPanel.vessleEdit->text().toDouble(&ok);
        if(!ok || diameter < 0)
        {
            QMessageBox::information(nullptr, QStringLiteral("输入错误"), QStringLiteral("请输入正确的数字"));
            return;
        }
        else if(diameter - 114514 == 0.0)
        {
            QMessageBox::information(nullptr, QStringLiteral("嗯？有在休息之类吗"), QStringLiteral("当然是王道征途 泡泡浴啦"));
            return;
        }

        mPtrVesselInfo->diameters[mChoosenVesselIndex] = diameter;
    }

    mVesselPanel->hide();
}

void DAVesselGraphSceneWidget::erasing(QPoint erasePoint)
{
    mEraseRect = QRect(erasePoint.x() - 2, erasePoint.y() - 2, 4, 4);
    QPolygon erasePolygon(mEraseRect);

    for(RegionPoints& regionPoints : mPtrVesselInfo->regionsSkeletonPoints)
    {
        RegionPoints eraseRegionPoints;
        for(const QPoint& regionPoint : regionPoints)
        {
            if(erasePolygon.containsPoint(regionPoint, Qt::FillRule::OddEvenFill))
            {
                eraseRegionPoints.push_back(regionPoint);
            }
        }
        if(!eraseRegionPoints.empty())
        {
            mErasedSomething = true;
            for(const QPoint& eraseRegionPoint : eraseRegionPoints)
            {
                regionPoints.removeOne(eraseRegionPoint);
            }
        }
    }
}
