#pragma once

#include <QWidget>
#include <QVector>
#include <QImage>
#include <QDebug>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QTime>
#include <QTimer>
#include <QMessageBox>
#include <QFileInfo>
#include <QScrollArea>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QDesktopServices>

#include "ui_DataAnalysis.h"

#include "VesselAlgorithm.h"
#include "GifPlayer.h"

#include "DADataDisplaySceneWidget.h"
#include "DAVesselGraphSceneWidget.h"

class DataAnalysis : public QWidget
{
    Q_OBJECT

public:
    explicit DataAnalysis(double fps, QWidget* p = nullptr);
    virtual ~DataAnalysis() override;

    void setVideoInfoMation(const QString& videoAbsPath, double pixelSize, int magnification, double fps);

    void setImageList(QVector<QImage>& imagelist);

    void setFirstSharpImageIndex(int firstSharpImageIndex);

public slots:
    void updateVesselInfo(VesselInfo& vesselInfo);

protected:
    virtual void closeEvent(QCloseEvent* e) override;

private slots:
    void slotExportData();

    void slotSwithDataDisplay();

    void slotSwitchVesselGraph();

    void slotDataDisplayListDoubleClick(const QModelIndex &index);

signals:
    void signalExit(DataAnalysis*);

private:
    Ui_DataAnalysis mUI;

    QFileInfo mVideoFileInfo;                   // 视频文件信息

    GifPlayer* mGifPlayer;                      // 动图播放

    QVector<QImage> mImageList;                 // 图像序列

    VesselInfo mVesselInfo;                     // 血管数据

    DADataDisplaySceneWidget* mDADataDisplaySceneWidget;    // 数据展示场景控件
    DAVesselGraphSceneWidget* mDAVesselGraphSceneWidget;    // 描记场景控件
};
