#include "DataAnalysis.h"

DataAnalysis::DataAnalysis(double fps, QWidget *p):
    QWidget(p), mGifPlayer(new GifPlayer(mImageList, fps, nullptr)), mVesselData(nullptr)
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

void DataAnalysis::updateVesselData(const VesselData &vesselData)
{
    mVesselData = &vesselData;
    mDADataDisplayScene->updateVesselData(vesselData);

    mUI.dataDisplayList->setRowCount(vesselData.vesselNumber);
    for(int i = 0; i < vesselData.vesselNumber; ++i)
    {
        QTableWidgetItem* item = new QTableWidgetItem;
        item->setText(QString::number(vesselData.diameters[i]));
        mUI.dataDisplayList->setItem(i, 0, item);

        item = new QTableWidgetItem;
        item->setText(QString::number(vesselData.lengths[i]));
        mUI.dataDisplayList->setItem(i, 1, item);

        item = new QTableWidgetItem;
        item->setText(QString::number(vesselData.flowrates[i]));
        mUI.dataDisplayList->setItem(i, 2, item);

        item = new QTableWidgetItem;
        item->setText(QString::number(vesselData.glycocalyx[i]));
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

    if(!mVesselData)
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
    for(int i = 0; i < mVesselData->vesselNumber; ++i)
    {
        QString writeRow(", %1, %2, %3, %4\n");
        writeRow = writeRow.arg(mVesselData->diameters[i]).arg(mVesselData->lengths[i]).arg(mVesselData->flowrates[i]).arg(mVesselData->glycocalyx[i]);
        diametersSum += mVesselData->diameters[i];
        lengthSum += mVesselData->lengths[i];
        flowrateSum += mVesselData->flowrates[i];
        glycocalyxSum += mVesselData->glycocalyx[i];
        excelFile.write(writeRow.toUtf8());
    }

    excelFile.write(QStringLiteral("平均值：, %1, %2, %3, %4\n")
                    .arg(diametersSum / mVesselData->vesselNumber)
                    .arg(lengthSum / mVesselData->vesselNumber)
                    .arg(flowrateSum / mVesselData->vesselNumber)
                    .arg(glycocalyxSum / mVesselData->vesselNumber)
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

