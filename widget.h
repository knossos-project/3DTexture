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
#include <cstdint>
#include <list>

struct TextureLayer {
    boost::multi_array<QOpenGLTexture*, 2> textures;
    float opacity = 1.0f;
    bool enabled = true;
    bool isOverlayData = false;
};

class widget : public QOpenGLWidget, protected QOpenGLFunctions_2_0 {
    virtual void initializeGL() override final;
    virtual void mouseMoveEvent(QMouseEvent * event) override final;
    virtual void mousePressEvent(QMouseEvent * event) override final;
    virtual void paintGL() override final;
    virtual void resizeGL(int, int) override final;
    virtual void wheelEvent(QWheelEvent * const) override final;

    std::unique_ptr<QOpenGLTexture> lut;
    std::list<TextureLayer> layers;

    QTimer continuousRefresh;
    float frame = 0;

    const int supercubeedge = 14;
    const int cpucubeedge = 128;
    const int gpucubeedge = 64;

    QPoint mouseDown;
    QVector3D deviation;

    QOpenGLDebugLogger ogllogger;
    QOpenGLShaderProgram raw_data_shader;
    QOpenGLShaderProgram overlay_data_shader;
public:
    widget();
    ~widget();
};

#endif//WIDGET_H
