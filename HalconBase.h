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
     */
    static void preProcess(const HObject& ImageOri, HObject *ImageGauss, HObject *RegionUnion);

    /*!
     * @brief
     * 清晰度计算
     */
    static void getImageSharpness(const HObject& ImageOri, HTuple* sharpness, HTuple* isSharp);

    /*!
     * @brief
     * 图像序列防抖
     */
    static void imagelistAntishake (HObject ImageList, HObject *RegionVesselConcat, HObject *ImageGaussConcat, HTuple AntiShakeFrameNumber, HTuple *TupleProcessImageIndex, HTuple *TupleTranPrevToRearRows, HTuple *TupleTranPrevToRearCols);

    /*!
     * @brief
     * 计算两个血管区域的防抖位移距离
     */
    static void regionStabilization (HObject RegionUnionPrev, HObject RegionUnionRear, HTuple Width, HTuple Height, HTuple *MaxIntersectAreaRow, HTuple *MaxIntersectAreaCol);

    /*!
     * @brief
     * 将防抖完毕的区域移动回到与$BeginFrame$帧重合的位置
     */
    static void alignAntishakeRegion (HObject RegionOrigin, HObject *RegionAntishaked, HTuple BeginFrame, HTuple EndFrame, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Type);

    /*!
     * @brief
     * 区域图形分割
     */
    static void splitVesselRegion(const HObject& RegionVesselUnion, int width, int height, HObject *CenterLines, HObject* RegionsSplited, HObject *CenterLineContours, HTuple* NumberCenterLines, HTuple* vesselDiameters, HTuple* vesselLengts);

    /*!
     * @brief
     * 计算糖萼
     */
    static void calculateGlycocalyx (HObject CenterLines, HObject RegionVesselConcat, HTuple NumberCenterLines, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Pixelsize, HTuple Magnification, HTuple *glycocalyx);

    /*!
     * @brief
     * 计算流速
     */
    static void calculateFlowrate (HObject ImageGaussConcat, HObject RegionVesselSplited, HTuple NumberCenterLines, HTuple TupleProcessImageIndex, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Pixelsize, HTuple Magnification, HTuple Fps, HTuple *Flowrate);
};

