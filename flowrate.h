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
    static void calFlowTrackDistances(QImage imagePrev, QImage imageRear, double& trackDistances, RegionPoints& regionPoints);

    static void calFlowTrackDistances(const QVector<QImage>& imagelist, QVector<double>& trackDistances, QVector<RegionPoints>& regionPoints);

    static double calcFlowrateFromFlowTrackDistances(const QVector<double>& trackDistances, double fps, double pixelSize, int magnification);

    static double getImageSharpness(const QImage& Image);

    static bool isSharp(double sharpness);

    static void getBoundaryVesselRegion(const QImage& image, RegionPoints& RegionBoundaryVesselPoints);

    static void splitVesselRegion(const QImage& image, QVector<RegionPoints>& RegionsSplitedPoints);

protected:
    static void preProcess(const QImage& image, HObject *ImageGauss, HObject *ho_RegionUnion);

    static double getImageSharpness(const HObject& ImageGauss, const HObject& RegionUnion);

    static void splitVesselRegion(const HObject& RegionVesselUnion, HObject* RegionsSplited);
};
