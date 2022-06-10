#pragma once

#include <QObject>
#include <QFileInfo>
#include <QDir>
#include <QKeyEvent>
#include <QTimer>

#include <QGraphicsScene>
#include <QGraphicsSceneWheelEvent>
#include <QGraphicsPathItem>
#include <QGraphicsSceneMouseEvent>

#include "VesselAlgorithm.h"

#include "ui_DataAnalysis.h"

class DADataDisplayScene : public QGraphicsScene
{
public:
    explicit DADataDisplayScene(Ui_DataAnalysis& ui, const QVector<QImage>& imageList, QObject* p = nullptr);
    virtual ~DADataDisplayScene() override;

    void setVideoAbsPath(const QString& videoAbsPath);

    void updateFrameCount(int frameCount);

    void updateVesselInfo(const VesselInfo& vesselInfo);

    void setFirstSharpImageIndex(int firstSharpImageIndex);

protected:
    virtual void keyPressEvent(QKeyEvent* e) override;

    virtual void wheelEvent(QGraphicsSceneWheelEvent* e) override;

    virtual void drawBackground(QPainter *painter, const QRectF &rect) override;

    virtual void mousePressEvent(QGraphicsSceneMouseEvent* e) override;

private slots:
    void slotChangeShowRegion(const QString& region);

    void slotExportCurrenImage();

    void slotExportImageList();

    void slotDataListClicked(const QModelIndex &index);

private:
    void prevFrame();

    void nextFrame();

private:
    Ui_DataAnalysis& mUI;

    const QVector<QImage>& mImageList;

    QImage mShowImage;              // 当前显示的图像

    QString mVideoAbsPath;          // 对应的视频路径

    int mCurrentFrameIndex;         // 当前帧下标
    int mTotalFrameCount;           // 总帧数

    const VesselInfo* mVesselInfo;  // 全部图片的血管数据

    double mSharpness;              // 清晰度数值
    bool mIsSharp;                  // 是否清晰

    bool mIsShowingRegionSkeleton;  // 是否正在显示血管中心线
    bool mIsShowingRegionBorder;    // 是否正在显示血管边缘
    bool mIsShowingRegionUnion;     // 是否正在显示血管完整区域

    QVector<QGraphicsPathItem*> mSkeletonPathItems;         // 血管中心线
    QVector<QGraphicsPathItem*> mBorderPathItems;           // 血管边缘轮廓
    QVector<QGraphicsPathItem*> mRegionUnionPathItems;      // 血管完整区域

    QTableWidgetItem* mChoosenVesselInfoRowItem;            // 选中的血管数据行
    QGraphicsPathItem* mCurrentChoosenPathItem;             // 选中的血管数据行对应的显示血管
    QGraphicsPathItem* mLastChoosenPathItem;
};
