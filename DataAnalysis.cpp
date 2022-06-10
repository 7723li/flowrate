#include "DataAnalysis.h"

DataAnalysis::DataAnalysis(double fps, QWidget *p):
    QWidget(p), mGifPlayer(new GifPlayer(mImageList, fps, nullptr))
{
    mUI.setupUi(this);

    mGifPlayer->resize(656, 492);

    connect(mUI.showGif, &QPushButton::clicked, mGifPlayer, &QWidget::show);
    connect(mUI.exportData, &QPushButton::clicked, this, &DataAnalysis::slotExportData);
    connect(mUI.dataDisplay, &QPushButton::clicked, this, &DataAnalysis::slotSwithDataDisplay);
    connect(mUI.vesselGraph, &QPushButton::clicked, this, &DataAnalysis::slotSwitchVesselGraph);

    mDADataDisplayScene = new DADataDisplayScene(mUI, mImageList, nullptr);
    mUI.view->setScene(mDADataDisplayScene);

    //mDAVesselGraphScene = new DAVesselGraphScene(mUI, mImageList, nullptr);

    this->setFocus(Qt::FocusReason::NoFocusReason);
    mUI.view->setFocus();
    mUI.view->setSceneRect(0, 0, 656, 492);
}

DataAnalysis::~DataAnalysis()
{
    delete mGifPlayer;
    mGifPlayer = nullptr;

    delete mDADataDisplayScene;
    mDADataDisplayScene = nullptr;
}

void DataAnalysis::setVideoAbsPath(const QString &videoAbsPath)
{
    mVideoFileInfo = videoAbsPath;
    mGifPlayer->setWindowTitle(videoAbsPath);
    mDADataDisplayScene->setVideoAbsPath(videoAbsPath);
}

void DataAnalysis::setImageList(QVector<QImage>& imagelist)
{
    mImageList = std::move(imagelist);

    mDADataDisplayScene->updateFrameCount(mImageList.size());
}

void DataAnalysis::setFirstSharpImageIndex(int firstSharpImageIndex)
{
    mDADataDisplayScene->setFirstSharpImageIndex(firstSharpImageIndex);
}

void DataAnalysis::updateVesselInfo(VesselInfo &vesselInfo)
{
    mVesselInfo = vesselInfo;
    mDADataDisplayScene->updateVesselInfo(mVesselInfo);

    mUI.dataDisplayList->setRowCount(mVesselInfo.vesselNumber);
    for(int i = 0; i < mVesselInfo.vesselNumber; ++i)
    {
        QTableWidgetItem* item = new QTableWidgetItem;
        item->setText(QString::number(mVesselInfo.diameters[i]));
        mUI.dataDisplayList->setItem(i, 0, item);

        item = new QTableWidgetItem;
        item->setText(QString::number(mVesselInfo.lengths[i]));
        mUI.dataDisplayList->setItem(i, 1, item);

        item = new QTableWidgetItem;
        item->setText(QString::number(mVesselInfo.flowrates[i]));
        mUI.dataDisplayList->setItem(i, 2, item);

        item = new QTableWidgetItem;
        item->setText(QString::number(mVesselInfo.glycocalyx[i]));
        mUI.dataDisplayList->setItem(i, 3, item);
    }
}

void DataAnalysis::closeEvent(QCloseEvent *e)
{
    e->accept();

    this->hide();

    emit signalExit(this);
}

void DataAnalysis::slotExportData()
{
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

    if(mVesselInfo.pkVesselInfoID.isEmpty())
    {
        return;
    }

    QFileInfo excelPath = currentDir.absoluteFilePath(mVideoFileInfo.fileName() + ".csv");

    QFile excelFile(excelPath.absoluteFilePath());
    if(!excelFile.open(QIODevice::OpenModeFlag::WriteOnly))
    {
        QMessageBox::information(nullptr, QStringLiteral("导出失败"), QStringLiteral("文件已存在或创建文件失败"));
    }

    excelFile.write(QStringLiteral(", 直径, 长度, 流速, 糖萼\n").toUtf8());

    //设置表格数据
    double diametersSum = 0.0, lengthSum = 0.0, flowrateSum = 0.0, glycocalyxSum = 0.0;
    int glycocalyxCount = 0;
    for(int i = 0; i < mVesselInfo.vesselNumber; ++i)
    {
        QString writeRow(", %1, %2, %3, %4\n");
        writeRow = writeRow.arg(mVesselInfo.diameters[i]).arg(mVesselInfo.lengths[i]).arg(mVesselInfo.flowrates[i]).arg(mVesselInfo.glycocalyx[i]);
        diametersSum += mVesselInfo.diameters[i];
        lengthSum += mVesselInfo.lengths[i];
        flowrateSum += mVesselInfo.flowrates[i];
        if(-mVesselInfo.glycocalyx[i] != 1.0)
        {
            glycocalyxSum += mVesselInfo.glycocalyx[i];
            ++glycocalyxCount;
        }
        excelFile.write(writeRow.toUtf8());
    }

    excelFile.write(QStringLiteral("平均值：, %1, %2, %3, %4\n")
                    .arg(diametersSum / mVesselInfo.vesselNumber)
                    .arg(lengthSum / mVesselInfo.vesselNumber)
                    .arg(flowrateSum / mVesselInfo.vesselNumber)
                    .arg(glycocalyxSum / glycocalyxCount)
                    .toUtf8());

    excelFile.close();

    QMessageBox::information(nullptr, QStringLiteral("导出"), QStringLiteral("导出完成"));
}

void DataAnalysis::slotSwithDataDisplay()
{
    mUI.view->setScene(mDADataDisplayScene);
}

void DataAnalysis::slotSwitchVesselGraph()
{
    //mUI.view->setScene(mDAVesselGraphScene);
}

