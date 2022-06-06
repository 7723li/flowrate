#include "VesselAlgorithm.h"

void VesselAlgorithm::getImageSharpness(const QImage &image, double &sharpness, bool &isSharp)
{
    HObject ImageOri, RegionUnion;
    HTuple tSharpness, tIsSharp;

    convertQImageToHObject(image, &ImageOri);
    HalconInterfaceBase::getImageSharpness(ImageOri, tSharpness, tIsSharp);

    sharpness = tSharpness.TupleReal();
    isSharp = (tIsSharp != 0);
}

void VesselAlgorithm::splitVesslRegion(const QVector<QImage> &imagelist, double pixelSize, int magnification, QVector<RegionPoints> &regionsSplitedPoints, QVector<double>& diameters, QVector<double>& lengths, QString type)
{
    HObject imagelistObj, RegionVesselConcat, ImageGaussConcat, RegionAlignVesselConcat, UnionRegionVessel;
    HObject CenterLines, RegionVesselSplited, CenterLineContours, RegionSelected;

    HTuple NumberCenterLines, vesselDiameters, vesselLengts;
    HTuple TupleProcessImageIndex, TupleTranPrevToRearRows, TupleTranPrevToRearCols;
    HTuple Rows, Cols, pointCount;

    regionsSplitedPoints.clear();
    diameters.clear();
    lengths.clear();

    if(imagelist.size() > 1)
    {
        convertQImagelistToHObject(imagelist, &imagelistObj);

        HalconInterfaceBase::imagelistAntishake(imagelistObj, &RegionVesselConcat, &ImageGaussConcat, 10, &TupleProcessImageIndex, &TupleTranPrevToRearRows, &TupleTranPrevToRearCols);

        if(TupleProcessImageIndex.TupleLength() < 10)
        {
            return;
        }

        HalconInterfaceBase::alignAntishakeRegion(RegionVesselConcat, &RegionAlignVesselConcat, 1, 10, TupleTranPrevToRearRows, TupleTranPrevToRearCols, "region");

        Union1(RegionAlignVesselConcat, &UnionRegionVessel);
    }
    else if(imagelist.size() == 1)
    {
        convertQImageToHObject(imagelist.front(), &imagelistObj);

        HalconInterfaceBase::preProcess(imagelistObj, &ImageGaussConcat, &UnionRegionVessel);
    }
    else
    {
        return;
    }

    HalconInterfaceBase::splitVesselRegion(UnionRegionVessel, imagelist.front().width(), imagelist.front().height(), &CenterLines, &RegionVesselSplited, &CenterLineContours, &NumberCenterLines, &vesselDiameters, &vesselLengts);

    for(int i = 0; i < NumberCenterLines; ++i)
    {
        diameters.push_back(vesselDiameters[i] * pixelSize / magnification);
        lengths.push_back(vesselLengts[i] * pixelSize / magnification);
    }

    auto getUnionPointsFunc = [&](){
        for(int i = 0; i < NumberCenterLines; ++i)
        {
            SelectObj(RegionVesselSplited, &RegionSelected, i + 1);
            GetRegionPoints(RegionSelected, &Rows, &Cols);
            pointCount = Rows.TupleLength();
            RegionPoints rp;
            for(int j = 0; j < pointCount; ++j)
            {
                rp.push_back(QPoint(Cols[j], Rows[j]));
            }
            regionsSplitedPoints.push_back(rp);
        }
    };

    auto getBorderPointsFunc = [&](){
        for(int i = 0; i < NumberCenterLines; ++i)
        {
            RegionPoints rp;
            for(int j = 1; j <= 2; ++j)
            {
                SelectObj(CenterLineContours, &RegionSelected, i * 2 + j);
                GetContourXld(RegionSelected, &Rows, &Cols);
                GenRegionPolygon(&RegionSelected, Rows, Cols);
                GetRegionPoints(RegionSelected, &Rows, &Cols);
                pointCount = Rows.TupleLength();
                for(int k = 0; k < pointCount; ++k)
                {
                    rp.push_back(QPoint(Cols[k], Rows[k]));
                }
            }
            regionsSplitedPoints.push_back(rp);
        }
    };

    auto getSkeletonPointsFunc = [&](){
        for(int i = 0; i < NumberCenterLines; ++i)
        {
            SelectObj(CenterLines, &RegionSelected, i + 1);
            GetContourXld(RegionSelected, &Rows, &Cols);
            GenRegionPolygon(&RegionSelected, Rows, Cols);
            GetRegionPoints(RegionSelected, &Rows, &Cols);
            pointCount = Rows.TupleLength();
            RegionPoints rp;
            for(int j = 0; j < pointCount; ++j)
            {
                rp.push_back(QPoint(Cols[j], Rows[j]));
            }
            regionsSplitedPoints.push_back(rp);
        }
    };

    if(type == "Union")
    {
        getUnionPointsFunc();
    }
    else if(type == "Border")
    {
        getBorderPointsFunc();
    }
    else if(type == "Skeleton")
    {
        getSkeletonPointsFunc();
    }
    else
    {
        getUnionPointsFunc();
    }
}

void VesselAlgorithm::calculateGlycocalyx(const QVector<QImage> &imagelist, double pixelSize, int magnification, QVector<double> &glycocalyx)
{
    HObject imagelistObj, RegionVesselConcat, ImageGaussConcat, RegionAlignVesselConcat, UnionRegionVessel;
    HObject CenterLines, RegionVesselSplited, CenterLineContours;

    HTuple NumberCenterLines, vesselDiameters, vesselLengts;
    HTuple TupleProcessImageIndex, TupleTranPrevToRearRows, TupleTranPrevToRearCols, tGlycocalyx;

    convertQImagelistToHObject(imagelist, &imagelistObj);

    HalconInterfaceBase::imagelistAntishake(imagelistObj, &RegionVesselConcat, &ImageGaussConcat, 50, &TupleProcessImageIndex, &TupleTranPrevToRearRows, &TupleTranPrevToRearCols);

    if(TupleProcessImageIndex.TupleLength() < 10)
    {
        return;
    }

    HalconInterfaceBase::alignAntishakeRegion(RegionVesselConcat, &RegionAlignVesselConcat, 1, 10, TupleTranPrevToRearRows, TupleTranPrevToRearCols, "region");

    Union1(RegionAlignVesselConcat, &UnionRegionVessel);
    HalconInterfaceBase::splitVesselRegion(UnionRegionVessel, imagelist.front().width(), imagelist.front().height(), &CenterLines, &RegionVesselSplited, &CenterLineContours, &NumberCenterLines, &vesselDiameters, &vesselLengts);

    HalconInterfaceBase::calculateGlycocalyx(CenterLines, RegionVesselConcat, NumberCenterLines, TupleTranPrevToRearRows, TupleTranPrevToRearCols, pixelSize, magnification, &tGlycocalyx);

    glycocalyx.clear();
    for(int i = 0; i < NumberCenterLines; ++i)
    {
        glycocalyx.push_back(tGlycocalyx[i]);
    }
}

void VesselAlgorithm::calculateFlowrate(QImage imagePrev, QImage imageRear, double pixelSize, int magnification, double fps, double &flowrate)
{
    QVector<double> flowrates;
    QVector<RegionPoints> _regionPoints;
    QVector<QImage> imagelist;

    imagelist.push_back(imagePrev);
    imagelist.push_back(imageRear);

    calculateFlowrate(imagelist, fps, pixelSize, magnification, flowrates);

    flowrate = flowrates[0];
}

void VesselAlgorithm::calculateFlowrate(const QVector<QImage> &imagelist, double pixelSize, int magnification, double fps, QVector<double> &flowrates)
{
    HObject imagelistObj, RegionVesselConcat, ImageGaussConcat, RegionAlignVesselConcat, UnionRegionVessel;
    HObject CenterLines, RegionVesselSplited, CenterLineContours;

    HTuple NumberCenterLines, vesselDiameters, vesselLengts;
    HTuple TupleProcessImageIndex, TupleTranPrevToRearRows, TupleTranPrevToRearCols, tFlowrates;

    convertQImagelistToHObject(imagelist, &imagelistObj);

    HalconInterfaceBase::imagelistAntishake(imagelistObj, &RegionVesselConcat, &ImageGaussConcat, 26, &TupleProcessImageIndex, &TupleTranPrevToRearRows, &TupleTranPrevToRearCols);

    if(TupleProcessImageIndex.TupleLength() < 10)
    {
        return;
    }

    HalconInterfaceBase::alignAntishakeRegion(RegionVesselConcat, &RegionAlignVesselConcat, 1, 10, TupleTranPrevToRearRows, TupleTranPrevToRearCols, "region");

    Union1(RegionAlignVesselConcat, &UnionRegionVessel);
    HalconInterfaceBase::splitVesselRegion(UnionRegionVessel, imagelist.front().width(), imagelist.front().height(), &CenterLines, &RegionVesselSplited, &CenterLineContours, &NumberCenterLines, &vesselDiameters, &vesselLengts);

    HalconInterfaceBase::calculateFlowrate(ImageGaussConcat, RegionVesselSplited, NumberCenterLines, TupleProcessImageIndex, TupleTranPrevToRearRows, TupleTranPrevToRearCols, pixelSize, magnification, fps, &tFlowrates);

    flowrates.clear();
    for(int i = 0; i < NumberCenterLines; ++i)
    {
        flowrates.push_back(tFlowrates[i]);
    }
}

void VesselAlgorithm::calculateAll(const QVector<QImage> &imagelist, double pixelSize, int magnification, double fps, VesselData &vesselData, int& firstSharpImageIndex)
{
    /*!
     * @brief  血管信息提取/糖萼算法/流速算法程序结构
     * @author lzx/yzt
     * @date   2022/05/22
     *
     * read_imagelist
     * [input：路径 output：图像序列/图像宽度/图像高度]
     *
     * --| antishake
     *     [input：ImageList-图像序列 AntiShakeFrameNumber-防抖帧数
     *     [output：RegionVesselConcat-血管区域 ImageGaussConcat-高斯模糊图像序列 TupleProcessImageIndex-处理过的图像序列下标 TupleTranPrevToRearRows/ TupleTranPrevToRearCols-防抖纵横位移]
     *     [调试输入：path/pathloop/debug]
     *
     * ----| split_vessel_region
     *       [input：组合血管区域 output：血管中心线/分解血管区域/直径/长度]
     *
     * ------| glycocalyx
     *         [input：组合血管区域 output：糖萼尺寸]
     *
     * ------| flowrate
     *         [input：图像序列/组合血管区域 output：红细胞流速]
     */

    HObject imagelistObj, RegionVesselConcat, ImageGaussConcat, RegionAlignVesselConcat, UnionRegionVessel;
    HObject CenterLines, RegionVesselSplited, CenterLineContours, RegionSelected;

    HTuple NumberCenterLines, vesselDiameters, vesselLengts;
    HTuple TupleProcessImageIndex, TupleTranPrevToRearRows, TupleTranPrevToRearCols;
    HTuple Rows, Cols, pointCount, tGlycocalyx, tFlowrates;

    // QImage队列转换为HALCON格式
    convertQImagelistToHObject(imagelist, &imagelistObj);

    // ***************************************消抖+提取完整血管区域***************************************
    /*!
     * @brief
     * 防抖需要帧数
     * 完整血管区域 前10帧
     * 糖萼计算     前10帧和后40帧
     * 流速计算     前25帧
     *
     * 因此防抖最多需要50帧
     */
    HalconInterfaceBase::imagelistAntishake(imagelistObj, &RegionVesselConcat, &ImageGaussConcat, 50, &TupleProcessImageIndex, &TupleTranPrevToRearRows, &TupleTranPrevToRearCols);

    // 少于10帧是清晰的 视频质量不过关 next one 甚至没有一帧是清晰的 离谱至极
    if(TupleProcessImageIndex.TupleLength() < 10)
    {
        return;
    }

    firstSharpImageIndex = TupleProcessImageIndex[0].I();

    // ***************************************血管区域分段***************************************
    // 将完整的血管区域移动回跟首帧重叠
    HalconInterfaceBase::alignAntishakeRegion(RegionVesselConcat, &RegionAlignVesselConcat, 1, 10, TupleTranPrevToRearRows, TupleTranPrevToRearCols, "region");

    // 判断中心线数量
    Union1(RegionAlignVesselConcat, &UnionRegionVessel);
    HalconInterfaceBase::splitVesselRegion(UnionRegionVessel, imagelist.front().width(), imagelist.front().height(), &CenterLines, &RegionVesselSplited, &CenterLineContours, &NumberCenterLines, &vesselDiameters, &vesselLengts);
    if(NumberCenterLines == 0)
    {
        return;
    }

    // ***************************************糖萼***************************************
    HalconInterfaceBase::calculateGlycocalyx(CenterLines, RegionVesselConcat, NumberCenterLines, TupleTranPrevToRearRows, TupleTranPrevToRearCols, pixelSize, magnification, &tGlycocalyx);

    // ***************************************流速***************************************
    HalconInterfaceBase::calculateFlowrate(ImageGaussConcat, RegionVesselSplited, NumberCenterLines, TupleProcessImageIndex, TupleTranPrevToRearRows, TupleTranPrevToRearCols, pixelSize, magnification, fps, &tFlowrates);

    // 打完收工
    VesselData tVesselData;

    tVesselData.vesselNumber = NumberCenterLines;

    for(int i = 0; i < NumberCenterLines; ++i)
    {
        tVesselData.diameters.push_back(vesselDiameters[i] * pixelSize / magnification);
        tVesselData.lengths.push_back(vesselLengts[i] * pixelSize / magnification);
    }

    for(int i = 0; i < NumberCenterLines; ++i)
    {
        RegionPoints rp;

        SelectObj(RegionVesselSplited, &RegionSelected, i + 1);
        GetRegionPoints(RegionSelected, &Rows, &Cols);
        pointCount = Rows.TupleLength();
        for(int j = 0; j < pointCount; ++j)
        {
            rp.push_back(QPoint(Cols[j], Rows[j]));
        }
        tVesselData.regionsUnionPoints.push_back(rp);
    }

    for(int i = 0; i < NumberCenterLines; ++i)
    {
        SelectObj(CenterLines, &RegionSelected, i + 1);
        GetContourXld(RegionSelected, &Rows, &Cols);
        GenRegionPolygon(&RegionSelected, Rows, Cols);
        GetRegionPoints(RegionSelected, &Rows, &Cols);
        pointCount = Rows.TupleLength();
        RegionPoints rp;
        for(int j = 0; j < pointCount; ++j)
        {
            rp.push_back(QPoint(Cols[j], Rows[j]));
        }
        tVesselData.regionsSkeletonPoints.push_back(rp);
    }

    for(int i = 0; i < NumberCenterLines; ++i)
    {
        RegionPoints rp;
        for(int j = 1; j <= 2; ++j)
        {
            SelectObj(CenterLineContours, &RegionSelected, i * 2 + j);
            GetContourXld(RegionSelected, &Rows, &Cols);
            GenRegionPolygon(&RegionSelected, Rows, Cols);
            GetRegionPoints(RegionSelected, &Rows, &Cols);
            pointCount = Rows.TupleLength();
            for(int k = 0; k < pointCount; ++k)
            {
                rp.push_back(QPoint(Cols[k], Rows[k]));
            }
        }
        tVesselData.regionsBorderPoints.push_back(rp);
    }

    for(int i = 0; i < NumberCenterLines; ++i)
    {
        tVesselData.glycocalyx.push_back(tGlycocalyx[i]);
    }

    for(int i = 0; i < NumberCenterLines; ++i)
    {
        tVesselData.flowrates.push_back(tFlowrates[i]);
    }

    vesselData = tVesselData;
}
