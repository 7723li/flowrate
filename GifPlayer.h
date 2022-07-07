#pragma once

#include <QOpenGLWidget>
#include <QTimer>
#include <QPaintEvent>
#include <QImage>
#include <QPushButton>
#include <QPainter>
#include <QDebug>
#include <QShowEvent>
#include <QCloseEvent>

class GifPlayer : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit GifPlayer(const QVector<QImage>& imagelist, QWidget* p = nullptr);
    virtual ~GifPlayer();

public slots:
    void setFps(double fps);

protected:
    virtual void showEvent(QShowEvent* e) override;

    virtual void paintEvent(QPaintEvent* e) override;

    virtual void closeEvent(QCloseEvent* e) override;

protected slots:
    void slotSwitchPlaying();

private:
    const QVector<QImage>& mImagelist;
    int mCurrentIndex;
    QImage mShowimage;

    QTimer mGifTimer;           // 动图播放定时器
    QPushButton mGifPlayButton; // 动图播放状态切换按钮
    bool mIsGifPlaying;         // 动图播放状态
    double mFps;                // 动图播放用的fps

    QRect mCurrentRect;
};
