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

#include "ui_DataAnalysis.h"

#include "VesselAlgorithm.h"
#include "GifPlayer.h"

#include "DADataDisplayScene.h"
//#include "DAVesselGraphScene.h"

class DataAnalysis : public QWidget
{
    Q_OBJECT

public:
    explicit DataAnalysis(double fps, QWidget* p = nullptr);
    virtual ~DataAnalysis() override;

    void setVideoAbsPath(const QString& videoAbsPath);

    void setImageList(QVector<QImage>& imagelist);

    void setFirstSharpImageIndex(int firstSharpImageIndex);

    void updateVesselData(const VesselData& vesselData);

protected:
    virtual void closeEvent(QCloseEvent* e) override;

private slots:
    void slotExportData();

    void slotSwithDataDisplay();

    void slotSwitchVesselGraph();

signals:
    void signalExit(DataAnalysis*);

private:
    Ui_DataAnalysis mUI;

    QFileInfo mVideoFileInfo;                   // 视频文件信息

    GifPlayer* mGifPlayer;                      // 动图播放

    QVector<QImage> mImageList;                 // 图像序列

    const VesselData* mVesselData;              // 血管数据

    DADataDisplayScene* mDADataDisplayScene;    // 流速场景
    //DAVesselGraphScene* mDAVesselGraphScene;  // 描记场景
};
