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

widget::widget() {
    //qDebug() << context()->format().swapBehavior() << ' ' << context()->format().swapInterval();

    QObject::connect(&continuousRefresh, &QTimer::timeout, this, static_cast<void (QOpenGLWidget::*)()>(&QOpenGLWidget::update));
    continuousRefresh.start(0);
    resize(gpucubeedge * supercubeedge, gpucubeedge * supercubeedge);
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

    std::vector<char> data;
    int z = 0;
    for (int y = 0; y < supercubeedge; ++y)
    for (int x = 0; x < supercubeedge; ++x) {
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

        layers.back().textures[QVector3D(x, y, z)].reset(new gpu_raw_cube(gpucubeedge));
        //boost::multi_array_ref<std::uint64_t, 3> cube(reinterpret_cast<std::uint64_t*>(data.data()), boost::extents[cpucubeedge][cpucubeedge][cpucubeedge]);
        boost::multi_array_ref<std::uint8_t, 3> cube(reinterpret_cast<std::uint8_t*>(data.data()), boost::extents[cpucubeedge][cpucubeedge][cpucubeedge]);
        const auto x_offset = gpucubeedge * (x % factor);
        const auto y_offset = gpucubeedge * (y % factor);
        const auto z_offset = 0;
        using range = boost::multi_array_types::index_range;
        const auto view = cube[boost::indices[range(0+z_offset,gpucubeedge+z_offset)][range(cpucubeedge-gpucubeedge-y_offset,cpucubeedge-0-y_offset)][range(0+x_offset,gpucubeedge+x_offset)]];
        static_cast<gpu_raw_cube*>(layers.back().textures[QVector3D(x, y, z)].get())->generate(view);
    }

    // ------- load overlay data -------
    layers.emplace_back();
    layers.back().opacity = 0.5f;
    layers.back().isOverlayData = true;

    for (int y = 0; y < supercubeedge; ++y)
    for (int x = 0; x < supercubeedge; ++x) {
        const auto factor = cpucubeedge / gpucubeedge;
        std::string path = "/run/media/mobile/00AAEB91AAEB8210/New folder/cubes/2012-03-07_AreaX14_mag1_x00" + std::to_string(29+x/factor) + "_y00" + std::to_string(52-(y/factor)) + "_z0023.raw.segmentation.raw";
        std::ifstream file(path, std::ios_base::binary);
        data.resize(std::pow(cpucubeedge, 3)*8);
        if (file) {
            file.read(data.data(), data.size());
        } else {
            std::fill(std::begin(data), std::end(data), 127);
            std::cout << path << " failed" << std::endl;
        }

        layers.back().textures[QVector3D(x, y, z)].reset(new gpu_lut_cube(gpucubeedge));
        boost::multi_array_ref<std::uint64_t, 3> cube(reinterpret_cast<std::uint64_t*>(data.data()), boost::extents[cpucubeedge][cpucubeedge][cpucubeedge]);
        const auto x_offset = gpucubeedge * (x % factor);
        const auto y_offset = gpucubeedge * (y % factor);
        const auto z_offset = 0;
        using range = boost::multi_array_types::index_range;
        const auto view = cube[boost::indices[range(0+z_offset,gpucubeedge+z_offset)][range(cpucubeedge-gpucubeedge-y_offset,cpucubeedge-0-y_offset)][range(0+x_offset,gpucubeedge+x_offset)]];
        static_cast<gpu_lut_cube*>(layers.back().textures[QVector3D(x, y, z)].get())->generate(view);
    }

    auto vertex_shader_code = R"shaderSource(
    #version 110
    uniform mat4 model_matrix;
    uniform mat4 view_matrix;
    uniform mat4 projection_matrix;
    attribute vec3 vertex;
    attribute vec3 texCoordVertex;
    varying vec3 texCoordFrag;
    void main() {
        mat4 mvp_mat = projection_matrix * view_matrix * model_matrix;
        gl_Position = mvp_mat * vec4(vertex, 1.0);
        texCoordFrag = texCoordVertex;
    })shaderSource";

    raw_data_shader.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_code);
    raw_data_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
    #version 110
    uniform float textureOpacity;
    uniform sampler3D texture;
    varying vec3 texCoordFrag;//in
    //varying vec4 gl_FragColor;//out
    void main() {
        gl_FragColor = vec4(vec3(texture3D(texture, texCoordFrag).r), textureOpacity);
    })shaderSource");

    raw_data_shader.link();
    raw_data_shader.bind();

    overlay_data_shader.addShaderFromSourceCode(QOpenGLShader::Vertex, vertex_shader_code);
    overlay_data_shader.addShaderFromSourceCode(QOpenGLShader::Fragment, R"shaderSource(
    #version 110
    uniform float textureOpacity;
    uniform sampler3D indexTexture;
    uniform sampler1D textureLUT;
    uniform float lutSize;//float(textureSize1D(textureLUT, 0));
    uniform float factor;//expand float to uint8 range
    varying vec3 texCoordFrag;//in
    void main() {
        float index = texture3D(indexTexture, texCoordFrag).r;
        index *= factor;
        gl_FragColor = texture1D(textureLUT, (index + 0.5) / lutSize);
        gl_FragColor.a = textureOpacity;
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
    for (float y = 0.0f; y < supercubeedge; ++y)
    for (float x = 0.0f; x < supercubeedge; ++x) {
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
    raw_data_shader.setUniformValue("texture", 0);

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
    overlay_data_shader.setUniformValue("indexTexture", 0);
    overlay_data_shader.setUniformValue("textureLUT", 1);
    overlay_data_shader.setUniformValue("factor", static_cast<float>(std::numeric_limits<std::uint16_t>::max()));

    for (auto & layer : layers) {
        if (layer.enabled && layer.opacity >= 0.0f) {
            if (layer.isOverlayData) {
                overlay_data_shader.bind();
                overlay_data_shader.setUniformValue("textureOpacity", layer.opacity);
            } else {
                raw_data_shader.bind();
                raw_data_shader.setUniformValue("textureOpacity", layer.opacity);
            }

            float z = 0;
            for (float y = 0; y < supercubeedge; ++y)
            for (float x = 0; x < supercubeedge; ++x) {
                auto ptr = layer.textures[QVector3D(x, y, z)].get();
                if (layer.isOverlayData) {
                    auto * punned = static_cast<gpu_lut_cube*>(ptr);
                    punned->cube.bind(0);
                    punned->lut.bind(1);
                    overlay_data_shader.setUniformValue("lutSize", static_cast<float>(punned->lut.width() * punned->lut.height() * punned->lut.depth()));
                } else {
                    ptr->cube.bind(0);;
                }
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
    const auto pixel_scroll_speed = 0.07f;
    QPoint numPixels = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8.0f;

    // use pixel scrolling if supported
    if (!numPixels.isNull()) {
        frame += numPixels.y() * pixel_scroll_speed;
    } else if (!numDegrees.isNull()) {
        QPoint numSteps = numDegrees / 15.0f;
        frame += numSteps.y();
    }

    // clip frame to cube edges
    const float cubeedgef = gpucubeedge;
    frame = std::fmod(frame + cubeedgef, cubeedgef);

    update();
}
