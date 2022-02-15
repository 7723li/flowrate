#pragma once

#include <QImage>
#include <QVector>

#include "HalconCpp.h"
#include "HDevThread.h"

using namespace HalconCpp;

class Flowrate
{
public:
    static double calFlowrate(QImage imagePrev, QImage imageRear);

    static QVector<double> calFlowrate(const QVector<QImage>& imagelist);

private:
    static void pre_process (const QImage& image, HObject* ho_Image, HObject *ho_ImageGauss, HObject *ho_RegionUnion);
};
