#pragma once

#include <QWidget>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QFileInfo>
#include <QDir>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QPainterPath>
#include <QShowEvent>
#include <QScrollBar>
#include <QHideEvent>

#include "CommonDataStruct.h"
#include "VesselAlgorithm.h"

#include "ui_DataAnalysis.h"

class DADataDisplaySceneWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DADataDisplaySceneWidget(Ui_DataAnalysis& ui, const QVector<QImage>& imageList,  QWidget *parent = nullptr);
    virtual ~DADataDisplaySceneWidget() override;

    void setVideoAbsPath(const QString& videoAbsPath);

    void updateFrameCount(int frameCount);

    void updateVesselInfo(const VesselInfo& vesselInfo);

    void setFirstSharpImageIndex(int firstSharpImageIndex);

protected:
    virtual void keyPressEvent(QKeyEvent* e) override;

    virtual void keyReleaseEvent(QKeyEvent *event) override;

    virtual void wheelEvent(QWheelEvent* e) override;

    virtual void paintEvent(QPaintEvent *event) override;

    virtual void resizeEvent(QResizeEvent *event) override;

    virtual void mousePressEvent(QMouseEvent* e) override;

    virtual void showEvent(QShowEvent *event) override;

    virtual void hideEvent(QHideEvent *event) override;

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

    bool mCtrlPressing;             // ctrl
    double mScale;                  // 缩放

    const VesselInfo* mPtrVesselInfo;               // 全部图片的血管数据

    double mSharpness;              // 清晰度数值
    bool mIsSharp;                  // 是否清晰

    bool mIsShowingRegionSkeleton;  // 是否正在显示血管中心线
    bool mIsShowingRegionBorder;    // 是否正在显示血管边缘
    bool mIsShowingRegionUnion;     // 是否正在显示血管完整区域

    QTableWidgetItem* mChoosenVesselInfoRowItem;    // 选中的血管数据行
    const RegionPoints* mCurrentChoosenPathItem;    // 选中的血管数据行对应的显示血管
};
