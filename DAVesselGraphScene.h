#pragma once

#include <QObject>

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsPathItem>
#include <QGraphicsSceneWheelEvent>

#include "VesselAlgorithm.h"

#include "ui_DataAnalysis.h"

class DAVesselGraphScene : public QGraphicsScene
{
    enum class UserMotionStatus
    {
        None = -1,
        Draw,
        Erase
    };

public:
    explicit DAVesselGraphScene(Ui_DataAnalysis& ui, const QVector<QImage>& imagelist, QObject* p = nullptr);
    virtual ~DAVesselGraphScene() override;

    void refresh();

protected:
    virtual void drawBackground(QPainter *painter, const QRectF &rect) override;

    virtual void mousePressEvent(QGraphicsSceneMouseEvent* e) override;

    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override;

    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override;

    virtual void wheelEvent(QGraphicsSceneWheelEvent* e) override;

private slots:
    void slotSwitchDrawStatus();

    void slotSwitchEraseStatus();

private:
    void pressChooseVessel(QPointF presspos);

private:
    Ui_DataAnalysis& mUI;

    const QVector<QImage>& mImageList;

    QVector<QGraphicsPathItem*> mPathItems;
    QVector<int> mVesselIndex;
    QVector<RegionPoints> mRegionPoints;
    QVector<double> mVesselDiameters;

    QPointF mDrawBeginPoint;
    RegionPoints mDrawingPoints;

    QTableWidgetItem* mPresseChooseDiameterRecordItem;

    UserMotionStatus mUserMotionStatus;

    QRectF mCurrentVisualRect;

    bool mIsDrawing;
    bool mIsErasing;
};
