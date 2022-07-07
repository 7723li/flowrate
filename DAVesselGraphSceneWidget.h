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
#include <QCursor>
#include <QPixmap>
#include <QHideEvent>
#include <QListWidget>
#include <QFontMetrics>
#include <QMessageBox>

#include "CommonDataStruct.h"
#include "VesselAlgorithm.h"
#include "DataBase.hpp"
#include "AsyncProcesser.h"

#include "ui_DataAnalysis.h"
#include "ui_VesselPanel.h"

class DAVesselGraphSceneWidget : public QWidget
{
    Q_OBJECT

    enum class UserMotionStatus
    {
        None = -1,
        Draw,
        Erase
    };

public:
    explicit DAVesselGraphSceneWidget(Ui_DataAnalysis& ui, const QVector<QImage>& imagelist, QWidget *parent = nullptr);
    virtual ~DAVesselGraphSceneWidget() override;

    void updateVesselInfo(VesselInfo& vesselInfo);

    void setFirstSharpImageIndex(int firstSharpImageIndex);

    bool isSaving();

protected:
    virtual void paintEvent(QPaintEvent *event) override;

    virtual void resizeEvent(QResizeEvent *event) override;

    virtual void showEvent(QShowEvent *event) override;

    virtual void hideEvent(QHideEvent *event) override;

    virtual void mousePressEvent(QMouseEvent* e) override;

    virtual void mouseMoveEvent(QMouseEvent* e) override;

    virtual void mouseReleaseEvent(QMouseEvent* e) override;

    virtual void keyPressEvent(QKeyEvent *event) override;

    virtual void keyReleaseEvent(QKeyEvent *event) override;

    virtual void wheelEvent(QWheelEvent *event) override;

private slots:
    void slotSwitchDrawStatus();

    void slotSwitchEraseStatus();

    void slotRevert();

    void slotRecover();

    void slotSave();

    void slotReanalysisFinish();

    void slotVesselPanelConfirm(const QModelIndex& confirmIndex);

    void slotDataListClicked(const QModelIndex &index);

    void slotVesselPanelSetDiameter();

private:
    void erasing(QPoint erasePoint);

signals:
    void signalUpdateVesselInfo(VesselInfo& vesselInfo);

private:
    Ui_DataAnalysis& mUI;

    const QVector<QImage>& mImageList;
    QImage mShowImage;

    bool mCtrlPressing;             // ctrl
    double mScale;                  // 缩放

    VesselInfo* mPtrVesselInfo;     // 全部图片的血管数据
    VesselInfo mOriVesselInfo;      // 更改对照组

    int mFirstSharpImageIndex;      // 清晰的首帧

    bool mIsDrawing;
    bool mIsErasing;

    QPoint mDrawingPoint;
    RegionPoints mDrawingPoints;

    int mChoosenVesselIndex;                        // 点击选中的血管下标
    QTableWidgetItem* mChoosenVesselInfoRowItem;    // 选中的血管数据行

    UserMotionStatus mUserMotionStatus;

    QRect mEraseRect;
    bool mErasedSomething;

    QVector<VesselInfo> mVesselInfoRevertStack;
    QVector<VesselInfo> mVesselInfoRecoverStack;

    AsyncDataReanalyser* mAsyncDataReanalyser;      // 重新分析计算的线程

    QWidget* mVesselPanel;
    Ui_VesselPanel mUIVesselPanel;

    bool mIsSaving;                 // 保存状态
};
