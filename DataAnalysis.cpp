#include "DataAnalysis.h"

DataAnalysis::DataAnalysis(double fps, QWidget *p):
    QWidget(p), mGifPlayer(new GifPlayer(mImageList, nullptr))
{
    mUI.setupUi(this);
    mUI.sceneWidgetScrollArea->setAlignment(Qt::AlignmentFlag::AlignCenter);
    mUI.stackedWidget->setCurrentWidget(mUI.pageDataDisplay);

    QWidget* useless = mUI.sceneWidgetScrollArea->takeWidget();
    delete useless;
    useless = nullptr;
    mUI.scrollAreaWidgetContents = nullptr;

    connect(mUI.showGif, &QPushButton::clicked, mGifPlayer, &QWidget::show);
    connect(mUI.exportData, &QPushButton::clicked, this, &DataAnalysis::slotExportData);
    connect(mUI.dataDisplay, &QPushButton::clicked, this, &DataAnalysis::slotSwithDataDisplay);
    connect(mUI.vesselGraph, &QPushButton::clicked, this, &DataAnalysis::slotSwitchVesselGraph);
    connect(mUI.dataDisplayList, &QTableWidget::doubleClicked, this, &DataAnalysis::slotDataDisplayListDoubleClick);

    mGifPlayer->resize(656, 492);

    mDADataDisplaySceneWidget = new DADataDisplaySceneWidget(mUI, mImageList, nullptr);
    mUI.sceneWidgetScrollArea->setWidget(mDADataDisplaySceneWidget);
    mDADataDisplaySceneWidget->setGeometry(0, 0, 656, 492);

    mDAVesselGraphSceneWidget = new DAVesselGraphSceneWidget(mUI, mImageList, nullptr);
    connect(mDAVesselGraphSceneWidget, &DAVesselGraphSceneWidget::signalUpdateVesselInfo, this, &DataAnalysis::updateVesselInfo);

    this->setFocusPolicy(Qt::FocusPolicy::NoFocus);
    mDADataDisplaySceneWidget->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    mDADataDisplaySceneWidget->setFocus(Qt::FocusReason::MouseFocusReason);
    mDAVesselGraphSceneWidget->setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    mDAVesselGraphSceneWidget->setFocus(Qt::FocusReason::MouseFocusReason);
}

DataAnalysis::~DataAnalysis()
{
    delete mGifPlayer;
    mGifPlayer = nullptr;

    delete mDADataDisplaySceneWidget;
    mDADataDisplaySceneWidget = nullptr;

    delete mDAVesselGraphSceneWidget;
    mDAVesselGraphSceneWidget = nullptr;
}

void DataAnalysis::setVideoInfoMation(const QString &videoAbsPath, double pixelSize, int magnification, double fps)
{
    mUI.videoAbsPath->setText(videoAbsPath);
    mUI.videoAbsPath->setToolTip(videoAbsPath);
    mUI.fps->setNum(fps);
    mUI.pixelsize->setNum(pixelSize);
    mUI.magnification->setNum(magnification);

    mGifPlayer->setFps(fps);

    mVideoFileInfo = videoAbsPath;
    mGifPlayer->setWindowTitle(videoAbsPath);
    mDADataDisplaySceneWidget->setVideoAbsPath(videoAbsPath);
}

void DataAnalysis::setImageList(QVector<QImage>& imagelist)
{
    mImageList = std::move(imagelist);

    mDADataDisplaySceneWidget->updateFrameCount(mImageList.size());

    if(!mImageList.empty())
    {
        mUI.imagesize->setText(QString("(%1, %2)").arg(mImageList.front().width()).arg(mImageList.front().height()));
        mDADataDisplaySceneWidget->setGeometry(0, 0, mImageList.front().width(), mImageList.front().height());
        mDAVesselGraphSceneWidget->setGeometry(0, 0, mImageList.front().width(), mImageList.front().height());
    }
}

void DataAnalysis::setFirstSharpImageIndex(int firstSharpImageIndex)
{
    mDADataDisplaySceneWidget->setFirstSharpImageIndex(firstSharpImageIndex);
    mDAVesselGraphSceneWidget->setFirstSharpImageIndex(firstSharpImageIndex);
}

void DataAnalysis::updateVesselInfo(VesselInfo &vesselInfo)
{
    mVesselInfo = vesselInfo;
    mDADataDisplaySceneWidget->updateVesselInfo(mVesselInfo);
    mDAVesselGraphSceneWidget->updateVesselInfo(mVesselInfo);

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
    if(mDAVesselGraphSceneWidget->isSaving())
    {
        QMessageBox messageBox(QMessageBox::Icon::Warning, QStringLiteral("警告"), QStringLiteral("正在更新保存中 现在关闭会丢失未保存的操作"),
                               /*QMessageBox::StandardButton::Ok | */QMessageBox::StandardButton::No, nullptr);
        //messageBox.button(QMessageBox::StandardButton::Ok)->setText(QStringLiteral("依然退出"));
        messageBox.button(QMessageBox::StandardButton::No)->setText(QStringLiteral("先等一等"));
        messageBox.setDefaultButton(QMessageBox::StandardButton::No);
        messageBox.setWindowIcon(QIcon(":/icons/image/icon.ico"));
        int button = messageBox.exec();
        if(button != QMessageBox::Ok)
        {
            e->ignore();
            return;
        }
    }

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
    mUI.stackedWidget->setCurrentWidget(mUI.pageDataDisplay);
    mUI.sceneWidgetScrollArea->takeWidget();
    mUI.sceneWidgetScrollArea->setWidget(mDADataDisplaySceneWidget);
}

void DataAnalysis::slotSwitchVesselGraph()
{
    mUI.stackedWidget->setCurrentWidget(mUI.pageVesselGraph);
    mUI.sceneWidgetScrollArea->takeWidget();
    mUI.sceneWidgetScrollArea->setWidget(mDAVesselGraphSceneWidget);
}

void DataAnalysis::slotDataDisplayListDoubleClick(const QModelIndex &index)
{
    return;
}
