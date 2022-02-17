#include "flowrate.h"

double Flowrate::calFlowrate(QImage imagePrev, QImage imageRear)
{
    return calFlowrate(QVector<QImage>::fromStdVector(std::vector<QImage>({imagePrev, imageRear}))).front();
}

QVector<double> Flowrate::calFlowrate(const QVector<QImage>& imagelist)
{
    if(imagelist.size() < 2)
    {
        return QVector<double>();
    }

    HObject  ImagePrev, ImagePrevGauss, RegionUnionPrev;
    HObject  ImageRear, ImageRearGauss, RegionUnionRear;
    HObject  RegionDifference, Line, RegionIntersection;
    HObject  ConnectedRegions, RegionUnionRearTrans, RegionIntersect;
    HObject  ImageRearGaussTrans, RegionUnionPrevRear;
    HObject  ImagePrevCalRegion, ImageRearCalRegion, ROIintersection;
    HObject  ImagePrevCalRegionEquHisto, ImageRearCalRegionEquHisto;
    HObject  RegionCellTrack, ImagePrevCalRegionInvert;
    HObject  ImageRearCalRegionInvert, ImageCellTrackRegion, ImageMean;

    HTuple  hv_AreaRegionUnionPrev, hv_useless, hv_AreaRegionUnionRear;
    HTuple  hv_dis_Row, hv_AreaRow, hv_dis_Col, hv_AreaCol;
    HTuple  hv_maxRowDis, hv_deviationRowDis, hv_maxColDis;
    HTuple  hv_deviationColDis, hv_maxIntersectAreaRow, hv_maxIntersectAreaCol;
    HTuple  hv_maxIntersectArea, hv_HomMat2D;
    HTuple  hv_AreaRegionIntersect, hv_RowRegionIntersect, hv_ColumnRegionIntersect;
    HTuple  hv_RegionCellTracksArea, hv_RegionCellTracksRow;
    HTuple  hv_RegionCellTracksColumn;

    QVector<double> flowrate(imagelist.size(), 0);

    //首帧预处理 对应的变量用在后续的后帧缓存
    PreProcess(imagelist[0], &ImagePrev, &ImagePrevGauss, &RegionUnionPrev);

    //遍历图像序列
    int imageCount = imagelist.size();
    for (int i = 1; i < imageCount; ++i)
    {
        const QImage& imageRear = imagelist[i];

        int width = imageRear.width(), height = imageRear.height();

        //前帧血管区域判空 若为空 next one
        AreaCenter(RegionUnionPrev, &hv_AreaRegionUnionPrev, &hv_useless, &hv_useless);
        if (hv_AreaRegionUnionPrev.TupleLength() == 0 || -1 == hv_AreaRegionUnionPrev)
        {
            PreProcess(imageRear, &ImagePrev, &ImagePrevGauss, &RegionUnionPrev);
            continue;
        }

        //后一帧预处理
        PreProcess(imageRear, &ImageRear, &ImageRearGauss, &RegionUnionRear);

        //后帧血管区域判空 若为空 next one
        AreaCenter(RegionUnionRear, &hv_AreaRegionUnionRear, &hv_useless, &hv_useless);
        if (hv_AreaRegionUnionRear.TupleLength() ==0 || -1 == hv_AreaRegionUnionRear)
        {
            PreProcess(imagelist[++i], &ImagePrev, &ImagePrevGauss, &RegionUnionPrev);
            continue;
        }

        //前后帧差异区域
        Difference(RegionUnionPrev, RegionUnionRear, &RegionDifference);

        //计算差异区域在各行的宽度
        hv_dis_Row = HTuple();
        for (int j = 0; j < height - 1; ++j)
        {
            GenRegionLine(&Line, j, 0, j, width - 1);
            Intersection(Line, RegionDifference, &RegionIntersection);
            Connection(RegionIntersection, &ConnectedRegions);
            AreaCenter(ConnectedRegions, &hv_AreaRow, &hv_useless, &hv_useless);
            hv_dis_Row = hv_dis_Row.TupleConcat(hv_AreaRow);
        }

        //计算差异区域在各列的高度
        hv_dis_Col = HTuple();
        for (int j = 0; j < height - 1; ++j)
        {
            GenRegionLine(&Line, 0, j, height - 1, j);
            Intersection(Line, RegionDifference, &RegionIntersection);
            Connection(RegionIntersection, &ConnectedRegions);
            AreaCenter(ConnectedRegions, &hv_AreaCol, &hv_useless, &hv_useless);
            hv_dis_Col = hv_dis_Col.TupleConcat(hv_AreaCol);
        }

        //最大距离的正负作为粗瞄范围 方差作为步进粗瞄距离
        TupleMax(hv_dis_Row, &hv_maxRowDis);
        TupleDeviation(hv_dis_Row, &hv_deviationRowDis);
        TupleMax(hv_dis_Col, &hv_maxColDis);
        TupleDeviation(hv_dis_Col, &hv_deviationColDis);

        //粗瞄 找出一个横向移动后 能使前后帧血管区域重叠面积最大的差异区域高度 减少后面防抖移动的次数
        //插值方式采用比较粗糙的nearest_neighbor即可 后面浮点数处再采用比较精准的插值方法
        hv_maxIntersectAreaRow = 0;
        hv_maxIntersectAreaCol = 0;
        hv_maxIntersectArea = 0;

        double steprow = hv_deviationRowDis.D(), stepcol = hv_deviationColDis.D();
        for (double row = -hv_maxRowDis; row <= hv_maxRowDis; row += steprow)
        {
            for (double col = -hv_maxColDis; col <= hv_maxColDis; col += stepcol)
            {
                HomMat2dIdentity(&hv_HomMat2D);
                HomMat2dTranslate(hv_HomMat2D, row, col, &hv_HomMat2D);
                AffineTransRegion(RegionUnionRear, &RegionUnionRearTrans, hv_HomMat2D, "nearest_neighbor");

                Intersection(RegionUnionPrev, RegionUnionRearTrans, &RegionIntersect);
                AreaCenter(RegionIntersect, &hv_AreaRegionIntersect, &hv_RowRegionIntersect, &hv_ColumnRegionIntersect);
                if (hv_AreaRegionIntersect > hv_maxIntersectArea)
                {
                    hv_maxIntersectArea = hv_AreaRegionIntersect;
                    hv_maxIntersectAreaRow = row;
                    hv_maxIntersectAreaCol = col;
                }
            }
        }

        //细瞄 将前帧的血管区域移动至的重叠面积最大的地方 达到防抖的效果
        //J循环是为了多找一次 提高精度
        for (int j = 0; j <= 1; ++j)
        {
            for (double x = -2.56; x <= 2.56; x += 0.16)
            {
                HomMat2dIdentity(&hv_HomMat2D);
                HomMat2dTranslate(hv_HomMat2D, hv_maxIntersectAreaRow, hv_maxIntersectAreaCol + x, &hv_HomMat2D);
                AffineTransRegion(RegionUnionRear, &RegionUnionRearTrans, hv_HomMat2D, "constant");

                Intersection(RegionUnionPrev, RegionUnionRearTrans, &RegionIntersect);
                AreaCenter(RegionIntersect, &hv_AreaRegionIntersect, &hv_RowRegionIntersect, &hv_ColumnRegionIntersect);
                if (hv_AreaRegionIntersect > hv_maxIntersectArea)
                {
                    hv_maxIntersectArea = hv_AreaRegionIntersect;
                    hv_maxIntersectAreaCol = hv_maxIntersectAreaCol + x;
                }
            }

            for (double y = -2.56; y <= 2.56; y += 0.16)
            {
                HomMat2dIdentity(&hv_HomMat2D);
                HomMat2dTranslate(hv_HomMat2D, hv_maxIntersectAreaRow + y, hv_maxIntersectAreaCol, &hv_HomMat2D);
                AffineTransRegion(RegionUnionRear, &RegionUnionRearTrans, hv_HomMat2D, "constant");

                Intersection(RegionUnionPrev, RegionUnionRearTrans, &RegionIntersect);
                AreaCenter(RegionIntersect, &hv_AreaRegionIntersect, &hv_RowRegionIntersect, &hv_ColumnRegionIntersect);
                if (hv_AreaRegionIntersect > hv_maxIntersectArea)
                {
                    hv_maxIntersectArea = hv_AreaRegionIntersect;
                    hv_maxIntersectAreaRow = hv_maxIntersectAreaRow + y;
                }
            }
        }

        HomMat2dIdentity(&hv_HomMat2D);
        HomMat2dTranslate(hv_HomMat2D, hv_maxIntersectAreaRow, hv_maxIntersectAreaCol, &hv_HomMat2D);
        AffineTransRegion(RegionUnionRear, &RegionUnionRearTrans, hv_HomMat2D, "constant");
        AffineTransImage(ImageRearGauss, &ImageRearGaussTrans, hv_HomMat2D, "constant", "false");

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
        AreaCenter(RegionCellTrack, &hv_RegionCellTracksArea, &hv_RegionCellTracksRow, &hv_RegionCellTracksColumn);
        flowrate[i - 1] = hv_RegionCellTracksArea.D();

        //后帧变前帧
        ImagePrev = ImageRear;
        ImagePrevGauss = ImageRearGauss;
        RegionUnionPrev = RegionUnionRear;
    }

    return flowrate;
}

double Flowrate::GetImageSharpness(const QImage &Image)
{
    HTuple sharpness = 0.0;
    HObject ImageOri, ImageGuass, RegionUnion;
    HObject ImageReduced, EdgeAmplitude, Rect;
    HObject Edges;
    HTuple Min, Max, Range, Length, AreaRegionUnion, useless, Sum;

    PreProcess(Image, &ImageOri, &ImageGuass, &RegionUnion);

    ReduceDomain(ImageOri, RegionUnion, &ImageReduced);

    SobelAmp(ImageReduced, &EdgeAmplitude, "sum_abs", 7);

    GenRectangle1(&Rect, 0, 0, Image.height(), Image.width());
    MinMaxGray(Rect, EdgeAmplitude, 0, &Min, &Max, &Range);

    if(0 == Min)
    {
        Min = 1;
    }
    else if(Min > 5)
    {
        Min = 5;
    }

    EdgesSubPix(EdgeAmplitude, &Edges, "canny", 1, Min, 5);

    LengthXld(Edges, &Length);
    if(Length.TupleLength() > 0)
    {
        AreaCenter(RegionUnion, &AreaRegionUnion, &useless, &useless);
        if(AreaRegionUnion > 0)
        {
            TupleSum(Length, &Sum);
            sharpness = Sum / AreaRegionUnion;
        }
    }

    return sharpness;
}

void Flowrate::PreProcess(const QImage& image, HObject* Image, HObject *ImageGauss, HObject *RegionUnion)
{
    HObject  ImageMean, RegionDynThresh, ConnectedRegions;
    HObject  SelectedRegions;

    HTuple  hv_Channels;

    GenImage1(Image, "byte", image.width(), image.height(), (Hlong)image.bits());

    GaussFilter(*Image, ImageGauss, 7);

    //寻找血管线 均值滤波+动态阈值
    MeanImage(*ImageGauss, &ImageMean, 43, 43);
    DynThreshold(*ImageGauss, ImageMean, &RegionDynThresh, 5, "dark");

    //分散出若干个连接起来的区域 大部分大概率是血管区域
    Connection(RegionDynThresh, &ConnectedRegions);

    //根据面积筛选掉噪音
    SelectShape(ConnectedRegions, &SelectedRegions, "area", "and", 400, image.width() * image.height());

    //合并 检测到的分散血管区域 后面有用
    Union1(SelectedRegions, &(*RegionUnion));
}
