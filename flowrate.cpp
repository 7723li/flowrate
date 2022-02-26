﻿#include "flowrate.h"

void Flowrate::calFlowTrackAreas(QImage imagePrev, QImage imageRear, double &trackArea, RegionPoints &regionPoints)
{
    QVector<double> trackAreas;
    QVector<RegionPoints> _regionPoints;
    calFlowTrackAreas(QVector<QImage>::fromStdVector(std::vector<QImage>({imagePrev, imageRear})), trackAreas, _regionPoints);

    trackArea = trackAreas[0];
    regionPoints = _regionPoints[0];
}

void Flowrate::calFlowTrackAreas(const QVector<QImage> &imagelist, QVector<double> &trackAreas, QVector<RegionPoints> &regionPoints)
{
    if(imagelist.size() < 2)
    {
        return;
    }

    HObject  ImagePrevGauss, RegionUnionPrev, ImageRearGauss, RegionUnionRear;
    HObject  RegionDifference, Line, RegionIntersection;
    HObject  ConnectedRegions, RegionUnionRearTrans, RegionIntersect;
    HObject  ImageRearGaussTrans, RegionUnionPrevRear;
    HObject  ImagePrevCalRegion, ImageRearCalRegion, ROIintersection;
    HObject  ImagePrevCalRegionEquHisto, ImageRearCalRegionEquHisto;
    HObject  RegionCellTrack, ImagePrevCalRegionInvert, BorderRegionCellTrack;
    HObject  ImageRearCalRegionInvert, ImageCellTrackRegion, ImageMean;

    HTuple  AreaRegionUnionPrev, useless, AreaRegionUnionRear;
    HTuple  dis_Row, AreaRow, dis_Col, AreaCol;
    HTuple  maxRowDis, deviationRowDis, maxColDis;
    HTuple  deviationColDis, maxIntersectAreaRow, maxIntersectAreaCol;
    HTuple  maxIntersectArea, HomMat2D;
    HTuple  AreaRegionIntersect, RowRegionIntersect, ColumnRegionIntersect;
    HTuple  RegionCellTracksArea, RegionCellTracksRow;
    HTuple  RegionCellTracksColumn;
    HTuple BorderRegionCellTrackRows, BorderRegionCellTrackCols;

    trackAreas = QVector<double>(imagelist.size() - 1, 0);
    regionPoints = QVector<RegionPoints>(imagelist.size() - 1);

    //首帧预处理 对应的变量用在后续的后帧缓存
    preProcess(imagelist[0], &ImagePrevGauss, &RegionUnionPrev);

    //遍历图像序列
    int imageCount = imagelist.size();
    for (int i = 1; i < imageCount; ++i)
    {
        const QImage& imageRear = imagelist[i];

        int width = imageRear.width(), height = imageRear.height();

        //前帧血管区域判空 若为空 next one
        AreaCenter(RegionUnionPrev, &AreaRegionUnionPrev, &useless, &useless);
        if (AreaRegionUnionPrev.TupleLength() == 0 || -1 == AreaRegionUnionPrev)
        {
            preProcess(imageRear, &ImagePrevGauss, &RegionUnionPrev);
            continue;
        }

        //后一帧预处理
        preProcess(imageRear, &ImageRearGauss, &RegionUnionRear);

        //后帧血管区域判空 若为空 next one
        AreaCenter(RegionUnionRear, &AreaRegionUnionRear, &useless, &useless);
        if (AreaRegionUnionRear.TupleLength() == 0 || -1 == AreaRegionUnionRear)
        {
            preProcess(imagelist[++i], &ImagePrevGauss, &RegionUnionPrev);
            continue;
        }

        //前后帧差异区域
        Difference(RegionUnionPrev, RegionUnionRear, &RegionDifference);

        //计算差异区域在各行的宽度
        dis_Row = HTuple();
        for (int j = 0; j < height; ++j)
        {
            GenRegionLine(&Line, j, 0, j, width - 1);
            Intersection(Line, RegionDifference, &RegionIntersection);
            Connection(RegionIntersection, &ConnectedRegions);
            AreaCenter(ConnectedRegions, &AreaRow, &useless, &useless);
            dis_Row = dis_Row.TupleConcat(AreaRow);
        }

        //计算差异区域在各列的高度
        dis_Col = HTuple();
        for (int j = 0; j < width; ++j)
        {
            GenRegionLine(&Line, 0, j, height - 1, j);
            Intersection(Line, RegionDifference, &RegionIntersection);
            Connection(RegionIntersection, &ConnectedRegions);
            AreaCenter(ConnectedRegions, &AreaCol, &useless, &useless);
            dis_Col = dis_Col.TupleConcat(AreaCol);
        }

        //最大距离的正负作为粗瞄范围 方差作为步进粗瞄距离
        TupleMax(dis_Row, &maxRowDis);
        TupleDeviation(dis_Row, &deviationRowDis);
        TupleMax(dis_Col, &maxColDis);
        TupleDeviation(dis_Col, &deviationColDis);

        //粗瞄 找出一个横向移动后 能使前后帧血管区域重叠面积最大的差异区域高度 减少后面防抖移动的次数
        //插值方式采用比较粗糙的nearest_neighbor即可 后面浮点数处再采用比较精准的插值方法
        maxIntersectAreaRow = 0;
        maxIntersectAreaCol = 0;
        maxIntersectArea = 0;

        double steprow = deviationRowDis.D(), stepcol = deviationColDis.D();
        for (double row = -maxRowDis; row <= maxRowDis; row += steprow)
        {
            for (double col = -maxColDis; col <= maxColDis; col += stepcol)
            {
                HomMat2dIdentity(&HomMat2D);
                HomMat2dTranslate(HomMat2D, row, col, &HomMat2D);
                AffineTransRegion(RegionUnionRear, &RegionUnionRearTrans, HomMat2D, "nearest_neighbor");

                Intersection(RegionUnionPrev, RegionUnionRearTrans, &RegionIntersect);
                AreaCenter(RegionIntersect, &AreaRegionIntersect, &RowRegionIntersect, &ColumnRegionIntersect);
                if (AreaRegionIntersect > maxIntersectArea)
                {
                    maxIntersectArea = AreaRegionIntersect;
                    maxIntersectAreaRow = row;
                    maxIntersectAreaCol = col;
                }
            }
        }

        //细瞄 将前帧的血管区域移动至的重叠面积最大的地方 达到防抖的效果
        //J循环是为了多找一次 提高精度
        for (int j = 0; j <= 1; ++j)
        {
            for (double x = -2.56; x <= 2.56; x += 0.16)
            {
                HomMat2dIdentity(&HomMat2D);
                HomMat2dTranslate(HomMat2D, maxIntersectAreaRow, maxIntersectAreaCol + x, &HomMat2D);
                AffineTransRegion(RegionUnionRear, &RegionUnionRearTrans, HomMat2D, "constant");

                Intersection(RegionUnionPrev, RegionUnionRearTrans, &RegionIntersect);
                AreaCenter(RegionIntersect, &AreaRegionIntersect, &RowRegionIntersect, &ColumnRegionIntersect);
                if (AreaRegionIntersect > maxIntersectArea)
                {
                    maxIntersectArea = AreaRegionIntersect;
                    maxIntersectAreaCol = maxIntersectAreaCol + x;
                }
            }

            for (double y = -2.56; y <= 2.56; y += 0.16)
            {
                HomMat2dIdentity(&HomMat2D);
                HomMat2dTranslate(HomMat2D, maxIntersectAreaRow + y, maxIntersectAreaCol, &HomMat2D);
                AffineTransRegion(RegionUnionRear, &RegionUnionRearTrans, HomMat2D, "constant");

                Intersection(RegionUnionPrev, RegionUnionRearTrans, &RegionIntersect);
                AreaCenter(RegionIntersect, &AreaRegionIntersect, &RowRegionIntersect, &ColumnRegionIntersect);
                if (AreaRegionIntersect > maxIntersectArea)
                {
                    maxIntersectArea = AreaRegionIntersect;
                    maxIntersectAreaRow = maxIntersectAreaRow + y;
                }
            }
        }

        // 最终找到 后帧对前帧的最佳移动位置
        HomMat2dIdentity(&HomMat2D);
        HomMat2dTranslate(HomMat2D, maxIntersectAreaRow, maxIntersectAreaCol, &HomMat2D);
        AffineTransRegion(RegionUnionRear, &RegionUnionRearTrans, HomMat2D, "constant");
        AffineTransImage(ImageRearGauss, &ImageRearGaussTrans, HomMat2D, "constant", "false");

        // 合并前后帧血管区域 形成一个膨胀的、前后血管区域兼备的ROI
        // 并计算连通区域 得到最终的ROI区域
        Union2(RegionUnionPrev, RegionUnionRearTrans, &RegionUnionPrevRear);

        //计算前后两帧的区别
        ReduceDomain(ImagePrevGauss, RegionUnionPrevRear, &ImagePrevCalRegion);
        ReduceDomain(ImageRearGaussTrans, RegionUnionPrevRear, &ImageRearCalRegion);

        //由于后帧发生过移动 因此ROI截取到的位置不一定完全重叠
        Intersection(ImagePrevCalRegion, ImageRearCalRegion, &ROIintersection);

        //截取最终的血管区域
        ReduceDomain(ImagePrevGauss, ROIintersection, &ImagePrevCalRegion);
        ReduceDomain(ImageRearGaussTrans, ROIintersection, &ImageRearCalRegion);

        //用帧差法检测流动轨迹
        //先直方图均衡降低亮度变化带来的影响
        //红细胞是低灰度 背景是高灰度 要获取红细胞轨迹 应该先反转灰度再相减
        EquHistoImage(ImagePrevCalRegion, &ImagePrevCalRegionEquHisto);
        EquHistoImage(ImageRearCalRegion, &ImageRearCalRegionEquHisto);
        InvertImage(ImagePrevCalRegionEquHisto, &ImagePrevCalRegionInvert);
        InvertImage(ImageRearCalRegionEquHisto, &ImageRearCalRegionInvert);
        SubImage(ImageRearCalRegionInvert, ImagePrevCalRegionInvert, &ImageCellTrackRegion, 1, 0);

        MeanImage(ImageCellTrackRegion, &ImageMean, 43, 43);
        DynThreshold(ImageCellTrackRegion, ImageMean, &RegionCellTrack, 6, "light");

        //计算流动轨迹面积
        //令 红细胞流速(mm/ms) = 流动轨迹面积(px) * 像素尺寸(mm/px) / 放大倍率 / 前后帧时间差(ms)
        //以大恒为例 若算出AB帧之间轨迹面积为1000px 像素尺寸为常量5.6um/px 放大倍率为常量5 帧率为常量30fps
        //则AB帧之间的流速为 1000 * (5.6/1000) / 5 / (1000/30) = 5.6 / 5 / 1000 * 30 = 0.0336mm/ms
        AreaCenter(RegionCellTrack, &RegionCellTracksArea, &RegionCellTracksRow, &RegionCellTracksColumn);
        trackAreas[i - 1] = RegionCellTracksArea.D();

        Boundary(RegionCellTrack, &BorderRegionCellTrack, "inner");
        GetRegionPoints(BorderRegionCellTrack, &BorderRegionCellTrackRows, &BorderRegionCellTrackCols);
        int BorderRegionCellTrackPointCount = BorderRegionCellTrackRows.TupleLength();
        for(int j = 0; j < BorderRegionCellTrackPointCount; ++j)
        {
            regionPoints[i - 1].push_back(QPoint(BorderRegionCellTrackCols[j], BorderRegionCellTrackRows[j]));
        }

        //后帧变前帧
        ImagePrevGauss = ImageRearGauss;
        RegionUnionPrev = RegionUnionRear;
    }
}

double Flowrate::getImageSharpness(const QImage &Image)
{
    double sharpness = 0.0;
    HObject BorderRegionUnion, ImageGauss, RegionUnion;
    HObject ImageGaussEquHisto, ImaAmp, ImaDir, ImageMean, BorderEdges;

    HTuple AreaBorderRegionUnion, AreaBorderEdges, useless;

    // 预处理函数中使用了均值滤波+动态阈值的方法获取连续的低灰度区域 可认为这些区域都是血管区域
    preProcess(Image, &ImageGauss, &RegionUnion);

    // 血管区域的外轮廓
    Boundary(RegionUnion, &BorderRegionUnion, "inner");

    // 通用方法获取图像边缘区域
    EquHistoImage(ImageGauss, &ImageGaussEquHisto);
    EdgesImage(ImageGaussEquHisto, &ImaAmp, &ImaDir, "canny", 1, "nms", -1, -1);
    MeanImage(ImaAmp, &ImageMean, 23, 23);
    DynThreshold(ImaAmp, ImageMean, &BorderEdges, 5, "light");

    // 计算 血管区域外轮廓的周长 以及 边缘区域长度
    AreaCenter(BorderRegionUnion, &AreaBorderRegionUnion, &useless, &useless);
    AreaCenter(BorderEdges, &AreaBorderEdges, &useless, &useless);

    // 理论上来说 在理想情况下 当一幅图像足够清晰时 其轮廓周长(A)与边缘长度(B)应该相等 即A / B = 1 比如一个线宽为1的黑框白色正方形
    // 经过几个数据集实验 此处清晰度取值范围为[0.91, 1.195)
    if(AreaBorderRegionUnion.TupleLength() == 1 && AreaBorderEdges.TupleLength() == 1 && AreaBorderRegionUnion > 0)
    {
        sharpness = AreaBorderEdges.D() / AreaBorderRegionUnion.D();
    }
    else
    {
        sharpness = 0;
    }

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

void Flowrate::getSplitVesselRegion(const QImage &image, QVector<RegionPoints> &RegionBranchsPoints, QVector<RegionPoints> &RegionNodesPoints)
{
    HObject ImageGauss, RegionVesselUnion;
    HObject RegionBranchs, RegionNodes, Branch, Node;

    HTuple RegionBranchsCount, RegionNodesCount, Rows, Cols;

    preProcess(image, &ImageGauss, &RegionVesselUnion);
    getSplitVesselRegion(RegionVesselUnion, &RegionBranchs, &RegionNodes);

    CountObj(RegionBranchs, &RegionBranchsCount);
    RegionBranchsPoints = QVector<RegionPoints>(RegionBranchsCount.I());
    for(int i = 1; i <= RegionBranchsCount; ++i)
    {
        SelectObj(RegionBranchs, &Branch, i);
        GetRegionPoints(Branch, &Rows, &Cols);
        int pointCount = Rows.TupleLength();
        for(int j = 0; j < pointCount; ++j)
        {
            RegionBranchsPoints[i - 1].push_back(QPoint(Cols[j], Rows[j]));
        }
    }

    CountObj(RegionNodes, &RegionNodesCount);
    RegionNodesPoints = QVector<RegionPoints>(RegionNodesCount.I());
    for(int i = 1; i <= RegionNodesCount; ++i)
    {
        SelectObj(RegionNodes, &Node, i);
        GetRegionPoints(Node, &Rows, &Cols);
        int pointCount = Rows.TupleLength();
        for(int j = 0; j < pointCount; ++j)
        {
            RegionNodesPoints[i - 1].push_back(QPoint(Cols[j], Rows[j]));
        }
    }
}

void Flowrate::preProcess(const QImage& image, HObject *ImageGauss, HObject *RegionUnion)
{
    HObject  ImageOri, ImageMean, RegionDynThresh, RegionClosing, RawRegionConnected, RegionConnected;

    HTuple  Channels;

    // 使用GenImage1将QImage的图像数据复制到HObject
    // 这里确保了QImage是单通道灰度图 因此hdev中原程序的格式转换步骤都可以忽略
    GenImage1(&ImageOri, "byte", image.width(), image.height(), (Hlong)image.bits());

    GaussFilter(ImageOri, ImageGauss, 7);

    // 寻找血管线 均值滤波+动态阈值
    MeanImage(*ImageGauss, &ImageMean, 43, 43);
    DynThreshold(*ImageGauss, ImageMean, &RegionDynThresh, 5, "dark");

    // 闭运算补小洞
    ClosingCircle(RegionDynThresh, &RegionClosing, 3);

    // 分散出若干个连接起来的区域 大部分大概率是血管区域
    Connection(RegionClosing, &RawRegionConnected);

    // 根据面积筛选掉噪音
    SelectShape(RawRegionConnected, &RegionConnected, "area", "and", 50, image.width() * image.height());

    // 合并 检测到的分散血管区域 后面有用
    Union1(RegionConnected, RegionUnion);
}

void Flowrate::getSplitVesselRegion(const HObject &RegionVesselUnion, HObject *RegionBranchs, HObject *RegionNodes)
{
    HObject skeleton, EndPoints, JuncPoints;
    HObject BorderNeedSplitRegions, Circles, UnionCircles;
    HObject RawRegionBranchsWithLines, OpeningRawRegionBranchs, RawRegionBranchs;
    HObject UnionRegionBranchs, UnionRegionNodes;

    HTuple RowsJuncPoints, ColsJuncPoints, DistanceMin, DistanceMax;

    // 求血管区域骨骼(Skeleton)及其分叉点(JuncPoints) 端点(EndPoints)没用
    // 注意：骨骼，即中心线
    Skeleton(RegionVesselUnion, &skeleton);
    JunctionsSkeleton(skeleton, &EndPoints, &JuncPoints);

    // 需要分割的血管区域的外边界(BorderNeedSplitRegions)
    Boundary(RegionVesselUnion, &BorderNeedSplitRegions, "inner");

    // 以JuncPoints中的各个交叉点为圆点 以JuncPoints中的各个交叉点到BorderNeedSplitRegions的最小距离为半径 作若干个圆(Circles)
    GetRegionPoints(JuncPoints, &RowsJuncPoints, &ColsJuncPoints);
    DistancePr(BorderNeedSplitRegions, RowsJuncPoints, ColsJuncPoints, &DistanceMin, &DistanceMax);
    GenCircle(&Circles, RowsJuncPoints, ColsJuncPoints, DistanceMin);

    // 在完整的血管区域上去掉这些圆 再做一次开运算去掉细线状的连接 得到分支区域
    Union1(Circles, &UnionCircles);
    Difference (RegionVesselUnion, Circles, &RawRegionBranchsWithLines);
    OpeningCircle(RawRegionBranchsWithLines, &OpeningRawRegionBranchs, 3.5);
    Connection(OpeningRawRegionBranchs, &RawRegionBranchs);
    ClosingCircle(RawRegionBranchs, RegionBranchs, 3.5);
    Union1(*RegionBranchs, &UnionRegionBranchs);

    // 剩下的归为节点区域
    Difference (RegionVesselUnion, UnionRegionBranchs, &UnionRegionNodes);
    Connection(UnionRegionNodes, RegionNodes);
}
