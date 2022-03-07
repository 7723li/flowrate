#include "flowrate.h"

void Flowrate::calFlowTrackDistances(QImage imagePrev, QImage imageRear, double &trackDistance, RegionPoints &regionPoints)
{
    QVector<double> trackDistances;
    QVector<RegionPoints> _regionPoints;
    calFlowTrackDistances(QVector<QImage>::fromStdVector(std::vector<QImage>({imagePrev, imageRear})), trackDistances, _regionPoints);

    trackDistance = trackDistances[0];
    regionPoints = _regionPoints[0];
}

void Flowrate::calFlowTrackDistances(const QVector<QImage> &imagelist, QVector<double> &trackDistances, QVector<RegionPoints> &regionPoints)
{
    if(imagelist.size() < 2)
    {
        return;
    }

    HObject ImagePrevGauss, RegionUnionPrev;
    HObject RegionCellTrackConcat, ImageRearGauss;
    HObject RegionUnionRear, RegionDifference, Line;
    HObject RegionIntersection, ConnectedRegions, RegionUnionPrevTrans;
    HObject RegionIntersect, TransMask;
    HObject RegionUnionRearMask, RegionUnionAntiShake;
    HObject ImagePrevCalRegion, ImageRearCalRegion, ImagePrevCalRegionEquHisto;
    HObject ImageRearCalRegionEquHisto, ImagePrevCalRegionInvert;
    HObject ImageRearCalRegionInvert, ImageCellTrackRegion;
    HObject ImageMean, RegionCellTrackMask;
    HObject RegionUnionCellTrack, RegionsSplited, RegionsSplitedFilter;
    HObject RawRegionObserve, DilationRawRegionObserve, RegionObserve, RegionObserveBorder;
    HObject RegionCellTrackConcatTrans, RegionCellTrackSelected;
    HObject RegionCellTrackObserve, UnionRegionCellTrackObserve;
    HObject RegionCellTrackObservePrev, RegionCellTrackObserveRear;
    HObject UnionRegionCellTrackMotion, ConnectedRegionCellTrackMotion, RegionCellTrackMotion;

    HTuple TupleDebugTransRows, TupleDebugTransCols;
    HTuple TupleProcessIndex, AreaRegionUnionRear;
    HTuple useless, dis_Row;
    HTuple AreaRow, dis_Col, AreaCol, maxRowDis;
    HTuple deviationRowDis, maxColDis, deviationColDis;
    HTuple maxIntersectAreaRow, maxIntersectAreaCol;
    HTuple maxIntersectArea, Row, Col, HomMat2D;
    HTuple AreaRegionIntersect, RowRegionIntersect, ColumnRegionIntersect;
    HTuple maxIntersectAreaRowCopy, maxIntersectAreaColCopy;
    HTuple X, HomMat2DPrevToRearTemp, Y, HomMat2DPrevToRear;
    HTuple RegionCellTrackMaskRow1;
    HTuple RegionCellTrackMaskColumn1, RegionCellTrackMaskRow2;
    HTuple RegionCellTrackMaskColumn2, AreaRegionUnionCellTrack;
    HTuple RegionsSplitedInnerRadius, RegionsSplitedInnerRadiusMax, MinInnerRadius, MaxInnerRadius, AreaRegionsSplitedFilter;
    HTuple ContLength, Grade, SortedGradeIndex;
    HTuple HomMat2DObserveRearToPrev, AreaRegionCellTrackObserve;
    HTuple UnionRegionCellTrackObserveRows, UnionRegionCellTrackObserveCols;
    HTuple NumberConnectedRegionCellTrackMotion, InnerRadiusRegionCellTrackMotion, DistanceRegionCellTrackMotion;
    HTuple TupleTrackDistance, TrackDistance, RegionObserveBorderRows, RegionObserveBorderCols;
    int width = 0, height = 0, i = 0, j = 0;

    trackDistances.resize(imagelist.size());
    regionPoints.resize(imagelist.size());

    //**************************************消抖+找到图像序列中稳定出现的区域BEGIN***************************************
    //首帧预处理 对应的变量用在后续的后帧缓存
    //首帧必然清晰才开始录像
    preProcess(imagelist[0], &ImagePrevGauss, &RegionUnionPrev);
    width = imagelist[0].width(), height = imagelist[0].height();

    //记录的流动轨迹区域
    GenRegionPoints(&RegionCellTrackConcat, HTuple(), HTuple());

    //记录的位移行/列/处理过的图像下标
    TupleDebugTransRows = 0;
    TupleDebugTransCols = 0;
    TupleProcessIndex = 0;

    //遍历图像序列
    int imagelistCount = imagelist.size();
    for (i = 1; i < imagelistCount; ++i)
    {
        //后一帧预处理
        preProcess(imagelist[i], &ImageRearGauss, &RegionUnionRear);

        //后帧血管区域判空 若为空 next one
        AreaCenter(RegionUnionRear, &AreaRegionUnionRear, &useless, &useless);
        if (AreaRegionUnionRear.TupleLength() == 0 || -1 == AreaRegionUnionRear)
        {
            continue;
        }

        //不清晰的也不要 next one
        double sharpness = getImageSharpness(ImageRearGauss, RegionUnionRear);
        bool _isSharp = isSharp(sharpness);
        if (!_isSharp)
        {
            continue;
        }

        //记录处理了一帧的下缀
        TupleProcessIndex = TupleProcessIndex.TupleConcat(i);

        //前帧到后帧的差异区域
        Difference(RegionUnionRear, RegionUnionPrev, &RegionDifference);

        //计算差异区域在各行的宽度
        dis_Row = HTuple();
        for (j = 0; j < height - 1; ++j)
        {
            GenRegionLine(&Line, j, 0, j, width - 1);
            Intersection(Line, RegionDifference, &RegionIntersection);
            Connection(RegionIntersection, &ConnectedRegions);
            AreaCenter(ConnectedRegions, &AreaRow, &useless, &useless);
            dis_Row = dis_Row.TupleConcat(AreaRow);
        }

          //计算差异区域在各列的高度
        dis_Col = HTuple();
        for (j = 0; j < width - 1; ++j)
        {
            GenRegionLine(&Line, 0, j, height - 1, j);
            Intersection(Line, RegionDifference, &RegionIntersection);
            Connection(RegionIntersection, &ConnectedRegions);
            AreaCenter(ConnectedRegions, &AreaCol, &useless, &useless);
            dis_Col = dis_Col.TupleConcat(AreaCol);
        }

        //最大距离的正负作为粗对齐范围 方差作为步进粗对齐距离
        TupleMax(dis_Row, &maxRowDis);
        TupleDeviation(dis_Row, &deviationRowDis);
        TupleMax(dis_Col, &maxColDis);
        TupleDeviation(dis_Col, &deviationColDis);

        //粗对齐 找出一个横向纵向位移后 能使->前后帧血管区域重叠面积最大<-的位移数值
        //注意：这里是为了减少后面细对齐位移的次数
        maxIntersectAreaRow = 0;
        maxIntersectAreaCol = 0;
        maxIntersectArea = 0;

        for (Row=-maxRowDis; Row.Continue(maxRowDis, deviationRowDis); Row += deviationRowDis)
        {
            for (Col=-maxColDis; Col.Continue(maxColDis, deviationColDis); Col += deviationColDis)
            {
                HomMat2dIdentity(&HomMat2D);
                HomMat2dTranslate(HomMat2D, Row, Col, &HomMat2D);
                AffineTransRegion(RegionUnionPrev, &RegionUnionPrevTrans, HomMat2D, "nearest_neighbor");

                Intersection(RegionUnionRear, RegionUnionPrevTrans, &RegionIntersect);
                AreaCenter(RegionIntersect, &AreaRegionIntersect, &RowRegionIntersect,&ColumnRegionIntersect);
                if (AreaRegionIntersect > maxIntersectArea)
                {
                    maxIntersectArea = AreaRegionIntersect;
                    maxIntersectAreaRow = Row;
                    maxIntersectAreaCol = Col;
                }
            }
        }

        //细对齐 将前帧的血管区域移动至的重叠面积最大的地方 达到防抖的效果
        //J循环是为了多找一次 提高精度
        maxIntersectAreaRowCopy = maxIntersectAreaRow;
        maxIntersectAreaColCopy = maxIntersectAreaCol;
        for (j = 0; j <= 1; j += 1)
        {
            for (X = -2.56; X <= 2.56; X += 0.04)
            {
                HomMat2dIdentity(&HomMat2DPrevToRearTemp);
                HomMat2dTranslate(HomMat2DPrevToRearTemp, maxIntersectAreaRow, maxIntersectAreaColCopy + X, &HomMat2DPrevToRearTemp);
                AffineTransRegion(RegionUnionPrev, &RegionUnionPrevTrans, HomMat2DPrevToRearTemp, "nearest_neighbor");

                Intersection(RegionUnionPrevTrans, RegionUnionRear, &RegionIntersect);
                AreaCenter(RegionIntersect, &AreaRegionIntersect, &useless, &useless);
                if (AreaRegionIntersect > maxIntersectArea)
                {
                    maxIntersectArea = AreaRegionIntersect;
                    maxIntersectAreaCol = maxIntersectAreaColCopy+X;
                }
            }

            for (Y = -2.56; Y <= 2.56; Y += 0.04)
            {
                HomMat2dIdentity(&HomMat2DPrevToRearTemp);
                HomMat2dTranslate(HomMat2DPrevToRearTemp, maxIntersectAreaRowCopy + Y, maxIntersectAreaCol, &HomMat2DPrevToRearTemp);
                AffineTransRegion(RegionUnionPrev, &RegionUnionPrevTrans, HomMat2DPrevToRearTemp, "nearest_neighbor");

                Intersection(RegionUnionPrevTrans, RegionUnionRear, &RegionIntersect);
                AreaCenter(RegionIntersect, &AreaRegionIntersect, &useless, &useless);
                if (AreaRegionIntersect>maxIntersectArea)
                {
                    maxIntersectArea = AreaRegionIntersect;
                    maxIntersectAreaRow = maxIntersectAreaRowCopy+Y;
                }
            }
        }

        //最终找到最佳的前帧对齐到后帧的位移数值
        HomMat2dIdentity(&HomMat2DPrevToRear);
        HomMat2dTranslate(HomMat2DPrevToRear, maxIntersectAreaRow, maxIntersectAreaCol, &HomMat2DPrevToRear);
        AffineTransRegion(RegionUnionPrev, &RegionUnionPrevTrans, HomMat2DPrevToRear, "nearest_neighbor");

        TupleDebugTransRows = TupleDebugTransRows.TupleConcat(maxIntersectAreaRow);
        TupleDebugTransCols = TupleDebugTransCols.TupleConcat(maxIntersectAreaCol);

        //记录合并细胞轨迹
        if (TupleProcessIndex.TupleLength() > 2)
        {
            //生成合成流动轨迹的掩膜
            AffineTransRegion(RegionCellTrackConcat, &RegionCellTrackConcat, HomMat2DPrevToRear, "nearest_neighbor");
            SmallestRectangle1(RegionCellTrackConcat, &RegionCellTrackMaskRow1, &RegionCellTrackMaskColumn1, &RegionCellTrackMaskRow2, &RegionCellTrackMaskColumn2);
            GenRectangle1(&RegionCellTrackMask, RegionCellTrackMaskRow1, RegionCellTrackMaskColumn1, RegionCellTrackMaskRow2, RegionCellTrackMaskColumn2);

            //只要与掩膜重叠的部分
            Intersection(RegionUnionPrevTrans, RegionCellTrackMask, &RegionUnionPrevTrans);
            ConcatObj(RegionCellTrackConcat, RegionUnionPrevTrans, &RegionCellTrackConcat);
        }
        else
        {
            Union1(RegionUnionPrevTrans, &RegionCellTrackConcat);
        }

        ImagePrevGauss = ImageRearGauss;
        RegionUnionPrev = RegionUnionRear;
    }

    //**************************************消抖+找到图像序列中稳定出现的区域END***************************************

    //**************************************血管轨迹区域分段BEGIN***************************************
    //分解
    Union1(RegionCellTrackConcat, &RegionUnionCellTrack);
    AreaCenter(RegionUnionCellTrack, &AreaRegionUnionCellTrack, &useless, &useless);
    if (AreaRegionUnionCellTrack > 0)
    {
        splitVesselRegion(RegionUnionCellTrack, &RegionsSplited);
    }
    else
    {
        return;
    }

    //选择足够细长 面积足够大又不大得过分的一个区域作为观察区域
    RegionFeatures(RegionsSplited, "inner_radius", &RegionsSplitedInnerRadius);
    TupleMax(RegionsSplitedInnerRadius, &RegionsSplitedInnerRadiusMax);
    MinInnerRadius = 4;
    MaxInnerRadius = 10;
    while (MinInnerRadius < RegionsSplitedInnerRadiusMax)
    {
        SelectShape(RegionsSplited, &RegionsSplitedFilter, "inner_radius", "and", MinInnerRadius, MaxInnerRadius);
        SelectShape(RegionsSplitedFilter, &RegionsSplitedFilter, "area", "and", 1000, 5000);
        AreaCenter(RegionsSplitedFilter, &AreaRegionsSplitedFilter, &useless, &useless);
        if (AreaRegionsSplitedFilter.TupleLength() > 0)
        {
            //根据权重 = 周长 * 面积开方选择一个原始观察区域
            Contlength(RegionsSplitedFilter, &ContLength);
            Grade = ContLength*(AreaRegionsSplitedFilter.TupleSqrt());
            TupleSortIndex(Grade, &SortedGradeIndex);
            SelectObj(RegionsSplitedFilter, &RawRegionObserve, HTuple(SortedGradeIndex[(SortedGradeIndex.TupleLength()) - 1]) + 1);

            //分解时做了一次闭运算 面积会略有损失 这里膨胀一下 取回与原区域重叠的部分作为正确的观察区域
            DilationCircle(RawRegionObserve, &DilationRawRegionObserve, 1.5);
            Intersection(RegionUnionCellTrack, DilationRawRegionObserve, &RegionObserve);
            break;
        }
        else
        {
            MinInnerRadius += 6;
            MaxInnerRadius += 6;
        }
    }
    //**************************************血管轨迹区域分段END***************************************

    //**************************************计算流动轨迹距离BEGIN***************************************
    /*!
    * @brief
    * 令 红细胞流速(um/s) = 流动轨迹距离(px) * 像素尺寸(um/px) / 放大倍率 / 前后帧时间差(s)
    *
    * 以大恒相机为例 像素尺寸取5.6mm/px 放大倍率取5 帧率取30fps
    * 若算出帧1与帧2之间轨迹距离为50px 则两帧之间的流速为 50 * 5.6 / 5 / (1/30) = 50 * 5.6 / 5 * 30 = 1680um/s
    */

    //倒序遍历图像序列
    for (i = TupleProcessIndex.TupleLength() - 1; i >= 1; --i)
    {
        // 调试显示
        Boundary(RegionObserve, &RegionObserveBorder, "inner");
        GetRegionPoints(RegionObserveBorder, &RegionObserveBorderRows, &RegionObserveBorderCols);
        int RegionObserveBorderPointCount = RegionObserveBorderRows.TupleLength();
        for(j = 0; j < RegionObserveBorderPointCount; ++j)
        {
            regionPoints[TupleProcessIndex[i]].push_back(QPoint(RegionObserveBorderCols[j], RegionObserveBorderRows[j]));
        }

        if(i <= 1)
        {
            break;
        }

        //取出一帧轨迹区域
        //计算观测区域内的运动轨迹
        SelectObj(RegionCellTrackConcat, &RegionCellTrackSelected, i);
        Intersection(RegionCellTrackSelected, RegionObserve, &RegionCellTrackObserveRear);

        //前帧同上
        SelectObj(RegionCellTrackConcat, &RegionCellTrackSelected, i - 1);
        Intersection(RegionCellTrackSelected, RegionObserve, &RegionCellTrackObservePrev);

        //计算前帧到后帧的流动形变
        Difference(RegionCellTrackObserveRear, RegionCellTrackObservePrev, &UnionRegionCellTrackMotion);
        Connection(UnionRegionCellTrackMotion, &ConnectedRegionCellTrackMotion);

        //计算各个形变到前帧区域的距离
        TupleTrackDistance = HTuple();
        CountObj(ConnectedRegionCellTrackMotion, &NumberConnectedRegionCellTrackMotion);
        for (j = 1; j <= NumberConnectedRegionCellTrackMotion; ++j)
        {
            SelectObj(ConnectedRegionCellTrackMotion, &RegionCellTrackMotion, j);
            //如果形变半径大于0.5 说明是已经形成孤岛的大形变(1.) 否则只是贴着前帧区域的小形变(2.)
            //1.形变外围半径的0.6倍 2.形变区域到前帧区域的最小距离
            //注意：1中不取直径是为了抵消遇到扁平孤岛时的严重畸变 取半径的0.7倍是为了弥补狭长孤岛的损失
            RegionFeatures(RegionCellTrackMotion, "inner_radius", &InnerRadiusRegionCellTrackMotion);
            if (InnerRadiusRegionCellTrackMotion.TupleLength() <= 0 || InnerRadiusRegionCellTrackMotion <= 0)
            {
                continue;
            }
            else if (InnerRadiusRegionCellTrackMotion > 0.5)
            {
                RegionFeatures(RegionCellTrackMotion, "outer_radius", &DistanceRegionCellTrackMotion);
                DistanceRegionCellTrackMotion = DistanceRegionCellTrackMotion*0.6;
            }
            else
            {
                DistanceRrMin(RegionCellTrackMotion, RegionCellTrackObservePrev, &DistanceRegionCellTrackMotion, &useless, &useless, &useless, &useless);
            }
            TupleTrackDistance = TupleTrackDistance.TupleConcat(DistanceRegionCellTrackMotion);
        }

        TupleSum(TupleTrackDistance, &TrackDistance);
        trackDistances[TupleProcessIndex[i]] = TrackDistance.D();

        RegionCellTrackObserveRear = RegionCellTrackObservePrev;

        //根据上面前一帧位移到后一帧的数值 后一帧的轨迹和观察区域反向位移到与前一帧 自然地这里的前帧就变成下次循环里的后帧
        HomMat2dIdentity(&HomMat2DObserveRearToPrev);
        HomMat2dTranslate(HomMat2DObserveRearToPrev, -HTuple(TupleDebugTransRows[i]), -HTuple(TupleDebugTransCols[i]), &HomMat2DObserveRearToPrev);
        AffineTransRegion(RegionObserve, &RegionObserve, HomMat2DObserveRearToPrev, "nearest_neighbor");
        AffineTransRegion(RegionCellTrackConcat, &RegionCellTrackConcat, HomMat2DObserveRearToPrev, "nearest_neighbor");
    }
    //**************************************计算流动轨迹距离END***************************************
}

double Flowrate::calcFlowrateFromFlowTrackDistances(const QVector<double> &trackLengths, double fps, double pixelSize, int magnification)
{
    // 取轨迹距离的平均数
    double trackAreaSum = 0.0, flowrate = 0.0;
    int trackcount = trackLengths.size(), legalTrackcount = 0;
    for(int i = 0; i < trackcount; ++i)
    {
        if(trackLengths[i] <= 0)
        {
            continue;
        }

        trackAreaSum += trackLengths[i];
        ++legalTrackcount;
    }

    if(legalTrackcount > 0)
    {
        flowrate = (trackAreaSum / legalTrackcount) * fps * pixelSize / magnification;
    }

    return flowrate;
}

double Flowrate::getImageSharpness(const QImage &Image)
{
    double sharpness = 0.0;
    HObject ImageGauss, RegionUnion;

    // 预处理函数中使用了均值滤波+动态阈值的方法获取连续的低灰度区域 可认为这些区域都是血管区域
    preProcess(Image, &ImageGauss, &RegionUnion);

    sharpness = getImageSharpness(ImageGauss, RegionUnion);

    return sharpness;
}

bool Flowrate::isSharp(double sharpness)
{
    return (sharpness >= 0.91 && sharpness < 1.195);
}

void Flowrate::getBoundaryVesselRegion(const QImage &image, RegionPoints &RegionBoundaryVesselPoints)
{
    HObject ImageGauss, RegionUnion, RegionUnionBorder;

    HTuple Rows, Cols;

    preProcess(image, &ImageGauss, &RegionUnion);
    Boundary(RegionUnion, &RegionUnionBorder, "inner");
    GetRegionPoints(RegionUnionBorder, &Rows, &Cols);

    RegionBoundaryVesselPoints = RegionPoints();
    int pointCount = Rows.TupleLength();
    for(int i = 0; i < pointCount; ++i)
    {
        RegionBoundaryVesselPoints.push_back(QPoint(Cols[i], Rows[i]));
    }
}

void Flowrate::splitVesselRegion(const QImage &image, QVector<RegionPoints> &RegionsSplitedPoints)
{
    HObject ImageGauss, RegionVesselUnion;
    HObject RegionsSplited, Branch, Node;

    HTuple RegionBranchsCount, RegionNodesCount, Rows, Cols;

    preProcess(image, &ImageGauss, &RegionVesselUnion);
    splitVesselRegion(RegionVesselUnion, &RegionsSplited);

    CountObj(RegionsSplited, &RegionBranchsCount);
    RegionsSplitedPoints = QVector<RegionPoints>(RegionBranchsCount.I());
    for(int i = 1; i <= RegionBranchsCount; ++i)
    {
        SelectObj(RegionsSplited, &Branch, i);
        GetRegionPoints(Branch, &Rows, &Cols);
        int pointCount = Rows.TupleLength();
        for(int j = 0; j < pointCount; ++j)
        {
            RegionsSplitedPoints[i - 1].push_back(QPoint(Cols[j], Rows[j]));
        }
    }
}

void Flowrate::preProcess(const QImage& image, HObject *ImageGauss, HObject *RegionUnion)
{
    HObject ImageOri, ImageMean, RegionDynThresh, RegionClosing;
    HObject RawRegionConnected, RegionConnected;

    HTuple Channels, AreaRawRegionConnected;
    HTuple useless, MinArea, MaxArea;

    // 使用GenImage1将QImage的图像数据复制到HObject
    // 这里确保了QImage是单通道灰度图 因此hdev中原程序的格式转换步骤都可以忽略
    GenImage1(&ImageOri, "byte", image.width(), image.height(), (Hlong)image.bits());

    GaussFilter(ImageOri, ImageGauss, 7);

    //寻找血管区域 均值滤波+动态阈值
    MeanImage(*ImageGauss, &ImageMean, 43, 43);
    DynThreshold(*ImageGauss, ImageMean, &RegionDynThresh, 5, "dark");

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

double Flowrate::getImageSharpness(const HObject &ImageGauss, const HObject &RegionUnion)
{
    double sharpness = 0.0;

    HObject BorderRegionUnion, ImageGaussEquHisto;
    HObject ImaAmp, ImaDir, ImageMean, BorderEdges;

    HTuple AreaBorderRegionUnion, useless;
    HTuple AreaBorderEdges;

    //血管区域的外轮廓
    Boundary(RegionUnion, &BorderRegionUnion, "inner");

    //通用方法获取图像边缘区域
    EquHistoImage(ImageGauss, &ImageGaussEquHisto);
    EdgesImage(ImageGaussEquHisto, &ImaAmp, &ImaDir, "canny", 1, "nms", -1, -1);
    MeanImage(ImaAmp, &ImageMean, 23, 23);
    DynThreshold(ImaAmp, ImageMean, &BorderEdges, 5, "light");

    //计算 血管区域外轮廓的周长 以及 边缘区域长度
    AreaCenter(BorderRegionUnion, &AreaBorderRegionUnion, &useless, &useless);
    AreaCenter(BorderEdges, &AreaBorderEdges, &useless, &useless);

    //理论上来说 在理想情况下 当一幅图像足够清晰时 其轮廓周长(A)与边缘长度(B)应该相等 即A / B = 1 比如一个线宽为1的黑框白色正方形
    //经过几个数据集实验 此处清晰度取值范围为[0.91, 1.195)
    if (AreaBorderRegionUnion.TupleLength() == 1 && AreaBorderEdges.TupleLength() == 1 && AreaBorderRegionUnion > 0)
    {
        sharpness = AreaBorderEdges.TupleReal() / AreaBorderRegionUnion.TupleReal();
    }
    else
    {
        sharpness = 0;
    }

    return sharpness;
}

void Flowrate::splitVesselRegion(const HObject &RegionVesselUnion, HObject *RegionsSplited)
{
    HObject ClosingRegionVesselUnion, RegionVesselConnection;
    HObject RegionVesselDoSplit, UnionRegionVesselDoSplit;
    HObject RegionVesselDoNotSplit, skeleton, BorderNeedSplitRegions;
    HObject Circles, CirclesWidthSection, Section;
    HObject CircleSections, CircleSectionBig, RegionConcatBig;
    HObject CircleSectionSmall, RegionConcatSmall, ObjectsConcat1;
    HObject RegionUnion1, ConnectedRegions3;

    HTuple AreaRegionVessel, useless, MaxArea;
    HTuple RowsSkeleton, ColumnsSkeleton, DistanceMin;
    HTuple IndicesZeros, Radius, MaxRadius, RadiusSection;
    HTuple Number, I, J;

    //先做一次闭运算平滑轨迹边缘
    ClosingCircle(RegionVesselUnion, &ClosingRegionVesselUnion, 3.5);

    //计算连续区域
    Connection(ClosingRegionVesselUnion, &RegionVesselConnection);

    //计算各个连续区域的像素面积
    AreaCenter(RegionVesselConnection, &AreaRegionVessel, &useless, &useless);

    //如果全部的连续区域都没有大于4000的 那就都不用分解 分解区域就等于连续区域
    TupleMax(AreaRegionVessel, &MaxArea);
    if (MaxArea < 4000)
    {
        (*RegionsSplited) = RegionVesselConnection;
        return;
    }

    //连续像素大于4000px的(RegionVesselDoSplit)要进行分解
    SelectShape(RegionVesselConnection, &RegionVesselDoSplit, "area", "and", 4000, MaxArea);
    Union1(RegionVesselDoSplit, &UnionRegionVesselDoSplit);

    //小于等于4000(RegionVesselDoSplit)的直接归到已经分解好的(RegionsSplited)
    Difference(RegionVesselConnection, RegionVesselDoSplit, &RegionVesselDoNotSplit);
    GenRegionPoints(&(*RegionsSplited), HTuple(), HTuple());
    ConcatObj((*RegionsSplited), RegionVesselDoNotSplit, &(*RegionsSplited));

    //求血管区域骨骼(Skeleton) 即中心线
    Skeleton(UnionRegionVesselDoSplit, &skeleton);
    GetRegionPoints(skeleton, &RowsSkeleton, &ColumnsSkeleton);

    //需要分割的血管区域的外边界(BorderNeedSplitRegions)
    Boundary(UnionRegionVesselDoSplit, &BorderNeedSplitRegions, "inner");

    //求出中心线到外边界的最小距离
    DistancePr(BorderNeedSplitRegions, RowsSkeleton, ColumnsSkeleton, &DistanceMin, &useless);

    //过滤掉最小距离中的0 沿中心线作若干个圆 得到的圆集合无限贴近于外边界
    TupleFind(DistanceMin, 0, &IndicesZeros);
    TupleRemove(DistanceMin, IndicesZeros, &Radius);
    TupleRemove(RowsSkeleton, IndicesZeros, &RowsSkeleton);
    TupleRemove(ColumnsSkeleton, IndicesZeros, &ColumnsSkeleton);
    GenCircle(&Circles, RowsSkeleton, ColumnsSkeleton, Radius);

    //将中心线上的圆 根据半径每差2px归为一个半径等级 [0, 2]等级1 [2, 4]等级2 [4, 6]等级3...
    TupleMax(Radius, &MaxRadius);
    RadiusSection = 0;
    while (RadiusSection < MaxRadius)
    {
        SelectShape(Circles, &CirclesWidthSection, "inner_radius", "and", RadiusSection, RadiusSection+2);

        Union1(CirclesWidthSection, &Section);
        if (RadiusSection > 1)
        {
            ConcatObj(CircleSections, Section, &CircleSections);
        }
        else
        {
            Union1(Section, &CircleSections);
        }

        RadiusSection += 2;
    }

    //通过观察推测出 如果邻近的半径等级出现跨级 比如等级6与等级4之间为等级2 可以认为这个等级2的区域需要被断开 最终得到3个等级分别为6 2 4的[独立区域]
    //但如果等级2出现在等级3与等级1之间 则可以认为这个等级2可以被保留 最终得到1个等级为3的[合并区域]
    //因此需要从高等级往低等级进行匹配
    CountObj(CircleSections, &Number);
    for (I = Number; I >= 2; I += -1)
    {
        //选择一个高等级
        SelectObj(CircleSections, &CircleSectionBig, I);

        //如果这个等级不是最高的 需要看看之前是否已经被合并了一部分
        //比如6个等级中的第5级 在匹配第4级之前 需要先把已经被第6级匹配过的部分过滤掉 避免重复匹配造成分割区域重叠
        if (0 != (int(I<Number)))
        {
            Difference(CircleSectionBig, (*RegionsSplited), &RegionConcatBig);
        }
        else
        {
            RegionConcatBig = CircleSectionBig;
        }

        for (J = I - 1; J >= 1; J += -1)
        {
            //选择一个低等级
            SelectObj(CircleSections, &CircleSectionSmall, J);

            //低等级也需要过滤掉已经被合并的部分
            //比如第6级在匹配第5级时 第4级已经被合并了一部分 那么第5级在匹配第4级时 也需要过滤掉这些部分
            //注意：脑子不要套娃了！
            Difference(CircleSectionSmall, (*RegionsSplited), &RegionConcatSmall);

            //检测与高等级的连接部分
            ConcatObj(RegionConcatSmall, RegionConcatBig, &ObjectsConcat1);
            Union1(ObjectsConcat1, &RegionUnion1);
            Connection(RegionUnion1, &ConnectedRegions3);

            //如果和高等级连接成功 那么等级就会自然变成高等级
            SelectShape(ConnectedRegions3, &RegionConcatBig, "inner_radius", "and", 2*(I-1), 2*I);
        }

        ConcatObj((*RegionsSplited), RegionConcatBig, &(*RegionsSplited));
    }
}
