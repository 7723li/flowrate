#include "HalconBase.h"

void HalconInterfaceBase::preProcess(const HObject &ImageOri, HObject *ImageGauss, HObject *RegionUnion)
{
    HObject ImageMean, RegionDynThresh, RegionClosing;
    HObject RawRegionConnected, RegionConnected;

    HTuple Channels, AreaRawRegionConnected;
    HTuple useless, MinArea, MaxArea;

    try
    {
        GaussFilter(ImageOri, ImageGauss, 7);
    }
    catch (HException &HDevExpDefaultException)
    {
        *ImageGauss = ImageOri;
        GenRegionPoints(RegionUnion, HTuple(), HTuple());
        return;
    }

    //寻找血管区域 均值滤波+动态阈值
    MeanImage(*ImageGauss, &ImageMean, 43, 43);
    DynThreshold(*ImageGauss, ImageMean, &RegionDynThresh, 9, "dark");

    //闭运算补小洞
    ClosingCircle(RegionDynThresh, &RegionClosing, 3);

    //分散出若干个连接起来的区域 大部分大概率是血管区域
    Connection(RegionClosing, &RawRegionConnected);

    //根据面积筛选掉噪音
    AreaCenter(RawRegionConnected, &AreaRawRegionConnected, &useless, &useless);
    MinArea = 50;
    TupleMax(AreaRawRegionConnected, &MaxArea);
    if (MaxArea < MinArea)
    {
        MinArea = 0;
    }
    SelectShape(RawRegionConnected, &RegionConnected, "area", "and", MinArea, MaxArea);

    //合并 检测到的分散血管区域 后面有用
    Union1(RegionConnected, &(*RegionUnion));
}

void HalconInterfaceBase::getImageSharpness(const HObject &ImageOri, HTuple *sharpness, HTuple *isSharp)
{
    HObject  Rectangle, ImageEmphasize, ImageGauss;
    HObject  ImaAmp, ImaDir, ImageMean, RegionDynThresh;
    HObject  RegionUnion, ConnectedRegions, SelectedRegions;
    HObject  skeleton;

    HTuple  Width, Height, Mean, Deviation;
    HTuple  Area2, useless, Max, Number, Area;
    HTuple  Area1;

    *sharpness = 0;
    *isSharp = false;

    //计算图像整体的灰度方差
    GetImageSize(ImageOri, &Width, &Height);
    GenRectangle1(&Rectangle, 0, 0, Height-1, Width-1);
    Intensity(Rectangle, ImageOri, &Mean, &Deviation);

    if (Deviation > 0)
    {
        //根据灰度方差进行对比度强化 如果灰度方差大 说明原来的对比度较高 则强化的程度可以降低 反之亦然
        //强化完成后做一次小型模糊 弱化噪点
        try
        {
            Emphasize(ImageOri, &ImageEmphasize, 255.0 / Deviation, 255.0 / Deviation, 1);
        }
        catch(...)
        {
            return;
        }
        GaussFilter(ImageEmphasize, &ImageGauss, 3);

        //canny计算并提取图像上的边缘
        EdgesImage(ImageGauss, &ImaAmp, &ImaDir, "canny", 1, "nms", 5, 8);
        MeanImage(ImaAmp, &ImageMean, 43, 43);
        DynThreshold(ImaAmp, ImageMean, &RegionDynThresh, 5, "light");

        //提取出的边缘必须起码符合有10条以上大于等于100px 否则直接判断为模糊
        Connection(RegionDynThresh, &ConnectedRegions);
        AreaCenter(ConnectedRegions, &Area2, &useless, &useless);
        TupleMax(Area2, &Max);
        if (Max >= 100)
        {
            SelectShape(ConnectedRegions, &SelectedRegions, "area", "and", 100, Max);

            CountObj(SelectedRegions, &Number);
            if (Number >= 10)
            {
                Union1(SelectedRegions, &RegionUnion);

                //用边缘和边缘骨架的像素之商作为清晰度评分
                //约清晰 评分约接近1 但永远不低于1
                Skeleton(RegionUnion, &skeleton);
                AreaCenter(RegionUnion, &Area, &useless, &useless);
                AreaCenter(skeleton, &Area1, &useless, &useless);

                if (Area > 0 && Area1 > 0)
                {
                    *sharpness = Area.TupleReal() / Area1.TupleReal();
                }
                else
                {
                    *sharpness= 0;
                }
            }
        }

        *isSharp = (*sharpness >= 1.0);
    }
}

void HalconInterfaceBase::imagelistAntishake(HObject ImageList, HObject *RegionVesselConcat, HObject *ImageGaussConcat, HTuple AntiShakeFrameNumber, HTuple *TupleProcessImageIndex, HTuple *TupleTranPrevToRearRows, HTuple *TupleTranPrevToRearCols)
{
    HObject  ImagePrev, ImagePrevGauss, RegionVesselPrev;
    HObject  RegionAntiShakeMergeMask, ImageRear, ImageRearGauss;
    HObject  RegionVesselRear, RegionAntiShakeMergeMaskTrans;
    HObject  RegionVesselConcatDebug, RegionVesselPrevDebug;
    HObject  UnionRegionVesselConcatDebug, BorderRegionVesselConcatDebug;

    HTuple  Width, Height;
    HTuple  AreaImage, ImageListNumber, I, AreaRegionVesselPrev, AreaRegionVesselRear;
    HTuple  useless, sharpness, isSharp, MaxIntersectAreaRow;
    HTuple  MaxIntersectAreaCol, HomMat2DPrevToRear, AreaRegionAntiShakeMergeMaskTrans;

    *TupleProcessImageIndex = HTuple();
    *TupleTranPrevToRearRows = HTuple();
    *TupleTranPrevToRearCols = HTuple();

    //首帧预处理 对应的变量用在后续的后帧缓存
    SelectObj(ImageList, &ImagePrev, 1);
    GetImageSize(ImagePrev, &Width, &Height);

    preProcess(ImagePrev, &ImagePrevGauss, &RegionVesselPrev);
    AreaCenter(ImagePrev, &AreaRegionVesselPrev, &useless, &useless);
    if(!(AreaRegionVesselPrev.TupleLength() == 0 || -1 == AreaRegionVesselPrev || 0 == AreaRegionVesselPrev))
    {
        getImageSharpness(ImagePrev, &sharpness, &isSharp);
        if(!!isSharp)
        {
            *TupleProcessImageIndex = 0;
            ++AntiShakeFrameNumber;
        }
    }

    //防抖后的合并掩膜
    GenRectangle1(&RegionAntiShakeMergeMask, 0, 0, Height, Width);

    //图像面积
    AreaImage = Width*Height;

    //遍历图像序列
    CountObj(ImageList, &ImageListNumber);

    GenEmptyObj(RegionVesselConcat);
    GenEmptyObj(ImageGaussConcat);

    for (I = 2; I <= ImageListNumber; ++I)
    {
        //后一帧预处理
        SelectObj(ImageList, &ImageRear, I);
        preProcess(ImageRear, &ImageRearGauss, &RegionVesselRear);
        if (Width == 0 || Height == 0)
        {
            continue;
        }

        //后帧血管区域判空 若为空 next one
        AreaCenter(RegionVesselRear, &AreaRegionVesselRear, &useless, &useless);
        if (AreaRegionVesselRear.TupleLength() == 0 || -1 == AreaRegionVesselRear || 0 == AreaRegionVesselRear)
        {
            continue;
        }

        //不清晰的也不要 next one
        getImageSharpness(ImageRear, &sharpness, &isSharp);
        if (!isSharp)
        {
            continue;
        }

        regionStabilization(RegionVesselPrev, RegionVesselRear, Width, Height, &MaxIntersectAreaRow, &MaxIntersectAreaCol);

        //最终找到最佳的前帧对齐到后帧的位移数值
        VectorAngleToRigid(0, 0, 0, MaxIntersectAreaRow, MaxIntersectAreaCol, 0, &HomMat2DPrevToRear);

        //移动掩膜
        AffineTransRegion(RegionAntiShakeMergeMask, &RegionAntiShakeMergeMaskTrans, HomMat2DPrevToRear, "nearest_neighbor");

        //如果掩膜已经移动得仅剩60%面积 说明抖动过大 不再进行下去
        AreaCenter(RegionAntiShakeMergeMaskTrans, &AreaRegionAntiShakeMergeMaskTrans, &useless, &useless);
        if (AreaRegionAntiShakeMergeMaskTrans < AreaImage * 0.6)
        {
            break;
        }
        RegionAntiShakeMergeMask = RegionAntiShakeMergeMaskTrans;

        //记录 处理帧序号/位移行列
        (*TupleProcessImageIndex) = (*TupleProcessImageIndex).TupleConcat(I-1);
        (*TupleTranPrevToRearRows) = (*TupleTranPrevToRearRows).TupleConcat(MaxIntersectAreaRow);
        (*TupleTranPrevToRearCols) = (*TupleTranPrevToRearCols).TupleConcat(MaxIntersectAreaCol);

        //记录合并血管区域和细胞轨迹
        ConcatObj(*RegionVesselConcat, RegionVesselPrev, RegionVesselConcat);
        ConcatObj(*ImageGaussConcat, ImagePrevGauss, ImageGaussConcat);

        if ((*TupleProcessImageIndex).TupleLength() >= AntiShakeFrameNumber)
        {
            break;
        }

        //前帧变后帧
        ImagePrevGauss = ImageRearGauss;
        RegionVesselPrev = RegionVesselRear;
    }
}

void HalconInterfaceBase::regionStabilization(HObject RegionUnionPrev, HObject RegionUnionRear, HTuple Width, HTuple Height, HTuple *MaxIntersectAreaRow, HTuple *MaxIntersectAreaCol)
{
    HObject  RegionDifference, lines, RegionIntersection;
    HObject  ConnectedRegions, SkeletonUnionPrev, SkeletonUnionRear;
    HObject  SkeletonUnionPrevTrans, RegionIntersect;

    HTuple  RowCoors, Zeros, ColCoors, dis_Row;
    HTuple  useless, dis_Col, maxRowDis, deviationRowDis;
    HTuple  maxColDis, deviationColDis, maxIntersectArea;
    HTuple  Row, Col, HomMat2D, AreaRegionIntersect;
    HTuple  maxIntersectAreaRowCopy, maxIntersectAreaColCopy;
    HTuple  J, X, Y, AreaRegionDifference;

    (*MaxIntersectAreaRow) = 0;
    (*MaxIntersectAreaCol) = 0;
    maxIntersectArea = 0;

    //前帧到后帧的差异区域
    Difference(RegionUnionRear, RegionUnionPrev, &RegionDifference);

    AreaCenter(RegionDifference, &AreaRegionDifference, &useless, &useless);
    if(AreaRegionDifference == 0)
    {
        return;
    }

    //计算差异区域在各行的宽度
    TupleGenSequence(0, Height-1, 1, &RowCoors);
    TupleGenConst(Height, 0, &Zeros);
    TupleGenConst(Height, Width-1, &ColCoors);
    GenRegionLine(&lines, RowCoors, Zeros, RowCoors, ColCoors);
    Intersection(lines, RegionDifference, &RegionIntersection);
    Connection(RegionIntersection, &ConnectedRegions);
    AreaCenter(ConnectedRegions, &dis_Row, &useless, &useless);

    //计算差异区域在各列的高度
    TupleGenSequence(0, Width-1, 1, &ColCoors);
    TupleGenConst(Width, 0, &Zeros);
    TupleGenConst(Width, Height-1, &RowCoors);
    GenRegionLine(&lines, Zeros, ColCoors, RowCoors, ColCoors);
    Intersection(lines, RegionDifference, &RegionIntersection);
    Connection(RegionIntersection, &ConnectedRegions);
    AreaCenter(ConnectedRegions, &dis_Col, &useless, &useless);

    //最大距离的正负作为粗对齐范围 方差作为步进粗对齐距离
    TupleMax(dis_Row, &maxRowDis);
    TupleDeviation(dis_Row, &deviationRowDis);
    TupleMax(dis_Col, &maxColDis);
    TupleDeviation(dis_Col, &deviationColDis);

    Skeleton(RegionUnionPrev, &SkeletonUnionPrev);
    Skeleton(RegionUnionRear, &SkeletonUnionRear);

    //粗对齐 找出一个横向纵向位移后 能使->前后帧血管区域重叠面积最大<-的位移数值
    //注意：这里是为了减少后面细对齐位移的次数
    for (Row = -maxRowDis; Row <= maxRowDis; Row += deviationRowDis)
    {
        for (Col = -maxColDis; Col <= maxColDis; Col += deviationColDis)
        {
            VectorAngleToRigid(0, 0, 0, Row, Col, 0, &HomMat2D);
            AffineTransRegion(SkeletonUnionPrev, &SkeletonUnionPrevTrans, HomMat2D, "nearest_neighbor");
            Intersection(SkeletonUnionPrevTrans, RegionUnionRear, &RegionIntersect);

            AreaCenter(RegionIntersect, &AreaRegionIntersect, &useless, &useless);

            if (AreaRegionIntersect > maxIntersectArea)
            {
                maxIntersectArea = AreaRegionIntersect;
                (*MaxIntersectAreaRow) = Row;
                (*MaxIntersectAreaCol) = Col;
            }
        }
    }

    //细对齐 将前帧的血管区域移动至的重叠面积最大的地方 达到防抖的效果
    //J循环是为了多找一次 提高精度
    maxIntersectAreaRowCopy = (*MaxIntersectAreaRow);
    maxIntersectAreaColCopy = (*MaxIntersectAreaCol);
    for (J=0; J<=1; J+=1)
    {
        for (X=-2.56; X<=2.56; X+=0.04)
        {
            VectorAngleToRigid(0, 0, 0, (*MaxIntersectAreaRow), maxIntersectAreaColCopy+X, 0, &HomMat2D);
            AffineTransRegion(SkeletonUnionPrev, &SkeletonUnionPrevTrans, HomMat2D, "nearest_neighbor");
            Intersection(SkeletonUnionPrevTrans, RegionUnionRear, &RegionIntersect);

            AreaCenter(RegionIntersect, &AreaRegionIntersect, &useless, &useless);

            if (AreaRegionIntersect > maxIntersectArea)
            {
                maxIntersectArea = AreaRegionIntersect;
                (*MaxIntersectAreaCol) = maxIntersectAreaColCopy + X;
            }
        }

        for (Y=-2.56; Y<=2.56; Y+=0.04)
        {
            VectorAngleToRigid(0, 0, 0, maxIntersectAreaRowCopy+Y, (*MaxIntersectAreaCol), 0, &HomMat2D);
            AffineTransRegion(SkeletonUnionPrev, &SkeletonUnionPrevTrans, HomMat2D, "nearest_neighbor");
            Intersection(SkeletonUnionPrevTrans, RegionUnionRear, &RegionIntersect);

            AreaCenter(RegionIntersect, &AreaRegionIntersect, &useless, &useless);

            if (AreaRegionIntersect > maxIntersectArea)
            {
                maxIntersectArea = AreaRegionIntersect;
                (*MaxIntersectAreaRow) = maxIntersectAreaRowCopy+Y;
            }
        }
    }
}

void HalconInterfaceBase::alignAntishakeRegion(HObject RegionOrigin, HObject *RegionAntishaked, HTuple BeginFrame, HTuple EndFrame, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Type)
{
    HObject  RegionAntiShakeMergeMask, RegionSelected;

    HTuple  CountRegionOrigin, Width, Height;
    HTuple  I, HomMat2DRearToPrev;

    CountObj(RegionOrigin, &CountRegionOrigin);
    if (CountRegionOrigin <= (EndFrame - BeginFrame))
    {
        EndFrame = (BeginFrame+CountRegionOrigin)-1;
    }

    if (Type == HTuple("region"))
    {
        SelectObj(RegionOrigin, &(*RegionAntishaked), BeginFrame);

        //防抖后的合并掩膜
        RegionFeatures((*RegionAntishaked), "width", &Width);
        RegionFeatures((*RegionAntishaked), "height", &Height);
        GenRectangle1(&RegionAntiShakeMergeMask, 0, 0, Height, Width);

        for (I = BeginFrame + 1; I <= EndFrame; ++I)
        {
            VectorAngleToRigid(0, 0, 0, -HTuple(TupleTranPrevToRearRows[I-2]), -HTuple(TupleTranPrevToRearCols[I-2]), 0, &HomMat2DRearToPrev);
            AffineTransRegion(RegionOrigin, &RegionOrigin, HomMat2DRearToPrev, "nearest_neighbor");
            AffineTransRegion(RegionAntiShakeMergeMask, &RegionAntiShakeMergeMask, HomMat2DRearToPrev, "nearest_neighbor");
            SelectObj(RegionOrigin, &RegionSelected, I);
            ConcatObj((*RegionAntishaked), RegionSelected, &(*RegionAntishaked));
        }

        Intersection((*RegionAntishaked), RegionAntiShakeMergeMask, &(*RegionAntishaked));
    }
    else if (0 != (int(Type==HTuple("image"))))
    {
        SelectObj(RegionOrigin, &(*RegionAntishaked), BeginFrame);

        for (I = BeginFrame + 1; I <= EndFrame; ++I)
        {
            VectorAngleToRigid(0, 0, 0, -HTuple(TupleTranPrevToRearRows[I-2]), -HTuple(TupleTranPrevToRearCols[I-2]), 0, &HomMat2DRearToPrev);
            AffineTransImage(RegionOrigin, &RegionOrigin, HomMat2DRearToPrev, "nearest_neighbor", "false");
            SelectObj(RegionOrigin, &RegionSelected, I);
            ConcatObj((*RegionAntishaked), RegionSelected, &(*RegionAntishaked));
        }
    }
}

void HalconInterfaceBase::splitVesselRegion(const HObject &RegionVesselConcat, int width, int height, HObject *CenterLines, HObject *RegionVesselSplited, HObject *CenterLineContours, HTuple *NumberCenterLines, HTuple *vesselDiameters, HTuple *vesselLengths)
{
    HObject  ImageRegionVesselConcat, RawCenterLines;
    HObject  RawCenterLine, NeedUnionContours, NoNeedUnionContours;
    HObject  RawCenterLinesCollinear, RawCenterLinesCotangential;
    HObject  RawUnionCenterLines, RealContourLeft, RealContourRight;
    HObject  RegionCenterLineLeft, RegionCenterLineRight;
    HObject  LineRegionCenterLineHead, LineRegionCenterLineTail;
    HObject  RegionCenterLineContourBody, RegionCenterLineContourConnect;
    HObject  RegionCenterLineContour, RegionCenterLineContours;
    HObject  FillupCenterLineContours, ClosingCenterLineContours;

    HTuple  InnerRadiusRegionVesselConcat, Sigma;
    HTuple  Low, High, LengthRawCenterLines, I, NumberRawCenterLines;
    HTuple  RowRawCenterLine, ColRawCenterLine, WidthLeftRawCenterLine;
    HTuple  WidthRightRawCenterLine, MeanWidthLeftRawCenterLine;
    HTuple  MeanWidthRightRawCenterLine, TupleMeanWidthLeftRawCenterLine;
    HTuple  TupleMeanWidthRightRawCenterLine, AngleRawCenterLine;
    HTuple  RowL, ColL, RowR, ColR, RealRowL;
    HTuple  RealColL, RealRowR, RealColR, K;

    //使用组合流动轨迹的最大内切半径的1.5倍 计算流动轨迹的各中心线
    //注：不用2倍是为了避免过大的内切直径 导致提取中心线时忽略了太多的小直径区域
    RegionFeatures(RegionVesselConcat, "inner_radius", &InnerRadiusRegionVesselConcat);

    GenImageConst(&ImageRegionVesselConcat, "byte", width, height);
    OverpaintRegion(ImageRegionVesselConcat, RegionVesselConcat, 255, "fill");

    calculate_lines_gauss_parameters(InnerRadiusRegionVesselConcat * 1.5, 255, &Sigma, &Low, &High);
    LinesGauss(ImageRegionVesselConcat, &RawCenterLines, Sigma, Low, High, "light", "true", "parabolic", "true");

    //选择长度100及以下的进行合并 大于100的不合并
    LengthXld(RawCenterLines, &LengthRawCenterLines);
    GenEmptyObj(&NeedUnionContours);
    GenEmptyObj(&NoNeedUnionContours);
    for (I = 1; I <= LengthRawCenterLines.TupleLength(); ++I)
    {
        SelectObj(RawCenterLines, &RawCenterLine, I);
        if (LengthRawCenterLines[I-1] <= 100)
        {
            ConcatObj(NeedUnionContours, RawCenterLine, &NeedUnionContours);
        }
        else
        {
            ConcatObj(NoNeedUnionContours, RawCenterLine, &NoNeedUnionContours);
        }
    }

    //合并共线轮廓
    //参数解析详见 https://blog.csdn.net/qq_18620653/article/details/105518295
    UnionCollinearContoursXld(NeedUnionContours, &RawCenterLinesCollinear, 1, 0.2, 15, HTuple(15).TupleRad(), "attr_keep");

    //合并端点曲率轮廓
    //参数解析详见 https://www.gkbc8.com/thread-13729-1-1.html
    try
    {
        UnionCotangentialContoursXld(RawCenterLinesCollinear, &RawCenterLinesCotangential, 0, "auto", HTuple(15).TupleRad(), 20, 20, 10, "attr_keep");
    }
    catch (...)
    {
        RawCenterLinesCotangential = RawCenterLinesCollinear;
    }

    //合并完的和不用合并的组合回来
    ConcatObj(NoNeedUnionContours, RawCenterLinesCotangential, &RawUnionCenterLines);

    // 界内中心线计数
    *NumberCenterLines = 0;

    //血管直径/长度
    *vesselDiameters = HTuple();
    *vesselLengths = HTuple();

    GenEmptyObj(CenterLines);
    GenEmptyObj(CenterLineContours);
    GenEmptyObj(&RegionCenterLineContours);

    //遍历中心线
    //1、过滤界外中心线
    //2、生成开放的左右区域 用显示
    //3、生成封闭的血管区域 用来计算区域流速
    CountObj(RawUnionCenterLines, &NumberRawCenterLines);

    for (I = 1; I <= NumberRawCenterLines; ++I)
    {
        SelectObj(RawUnionCenterLines, &RawCenterLine, I);

        //获取中心线的坐标
        GetContourXld(RawCenterLine, &RowRawCenterLine, &ColRawCenterLine);

        //获取中心线的左右宽度
        GetContourAttribXld(RawCenterLine, "width_left", &WidthLeftRawCenterLine);
        GetContourAttribXld(RawCenterLine, "width_right", &WidthRightRawCenterLine);

        //计算左右宽度平均值 然后以平均值作为中心线的左右宽度
        //注：计算平均值是为了生成平滑的左右轮廓 使用原始的宽度生成的轮廓会比较不理想
        TupleMean(WidthLeftRawCenterLine, &MeanWidthLeftRawCenterLine);
        TupleMean(WidthRightRawCenterLine, &MeanWidthRightRawCenterLine);

        //防抖之后的流动轨迹 对比原轨迹 有很大可能会被膨胀 参考lines_gauss例程乘上sqrt(0.75)消除膨胀
        MeanWidthLeftRawCenterLine = MeanWidthLeftRawCenterLine*(HTuple(0.75).TupleSqrt());
        MeanWidthRightRawCenterLine = MeanWidthRightRawCenterLine*(HTuple(0.75).TupleSqrt());

        //如果左右宽度平均值为0的中心线 过滤掉
        if (MeanWidthLeftRawCenterLine <= 0 || MeanWidthRightRawCenterLine <= 0)
        {
            continue;
        }

        //生成宽度平均值数组
        TupleGenConst(WidthLeftRawCenterLine.TupleLength(), MeanWidthLeftRawCenterLine, &TupleMeanWidthLeftRawCenterLine);
        TupleGenConst(WidthRightRawCenterLine.TupleLength(), MeanWidthRightRawCenterLine, &TupleMeanWidthRightRawCenterLine);

        //获取中心线的角度
        GetContourAttribXld(RawCenterLine, "angle", &AngleRawCenterLine);

        //计算中心线左右轮廓的坐标
        //关于sqrt(0.75)的解释 详见 https://blog.csdn.net/y363703390/article/details/85416327
        //为了显示线宽，只有线和背景在该点的灰度差达到25%才会显示，
        //该点是通过抛物线乘以sqrt(3/4)给出
        RowL = RowRawCenterLine-(((AngleRawCenterLine.TupleCos())*TupleMeanWidthLeftRawCenterLine)*(HTuple(0.75).TupleSqrt()));
        ColL = ColRawCenterLine-(((AngleRawCenterLine.TupleSin())*TupleMeanWidthLeftRawCenterLine)*(HTuple(0.75).TupleSqrt()));
        RowR = RowRawCenterLine+(((AngleRawCenterLine.TupleCos())*TupleMeanWidthRightRawCenterLine)*(HTuple(0.75).TupleSqrt()));
        ColR = ColRawCenterLine+(((AngleRawCenterLine.TupleSin())*TupleMeanWidthRightRawCenterLine)*(HTuple(0.75).TupleSqrt()));

        //过滤掉坐标中的负值 确保左右轮廓都在图像界内
        RealRowL = HTuple();
        RealColL = HTuple();
        RealRowR = HTuple();
        RealColR = HTuple();

        HTuple end_val98 = (RowL.TupleLength())-1;
        HTuple step_val98 = 1;
        for (K=0; K.Continue(end_val98, step_val98); K += step_val98)
        {
            if (0 != (HTuple(int(HTuple(RowL[K])>0)).TupleAnd(int(HTuple(ColL[K])>0))))
            {
                RealRowL = RealRowL.TupleConcat(HTuple(RowL[K]));
                RealColL = RealColL.TupleConcat(HTuple(ColL[K]));
            }
        }

        HTuple end_val105 = RowR.TupleLength() - 1;
        for (K = 0; K <= end_val105; ++K)
        {
            if (RowR[K] > 0 && ColR[K] > 0)
            {
                RealRowR = RealRowR.TupleConcat(HTuple(RowR[K]));
                RealColR = RealColR.TupleConcat(HTuple(ColR[K]));
            }
        }

        //如果中心线整体都在界外 过滤掉
        if (RealRowL.TupleLength() == 0 || RealColL.TupleLength() == 0 || RealRowR.TupleLength() == 0 || RealColR.TupleLength() == 0)
        {
            continue;
        }

        //记录直径
        (*vesselDiameters) = (*vesselDiameters).TupleConcat(MeanWidthLeftRawCenterLine + MeanWidthRightRawCenterLine);

        //生成左右轮廓
        GenContourPolygonXld(&RealContourLeft, RealRowL, RealColL);
        GenContourPolygonXld(&RealContourRight, RealRowR, RealColR);

        //生成左右轮廓区域
        GenRegionPolygon(&RegionCenterLineLeft, RealRowL, RealColL);
        GenRegionPolygon(&RegionCenterLineRight, RealRowR, RealColR);

        //封闭左右轮廓区域 封闭失败的中心线过滤掉
        try
        {
            GenRegionLine(&LineRegionCenterLineHead, HTuple(RealRowL[0]), HTuple(RealColL[0]), HTuple(RealRowR[0]), HTuple(RealColR[0]));
            GenRegionLine(&LineRegionCenterLineTail, HTuple(RealRowL[(RealRowL.TupleLength())-1]), HTuple(RealColL[(RealColL.TupleLength())-1]), HTuple(RealRowR[(RealRowR.TupleLength())-1]), HTuple(RealColR[(RealColR.TupleLength())-1]));
        }
        catch (HException &HDevExpDefaultException)
        {
            continue;
        }

        Union2(RegionCenterLineLeft, RegionCenterLineRight, &RegionCenterLineContourBody);
        Union2(LineRegionCenterLineHead, LineRegionCenterLineTail, &RegionCenterLineContourConnect);
        Union2(RegionCenterLineContourBody, RegionCenterLineContourConnect, &RegionCenterLineContour);

        //收集 界内中心线/左右轮廓/封闭区域
        ConcatObj(*CenterLines, RawCenterLine, CenterLines);

        ConcatObj(*CenterLineContours, RealContourLeft, CenterLineContours);
        ConcatObj(*CenterLineContours, RealContourRight, CenterLineContours);

        ConcatObj(RegionCenterLineContours, RegionCenterLineContour, &RegionCenterLineContours);

        ++(*NumberCenterLines);
    }

    //记录长度
    LengthXld((*CenterLines), &(*vesselLengths));

    //填充封闭区域 生成计算流速用的血管区域
    FillUp(RegionCenterLineContours, &FillupCenterLineContours);
    ClosingCircle(FillupCenterLineContours, &ClosingCenterLineContours, InnerRadiusRegionVesselConcat);
    Intersection(ClosingCenterLineContours, RegionVesselConcat, &(*RegionVesselSplited));
}

void HalconInterfaceBase::calculateGlycocalyx(HObject CenterLines, HObject RegionVesselConcat, HTuple NumberCenterLines, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Pixelsize, HTuple Magnification, HTuple *glycocalyx)
{
    HObject  VesselLines, CenterLine, VesselLine;
    HObject  line, PreLine, RegionAlignVesselConcat;
    HObject  RegionAlignVessel, VessellLineIntersection;

    HTuple  XJ, Rows, Cols, Nums, J;
    HTuple  Row, Col, Attrib, WidthR, WidthL;
    HTuple  Diameter, Diam, K, HomMat2DLine, VesselPointNum;
    HTuple  Areas, count, I, Area, useless, tangeX;
    HTuple  tangeY, Start, End, AllPointsNum, Num;
    HTuple  X0, tange, tangeSorted, tangeInverted;
    HTuple  tangeRange, Median, Range, l, Index;
    HTuple  y, x, xtx, xty, invxtx, beta;
    HTuple  Values, Value, Indices, X0Mean;

    //@brief  XJ是我选择的有用的血管  X0是单个血管点的 X0Mean 是单条血管的
    //@author yzt
    //@date   2022/05/25
    XJ = HTuple();
    (*glycocalyx) = HTuple();
    Rows = HTuple();
    Cols = HTuple();
    Nums = HTuple();

    //********************************************画测量线、计算有效血管点begin********************************************
    GenEmptyObj(&VesselLines);

    for (J = 1; J <= NumberCenterLines; ++J)
    {
        (*glycocalyx) = (*glycocalyx).TupleConcat(-1);

        //获取单条血管
        SelectObj(CenterLines, &CenterLine, J);

        //获取单条血管的位置/角度/
        GetContourXld(CenterLine, &Row, &Col);

        GetContourAttribXld(CenterLine, "angle", &Attrib);
        GenEmptyObj(&VesselLine);

        GetContourAttribXld(CenterLine, "width_right", &WidthR);
        GetContourAttribXld(CenterLine, "width_left", &WidthL);

        Diameter = (WidthL+WidthR)*(HTuple(0.75).TupleSqrt());
        Diam = (Diameter.TupleSum())/(Diameter.TupleLength());

        if (Diam >= 5 && Diam <= 30 && Row.TupleLength() > 100)
        {
            //记录下J
            XJ = XJ.TupleConcat(J-1);
            //画测量线
            HTuple end_val33 = (Row.TupleLength())-10;
            for (K = 10; K <= end_val33; K += 10)
            {
                GenRegionLine(&line, HTuple(Row[K])-16, HTuple(Col[K]), HTuple(Row[K])+16, HTuple(Col[K]));
                VectorAngleToRigid(HTuple(Row[K]), HTuple(Col[K]), 0, HTuple(Row[K]), HTuple(Col[K]), HTuple(Attrib[K]), &HomMat2DLine);
                AffineTransRegion(line, &PreLine, HomMat2DLine, "nearest_neighbor");
                ConcatObj(VesselLine, PreLine, &VesselLine);
            }
            //
            //存储待测量血管点数量, 这里统计每一条血管的血管点数，方便后续计算单条血管的糖萼值。
            CountObj(VesselLine, &VesselPointNum);
            ConcatObj(VesselLines, VesselLine, &VesselLines);
            Nums = Nums.TupleConcat(VesselPointNum);

            Rows = Rows.TupleConcat(HTuple(Row[(Row.TupleLength())/2]));
            Cols = Cols.TupleConcat(HTuple(Col[(Col.TupleLength())/2]));
        }
        else
        {
            continue;
        }
    }

    if (Nums.TupleLength() == 0)
    {
        return;
    }
    //************************************************画测量线、计算有效血管点end*********************************************

    //***********************************************获取血管点在所有帧的宽度begin*********************************************
    Areas = HTuple();

    count = 1;

    alignAntishakeRegion(RegionVesselConcat, &RegionAlignVesselConcat, 1, TupleTranPrevToRearRows.TupleLength(), TupleTranPrevToRearRows, TupleTranPrevToRearCols, "region");

    HTuple end_val65 = TupleTranPrevToRearRows.TupleLength();
    for (I = 11; I <= end_val65; ++I)
    {
        SelectObj(RegionAlignVesselConcat, &RegionAlignVessel, I);

        count += 1;
        Intersection(VesselLines, RegionAlignVessel, &VessellLineIntersection);
        AreaCenter(VessellLineIntersection, &Area, &useless, &useless);

        Areas = Areas.TupleConcat(Area);
    }
    //*********************************************获取血管点在所有帧的宽度end************************************************

    //求取糖萼的纵坐标范围
    tangeX = (count*0.25).TupleInt();
    tangeY = (count*0.75).TupleInt();

    //少于20帧影响数据精确
    if (count <= 20)
    {
        return;
    }

    //*******************************************糖萼计算begin***************************************
    Start = 0;
    End = HTuple(Nums[0])-1;
    CountObj(VesselLines, &AllPointsNum);

    HTuple end_val90 = (Nums.TupleLength())-1;
    for (Num = 0; Num <= end_val90; ++Num)
    {
        X0 = HTuple();

        for (I = Start; I <= End; ++I)
        {
            tange = HTuple();

            //取该血管上单个血管点的宽度值
            HTuple end_val97 = (Areas.TupleLength())-1;
            for (J=I; J <= end_val97; J += AllPointsNum)
            {
                tange = tange.TupleConcat(HTuple(Areas[J]));
            }

            TupleSort(tange, &tangeSorted);
            TupleInverse(tangeSorted, &tangeInverted);

            TupleSelectRange(tangeInverted, tangeX, tangeY, &tangeRange);

            if (tangeRange[0] ==tangeRange[(tangeRange.TupleLength())-1])
            {
                X0 = X0.TupleConcat(0);
                continue;
            }

            //中位数
            TupleMedian(tangeInverted, &Median);

            Range = HTuple();
            l = HTuple();

            for (Index = tangeX; Index <= tangeY; ++Index)
            {
                Range = Range.TupleConcat(Index);
                l = l.TupleConcat(1);
            }

            tangeRange = tangeRange.TupleConcat(l);

            //*****************接下来用矩阵进行最小二乘求解. y = ax + b (start)*******************
            //XT 代表X的转置 inv(*)代表*的逆
            //x beta = y
            //xT x beta = xT y
            //beta = inv( xT*x)*xT*y,求得beta的值，values矩阵里的值为[a,b]，即可取得线性回归方程

            //创建矩阵
            //Y轴
            CreateMatrix(1, Range.TupleLength(), Range, &y);
            CreateMatrix(2, Range.TupleLength(), tangeRange, &x);

            //矩阵相乘
            //这里需要注意矩阵相乘的前后顺序，否则会出错；例如：【2*2】与【2*1】相乘时得到的矩阵为【2*1】，无法前后调换。
            MultMatrix(x, x, "ABT", &xtx);
            MultMatrix(y, x, "ABT", &xty);

            //逆矩阵
            InvertMatrix(xtx, "general", 0.0, &invxtx);

            MultMatrix(xty, invxtx, "AB", &beta);
            GetFullMatrix(beta, &Values);

            Value = (HTuple(Values[1])/((HTuple(Values[0])-Median).TupleAbs()))/2;

            //X0 代表 线性回归方程与X轴的交点。
            X0 = X0.TupleConcat(Value);
            //*******************************用矩阵进行最小二乘求解(end)*****************************
        }

        Start += HTuple(Nums[Num]);
        End = (Start+HTuple(Nums[Num])) - 1;

        TupleFind(X0, 0, &Indices);
        TupleRemove(X0, Indices, &X0);
        if (0 != (int((X0.TupleLength())==0)))
        {
            (*glycocalyx) = (*glycocalyx).TupleConcat(0);
            continue;
        }
        else
        {
            TupleMean(X0, &X0Mean);

            (*glycocalyx)[HTuple(XJ[Num])] = (X0Mean*Pixelsize)/Magnification;
        }
    }
}

void HalconInterfaceBase::calculateFlowrate(HObject ImageGaussConcat, HObject RegionVesselSplited, HTuple NumberCenterLines, HTuple TupleProcessImageIndex, HTuple TupleTranPrevToRearRows, HTuple TupleTranPrevToRearCols, HTuple Pixelsize, HTuple Magnification, HTuple Fps, HTuple *Flowrate)
{
    HObject  ImageGaussAlignConcat, SkeletonRegionVesselSplited;
    HObject  UnionSkeletonRegionVesselSplited, ImageAlgoithmCommonPrev;
    HObject  ImageAlgoithmCommonRear, ImageVesselPrev;
    HObject  ImageVesselRear, ImageVesselPrevInvert, ImageVesselRearInvert;
    HObject  ImageFlowMotion, ImageFlowMotionMean, UnionRegionFlowMotion;
    HObject  RegionFlowMotion;

    HTuple  NumberImageAlgoithmCommonConcat, TrackAreas;
    HTuple  TrackAreasCount, I, TrackArea, useless;
    HTuple  TrackAreasCountPlus, ZeroIndices, MeanTrackArea;

    //将血管图像移动回跟首帧重叠
    alignAntishakeRegion(ImageGaussConcat, &ImageGaussAlignConcat, 1, 26, TupleTranPrevToRearRows,
    TupleTranPrevToRearCols, "image");

    //////////////////////////////////////////////////////////////////////////////////// *
    //
    //@note
    //红细胞流速(um/s) = 流动轨迹距离 / 前后帧时间差(s) * 像素尺寸(um/px) / 放大倍率
    //
    //令血管骨架长度为血管总长度 则可认为血管总长度为血管内可流动的最大距离
    //可推算 流动轨迹距离 = 血管骨架上的流动面积
    //
    //除以两帧之间的时间差 等价于帧率相乘代替
    //
    //即 红细胞流速(um/s) = 血管骨架上的流动面积 * 帧率 * 像素尺寸(um/px) / 放大倍率
    //
    //////////////////////////////////////////////////////////////////////////////////// *
    Skeleton(RegionVesselSplited, &SkeletonRegionVesselSplited);
    Union1(SkeletonRegionVesselSplited, &UnionSkeletonRegionVesselSplited);

    CountObj(ImageGaussAlignConcat, &NumberImageAlgoithmCommonConcat);
    if (NumberImageAlgoithmCommonConcat > 25)
    {
        NumberImageAlgoithmCommonConcat = 25;
    }
    else
    {
        NumberImageAlgoithmCommonConcat = NumberImageAlgoithmCommonConcat - 1;
    }

    //流动面积总和 流动面积有效计数
    TupleGenConst(NumberCenterLines, 0, &TrackAreas);
    TupleGenConst(NumberCenterLines, 0, &TrackAreasCount);

    for (I = 1; I <= NumberImageAlgoithmCommonConcat; ++I)
    {
        SelectObj(ImageGaussAlignConcat, &ImageAlgoithmCommonPrev, I);
        SelectObj(ImageGaussAlignConcat, &ImageAlgoithmCommonRear, I + 1);

        //计算流动轨迹
        //注：用前后帧防抖后的共同区域 而非之前的合并区域进行计算 避免防抖造成的数据误差
        //注2：红细胞体现为低灰度 因此需要将图片反色 相减后得出的才是后帧的红细胞
        ReduceDomain(ImageAlgoithmCommonPrev, UnionSkeletonRegionVesselSplited, &ImageVesselPrev);
        ReduceDomain(ImageAlgoithmCommonRear, UnionSkeletonRegionVesselSplited, &ImageVesselRear);

        InvertImage(ImageVesselPrev, &ImageVesselPrevInvert);
        InvertImage(ImageVesselRear, &ImageVesselRearInvert);

        SubImage(ImageVesselRear, ImageVesselPrev, &ImageFlowMotion, 1, 0);
        MeanImage(ImageFlowMotion, &ImageFlowMotionMean, 23, 23);
        DynThreshold(ImageFlowMotion, ImageFlowMotionMean, &UnionRegionFlowMotion, 5, "light");

        Intersection(SkeletonRegionVesselSplited, UnionRegionFlowMotion, &RegionFlowMotion);

        AreaCenter(RegionFlowMotion, &TrackArea, &useless, &useless);
        if (TrackArea.TupleLength() > 0)
        {
            TrackAreas += TrackArea;

            //计算流速所用的帧数 需要考虑由于不清晰造成的跳帧
            TupleGenConst(TrackArea.TupleLength(), HTuple(TupleProcessImageIndex[I])-HTuple(TupleProcessImageIndex[I-1]), &TrackAreasCountPlus);

            TupleFind(TrackArea, 0, &ZeroIndices);
            if (ZeroIndices.TupleLength() > 0 && ZeroIndices != -1)
            {
                TupleReplace(TrackAreasCountPlus, ZeroIndices, 0, &TrackAreasCountPlus);
            }

            TrackAreasCount += TrackAreasCountPlus;
        }

        TupleFind(TrackAreasCount, 0, &ZeroIndices);
        if (ZeroIndices.TupleLength() > 0 && ZeroIndices!=-1)
        {
            TupleReplace(TrackAreasCount, ZeroIndices, 1, &TrackAreasCount);
        }
    }

    //计算流速 公式见上 此处以大恒相机为例
    MeanTrackArea = (TrackAreas.TupleReal())/(TrackAreasCount.TupleReal());
    (*Flowrate) = ((MeanTrackArea*Fps)*Pixelsize)/Magnification;
}
