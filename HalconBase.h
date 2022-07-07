#pragma once

#include "HalconCpp.h"

using namespace HalconCpp;

/*!
 * @brief
 * HALCON函数基类
 */
class HALCONBASE
{
protected:
    /*!
     * @brief
     * 计算高斯中心线算子的参数
     */
    static void calculate_lines_gauss_parameters (HTuple MaxLineWidth, HTuple Contrast, HTuple *Sigma, HTuple *Low, HTuple *High)
    {
        HTuple  ContrastHigh, ContrastLow, HalfWidth, Help;

        ContrastHigh = ((const HTuple&)Contrast)[0];

        if (0 != (int((Contrast.TupleLength())==2)))
        {
            ContrastLow = ((const HTuple&)Contrast)[1];
        }
        else
        {
            ContrastLow = ContrastHigh/3.0;
        }

        if (0 != (int(MaxLineWidth<(HTuple(3.0).TupleSqrt()))))
        {
            ContrastLow = (ContrastLow*MaxLineWidth)/(HTuple(3.0).TupleSqrt());
            ContrastHigh = (ContrastHigh*MaxLineWidth)/(HTuple(3.0).TupleSqrt());
            MaxLineWidth = HTuple(3.0).TupleSqrt();
        }

        HalfWidth = MaxLineWidth/2.0;
        (*Sigma) = HalfWidth/(HTuple(3.0).TupleSqrt());
        Help = ((-2.0*HalfWidth)/((HTuple(6.283185307178).TupleSqrt())*((*Sigma).TuplePow(3.0))))*((-0.5*((HalfWidth/(*Sigma)).TuplePow(2.0))).TupleExp());
        (*High) = (ContrastHigh*Help).TupleFabs();
        (*Low) = (ContrastLow*Help).TupleFabs();
    }
};

/*!
 * @brief
 * HALCON接口基类
 *
 * @author lzx
 * @date   2022/05/30
 */
class HalconInterfaceBase : public HALCONBASE
{
protected:
    /*!
     * @brief
     * 预处理函数
     *
     * @param ImageOri      输入原图
     * @param ImageGauss    输出的高斯图像
     * @param RegionUnion   提取出的血管区域
     */
    static void preProcess(const HObject& ImageOri, HObject *ImageGauss, HObject *RegionUnion);

    /*!
     * @brief
     * 清晰度计算
     *
     * @param ImageOri  输入的原图
     * @param sharpness 清晰度数值
     * @param isSharp   清晰度判断
     */
    static void getImageSharpness(const HObject& ImageOri, HTuple* sharpness, HTuple* isSharp);

    /*!
     * @brief
     * 图像序列防抖
     *
     * @param ImageList                 输入的图像序列
     * @param RegionVesselConcat        图像序列中每帧的血管区域
     * @param ImageGaussConcat          防抖完成后的高斯图像序列
     * @param AntiShakeFrameNumber      防抖使用的帧数
     * @param TupleProcessImageIndex    输出的防抖使用过的帧下标
     * @param TupleTranPrevToRearRows   输出的纵向防抖位移距离
     * @param TupleTranPrevToRearCols   输出的横向防抖位移距离
     */
    static void imagelistAntishake (HObject ImageList, HObject *RegionVesselConcat, HObject *ImageGaussConcat, HTuple AntiShakeFrameNumber, HTuple *TupleProcessImageIndex, HTuple *TupleTranPrevToRearRows, HTuple *TupleTranPrevToRearCols);

    /*!
     * @brief
     * 计算两个血管区域的防抖位移距离
     *
     * @param RegionUnionPrev       前一帧的血管区域
     * @param RegionUnionRear       后一帧的血管区域
     * @param Width                 图像宽度
     * @param Height                图像高度
     * @param MaxIntersectAreaRow   纵向位移
     * @param MaxIntersectAreaCol   横向位移
     */
    static void regionStabilization (HObject RegionUnionPrev, HObject RegionUnionRear, HTuple Width, HTuple Height, HTuple *MaxIntersectAreaRow, HTuple *MaxIntersectAreaCol);

    /*!
     * @brief
     * 将防抖完毕的区域移动回到与$BeginFrame$帧重合的位置
     *
     * @param RegionOrigin              输入的血管区域序列
     * @param RegionAntishaked          防抖完成后的血管区域序列
     * @param BeginFrame                防抖开始的帧下标
     * @param EndFrame                  防抖结束的帧下标
     * @param TupleTranPrevToRearRows   输入的纵向位移距离
     * @param TupleTranPrevToRearCols   输入的横向位移距离
     * @param Type                      "region"和"image"
     */
    static void alignAntishakeRegion (HObject RegionOrigin, HObject *RegionAntishaked, HTuple BeginFrame, HTuple EndFrame, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Type);

    /*!
     * @brief
     * 区域图形分割
     *
     * @param RegionVesselUnion     序列合成血管区域
     * @param Width                 原图宽度 生成合成血管区域
     * @param Height                原图高度 生成合成血管区域
     * @param CenterLines           提取出的中心线
     * @param RegionsSplited        分割出的血管区域
     * @param CenterLineContours    提取出的中心线左右轮廓
     * @param NumberCenterLines     输出的中心线数量
     * @param vesselDiameters       计算出的血管区域直径(px)
     * @param vesselLengths         计算出的血管区域长度(px)
     */
    static void splitVesselRegion(const HObject& RegionVesselUnion, HTuple Width, HTuple Height, HObject *CenterLines, HObject* RegionsSplited, HObject *CenterLineContours, HTuple* NumberCenterLines, HTuple* vesselDiameters, HTuple* vesselLengths);

    /*!
     * @brief
     * 根据手动描绘或擦除的中心线和给定的半径 重新进行区域图形分割
     *
     * 给定的半径由两种途径给定
     * 擦除的区域由之前算法得出的半径给定
     * 描绘的区域由用户描绘完成后输入的数字给定
     *
     * @param RegionVesselUnion     序列合成血管区域
     * @param Width                 原图宽度 生成合成血管区域
     * @param Height                原图高度 生成合成血管区域
     * @param RegionCenterLines     要重新分割的中心线
     * @param oriDiameters          对应的给定半径
     * @param CenterLines           提取出的中心线
     * @param RegionsSplited        分割出的血管区域
     * @param CenterLineContours    提取出的中心线左右轮廓
     * @param NumberCenterLines     输出的中心线数量
     * @param vesselDiameters       计算出的血管区域直径(px)
     * @param vesselLengths         计算出的血管区域长度(px)
     *
     * @attention
     * Qt接口专供
     */
    static void reSplitVesselRegion(const HObject& RegionVesselUnion, HTuple Width, HTuple Height, const HObject& RegionCenterLines, const HTuple& oriDiameters, HObject *CenterLines, HObject* RegionsSplited, HObject *CenterLineContours, HTuple* NumberCenterLines, HTuple* vesselDiameters, HTuple* vesselLengths);

    /*!
     * @brief
     * 计算糖萼
     *
     * @attention
     * lzx 2022/06/27
     * 参数 新增了是否手动刷新数据的判断 新增了"左右半径"参数
     *
     * @param CenterLines               中心线
     * @param isManual                  是否手动刷新数据的判断
     * @param vesselDiameters           "直径"参数
     * @param RegionVesselConcat        各帧血管区域
     * @param NumberCenterLines         血管中心线数量
     * @param TupleTranPrevToRearRows   对齐用的纵向位移
     * @param TupleTranPrevToRearCols   对齐用的横向位移
     * @param Pixelsize                 像素尺寸
     * @param Magnification             放大倍率
     * @param glycocalyx                输出的糖萼参数
     */
    static void calculateGlycocalyx (HObject CenterLines, bool isManual, HTuple vesselDiameters, HObject RegionVesselConcat, HTuple NumberCenterLines, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Pixelsize, HTuple Magnification, HTuple *glycocalyx);

    /*!
     * @brief
     * 计算流速
     *
     * @param ImageGaussConcat          高斯图像序列
     * @param RegionVesselSplited       分割出的血管区域
     * @param NumberCenterLines         血管中心线数量
     * @param TupleTranPrevToRearRows   对齐用的纵向位移
     * @param TupleTranPrevToRearCols   对齐用的横向位移
     * @param Pixelsize                 像素尺寸
     * @param Magnification             放大倍率
     * @param Fps                       帧率
     * @param Flowrate                  输出的流速参数
     */
    static void calculateFlowrate (HObject ImageGaussConcat, HObject RegionVesselSplited, HTuple NumberCenterLines, HTuple TupleProcessImageIndex, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Pixelsize, HTuple Magnification, HTuple Fps, HTuple *Flowrate);
};

