#pragma once

#include <QImage>
#include <QVector>
#include <QPoint>
#include <QDebug>

#include "HalconCpp.h"
#include "HDevThread.h"

using RegionPoints = QVector<QPoint>;

using namespace HalconCpp;

class Flowrate
{
public:
    static void calFlowTrackAreas(QImage imagePrev, QImage imageRear, double& trackArea, RegionPoints& regionPoints);

    static void calFlowTrackAreas(const QVector<QImage>& imagelist, QVector<double>& trackAreas, QVector<RegionPoints>& regionPoints);

    static double getImageSharpness(const QImage& Image);

    static bool isSharp(double sharpness);

    static void getBoundaryVesselRegion(const QImage& image, RegionPoints& RegionBoundaryVesselPoints);

    static void getSplitVesselRegion(const QImage& image, QVector<RegionPoints>& RegionBranchsPoints, QVector<RegionPoints>& RegionNodesPoints);

protected:
    static void preProcess(const QImage& image, HObject *ImageGauss, HObject *ho_RegionUnion);

    static void getSplitVesselRegion(const HObject& RegionVesselUnion, HObject* RegionBranchs, HObject* RegionNodes);
};
