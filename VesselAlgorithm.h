#pragma once

#include <QImage>
#include <QVector>
#include <QPoint>
#include <QDebug>

#include "HalconBase.h"

using namespace HalconCpp;
using RegionPoints = QVector<QPoint>;

struct VesselData
{
    VesselData() : vesselNumber(0){}

    QString absVideoPath;
    int vesselNumber;
    QVector<RegionPoints> regionsSkeletonPoints;
    QVector<RegionPoints> regionsBorderPoints;
    QVector<RegionPoints> regionsUnionPoints;
    QVector<double> diameters;
    QVector<double> lengths;
    QVector<double> glycocalyx;
    QVector<double> flowrates;
};

/*!
 * @brief
 * 流速算法接口类
 *
 * @author  lzx
 * @date    2022/01/01
 */
class VesselAlgorithm : public HalconInterfaceBase
{
public:
    /*!
     * @brief
     * HALCON导出函数 计算单帧的清晰度数值
     */
    static void getImageSharpness(const QImage& image, double& sharpness, bool& isSharp);

    /*!
     * @brief
     * 输入血管图像序列 分割为若干段血管
     *
     * @param imagelist             输入的血管图像序列
     * @param regionsSplitedPoints  输出的分割区域坐标点集合
     * @param diameters             血管直径
     * @param lengths               血管长度
     * @param type                  区域形式
     *                              Union       原区域
     *                              Border      外边缘
     *                              Skeleton    中心线
     */
    static void splitVesslRegion(const QVector<QImage>& imagelist, double pixelSize, int magnification, QVector<RegionPoints>& regionsSplitedPoints, QVector<double>& diameters, QVector<double>& lengths, QString type = "Skeleton");

    /*!
     * @brief
     * 计算图像序列的糖萼尺寸
     *
     * @param imagelist         输入的图像序列
     * @param pixelSize         像素尺寸
     * @param magnification     放大倍率
     * @param glycocalyx        糖萼尺寸
     */
    static void calculateGlycocalyx(const QVector<QImage>& imagelist, double pixelSize, int magnification, QVector<double>& glycocalyx);

    /*!
     * @brief
     * 计算两帧之间的流动速度
     *
     * @param imagePrev         前帧
     * @param imageRear         后帧
     * @param fps               帧率
     * @param pixelSize         像素尺寸
     * @param magnification     放大倍率
     * @param flowrate          流速
     */
    static void calculateFlowrate(QImage imagePrev, QImage imageRear, double pixelSize, int magnification, double fps, double& flowrate);

    /*!
     * @brief
     * 计算图像序列之间的流动速度
     *
     * @param imagelist         输入的图像序列
     * @param fps               帧率
     * @param pixelSize         像素尺寸
     * @param magnification     放大倍率
     * @param flowrates         流速
     *
     * @attention
     * 输出的结果总为输入的图像序列大小减一
     */
    static void calculateFlowrate(const QVector<QImage>& imagelist, double pixelSize, int magnification, double fps, QVector<double>& flowrates);

    /*!
     * @brief
     * 一条龙服务
     */
    static void calculateAll(const QVector<QImage>& imagelist, double pixelSize, int magnification, double fps, VesselData& vesselData, int& firstSharpImageIndex);

private:
    /*!
     * @brief
     * 将QImage转换为HObject格式
     */
    static void convertQImageToHObject(const QImage& image, HObject *imageObj)
    {
        Q_ASSERT(image.depth() == 1);

        // 使用GenImage1将QImage的图像数据复制到HObject
        // 这里确保了QImage是单通道灰度图 因此hdev中原程序的格式转换步骤都可以忽略
        GenImage1(imageObj, "byte", image.width(), image.height(), (Hlong)image.bits());
    }

    /*!
     * @brief
     * 将QImage队列转换味HObject格式
     */
    static void convertQImagelistToHObject(const QVector<QImage>& imagelist, HObject* imagelistObj)
    {
        HObject imageObj;
        GenEmptyObj(imagelistObj);
        for(const QImage& image : imagelist)
        {
            convertQImageToHObject(image, &imageObj);
            ConcatObj(*imagelistObj, imageObj, imagelistObj);
        }
    }
};
