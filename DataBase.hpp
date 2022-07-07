#pragma once

#include <QFileInfo>
#include <QDir>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QVariant>
#include <QDebug>
#include <QUuid>

#include "CommonDataStruct.h"

class DataBase
{
public:
    DataBase()
    {
        QDir currentDir(QDir::current());
        currentDir.cdUp();
        if(currentDir.exists("data"))
        {
            currentDir.cd("data");
        }
        else if(currentDir.mkdir("data"))
        {
            currentDir.cd("data");
        }

        if(QSqlDatabase::contains("qt_sql_default_connection"))
        {
            mDb = QSqlDatabase::database("qt_sql_default_connection");
        }
        else
        {
            mDb = QSqlDatabase::addDatabase("QSQLITE");
        }
        mDb.setDatabaseName(currentDir.absoluteFilePath("data.db"));
        mDb.open();
    }

    ~DataBase()
    {
        mDb.close();
    }

    QString generateGuid()
    {
        QUuid uid = QUuid::createUuid();
        return uid.toString();
    }

protected:
    QSqlDatabase mDb;
};

class TableVideoInfo : public DataBase
{
public:
    static TableVideoInfo& getTableVideoInfo()
    {
        static TableVideoInfo sTableVideoInfo;
        return sTableVideoInfo;
    }

    bool insertVideoInfo(VideoInfo& videoInfo)
    {
        QString sql = "insert into TableVideoInfo(pkVideoInfoID, fkVesselInfoID, collectTime, videoDuration, absVideoPath, pixelSize, magnification, fps, "
                      "firstSharpFrameIndex, analysisFinishTime) values('%1', '%2', %3, %4, '%5', %6, %7, %8, %9, %10)";
        QSqlQuery query(mDb);

        if(videoInfo.pkVideoInfoID.isEmpty())
        {
            videoInfo.pkVideoInfoID = generateGuid();
        }
        sql = sql.arg(videoInfo.pkVideoInfoID).arg(videoInfo.fkVesselInfoID).arg(videoInfo.collectTime).arg(videoInfo.videoDuration).arg(videoInfo.absVideoPath)
                .arg(videoInfo.pixelSize).arg(videoInfo.magnification).arg(videoInfo.fps).arg(videoInfo.firstSharpFrameIndex).arg(videoInfo.analysisFinishTime);

        return query.exec(sql);
    }

    bool insertVideoInfo(QList<VideoInfo>& videoInfoList)
    {
        QSqlQuery query(mDb);

        if(mDb.transaction())
        {
            for(VideoInfo& videoInfo : videoInfoList)
            {
                if(!insertVideoInfo(videoInfo))
                {
                    break;
                }
            }
        }

        return mDb.commit();
    }

    bool selectVideoInfo(QVector<VideoInfo>& videoInfoList)
    {
        QString sql = "select * from TableVideoInfo;";
        QSqlQuery query(mDb);

        if(!query.exec(sql))
        {
            return false;
        }

        VideoInfo tVideoInfo;
        while(query.next())
        {
            tVideoInfo.pkVideoInfoID = query.value(0).toString();
            tVideoInfo.fkVesselInfoID = query.value(1).toString();
            tVideoInfo.collectTime = query.value(2).toUInt();
            tVideoInfo.videoDuration = query.value(3).toInt();
            tVideoInfo.absVideoPath = query.value(4).toString();
            tVideoInfo.pixelSize = query.value(5).toDouble();
            tVideoInfo.magnification = query.value(6).toInt();
            tVideoInfo.fps = query.value(7).toDouble();
            tVideoInfo.firstSharpFrameIndex = query.value(8).toInt();
            tVideoInfo.analysisFinishTime = query.value(9).toUInt();

            videoInfoList.push_back(tVideoInfo);
        }

        return true;
    }

    bool selectVideoInfo(VideoInfo& videoInfo, const QString& pkVideoInfoID)
    {
        QString sql = "select * from TableVideoInfo where pkVideoInfoID = '%1';";
        QSqlQuery query(mDb);

        sql = sql.arg(pkVideoInfoID);
        if(query.exec(sql) && query.next())
        {
            videoInfo.pkVideoInfoID = query.value(0).toString();
            videoInfo.fkVesselInfoID = query.value(1).toString();
            videoInfo.collectTime = query.value(2).toUInt();
            videoInfo.videoDuration = query.value(3).toInt();
            videoInfo.absVideoPath = query.value(4).toString();
            videoInfo.pixelSize = query.value(5).toDouble();
            videoInfo.magnification = query.value(6).toInt();
            videoInfo.fps = query.value(7).toDouble();
            videoInfo.firstSharpFrameIndex = query.value(8).toInt();
            videoInfo.analysisFinishTime = query.value(9).toUInt();

            return true;
        }

        return false;
    }

    bool updateVideoInfo(const VideoInfo& videoInfo)
    {
        if(videoInfo.pkVideoInfoID.isEmpty())
        {
            return false;
        }

        QString sql = "update TableVideoInfo set fkVesselInfoID='%1', collectTime=%2, videoDuration=%3, absVideoPath='%4', pixelSize=%5, magnification=%6, fps=%7, "
                      "firstSharpFrameIndex=%8, analysisFinishTime=%9 where pkVideoInfoID='%10'";
        QSqlQuery query(mDb);

        sql = sql.arg(videoInfo.fkVesselInfoID).arg(videoInfo.collectTime).arg(videoInfo.videoDuration).arg(videoInfo.absVideoPath).arg(videoInfo.pixelSize).
                arg(videoInfo.magnification).arg(videoInfo.fps).arg(videoInfo.firstSharpFrameIndex).arg(videoInfo.analysisFinishTime).arg(videoInfo.pkVideoInfoID);

        return query.exec(sql);
    }

private:
    TableVideoInfo() :
        DataBase()
    {
        if(mDb.isOpen())
        {
            QSqlQuery query(mDb);
            query.exec("CREATE TABLE IF NOT EXISTS TableVideoInfo ("
                        "pkVideoInfoID TEXT(128),"
                        "fkVesselInfoID TEXT(128),"
                        "collectTime INTEGER,"
                        "videoDuration INTEGER,"
                        "absVideoPath TEXT(128),"
                        "pixelSize REAL,"
                        "magnification INTEGER,"
                        "fps REAL,"
                        "firstSharpFrameIndex INTEGER,"
                        "analysisFinishTime INTEGER,"
                        "PRIMARY KEY(pkVideoInfoID));");
        }
    }
};

class TableVesselInfo : public DataBase
{
public:
    static TableVesselInfo& getTableVesselInfo()
    {
        static TableVesselInfo sTableVesselInfo;
        return sTableVesselInfo;
    }

    bool insertVesselInfo(VesselInfo& vesselInfo)
    {
        QString sql = "insert into TableVesselInfo values (:1,:2,:3,:4,:5,:6,:7,:8,:9,:10,:11,:12);";
        QSqlQuery query(mDb);

        if(vesselInfo.pkVesselInfoID.isEmpty())
        {
            vesselInfo.pkVesselInfoID = generateGuid();
        }

        query.prepare(sql);
        query.bindValue(":1", vesselInfo.pkVesselInfoID);
        query.bindValue(":2", vesselInfo.vesselNumber);

        RegionPoints dbSkeletonPoints, dbBorderPoints, dbUnionPoints;
        QVector<int> skelegonPointsCount, borderPointsCount, unionPointsCount;
        for(int i = 0 ;i < vesselInfo.vesselNumber; ++i)
        {
            dbSkeletonPoints.append(vesselInfo.regionsSkeletonPoints[i]);
            dbBorderPoints.append(vesselInfo.regionsBorderPoints[i]);
            dbUnionPoints.append(vesselInfo.regionsUnionPoints[i]);

            skelegonPointsCount.push_back(vesselInfo.regionsSkeletonPoints[i].size());
            borderPointsCount.push_back(vesselInfo.regionsBorderPoints[i].size());
            unionPointsCount.push_back(vesselInfo.regionsUnionPoints[i].size());
        }

        query.bindValue(":3", QByteArray((const char*)dbSkeletonPoints.data(), dbSkeletonPoints.size() * sizeof (QPoint)));
        query.bindValue(":4", QByteArray((const char*)dbBorderPoints.data(), dbBorderPoints.size() * sizeof (QPoint)));
        query.bindValue(":5", QByteArray((const char*)dbUnionPoints.data(), dbUnionPoints.size() * sizeof (QPoint)));
        query.bindValue(":6", QByteArray((const char*)skelegonPointsCount.data(), skelegonPointsCount.size() * sizeof (int)));
        query.bindValue(":7", QByteArray((const char*)borderPointsCount.data(), borderPointsCount.size() * sizeof (int)));
        query.bindValue(":8", QByteArray((const char*)unionPointsCount.data(), unionPointsCount.size() * sizeof (int)));

        query.bindValue(":9", QByteArray((const char*)vesselInfo.diameters.data(), vesselInfo.diameters.size() *sizeof (double)));
        query.bindValue(":10", QByteArray((const char*)vesselInfo.lengths.data(), vesselInfo.lengths.size() *sizeof (double)));
        query.bindValue(":11", QByteArray((const char*)vesselInfo.glycocalyx.data(), vesselInfo.glycocalyx.size() *sizeof (double)));
        query.bindValue(":12", QByteArray((const char*)vesselInfo.flowrates.data(), vesselInfo.flowrates.size() *sizeof (double)));

        return query.exec();
    }

    bool selectVesselInfo(QVector<VesselInfo>& vesselInfoList)
    {
        QString sql = "select * from TableVesselInfo;";
        QSqlQuery query(mDb);

        if(!query.exec(sql))
        {
            return false;
        }

        while(query.next())
        {
            VesselInfo vesselInfo;
            convertSqlQueryToVesselInfo(vesselInfo, query);
            vesselInfoList.push_back(vesselInfo);
        }

        return true;
    }

    bool selectVesselInfo(VesselInfo& vesselInfo, const QString& pkVesselInfoID)
    {
        QString sql = "select * from TableVesselInfo where pkVesselInfoID='%1';";
        QSqlQuery query(mDb);
        sql = sql.arg(pkVesselInfoID);

        if(query.exec(sql) && query.next())
        {
            convertSqlQueryToVesselInfo(vesselInfo, query);

            return true;
        }

        return false;
    }

    bool updateVesselInfo(const VesselInfo& vesselInfo)
    {
        if(vesselInfo.pkVesselInfoID.isEmpty())
        {
            return false;
        }

        if(!deleteVesselInfo(vesselInfo))
        {
            return false;
        }
        VesselInfo tVesselInfo(vesselInfo);
        if(!insertVesselInfo(tVesselInfo))
        {
            return false;
        }

        return true;
    }

    bool deleteVesselInfo(const VesselInfo& vesselInfo)
    {
        if(vesselInfo.pkVesselInfoID.isEmpty())
        {
            return false;
        }

        QString sql = "delete from TableVesselInfo where pkVesselInfoID='%1';";
        QSqlQuery query(mDb);
        sql = sql.arg(vesselInfo.pkVesselInfoID);
        return query.exec(sql);
    }

private:
    TableVesselInfo():
        DataBase()
    {
        if(mDb.isOpen())
        {
            QSqlQuery query(mDb);
            query.exec("CREATE TABLE IF NOT EXISTS TableVesselInfo ("
                        "pkVesselInfoID TEXT(128),"
                        "vesselNumber INTEGER,"
                        "regionsSkeletonPoints BLOB,"
                        "regionsBorderPoints BLOB,"
                        "regionsUnionPoints BLOB,"
                        "skelegonPointsCount BLOB,"
                        "borderPointsCount BLOB,"
                        "unionPointsCount BLOB,"
                        "diameters BLOB,"
                        "lengths BLOB,"
                        "glycocalyx BLOB,"
                        "flowrates BLOB,"
                        "PRIMARY KEY(pkVesselInfoID));");
        }
    }

    virtual ~TableVesselInfo()
    {

    }

    void convertSqlQueryToVesselInfo(VesselInfo& vesselInfo, const QSqlQuery query)
    {
        QByteArray tByteArray;
        int posSkeleton = 0, posBorder = 0, posUnion = 0;

        vesselInfo.pkVesselInfoID = query.value(0).toString();
        vesselInfo.vesselNumber = query.value(1).toInt();

        RegionPoints dbSkeletonPoints, dbBorderPoints, dbUnionPoints;
        QVector<int> skelegonPointsCount, borderPointsCount, unionPointsCount;

        tByteArray = query.value(2).toByteArray();
        dbSkeletonPoints.resize(tByteArray.length() / sizeof (QPoint));
        memcpy(dbSkeletonPoints.data(), tByteArray.data(), tByteArray.length());

        tByteArray = query.value(3).toByteArray();
        dbBorderPoints.resize(tByteArray.length() / sizeof (QPoint));
        memcpy(dbBorderPoints.data(), tByteArray.data(), tByteArray.length());

        tByteArray = query.value(4).toByteArray();
        dbUnionPoints.resize(tByteArray.length() / sizeof (QPoint));
        memcpy(dbUnionPoints.data(), tByteArray.data(), tByteArray.length());

        tByteArray = query.value(5).toByteArray();
        skelegonPointsCount.resize(tByteArray.length() / sizeof (int));
        memcpy(skelegonPointsCount.data(), tByteArray.data(), tByteArray.length());

        tByteArray = query.value(6).toByteArray();
        borderPointsCount.resize(tByteArray.length() / sizeof (int));
        memcpy(borderPointsCount.data(), tByteArray.data(), tByteArray.length());

        tByteArray = query.value(7).toByteArray();
        unionPointsCount.resize(tByteArray.length() / sizeof (int));
        memcpy(unionPointsCount.data(), tByteArray.data(), tByteArray.length());

        for(int i = 0; i < vesselInfo.vesselNumber; ++i)
        {
            vesselInfo.regionsSkeletonPoints.push_back(dbSkeletonPoints.mid(posSkeleton, skelegonPointsCount[i]));
            posSkeleton += skelegonPointsCount[i];
        }
        for(const int& pointsCount : borderPointsCount)
        {
            vesselInfo.regionsBorderPoints.push_back(dbBorderPoints.mid(posBorder, pointsCount));
            posBorder += pointsCount;
        }
        for(const int& pointsCount : unionPointsCount)
        {
            vesselInfo.regionsUnionPoints.push_back(dbUnionPoints.mid(posUnion, pointsCount));
            posUnion += pointsCount;
        }

        tByteArray = query.value(8).toByteArray();
        vesselInfo.diameters.resize(tByteArray.length() / sizeof (double));
        memcpy(vesselInfo.diameters.data(), tByteArray.data(), tByteArray.length());

        tByteArray = query.value(9).toByteArray();
        vesselInfo.lengths.resize(tByteArray.length() / sizeof (double));
        memcpy(vesselInfo.lengths.data(), tByteArray.data(), tByteArray.length());

        tByteArray = query.value(10).toByteArray();
        vesselInfo.glycocalyx.resize(tByteArray.length() / sizeof (double));
        memcpy(vesselInfo.glycocalyx.data(), tByteArray.data(), tByteArray.length());

        tByteArray = query.value(11).toByteArray();
        vesselInfo.flowrates.resize(tByteArray.length() / sizeof (double));
        memcpy(vesselInfo.flowrates.data(), tByteArray.data(), tByteArray.length());
    }
};
