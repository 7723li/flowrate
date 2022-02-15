#include "CameraParamWidget.h"

CameraParamWidget::CameraParamWidget(QLibrary& lib, QWidget *p) :
QWidget(p), mCurrentTreeItem(nullptr), mLib(lib)
{
    mUI.setupUi(this);

    mUI.treeWidget->setColumnCount(2);
    mUI.treeWidget->setHeaderHidden(true);

    mCommonFuncPtr = mLib.resolve("init");
	((initFunc*)mCommonFuncPtr)(mCameraTypes);

    mCommonFuncPtr = mLib.resolve("cameraType");

    for(GenericCameraModule* co : mCameraTypes)
    {
        std::string cameraType;

        ((cameraTypeFunc*)mCommonFuncPtr)(co, cameraType);

        if(!cameraType.empty())
        {
            qDebug() << QString::fromStdString(cameraType);

            QTreeWidgetItem* tCameraTypeItem = new QTreeWidgetItem;
            tCameraTypeItem->setText(0, QString::fromStdString(cameraType));
            mUI.treeWidget->addTopLevelItem(tCameraTypeItem);

            mMapCameraToTopLevelTree[co] = tCameraTypeItem;
        }
    }
    mUI.captureButton->setEnabled(false);

    connect(mUI.treeWidget, &QTreeWidget::itemClicked, this, &CameraParamWidget::slotCameraTreeWidgetClicked);

    connect(mUI.refreshButton, &QPushButton::clicked, this, &CameraParamWidget::asyncUpdateAvaliableCameras);
    connect(mUI.applyButton, &QPushButton::clicked, this, &CameraParamWidget::signalOpenCamera);
    connect(mUI.captureButton, &QPushButton::clicked, this, &CameraParamWidget::signalBeginCapture);

    connect(&mCameraParamProxy, &CameraObjectParamProxy::signalParamChanged, this, &CameraParamWidget::changeCameraParam);
}

CameraParamWidget::~CameraParamWidget()
{

}

GenericCameraModule* CameraParamWidget::getUsingCamera() const
{
    return mUsingCamera;
}

std::string CameraParamWidget::getUsingCameraSN() const
{
    return mUsingCameraSN;
}

void CameraParamWidget::updateCameraParam(std::vector<CameraParamStruct>& cameraParams)
{
    mCameraParams = std::move(cameraParams);

    qDebug() << "tCameraParamList.size()" << mCameraParams.size();

    mCameraParamProxy.updateCameraParam(mCameraParams, mUI.cameraObjectParamWidget, mUI.cameraObjectParamVLayout);
}

void CameraParamWidget::changeCameraParam(CameraParamStruct *cameraParamPtr)
{
    mCommonFuncPtr = mLib.resolve("setOpenedCameraParam");
    ((setOpenedCameraParamFunc*)mCommonFuncPtr)(mUsingCamera, *cameraParamPtr);
}

void CameraParamWidget::switchCameraOpeningStyle()
{
    mUI.refreshButton->setEnabled(false);
    mUI.captureButton->setEnabled(true);
    mUI.treeWidget->setEnabled(false);
    mUI.cameraObjectParamWidget->setEnabled(true);

    mUI.applyButton->setText(QApplication::translate("CameraObjectCallWidget", "\346\226\255\345\274\200\350\277\236\346\216\245", Q_NULLPTR));    // 断开连接
    disconnect(mUI.applyButton, nullptr, nullptr, nullptr);
    connect(mUI.applyButton, &QPushButton::clicked, this, &CameraParamWidget::signalCloseCamera);
}

void CameraParamWidget::switchCameraClosingStyle()
{
    mUI.applyButton->setText(QApplication::translate("CameraObjectCallWidget", "\346\211\223\345\274\200", Q_NULLPTR)); // 连接
    disconnect(mUI.applyButton, nullptr, nullptr, nullptr);
    connect(mUI.applyButton, &QPushButton::clicked, this, &CameraParamWidget::signalOpenCamera);

    mUI.captureButton->setEnabled(false);
    mUI.refreshButton->setEnabled(true);
    mUI.treeWidget->setEnabled(true);
    mUI.cameraObjectParamWidget->setEnabled(false);
}

void CameraParamWidget::switchCameraCapturingStyle()
{
    disconnect(mUI.captureButton, nullptr, nullptr, nullptr);
    connect(mUI.captureButton, &QPushButton::clicked, this, &CameraParamWidget::signalStopCapture);
    mUI.captureButton->setText(QApplication::translate("CameraObjectCallWidget", "\345\201\234\346\255\242\351\207\207\351\233\206", Q_NULLPTR));  // 停止采集

    mUI.cameraObjectParamWidget->setEnabled(false);
}

void CameraParamWidget::switchCameraUncapturingStyle()
{
    disconnect(mUI.captureButton, nullptr, nullptr, nullptr);
    connect(mUI.captureButton, &QPushButton::clicked, this, &CameraParamWidget::signalBeginCapture);
    mUI.captureButton->setText(QApplication::translate("CameraObjectCallWidget", "\351\207\207\351\233\206", Q_NULLPTR));  // 采集

    mUI.cameraObjectParamWidget->setEnabled(true);
}

void CameraParamWidget::asyncUpdateAvaliableCameras()
{
	std::function<void()> func = std::bind(&CameraParamWidget::doAsyncUpdateAvaliableCameras, this);
	AsyncCameraObserver* pvrco = new AsyncCameraObserver(func);
	pvrco->setAutoDelete(true);
	QThreadPool::globalInstance()->start(pvrco);
}

void CameraParamWidget::slotCameraTreeWidgetClicked(QTreeWidgetItem *item, int column)
{
    if(column != 0)
    {
        return;
    }

    if(item != mCurrentTreeItem)
    {
        if(mCurrentTreeItem)
        {
            mCurrentTreeItem->setCheckState(0, Qt::CheckState::Unchecked);
        }
        mCurrentTreeItem = item;

        mUsingCamera = mMapCameraLineToCameraObjectAndSN[mCurrentTreeItem].first;
        mUsingCameraSN = mMapCameraLineToCameraObjectAndSN[mCurrentTreeItem].second;
    }
    else
    {
        if(item->checkState(0) == Qt::CheckState::Unchecked)
        {
            mCurrentTreeItem = nullptr;

            mUsingCamera = nullptr;
            mUsingCameraSN.clear();
        }
    }
}

void CameraParamWidget::closeEvent(QCloseEvent *e)
{
    e->accept();
    this->hide();
}

void CameraParamWidget::doAsyncUpdateAvaliableCameras()
{
	mUI.refreshButton->setEnabled(false);

	qDebug() << "update avaliable cameras begin" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

	mMapCameraLineToCameraObjectAndSN.clear();
	mUsingCamera = nullptr;
	mUsingCameraSN.clear();
	if (mCurrentTreeItem)
	{
		mCurrentTreeItem->setCheckState(0, Qt::CheckState::Unchecked);
	}

	int cameraCount = 0;

	for (GenericCameraModule* co : mCameraTypes)
	{
		std::vector<std::string> tNameList;
		std::vector<std::string> tSNList;

		mCommonFuncPtr = mLib.resolve("avaliableCameraDisplayNameList");
		((avaliableCameraDisplayNameListFunc*)mCommonFuncPtr)(co, tNameList);

		mCommonFuncPtr = mLib.resolve("avaliableCameraSNList");
		((avaliableCameraDisplayNameListFunc*)mCommonFuncPtr)(co, tSNList);

		clearTopLevelTreeNode(co);

		if (tNameList.size() == tSNList.size())
		{
			cameraCount += tSNList.size();

			updateCameraTreeWidget(co, tNameList, tSNList);
		}
	}

	qDebug() << "update avaliable cameras done" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

	emit signalUpdateCameraListFinish(cameraCount);

	mUI.refreshButton->setEnabled(true);
}

void CameraParamWidget::clearTopLevelTreeNode(GenericCameraModule *co)
{
    QTreeWidgetItem* tTopLevel = mMapCameraToTopLevelTree[co];
    QList<QTreeWidgetItem*>&& children = tTopLevel->takeChildren();
    for(QTreeWidgetItem* item : children)
    {
        tTopLevel->removeChild(item);
        delete item;
        item = nullptr;
    }

    mCurrentTreeItem = nullptr;
}

void CameraParamWidget::updateCameraTreeWidget(GenericCameraModule *co, const std::vector<std::string>& nameList, const std::vector<std::string>& SNList)
{
    int index = 0;
    QTreeWidgetItem* tTopLevel = mMapCameraToTopLevelTree[co];

    for(const std::string& name : nameList)
    {
        qDebug() << QString::fromStdString(name);

        QTreeWidgetItem* tCameraDeviceItem = new QTreeWidgetItem;
        tCameraDeviceItem->setCheckState(0, Qt::CheckState::Unchecked);
        tCameraDeviceItem->setText(1, QString::fromStdString(name));
        tCameraDeviceItem->setToolTip(1, QString::fromStdString(name));

        tTopLevel->addChild(tCameraDeviceItem);

        if(mMapCameraLineToCameraObjectAndSN.empty())
        {
            mUsingCamera = co;
            mUsingCameraSN = SNList[index];
            mCurrentTreeItem = tCameraDeviceItem;
            mCurrentTreeItem ->setCheckState(0, Qt::CheckState::Checked);
        }

        mMapCameraLineToCameraObjectAndSN[tCameraDeviceItem] = QPair<GenericCameraModule*, std::string>(co, SNList[index]);
        ++index;
    }

    mUI.treeWidget->expandAll();
}
