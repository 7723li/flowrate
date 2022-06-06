#include "VideoRecord.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    VideoRecord w;
    w.show();
    return a.exec();
}
