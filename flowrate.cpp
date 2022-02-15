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

    HObject  ho_ImagePrev, ho_ImagePrevGauss, ho_RegionUnionPrev;
    HObject  ho_ImageRear, ho_ImageRearGauss, ho_RegionUnionRear;
    HObject  ho_RegionDifference, ho_Line, ho_RegionIntersection;
    HObject  ho_ConnectedRegions, ho_RegionUnionRearTrans, ho_RegionIntersect;
    HObject  ho_ImageRearGaussTrans, ho_RegionUnionPrevRear;
    HObject  ho_ImagePrevCalRegion, ho_ImageRearCalRegion, ho_ROIintersection;
    HObject  ho_ImagePrevCalRegionILL, ho_ImageRearCalRegionILL;
    HObject  ho_RegionCellTrack, ho_ImagePrevCalRegionInvert;
    HObject  ho_ImageRearCalRegionInvert, ho_ImageCellTrackRegion;

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
    pre_process(imagelist[0], &ho_ImagePrev, &ho_ImagePrevGauss, &ho_RegionUnionPrev);

    //遍历图像序列
    int imageCount = imagelist.size();
    for (int i = 1; i < imageCount; ++i)
    {
        const QImage& imageRear = imagelist[i];

        int width = imageRear.width(), height = imageRear.height();

        //前帧血管区域判空 若为空 next one
        AreaCenter(ho_RegionUnionPrev, &hv_AreaRegionUnionPrev, &hv_useless, &hv_useless);
        if (hv_AreaRegionUnionPrev.TupleLength() == 0 || -1 == hv_AreaRegionUnionPrev)
        {
            pre_process(imageRear, &ho_ImagePrev, &ho_ImagePrevGauss, &ho_RegionUnionPrev);
            continue;
        }

        //后一帧预处理
        pre_process(imageRear, &ho_ImageRear, &ho_ImageRearGauss, &ho_RegionUnionRear);

        //后帧血管区域判空 若为空 next one
        AreaCenter(ho_RegionUnionRear, &hv_AreaRegionUnionRear, &hv_useless, &hv_useless);
        if (hv_AreaRegionUnionRear.TupleLength() ==0 || -1 == hv_AreaRegionUnionRear)
        {
            pre_process(imagelist[++i], &ho_ImagePrev, &ho_ImagePrevGauss, &ho_RegionUnionPrev);
            continue;
        }

        //前后帧差异区域
        Difference(ho_RegionUnionPrev, ho_RegionUnionRear, &ho_RegionDifference);

        //计算差异区域在各行的宽度
        hv_dis_Row = HTuple();
        for (int j = 0; j < height - 1; ++j)
        {
            GenRegionLine(&ho_Line, j, 0, j, width - 1);
            Intersection(ho_Line, ho_RegionDifference, &ho_RegionIntersection);
            Connection(ho_RegionIntersection, &ho_ConnectedRegions);
            AreaCenter(ho_ConnectedRegions, &hv_AreaRow, &hv_useless, &hv_useless);
            hv_dis_Row = hv_dis_Row.TupleConcat(hv_AreaRow);
        }

        //计算差异区域在各列的高度
        hv_dis_Col = HTuple();
        for (int j = 0; j < height - 1; ++j)
        {
            GenRegionLine(&ho_Line, 0, j, height - 1, j);
            Intersection(ho_Line, ho_RegionDifference, &ho_RegionIntersection);
            Connection(ho_RegionIntersection, &ho_ConnectedRegions);
            AreaCenter(ho_ConnectedRegions, &hv_AreaCol, &hv_useless, &hv_useless);
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
                AffineTransRegion(ho_RegionUnionRear, &ho_RegionUnionRearTrans, hv_HomMat2D, "nearest_neighbor");

                Intersection(ho_RegionUnionPrev, ho_RegionUnionRearTrans, &ho_RegionIntersect);
                AreaCenter(ho_RegionIntersect, &hv_AreaRegionIntersect, &hv_RowRegionIntersect, &hv_ColumnRegionIntersect);
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
                AffineTransRegion(ho_RegionUnionRear, &ho_RegionUnionRearTrans, hv_HomMat2D, "constant");

                Intersection(ho_RegionUnionPrev, ho_RegionUnionRearTrans, &ho_RegionIntersect);
                AreaCenter(ho_RegionIntersect, &hv_AreaRegionIntersect, &hv_RowRegionIntersect, &hv_ColumnRegionIntersect);
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
                AffineTransRegion(ho_RegionUnionRear, &ho_RegionUnionRearTrans, hv_HomMat2D, "constant");

                Intersection(ho_RegionUnionPrev, ho_RegionUnionRearTrans, &ho_RegionIntersect);
                AreaCenter(ho_RegionIntersect, &hv_AreaRegionIntersect, &hv_RowRegionIntersect, &hv_ColumnRegionIntersect);
                if (hv_AreaRegionIntersect > hv_maxIntersectArea)
                {
                    hv_maxIntersectArea = hv_AreaRegionIntersect;
                    hv_maxIntersectAreaRow = hv_maxIntersectAreaRow + y;
                }
            }
        }

        HomMat2dIdentity(&hv_HomMat2D);
        HomMat2dTranslate(hv_HomMat2D, hv_maxIntersectAreaRow, hv_maxIntersectAreaCol, &hv_HomMat2D);
        AffineTransRegion(ho_RegionUnionRear, &ho_RegionUnionRearTrans, hv_HomMat2D, "constant");
        AffineTransImage(ho_ImageRearGauss, &ho_ImageRearGaussTrans, hv_HomMat2D, "constant", "false");

        Union2(ho_RegionUnionPrev, ho_RegionUnionRearTrans, &ho_RegionUnionPrevRear);

        //计算前后两帧的区别
        ReduceDomain(ho_ImagePrevGauss, ho_RegionUnionPrevRear, &ho_ImagePrevCalRegion);
        ReduceDomain(ho_ImageRearGaussTrans, ho_RegionUnionPrevRear, &ho_ImageRearCalRegion);

        //由于后帧发生过移动 因此ROI截取到的位置不一定完全重叠
        Intersection(ho_ImagePrevCalRegion, ho_ImageRearCalRegion, &ho_ROIintersection);

        ReduceDomain(ho_ImagePrevGauss, ho_ROIintersection, &ho_ImagePrevCalRegion);
        ReduceDomain(ho_ImageRearGaussTrans, ho_ROIintersection, &ho_ImageRearCalRegion);

        //平衡亮度
        Illuminate(ho_ImagePrevCalRegion, &ho_ImagePrevCalRegionILL, 40, 40, 0.55);
        Illuminate(ho_ImageRearCalRegion, &ho_ImageRearCalRegionILL, 40, 40, 0.55);

        //轨迹检测法1 卡尔曼滤波
        //create_bg_esti (ImagePrevCalRegion, 0.7, 0.7, 'fixed', 0.002, 0.02, 'on', 7, 10, 3.25, 15, BgEstiHandle)
        //run_bg_esti (ImageRearCalRegion, RegionCellTrack, BgEstiHandle)

        //轨迹检测法2 帧差法
        //注意：红细胞是低灰度 背景是高灰度 要获取红细胞轨迹 应该先反转灰度再相减
        InvertImage(ho_ImagePrevCalRegionILL, &ho_ImagePrevCalRegionInvert);
        InvertImage(ho_ImageRearCalRegionILL, &ho_ImageRearCalRegionInvert);
        SubImage(ho_ImageRearCalRegionInvert, ho_ImagePrevCalRegionInvert, &ho_ImageCellTrackRegion, 1, 0);
        Threshold(ho_ImageCellTrackRegion, &ho_RegionCellTrack, 6, 255);

        //计算流动轨迹面积
        //令 红细胞流速(mm/ms) = 流动轨迹面积(px) * 像素尺寸(mm/px) / 放大倍率 / 前后帧时间差(ms)
        //以大恒为例 若算出AB帧之间轨迹面积为1000px 像素尺寸为常量5.6um/px 放大倍率为常量5 帧率为常量30fps
        //则AB帧之间的流速为 1000 * (5.6/1000) / 5 / (1000/30) = 5.6 / 5 / 1000 * 30 = 0.0336mm/ms
        AreaCenter(ho_RegionCellTrack, &hv_RegionCellTracksArea, &hv_RegionCellTracksRow, &hv_RegionCellTracksColumn);
        flowrate[i - 1] = hv_RegionCellTracksArea.D();

        //后帧变前帧
        ho_ImagePrev = ho_ImageRear;
        ho_ImagePrevGauss = ho_ImageRearGauss;
        ho_RegionUnionPrev = ho_RegionUnionRear;
    }

    return flowrate;
}

void Flowrate::pre_process(const QImage& image, HObject* ho_Image, HObject *ho_ImageGauss, HObject *ho_RegionUnion)
{
    HObject  ho_ImageMean, ho_RegionDynThresh, ho_ConnectedRegions;
    HObject  ho_SelectedRegions;

    HTuple  hv_Channels;

    GenImage1(ho_Image, "byte", image.width(), image.height(), (Hlong)image.bits());

    GaussFilter(*ho_Image, ho_ImageGauss, 7);

    //寻找血管线 均值滤波+动态阈值
    MeanImage(*ho_ImageGauss, &ho_ImageMean, 43, 43);
    DynThreshold(*ho_ImageGauss, ho_ImageMean, &ho_RegionDynThresh, 5, "dark");

    //分散出若干个连接起来的区域 大部分大概率是血管区域
    Connection(ho_RegionDynThresh, &ho_ConnectedRegions);

    //根据面积筛选掉噪音
    SelectShape(ho_ConnectedRegions, &ho_SelectedRegions, "area", "and", 400, image.width() * image.height());

    //合并 检测到的分散血管区域 后面有用
    Union1(ho_SelectedRegions, &(*ho_RegionUnion));
}
