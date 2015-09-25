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
    for(auto & layer : layers) {
        for (auto & texture : boost::make_iterator_range(layer.textures.data(), layer.textures.data() + layer.textures.num_elements())) {
            delete texture;
        }
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
    // glEnable(GL_DEPTH_TEST);
    // glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

    // ------- load raw data -------
    layers.emplace_back();
    layers.back().opacity = 1.0f;
    auto& textures = layers.back().textures;

    lut.reset(new QOpenGLTexture(QOpenGLTexture::Target1D));
    QOpenGLTexture & texture = *lut;
    texture.setAutoMipMapGenerationEnabled(false);
    texture.setSize(1024);
    texture.setMipLevels(1);
    texture.setMinificationFilter(QOpenGLTexture::Nearest);
    texture.setMagnificationFilter(QOpenGLTexture::Nearest);
    texture.setFormat(QOpenGLTexture::RGBA8_UNorm);
    texture.allocateStorage();

    QHash<quint64, quint32> gpuIds;
    std::vector<std::array<uint8_t, 4>> colors;
    decltype(gpuIds)::mapped_type highestId = 0;

    const auto dims = boost::extents[supercubeedge][supercubeedge];
    for (auto & texture : boost::make_iterator_range(textures.data(), textures.data() + textures.num_elements())) {
        delete texture;
    }
    textures.resize(boost::extents[supercubeedge][supercubeedge]);
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
                std::uint8_t idc = id;
                colors.push_back({{idc, 0, 0, 255}});
            }
        }
        colors.resize(lut->width() * lut->height() * lut->depth());

        texture.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data2.data());
        lut->setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt32_RGBA8_Rev, colors.data());
    }

    // ------- load overlay data -------
    layers.emplace_back();
    layers.back().opacity = 0.5f;
    layers.back().isOverlayData = true;
    auto& otextures = layers.back().textures;

    for (auto & texture : boost::make_iterator_range(otextures.data(), otextures.data() + otextures.num_elements())) {
        delete texture;
    }
    otextures.resize(boost::extents[supercubeedge][supercubeedge]);
    for (auto & texture : boost::make_iterator_range(otextures.data(), otextures.data() + otextures.num_elements())) {
        texture = nullptr;
    }

    for (int sy = 0; sy < supercubeedge; ++sy)
    for (int sx = 0; sx < supercubeedge; ++sx) {
        overlay_data.resize(std::pow(cpucubeedge, 3));
        std::fill(std::begin(overlay_data), std::end(overlay_data), 1338);

        otextures[sy][sx] = new QOpenGLTexture(QOpenGLTexture::Target3D);
        QOpenGLTexture & otexture = *otextures[sy][sx];
        otexture.setAutoMipMapGenerationEnabled(false);
        otexture.setSize(cpucubeedge, cpucubeedge, cpucubeedge);
        otexture.setMipLevels(1);
        otexture.setMinificationFilter(QOpenGLTexture::Linear);
        otexture.setMagnificationFilter(QOpenGLTexture::Linear);
        otexture.setFormat(QOpenGLTexture::R16_UNorm);
        otexture.allocateStorage();
        otexture.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16, overlay_data.data());
    }

    raw_data_shader.addShaderFromSourceCode(QOpenGLShader::Vertex, R"shaderSource(
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

    raw_data_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
    //#version 110
    uniform float texture_opacity;
    uniform sampler3D textureCentral;
    uniform sampler3D textureLeft;
    uniform sampler3D textureRight;
    uniform sampler3D textureTop;
    uniform sampler3D textureBottom;
    uniform sampler1D textureLUT;
    uniform float cubeedgelength;
    varying vec3 texCoordFrag;//in
    //varying vec4 gl_FragColor;//out
    void main() {
//        gl_FragColor = vec4(texCoordFrag, texture_opacity);
//        gl_FragColor = vec4(vec3(texture3D(textureCentral, texCoordFrag).r), texture_opacity);
        float index = texture3D(textureCentral, texCoordFrag).r;
        float size = 1024.0;//float(textureSize1D(textureLUT, 0));
        index *= 256.0;//expand float to uint8 range
        gl_FragColor = texture1D(textureLUT, (index + 0.5) / size);
        gl_FragColor.a = texture_opacity;
        //gl_FragColor = texelFetch(textureLUT, int(index), 0);//requires version 130
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

    raw_data_shader.link();
    raw_data_shader.bind();

    overlay_data_shader.addShaderFromSourceCode(QOpenGLShader::Vertex, R"shaderSource(
    //#version 110

    uniform mat4 model_matrix;
    uniform mat4 view_matrix;
    uniform mat4 projection_matrix;
    attribute vec3 vertex;
    attribute vec3 texCoordVertex;
    varying vec3 texCoordFrag;

    void main() {
        mat4 mvp_mat = projection_matrix * view_matrix * model_matrix;
        gl_Position = mvp_mat * vec4(vertex, 1.0f);
        texCoordFrag = texCoordVertex;
    })shaderSource");

    overlay_data_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
    //#version 110

    uniform float texture_opacity;
    uniform sampler3D textureCentral;
    varying vec3 texCoordFrag;//in

    void main() {
        if(texture3D(textureCentral, texCoordFrag).r == 1337.0f / 65535.0f)
            gl_FragColor = vec4(1.0f, 0.0f, 0.0, texture_opacity);
        else
            gl_FragColor = vec4(0.0f);
    })shaderSource");

    overlay_data_shader.link();
    overlay_data_shader.bind();
}

void widget::mouseMoveEvent(QMouseEvent *event) {
    auto test = mouseDown - event->pos();
    deviation += QVector3D(test.x(), test.y(), 0.0f);
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

        triangleVertices.push_back({{startx, starty, 0.0f}});
        triangleVertices.push_back({{startx, endy, 0.0f}});
        triangleVertices.push_back({{endx, endy, 0.0f}});
        triangleVertices.push_back({{endx, starty, 0.0f}});

        textureVertices.push_back({{0.0f, 1.0f, starttexR}});
        textureVertices.push_back({{0.0f, 0.0f, starttexR}});
        textureVertices.push_back({{1.0f, 0.0f, endtexR}});
        textureVertices.push_back({{1.0f, 1.0f, endtexR}});
    }

    QMatrix4x4 modelMatrix; //identity
    QMatrix4x4 viewMatrix; //identity
    QMatrix4x4 projectionMatrix;
    projectionMatrix.ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
    viewMatrix.translate(deviation / QVector3D{-1.0f, 1.0f, 1.0f});

    // raw data shader
    raw_data_shader.bind();
    int vertexLocation = raw_data_shader.attributeLocation("vertex");
    int texLocation = raw_data_shader.attributeLocation("texCoordVertex");
    raw_data_shader.enableAttributeArray(vertexLocation);
    raw_data_shader.enableAttributeArray(texLocation);
    raw_data_shader.setAttributeArray(vertexLocation, triangleVertices.data()->data(), 3);
    raw_data_shader.setAttributeArray(texLocation, textureVertices.data()->data(), 3);
    raw_data_shader.setUniformValue("model_matrix", modelMatrix);
    raw_data_shader.setUniformValue("view_matrix", viewMatrix);
    raw_data_shader.setUniformValue("projection_matrix", projectionMatrix);
    raw_data_shader.setUniformValue("textureCentral", 0);
    raw_data_shader.setUniformValue("textureLeft", 1);
    raw_data_shader.setUniformValue("textureRight", 2);
    raw_data_shader.setUniformValue("textureTop", 3);
    raw_data_shader.setUniformValue("textureBottom", 4);
    raw_data_shader.setUniformValue("textureLUT", 5);
    const float cubeedgef = gpucubeedge;
    raw_data_shader.setUniformValue("cubeedgelength", cubeedgef);

    // overlay data shader
    overlay_data_shader.bind();
    int overtexLocation = overlay_data_shader.attributeLocation("vertex");
    int otexLocation = overlay_data_shader.attributeLocation("texCoordVertex");
    overlay_data_shader.enableAttributeArray(overtexLocation);
    overlay_data_shader.enableAttributeArray(otexLocation);
    overlay_data_shader.setAttributeArray(overtexLocation, triangleVertices.data()->data(), 3);
    overlay_data_shader.setAttributeArray(otexLocation, textureVertices.data()->data(), 3);
    overlay_data_shader.setUniformValue("model_matrix", modelMatrix);
    overlay_data_shader.setUniformValue("view_matrix", viewMatrix);
    overlay_data_shader.setUniformValue("projection_matrix", projectionMatrix);
    overlay_data_shader.setUniformValue("textureCentral", 0);

    for(const auto& layer : layers) {
        if(layer.enabled && layer.opacity >= 0.0f) {
            if(layer.isOverlayData) {
                overlay_data_shader.bind();
                overlay_data_shader.setUniformValue("texture_opacity", layer.opacity);
            } else {
                raw_data_shader.bind();
                raw_data_shader.setUniformValue("texture_opacity", layer.opacity);
            }

            for (float y = 0; y < supercubeedge; ++y)
            for (float x = 0; x < supercubeedge; ++x) {
                layer.textures[y][x]->bind(0);
                lut->bind(5);
                glDrawArrays(GL_QUADS, 4 * (y * supercubeedge + x), 4);
            }
        }
    }

    raw_data_shader.disableAttributeArray(vertexLocation);
    raw_data_shader.disableAttributeArray(texLocation);
    overlay_data_shader.disableAttributeArray(overtexLocation);
    overlay_data_shader.disableAttributeArray(otexLocation);
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
