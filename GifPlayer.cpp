#include "GifPlayer.h"

GifPlayer::GifPlayer(const QVector<QImage>& imagelist, double fps, QWidget *p) :
    QOpenGLWidget(p), mImagelist(imagelist), mCurrentIndex(0), mIsGifPlaying(true), mFps(fps)
{
    mGifPlayButton.setParent(this);
    mGifPlayButton.setStyleSheet("\
                                 QPushButton {border:0px;background-color:transparent;}\
                                 QPushButton::hover{border:0px black solid;background-color:rgb(255, 247, 237);}\
                                 QPushButton::click{border:1px black solid;background-color:rgb(255, 247, 237);}");
    mGifPlayButton.setText(QStringLiteral("停"));

    mGifTimer.setInterval(static_cast<int>(1000.0 / fps));

    connect(&mGifTimer, SIGNAL(timeout()), this, SLOT(update()));
    connect(&mGifPlayButton, &QPushButton::clicked, this, &GifPlayer::slotSwitchPlaying);
}

GifPlayer::~GifPlayer()
{

}

void GifPlayer::showEvent(QShowEvent *e)
{
    e->accept();
    if(mCurrentRect.size().width() > 0 && mCurrentRect.size().height() > 0)
    {
        this->setGeometry(mCurrentRect);
    }
    mGifTimer.start();
}

void GifPlayer::paintEvent(QPaintEvent *e)
{
    e->accept();

    QImage showimage;
    if(mIsGifPlaying)
    {
        showimage = mImagelist[mCurrentIndex++];
        if(mCurrentIndex >= mImagelist.size())
        {
            mCurrentIndex = 0;
        }
    }
    else
    {
        showimage = mImagelist[mCurrentIndex];
    }

    QPainter painter;
    painter.begin(this);
    painter.drawImage(QRect(0, 0, this->width(), this->height()), showimage, QRect(0, 0, showimage.width(), showimage.height()));
    painter.end();

    mGifPlayButton.move(20, this->height() - 20);
}

void GifPlayer::closeEvent(QCloseEvent *e)
{
    e->accept();
    mGifTimer.stop();
    mCurrentRect = this->geometry();
    this->hide();
}

void GifPlayer::slotSwitchPlaying()
{
    if(mGifTimer.isActive())
    {
        mGifPlayButton.setText(QStringLiteral("播"));
        mGifTimer.stop();
        mIsGifPlaying = false;
    }
    else
    {
        mGifPlayButton.setText(QStringLiteral("停"));
        mGifTimer.start();
        mIsGifPlaying = true;
    }
}
