#ifndef WIDGET_H
#define WIDGET_H

#include <QGLFormat>
#include <QGLWidget>
#include <QGLShaderProgram>
#include <QOpenGLFunctions_2_0>
#include <QOpenGLDebugLogger>
#include <QOpenGLTexture>
#include <QtOpenGLExtensions/QOpenGLExtensions>
#include <QTimer>
#include <QVBoxLayout>

#include <boost/multi_array.hpp>

class widget : public QGLWidget, protected QOpenGLFunctions_2_0 {
    virtual void initializeGL() override final;
    virtual void paintGL() override final;
    virtual void resizeGL(int, int) override final;
    virtual void wheelEvent(QWheelEvent * const) override final;

    boost::multi_array<GLuint, 2> textures;
    std::vector<std::unique_ptr<QOpenGLTexture>> textures2;
    QTimer continuousRefresh;
    std::mt19937_64 twister;
    std::uniform_real_distribution<> dist;
    std::vector<char> data;
    int frame = 0;
    const int supercubeedge = 5;

    QOpenGLDebugLogger ogllogger;
    QGLShaderProgram program{context()->contextHandle()};
public:
    widget();
};

#endif//WIDGET_H
