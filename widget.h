#ifndef WIDGET_H
#define WIDGET_H

#include <QGLFormat>
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions_2_0>
#include <QOpenGLDebugLogger>
#include <QOpenGLTexture>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <QTimer>
#include <QVBoxLayout>

#include <boost/multi_array.hpp>

class widget : public QOpenGLWidget, protected QOpenGLFunctions_2_0 {
    virtual void initializeGL() override final;
    virtual void mouseMoveEvent(QMouseEvent * event) override final;
    virtual void mousePressEvent(QMouseEvent * event) override final;
    virtual void paintGL() override final;
    virtual void resizeGL(int, int) override final;
    virtual void wheelEvent(QWheelEvent * const) override final;

    boost::multi_array<QOpenGLTexture*, 2> textures;
    QTimer continuousRefresh;
    std::mt19937_64 twister;
    std::uniform_real_distribution<> dist;
    std::vector<char> data;
    int frame = 0;
    const int supercubeedge = 5;
    const int cubeedge = 64;
    QPoint mouseDown;
    QVector3D deviation;

    QOpenGLDebugLogger ogllogger;
    QOpenGLShaderProgram program;
public:
    widget();
    ~widget();
};

#endif//WIDGET_H
