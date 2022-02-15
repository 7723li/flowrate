#pragma once

#include "ui_CameraParamWidget.h"

#include <QWidget>
#include <QLibrary>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QTimer>
#include <QThread>
#include <QRunnable>
#include <QDateTime>
#include <QFileInfo>
#include <QLineEdit>
#include <QLabel>
#include <QCloseEvent>
#include <QThreadPool>
#include <QRunnable>

#include <functional>

#ifdef _WIN32
#include <Windows.h>
#include <Dbt.h>
#elif __unix
#endif

#include "CameraObjectParamProxy.h"

class GenericCameraModule;

class AsyncCameraObserver : public QRunnable
{
public:
	AsyncCameraObserver(std::function<void()> func) : mFunc(func) {}

protected:
	virtual void run() override { mFunc(); }

private:
	std::function<void()> mFunc;
};

/*!
* @brief
* 相机参数设置控件 不对一般人开放
* 
* @attention
* 能看到用到的都是人上人
*/
class CameraParamWidget : public QWidget
{
    Q_OBJECT

    using initFunc = void*(std::vector<GenericCameraModule*>&);
    using cameraTypeFunc = void*(GenericCameraModule*, std::string&);
    using avaliableCameraDisplayNameListFunc = void*(GenericCameraModule*, std::vector<std::string>&);
    using getOpenedCameraParamFunc = void*(GenericCameraModule*, std::vector<CameraParamStruct>&);
    using setOpenedCameraParamFunc = void*(GenericCameraModule*, CameraParamStruct);

public:
    explicit CameraParamWidget(QLibrary& lib, QWidget* p = nullptr);
    ~CameraParamWidget();

    GenericCameraModule* getUsingCamera() const;

    std::string getUsingCameraSN() const;

    /*!
     * @brief
     * 更新当前相机的参数列表
     */
    void updateCameraParam(std::vector<CameraParamStruct>& cameraParams);

public slots:
    void changeCameraParam(CameraParamStruct* cameraParamPtr);

public:
    /*!
     * @brief
     * 设置为相机被打开的样式
     */
    void switchCameraOpeningStyle();

    /*!
     * @brief
     * 设置为相机被关闭的样式
     */
    void switchCameraClosingStyle();

    /*!
     * @brief
     * 设置为相机在采集的样式
     */
    void switchCameraCapturingStyle();

    /*!
     * @brief
     * 设置为相机在采集的样式
     */
    void switchCameraUncapturingStyle();

public slots:
    /*!
     * @brief
     * 异步更新各品牌已经与主机相连的相机列表
     */
    void asyncUpdateAvaliableCameras();

    /*!
     * @brief
     * 处理treeWidget发生变化的信号 此处只处理单击勾选的情况
     */
    void slotCameraTreeWidgetClicked(QTreeWidgetItem *item, int column);

protected:
    virtual void closeEvent(QCloseEvent* e) override;

	/*!
	* @brief
	* 实行异步更新相机列表
	*/
	void doAsyncUpdateAvaliableCameras();

private:
    /*!
     * @brief
     * 清理一棵树
     */
    inline void clearTopLevelTreeNode(GenericCameraModule* co);

    /*!
     * @brief
     * 更新与主机相连的相机列表后 刷新树状列表
     */
    void updateCameraTreeWidget(GenericCameraModule* co, const std::vector<std::string>& nameList, const std::vector<std::string>& SNList);

signals:
    void signalOpenCamera();
    void signalCloseCamera();
    void signalBeginCapture();
    void signalStopCapture();

	//void signalUSBDeviceInsert();

    void signalUpdateCameraListFinish(int cameraCount);

private:
    Ui_CameraParamWidget mUI;

    QTreeWidgetItem* mCurrentTreeItem;              // 当前选中的相机行

    GenericCameraModule* mUsingCamera;
    std::string mUsingCameraSN;

    QLibrary& mLib;                                 // 动态库
    QFunctionPointer mCommonFuncPtr;                // 通用动态库函数指针

    std::vector<GenericCameraModule*> mCameraTypes;                                        // 可用相机列表
    std::map<GenericCameraModule*,QTreeWidgetItem*> mMapCameraToTopLevelTree;              // 配对相机和对应的树结构顶点 程序运行过程中无需操作

    QMap<QTreeWidgetItem*, QPair<GenericCameraModule*, std::string>> mMapCameraLineToCameraObjectAndSN;     // 配对相机行和相机系统以及SN

    std::vector<CameraParamStruct> mCameraParams;   // 相机参数
    CameraObjectParamProxy mCameraParamProxy;       // 相机参数设置代理
};
