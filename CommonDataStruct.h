#pragma once

#include <QVector>
#include <QPoint>
#include <QString>

using RegionPoints = QVector<QPoint>;

struct VideoInfo
{
    VideoInfo():
        collectTime(0), videoDuration(0), pixelSize(0.0), magnification(1), fps(0.0), firstSharpFrameIndex(0), analysisFinishTime(0)
    {

    }

    QString pkVideoInfoID;
    QString fkVesselInfoID;     // 映射到对应血管信息的外键 若为空 说明仍为分析中的状态

    unsigned int collectTime;
    int videoDuration;

    QString absVideoPath;
    double pixelSize;
    int magnification;
    double fps;

    int firstSharpFrameIndex;   // 清晰的首帧位置
    unsigned int analysisFinishTime;
};

struct VesselInfo
{
    VesselInfo() :
        vesselNumber(0)
    {

    }

    QString pkVesselInfoID;

    int vesselNumber;
    QVector<RegionPoints> regionsSkeletonPoints;
    QVector<RegionPoints> regionsBorderPoints;
    QVector<RegionPoints> regionsUnionPoints;
    QVector<double> diameters;
    QVector<double> lengths;
    QVector<double> glycocalyx;
    QVector<double> flowrates;
};
