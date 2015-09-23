#include "widget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QOpenGLTexture>
#include <QOpenGLTimerQuery>
#include <QWheelEvent>

#include <boost/range/iterator_range_core.hpp>

#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

QGLContext * newFavoriteQGLContext() {
    auto * context = new QOpenGLContext;

    QSurfaceFormat format;
    format.setMajorVersion(2);
    format.setMinorVersion(0);
    //format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
    format.setProfile(QSurfaceFormat::NoProfile);
    format.setOption(QSurfaceFormat::DebugContext);
    context->setFormat(format);
    context->create();
    return QGLContext::fromOpenGLContext(context);
}

widget::widget() : //QGLWidget(newFavoriteQGLContext()),
        twister(std::random_device{}()), dist(0.0, 1.0) {
    //qDebug() << context()->format().swapBehavior() << ' ' << context()->format().swapInterval();

    QObject::connect(&continuousRefresh, &QTimer::timeout, this, static_cast<void (QOpenGLWidget::*)()>(&QOpenGLWidget::update));
    continuousRefresh.start(0);
    resize(128*supercubeedge, 128*supercubeedge);
}

widget::~widget() {
    makeCurrent();
    glDeleteTextures(textures.num_elements(), textures.data());
    for (auto & texture : boost::make_iterator_range(textures2.data(), textures2.data() + textures2.num_elements())) {
        delete texture;
    }
}

void widget::initializeGL() {
    initializeOpenGLFunctions();
    ogllogger.initialize();
    QObject::connect(&ogllogger, &QOpenGLDebugLogger::messageLogged, [](const QOpenGLDebugMessage & msg){
        qDebug() << msg;
    });
    ogllogger.startLogging();

    GLint iUnits, texture_units, max_tu;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &iUnits);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &max_tu);
    std::cout << "MultiTexture: " << iUnits << ' ' << texture_units << ' ' << max_tu << std::endl;

    const auto dims = boost::extents[supercubeedge][supercubeedge];
    textures.resize(dims);
    for (auto & texture : boost::make_iterator_range(textures2.data(), textures2.data() + textures2.num_elements())) {
        delete texture;
    }
    textures2.resize(dims);
    for (auto & texture : boost::make_iterator_range(textures2.data(), textures2.data() + textures2.num_elements())) {
        texture = nullptr;
    }
    glGenTextures(textures.num_elements(), textures.data());

    glEnable(GL_TEXTURE_3D);
    glEnable(GL_DEPTH_TEST);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);// our texture colors will replace the untextured colors
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glColor4d(0.0, 0.0, 0.0, 1.0);
    glPointSize(3.0);

    std::ios_base::sync_with_stdio(false);

    for (int y = 0; y < supercubeedge; ++y)
    for (int x = 0; x < supercubeedge; ++x) {
        QElapsedTimer time;
        time.start();
        glBindTexture(GL_TEXTURE_3D, textures[y][x]);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);//GL_LINEAR_MIPMAP_LINEAR
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        std::string path = "cubes/2012-03-07_AreaX14_mag1_x00" + std::to_string(29+x) + "_y00" + std::to_string(52-y) + "_z0023.raw";
//        std::string path = "C:/New folder/cubes/2012-03-07_AreaX14_mag1_x00" + std::to_string(29+x) + "_y00" + std::to_string(52-y) + "_z0023.raw";
//        std::string path = "\\\\mobile/New folder/cubes/2012-03-07_AreaX14_mag1_x00" + std::to_string(29+x) + "_y00" + std::to_string(52-y) + "_z0023.raw";
        std::ifstream file(path, std::ios_base::binary);
        data.resize(128*128*128);
        if (file) {
            //data = std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>{});
            file.read(data.data(), data.size());
            //std::cout << path << " loaded" << std::endl;
        } else {
            std::fill(std::begin(data), std::end(data), 127);
            std::cout << path << " failed" << std::endl;
        }
        // glTexImage3D(GL_TEXTURE_3D, 0, GL_LUMINANCE8, 128, 128, 128, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data.data());//8 bit per pixel
//        data[0] = 0;
//        data[127] = -1;e
        //std::cout << time.restart() << ' ';
        textures2[y][x] = new QOpenGLTexture(QOpenGLTexture::Target3D);
        QOpenGLTexture & texture = *textures2[y][x];
        texture.setAutoMipMapGenerationEnabled(false);
        //texture.setFormat(QOpenGLTexture::LuminanceFormat);
        texture.setSize(128, 128, 128);
        texture.setMipLevels(1);
        texture.setMinificationFilter(QOpenGLTexture::Linear);
        texture.setMagnificationFilter(QOpenGLTexture::Linear);

        texture.setFormat(QOpenGLTexture::R8_UNorm);
        texture.allocateStorage();
        texture.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data.data());
    }

//    glMatrixMode(GL_PROJECTION);
//    //gluPerspective(90, 4.0/3.0, 0.1, 10.0);
//    glLoadIdentity();
//    glOrtho(-1, 1, -1, 1, 0.1, 10);
//    glMatrixMode(GL_MODELVIEW);

    program.addShaderFromSourceCode(QOpenGLShader::Vertex,R"shaderSource(
    //#version 110
    uniform mat4 model_matrix;
    uniform mat4 view_matrix;
    uniform mat4 projection_matrix;
    attribute vec3 vertex;
    attribute vec3 texCoordVertex;
    varying vec3 texCoordFrag;
    void main() {
        vec4 vWorld = model_matrix * vec4(vertex, 1);
        vec4 vEye = view_matrix * vWorld;
        vec4 vClip = projection_matrix * vEye;
        gl_Position = vClip;
        texCoordFrag = texCoordVertex;
    })shaderSource");

    program.addShaderFromSourceCode(QOpenGLShader::Fragment,R"shaderSource(
    //#version 110
    uniform sampler3D textureCentral;
    uniform sampler3D textureLeft;
    uniform sampler3D textureRight;
    uniform sampler3D textureTop;
    uniform sampler3D textureBottom;
    uniform float cubeedgelength;
    varying vec3 texCoordFrag;//in
    //varying vec4 gl_FragColor;//out
    void main() {
//        gl_FragColor = vec4(texCoordFrag, 1);
        gl_FragColor = vec4(vec3(texture3D(textureCentral, texCoordFrag).r), 1.0f);
//        vec4 left = texCoordFrag.x == 0.0f ? texture3D(textureLeft, vec3(cubeedgelength, texCoordFrag.yz)) : texture3D(textureCentral, texCoordFrag);
//        vec4 right = texCoordFrag.x == cubeedgelength ? texture3D(textureRight, vec3(0, texCoordFrag.yz)) : texture3D(textureCentral, texCoordFrag);
//        vec4 top = texCoordFrag.y == 0.0f ? texture3D(textureTop, vec3(texCoordFrag.x, cubeedgelength, texCoordFrag.z)) : texture3D(textureCentral, texCoordFrag);
//        vec4 bottom = texCoordFrag.y == cubeedgelength ? texture3D(textureBottom, vec3(texCoordFrag.x, 0, texCoordFrag.z)) : texture3D(textureCentral, texCoordFrag);
//        vec4 center = texture3D(textureCentral, texCoordFrag);
//        center = mix(left, center, 0.5);
//        center = mix(right, center, 0.5);
//        center = mix(top, center, 0.5);
//        gl_FragColor = mix(bottom, center, 0.5);
    })shaderSource");

    program.link();
    program.bind();
}

void widget::mouseMoveEvent(QMouseEvent *event) {
    auto test = mouseDown - event->pos();
    deviation += {test.x(), test.y(), 0};
    deviation = {std::fmod(deviation.x(), 128), std::fmod(deviation.y(), 128), std::fmod(deviation.z(), 128)};
    mouseDown = event->pos();
}

void widget::mousePressEvent(QMouseEvent *event) {
    mouseDown = event->pos();
}

void widget::resizeGL(int width, int height) {
    glViewport(0, 0, width, height);
}

void widget::paintGL() {
    static QElapsedTimer time;
    qDebug() << time.restart();

//    const auto start = std::chrono::high_resolution_clock::now();
//    glClearColor(dist(twister), dist(twister), dist(twister), 0);

//    for (int y = 0; y < supercubeedge; ++y)
//    for (int x = 0; x < supercubeedge; ++x) {
//        glBindTexture(GL_TEXTURE_3D, textures[y][x]);
//        glTexImage3D(GL_TEXTURE_3D, 0, GL_LUMINANCE8, 128, 128, 128, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, data.data());//8 bit per pixel
//    }

    QOpenGLTimeMonitor times;
    times.setSampleCount(3);
    times.create();
    times.recordSample();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //*

    const float width = 1.f*this->width();
    const float height = 1.f*this->height();
    std::vector<std::array<GLfloat, 3>> triangleVertices;
    std::vector<std::array<GLfloat, 3>> textureVertices;
    for (float y = 0; y < supercubeedge; ++y)
    for (float x = 0; x < supercubeedge; ++x) {
        auto startx = x * (width / supercubeedge);
        auto starty = y * (height / supercubeedge);
        auto endx = startx + width / supercubeedge;
        auto endy = starty + height / supercubeedge;
        auto starttexR = (0.5f + frame + deviation.z()) / 128.0f;
        auto endtexR = (0.5f + frame + deviation.z()) / 128.0f;

        triangleVertices.push_back({startx, starty, 0});
        triangleVertices.push_back({startx, endy, 0});
        triangleVertices.push_back({endx, endy, 0});
        triangleVertices.push_back({endx, starty, 0});

        textureVertices.push_back({0.0f, 1.0f, starttexR});
        textureVertices.push_back({0.0f, 0.0f, starttexR});
        textureVertices.push_back({1.0f, 0.0f, endtexR});
        textureVertices.push_back({1.0f, 1.0f, endtexR});
    }

    QMatrix4x4 modelMatrix;//identity
    QMatrix4x4 viewMatrix;//identity
    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(0, width, 0, height, -1, 1);
    viewMatrix.translate(deviation / QVector3D{-1.f, 1.f, 1});

    int vertexLocation = program.attributeLocation("vertex");
    int texLocation = program.attributeLocation("texCoordVertex");
    program.enableAttributeArray(vertexLocation);
    program.enableAttributeArray(texLocation);
    program.setAttributeArray(vertexLocation, triangleVertices.data()->data(), 3);
    program.setAttributeArray(texLocation, textureVertices.data()->data(), 3);
    program.setUniformValue("model_matrix", modelMatrix);
    program.setUniformValue("view_matrix", viewMatrix);
    program.setUniformValue("projection_matrix", projectionMatrix);
    program.setUniformValue("textureCentral", 0);
    program.setUniformValue("textureLeft", 1);
    program.setUniformValue("textureRight", 2);
    program.setUniformValue("textureTop", 3);
    program.setUniformValue("textureBottom", 4);
    program.setUniformValue("cubeedgelength", 128.0f);

    for (float y = 0; y < supercubeedge; ++y)
    for (float x = 0; x < supercubeedge; ++x) {
        glActiveTexture(GL_TEXTURE0);
        //glBindTexture(GL_TEXTURE_3D, textures[y][x]);
        textures2[y][x]->bind();
//		glActiveTexture(GL_TEXTURE1);
//		glBindTexture(GL_TEXTURE_3D, textures[x-1][y]);
//		glActiveTexture(GL_TEXTURE2);
//		glBindTexture(GL_TEXTURE_3D, textures[x+1][y]);
//		glActiveTexture(GL_TEXTURE3);
//		glBindTexture(GL_TEXTURE_3D, textures[x][y-1]);
//		glActiveTexture(GL_TEXTURE4);
//		glBindTexture(GL_TEXTURE_3D, textures[x][y+1]);

        glDrawArrays(GL_QUADS, 4 * (y * supercubeedge + x), 4);
    }

    program.disableAttributeArray(vertexLocation);
    program.disableAttributeArray(texLocation);
    /*/
    const double width = 2.0;
    const double height = 2.0;

    glLoadIdentity();
    glTranslated(0, 0, -1);
    glEnable(GL_TEXTURE_3D);

    for (double y = 0; y < supercubeedge; ++y)
    for (double x = 0; x < supercubeedge; ++x) {
        double starty = -1.0 + y * (height / supercubeedge);
        double startx = -1.0 + x * (width / supercubeedge);
        auto starttexR = (0.5 + frame / supercubeedge * x) / 128.0;//
        auto endtexR = (0.5 + frame / supercubeedge * (x + 1)) / 128.0;//
        textures2[y * supercubeedge + x]->bind();
        //glBindTexture(GL_TEXTURE_3D, textures[y][x]);
        glBegin(GL_QUADS);
            glTexCoord3d(0.0, 1.0, starttexR);
            glVertex3d(startx, starty, 0);
            glTexCoord3d(0.0, 0.0, starttexR);
            glVertex3d(startx, starty + height / supercubeedge, 0);
            glTexCoord3d(1.0, 0.0, endtexR);
            glVertex3d(startx + width / supercubeedge, starty + height / supercubeedge, 0);
            glTexCoord3d(1.0, 1.0, endtexR);
            glVertex3d(startx + width / supercubeedge, starty, 0);
        glEnd();
    }

    glDisable(GL_TEXTURE_3D);
    glColor4d(1, 0, 0, 1);
    glPointSize(3.0);
    glBegin(GL_POINTS);
        glVertex3d(0, 0, 0.1);
    glEnd();
    //*/
    times.recordSample();

    qDebug() << "render time: " << times.waitForIntervals();

//    const auto end = std::chrono::high_resolution_clock::now();
//    auto mspf = time.elapsed();//std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()/1000.0/1000.0;
//    auto fps = 1000.0 / (mspf != 0 ? mspf : 1);
//    std::cout << mspf << " mpfs – \t" << fps << " fps" << std::endl;
}

void widget::wheelEvent(QWheelEvent * const event) {
    const int direction = std::trunc(event->angleDelta().y());
    frame += direction / 120;
    frame = std::fmod(frame, 128);
    std::cout << direction << " " << event->angleDelta().y() << " " << frame << std::endl;
    update();
}
