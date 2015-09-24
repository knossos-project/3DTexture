#include "widget.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QOpenGLTexture>
#include <QOpenGLTimerQuery>
#include <QWheelEvent>

#include <boost/range/iterator_range_core.hpp>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <vector>

widget::widget() : twister(std::random_device{}()), dist(0.0, 1.0) {
    //qDebug() << context()->format().swapBehavior() << ' ' << context()->format().swapInterval();

    QObject::connect(&continuousRefresh, &QTimer::timeout, this, static_cast<void (QOpenGLWidget::*)()>(&QOpenGLWidget::update));
    continuousRefresh.start(0);
    resize(gpucubeedge * supercubeedge, gpucubeedge * supercubeedge);
}

widget::~widget() {
    makeCurrent();
    for (auto & texture : boost::make_iterator_range(textures.data(), textures.data() + textures.num_elements())) {
        delete texture;
    }
}

void widget::initializeGL() {
    std::ios_base::sync_with_stdio(false); // turns off sync between C and C++ output streams(to increase output speed)

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

    glEnable(GL_TEXTURE_3D);
    glEnable(GL_DEPTH_TEST);
    glClearColor(1.0, 1.0, 1.0, 1.0);

    lut.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
    QOpenGLTexture & texture = *lut;
    texture.setAutoMipMapGenerationEnabled(false);
    texture.setSize(1024, 1024);
    texture.setMipLevels(1);
    texture.setMinificationFilter(QOpenGLTexture::Nearest);
    texture.setMagnificationFilter(QOpenGLTexture::Nearest);
    texture.setFormat(QOpenGLTexture::RGBA32U);
    texture.allocateStorage();

    QHash<quint64, quint32> gpuIds;
    std::vector<std::array<uint8_t, 4>> colors;
    decltype(gpuIds)::mapped_type highestId = 0;

    const auto dims = boost::extents[supercubeedge][supercubeedge];
    for (auto & texture : boost::make_iterator_range(textures.data(), textures.data() + textures.num_elements())) {
        delete texture;
    }
    textures.resize(dims);
    for (auto & texture : boost::make_iterator_range(textures.data(), textures.data() + textures.num_elements())) {
        texture = nullptr;
    }

    for (int y = 0; y < supercubeedge; ++y)
    for (int x = 0; x < supercubeedge; ++x) {
        QElapsedTimer time;
        time.start();
        const auto factor = cpucubeedge / gpucubeedge;
        std::string path = "cubes/2012-03-07_AreaX14_mag1_x00" + std::to_string(29+x/factor) + "_y00" + std::to_string(52-y/factor) + "_z0023.raw";
//        std::string path = "C:/New folder/cubes/2012-03-07_AreaX14_mag1_x00" + std::to_string(29+x) + "_y00" + std::to_string(52-y) + "_z0023.raw";
//        std::string path = "\\\\mobile/New folder/cubes/2012-03-07_AreaX14_mag1_x00" + std::to_string(29+x) + "_y00" + std::to_string(52-y) + "_z0023.raw";
//        std::string path = QString("/run/user/1000/gvfs/sftp:host=soma06,user=npfeiler/lustre/sdorkenw/j0126_cubed_64/mag1/x00%1/y00%2/z0023/2012-03-07_AreaX14_mag1_x00%1_y00%2_z0023.raw").arg(29+x).arg(52-y).toStdString();
        std::ifstream file(path, std::ios_base::binary);
        data.resize(std::pow(cpucubeedge, 3));
        if (file) {
            file.read(data.data(), data.size());
        } else {
            std::fill(std::begin(data), std::end(data), 127);
            std::cout << path << " failed" << std::endl;
        }

        textures[y][x] = new QOpenGLTexture(QOpenGLTexture::Target3D);
        QOpenGLTexture & texture = *textures[y][x];
        texture.setAutoMipMapGenerationEnabled(false);
        texture.setSize(gpucubeedge, gpucubeedge, gpucubeedge);
        texture.setMipLevels(1);
        texture.setMinificationFilter(QOpenGLTexture::Linear);
        texture.setMagnificationFilter(QOpenGLTexture::Linear);
        texture.setFormat(QOpenGLTexture::R8_UNorm);
        texture.setWrapMode(QOpenGLTexture::ClampToEdge);
        texture.allocateStorage();

        boost::multi_array_ref<char, 3> cube(data.data(), boost::extents[cpucubeedge][cpucubeedge][cpucubeedge]);
        const auto x_offset = gpucubeedge * (x % factor);
        const auto y_offset = gpucubeedge * (y % factor);
        const auto z_offset = 0;
        using range = boost::multi_array_types::index_range;
        const auto view = cube[boost::indices[range(0+z_offset,gpucubeedge+z_offset)][range(cpucubeedge-gpucubeedge-y_offset,cpucubeedge-0-y_offset)][range(0+x_offset,gpucubeedge+x_offset)]];
        std::vector<char> data2;
        for (const auto & d2 : view)
        for (const auto & d1 : d2)
        for (const auto & elem : d1) {
            const auto it = gpuIds.find(elem);
            const auto existing = it != std::end(gpuIds);
            const auto id = existing ? *it : (gpuIds[elem] = highestId++);
            data2.emplace_back(id);
            if (!existing) {
                colors.push_back({id, id, id, 255});
            }
        }
        colors.resize(lut->width() * lut->height());

        texture.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data2.data());
        lut->setData(QOpenGLTexture::RGBA_Integer, QOpenGLTexture::UInt32_RGBA8, colors.data());
    }

    program.addShaderFromSourceCode(QOpenGLShader::Vertex, R"shaderSource(
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

    program.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
    //#version 110
    uniform sampler3D textureCentral;
    uniform sampler3D textureLeft;
    uniform sampler3D textureRight;
    uniform sampler3D textureTop;
    uniform sampler3D textureBottom;
    uniform sampler2D textureLUT;
    uniform float cubeedgelength;
    varying vec3 texCoordFrag;//in
    //varying vec4 gl_FragColor;//out
    void main() {
//        gl_FragColor = vec4(texCoordFrag, 1);
        gl_FragColor = vec4(vec3(texture3D(textureCentral, texCoordFrag).r), 1.0f);
        float index = texture3D(textureCentral, texCoordFrag).r;
        //float size = textureSize(textureLUT).x;
        //gl_FragColor = texture2D(textureLUT, ivec2((index + 0.5) / size, 0.5));
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
    deviation += QVector3D(test.x(), test.y(), 0);
    const float cubeedgef = gpucubeedge;
    deviation = {std::fmod(deviation.x(), cubeedgef), std::fmod(deviation.y(), cubeedgef), std::fmod(deviation.z(), cubeedgef)};
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

    QOpenGLTimeMonitor times;
    times.setSampleCount(3);
    times.create();
    times.recordSample();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float width = 1.0f * this->width();
    const float height = 1.0f * this->height();
    std::vector<std::array<GLfloat, 3>> triangleVertices;
    std::vector<std::array<GLfloat, 3>> textureVertices;
    for (float y = 0; y < supercubeedge; ++y)
    for (float x = 0; x < supercubeedge; ++x) {
        auto startx = x * (width / supercubeedge);
        auto starty = y * (height / supercubeedge);
        auto endx = startx + width / supercubeedge;
        auto endy = starty + height / supercubeedge;
        const float cubeedgef = gpucubeedge;
        auto starttexR = (0.5f + frame + deviation.z()) / cubeedgef;
        auto endtexR = (0.5f + frame + deviation.z()) / cubeedgef;

        triangleVertices.push_back({{startx, starty, 0}});
        triangleVertices.push_back({{startx, endy, 0}});
        triangleVertices.push_back({{endx, endy, 0}});
        triangleVertices.push_back({{endx, starty, 0}});

        textureVertices.push_back({{0.0f, 1.0f, starttexR}});
        textureVertices.push_back({{0.0f, 0.0f, starttexR}});
        textureVertices.push_back({{1.0f, 0.0f, endtexR}});
        textureVertices.push_back({{1.0f, 1.0f, endtexR}});
    }

    QMatrix4x4 modelMatrix; //identity
    QMatrix4x4 viewMatrix; //identity
    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(0, width, 0, height, -1, 1);
    viewMatrix.translate(deviation / QVector3D{-1.0f, 1.0f, 1});

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
    program.setUniformValue("textureLUT", 5);
    const float cubeedgef = gpucubeedge;
    program.setUniformValue("cubeedgelength", cubeedgef);

    for (float y = 0; y < supercubeedge; ++y)
    for (float x = 0; x < supercubeedge; ++x) {
        glActiveTexture(GL_TEXTURE0);
        textures[y][x]->bind();
        glActiveTexture(GL_TEXTURE1);
        lut->bind();
        glDrawArrays(GL_QUADS, 4 * (y * supercubeedge + x), 4);
    }

    program.disableAttributeArray(vertexLocation);
    program.disableAttributeArray(texLocation);
    times.recordSample();

    qDebug() << "render time: " << times.waitForIntervals();
}

void widget::wheelEvent(QWheelEvent * const event) {
    const int direction = std::trunc(event->angleDelta().y());
    frame += direction / 120;
    const float cubeedgef = gpucubeedge;
    frame = std::fmod(frame+cubeedgef, cubeedgef);
    std::cout << direction << " " << event->angleDelta().y() << " " << frame << std::endl;
    update();
}
