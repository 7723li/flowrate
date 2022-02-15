#pragma once

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QLayout>
#include <QDebug>

typedef enum CameraParamType
{
	NOPROCESS = -1,             // 不处理的类型
	CameraParamType_INT = 0,
	CameraParamType_FLOAT,
	CameraParamType_ENUM,
	CameraParamType_BOOL,
	CameraParamType_STRING,
}
CameraParamType;

typedef struct CameraParamStruct
{
	CameraParamStruct() : type(CameraParamType::NOPROCESS)
	{

	}

	CameraParamType type;

	std::string name;

	std::string value;

	std::string min;
	std::string max;

	std::vector<std::string> entryList;
}
CameraParamStruct;

/*!
 * @brief
 * 参数设置控件代理
 */
class CameraObjectParamProxy : public QObject
{
    Q_OBJECT

public:
    ~CameraObjectParamProxy()
    {
        clear();
    }

    /*!
     * @brief
     * 更新当前在用的相机参数列表
     */
    void updateCameraParam(std::vector<CameraParamStruct>& cameraParams, QWidget* scrollWidget, QVBoxLayout* layout)
    {
        clear();

        for(CameraParamStruct& cameraParam : cameraParams)
        {
            QWidget* cameraParamWidgetBackground = nullptr;

            if(cameraParam.type == CameraParamType::CameraParamType_INT)
            {
                cameraParamWidgetBackground = newIntWidget(cameraParam, scrollWidget);
            }
            else if(cameraParam.type == CameraParamType::CameraParamType_FLOAT)
            {
                cameraParamWidgetBackground = newFloatWidget(cameraParam, scrollWidget);
            }
            else if(cameraParam.type == CameraParamType::CameraParamType_ENUM)
            {
                cameraParamWidgetBackground = newEnumWidget(cameraParam, scrollWidget);
            }
            else if(cameraParam.type == CameraParamType::CameraParamType_STRING)
            {
                cameraParamWidgetBackground = newStringWidget(cameraParam, scrollWidget);
            }
            else if(cameraParam.type == CameraParamType::CameraParamType_BOOL)
            {
                cameraParamWidgetBackground = newBoolWidget(cameraParam, scrollWidget);
            }

            if(cameraParamWidgetBackground)
            {
                layout->addWidget(cameraParamWidgetBackground);

                cameraParamWidgetBackground->show();
                mVecParamWidgets.push_back(cameraParamWidgetBackground);
            }
        }
    }

private:
    void clear()
    {
        qDebug() << "CameraObjectParamProxy::clear begin";

        for(QWidget* widget : mVecParamWidgets)
        {
            delete widget;
            widget = nullptr;
        }
        mVecParamWidgets.clear();
        mMapValueWidgetToParam.clear();

        qDebug() << "CameraObjectParamProxy::clear success";
    }

    QWidget* newIntWidget(CameraParamStruct& cameraParam, QWidget* scrollWidget)
    {
        QWidget* cameraParamWidgetBackground = new QWidget(scrollWidget);

        QHBoxLayout* hLayout = new QHBoxLayout(cameraParamWidgetBackground);
        cameraParamWidgetBackground->setLayout(hLayout);

        QLabel* name = new QLabel(cameraParamWidgetBackground);
        name->setText(QString::fromStdString(cameraParam.name));

        QLineEdit* value = new QLineEdit(cameraParamWidgetBackground);
        value->setText(QString::fromStdString(cameraParam.value));

        mMapValueWidgetToParam[value] = &cameraParam;
        connect(value, &QLineEdit::returnPressed, this, &CameraObjectParamProxy::slotLineEditChanged);

        QLabel* min = new QLabel(cameraParamWidgetBackground);
        min->setText(QString::fromStdString(cameraParam.min));

        QLabel* max = new QLabel(cameraParamWidgetBackground);
        max->setText(QString::fromStdString(cameraParam.max));

        hLayout->addWidget(name);
        hLayout->addWidget(value);
        hLayout->addWidget(min);
        hLayout->addWidget(max);

        hLayout->setStretch(0, 1);
        hLayout->setStretch(1, 1);
        hLayout->setStretch(2, 1);
        hLayout->setStretch(3, 1);

        return cameraParamWidgetBackground;
    }

    QWidget* newFloatWidget(CameraParamStruct& cameraParam, QWidget* scrollWidget)
    {
        QWidget* cameraParamWidgetBackground = new QWidget(scrollWidget);

        QHBoxLayout* hLayout = new QHBoxLayout(cameraParamWidgetBackground);
        cameraParamWidgetBackground->setLayout(hLayout);

        QLabel* name = new QLabel(cameraParamWidgetBackground);
        name->setText(QString::fromStdString(cameraParam.name));

        QLineEdit* value = new QLineEdit(cameraParamWidgetBackground);
        value->setText(QString::fromStdString(cameraParam.value));

        mMapValueWidgetToParam[value] = &cameraParam;
        connect(value, &QLineEdit::returnPressed, this, &CameraObjectParamProxy::slotLineEditChanged);

        QLabel* min = new QLabel(cameraParamWidgetBackground);
        min->setText(QString::fromStdString(cameraParam.min));

        QLabel* max = new QLabel(cameraParamWidgetBackground);
        max->setText(QString::fromStdString(cameraParam.max));

        hLayout->addWidget(name);
        hLayout->addWidget(value);
        hLayout->addWidget(min);
        hLayout->addWidget(max);

        hLayout->setStretch(0, 1);
        hLayout->setStretch(1, 1);
        hLayout->setStretch(2, 1);
        hLayout->setStretch(3, 1);

        return cameraParamWidgetBackground;
    }

    QWidget* newStringWidget(CameraParamStruct& cameraParam, QWidget* scrollWidget)
    {
        QWidget* cameraParamWidgetBackground = new QWidget(scrollWidget);

        QHBoxLayout* hLayout = new QHBoxLayout(cameraParamWidgetBackground);
        cameraParamWidgetBackground->setLayout(hLayout);

        QLabel* name = new QLabel(cameraParamWidgetBackground);
        name->setText(QString::fromStdString(cameraParam.name));

        QLabel* value = new QLabel(cameraParamWidgetBackground);
        value->setText(QString::fromStdString(cameraParam.value));

        hLayout->addWidget(name);
        hLayout->addWidget(value);

        hLayout->setStretch(0, 1);
        hLayout->setStretch(1, 1);

        return cameraParamWidgetBackground;
    }

    QWidget* newEnumWidget(CameraParamStruct& cameraParam, QWidget* scrollWidget)
    {
        QWidget* cameraParamWidgetBackground = new QWidget(scrollWidget);

        QHBoxLayout* hLayout = new QHBoxLayout(cameraParamWidgetBackground);
        cameraParamWidgetBackground->setLayout(hLayout);

        QLabel* name = new QLabel(cameraParamWidgetBackground);
        name->setText(QString::fromStdString(cameraParam.name));

        QComboBox* value = new QComboBox(cameraParamWidgetBackground);
        for(const std::string& entry : cameraParam.entryList)
        {
            value->addItem(QString::fromStdString(entry));
        }
        value->setCurrentText(QString::fromStdString(cameraParam.value));

        mMapValueWidgetToParam[value] = &cameraParam;
        connect(value, &QComboBox::currentTextChanged, this, &CameraObjectParamProxy::slotComboBoxChanged);

        hLayout->addWidget(name);
        hLayout->addWidget(value);

        hLayout->setStretch(0, 1);
        hLayout->setStretch(1, 1);

        return cameraParamWidgetBackground;
    }

    QWidget* newBoolWidget(CameraParamStruct& cameraParam, QWidget* scrollWidget)
    {
        QWidget* cameraParamWidgetBackground = new QWidget(scrollWidget);

        QHBoxLayout* hLayout = new QHBoxLayout(cameraParamWidgetBackground);
        cameraParamWidgetBackground->setLayout(hLayout);

        QLabel* name = new QLabel(cameraParamWidgetBackground);
        name->setText(QString::fromStdString(cameraParam.name));

        QComboBox* value = new QComboBox(cameraParamWidgetBackground);
        value->addItem("0");
        value->addItem("1");
        value->setCurrentText(QString::fromStdString(cameraParam.value));

        mMapValueWidgetToParam[value] = &cameraParam;
        connect(value, &QComboBox::currentTextChanged, this, &CameraObjectParamProxy::slotComboBoxChanged);

        hLayout->addWidget(name);
        hLayout->addWidget(value);

        hLayout->setStretch(0, 1);
        hLayout->setStretch(1, 1);

        return cameraParamWidgetBackground;
    }

private slots:
    void slotLineEditChanged()
    {
        QLineEdit* changer = static_cast<QLineEdit*>(this->sender());
        mMapValueWidgetToParam[this->sender()]->value = changer->text().toStdString();

        emit signalParamChanged(mMapValueWidgetToParam[this->sender()]);
    }

    void slotComboBoxChanged(const QString& text)
    {
        mMapValueWidgetToParam[this->sender()]->value = text.toStdString();

        emit signalParamChanged(mMapValueWidgetToParam[this->sender()]);
    }

signals:
    void signalParamChanged(CameraParamStruct* cameraParamPtr);

private:
    std::vector<QWidget*> mVecParamWidgets;                         // 相机参数设置控件
    std::map<QObject*, CameraParamStruct*> mMapValueWidgetToParam;  // 配对数值更改控件和参数
};
