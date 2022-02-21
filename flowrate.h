#pragma once

#include <QImage>
#include <QVector>

#include "HalconCpp.h"
#include "HDevThread.h"

using namespace HalconCpp;

class Flowrate
{
public:
    static double calFlowTrackAreas(QImage imagePrev, QImage imageRear);

    static QVector<double> calFlowTrackAreas(const QVector<QImage>& imagelist);

    static double getImageSharpness(const QImage& Image);

    static bool isSharp(double sharpness);

protected:
    static void splitVesselRegion(const QImage& image, const HObject& ImageGauss, const HObject& RegionConnected, HObject* RegionBranchs, HObject* RegionNodes);

    static void preProcess(const QImage& image, HObject *ImageGauss, HObject *RegionConnected, HObject *ho_RegionUnion);
};
