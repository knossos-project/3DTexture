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

#include <boost/functional/hash.hpp>
#include <boost/multi_array.hpp>
#include <cstdint>
#include <list>
#include <unordered_map>

namespace std {
template<>
struct hash<QVector3D> {
    std::size_t operator()(const QVector3D & cord) const {
        return boost::hash_value(std::make_tuple(cord.x(), cord.y(), cord.z()));
    }
};
}

class gpu_raw_cube {
public:
    QOpenGLTexture cube{QOpenGLTexture::Target3D};

    gpu_raw_cube(const int gpucubeedge, const bool index = false) {
        cube.setAutoMipMapGenerationEnabled(false);
        cube.setSize(gpucubeedge, gpucubeedge, gpucubeedge);
        cube.setMipLevels(1);
        cube.setMinificationFilter(index ? QOpenGLTexture::Nearest : QOpenGLTexture::Linear);
        cube.setMagnificationFilter(index ? QOpenGLTexture::Nearest : QOpenGLTexture::Linear);
        cube.setFormat(index ? QOpenGLTexture::R16_UNorm : QOpenGLTexture::R8_UNorm);
        cube.setWrapMode(QOpenGLTexture::ClampToEdge);
        cube.allocateStorage();
    }

    void generate(boost::multi_array_ref<std::uint8_t, 3>::const_array_view<3>::type view) {
        std::vector<char> data;
        for (const auto & d2 : view)
        for (const auto & d1 : d2)
        for (const auto & elem : d1) {
            data.emplace_back(elem);
        }
        cube.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8, data.data());
    }
};

class gpu_lut_cube : public gpu_raw_cube {
public:
    using gpu_index = quint16;
private:
    QHash<quint64, gpu_index> id_to_lut_index;
    gpu_index highest_index = 0;
    std::vector<std::array<uint8_t, 4>> colors;
public:
    QOpenGLTexture lut{QOpenGLTexture::Target1D};

    gpu_lut_cube(const int gpucubeedge) : gpu_raw_cube(gpucubeedge, true) {
        lut.setAutoMipMapGenerationEnabled(false);
        lut.setMipLevels(1);
        lut.setMinificationFilter(QOpenGLTexture::Nearest);
        lut.setMagnificationFilter(QOpenGLTexture::Nearest);
        lut.setFormat(QOpenGLTexture::RGBA8_UNorm);
    }

    void generate(boost::multi_array_ref<std::uint64_t, 3>::const_array_view<3>::type view) {
        std::vector<std::uint16_t> data;
        for (const auto & d2 : view)
        for (const auto & d1 : d2)
        for (const auto & elem : d1) {
            const auto it = id_to_lut_index.find(elem);
            const auto existing = it != std::end(id_to_lut_index);
            const auto index = existing ? *it : (id_to_lut_index[elem] = highest_index++);//increment after assignment
            data.emplace_back(index);
            if (!existing) {
                if (elem == 0) {
                    colors.push_back({{0, 0, 0, 0}});
                } else if (elem == 1) {
                    colors.push_back({{255, 0, 0, 255}});
                } else if (elem == 2) {
                    colors.push_back({{0, 255, 0, 255}});
                } else if (elem == 3) {
                    colors.push_back({{0, 0, 255, 255}});
                } else {
                    std::uint8_t idc = elem;
                    colors.push_back({{idc, 0, 0, 255}});
                }
            }
        }
        const auto lutSize = std::pow(2, std::ceil(std::log2(colors.size())));
        colors.resize(lutSize);
        lut.setSize(lutSize);
        lut.allocateStorage();

        cube.setData(QOpenGLTexture::Red, QOpenGLTexture::UInt16, data.data());
        lut.setData(QOpenGLTexture::RGBA, QOpenGLTexture::UInt32_RGBA8_Rev, colors.data());
    }
};

struct TextureLayer {
    std::unordered_map<QVector3D, std::unique_ptr<gpu_raw_cube>> textures;
    std::unique_ptr<gpu_raw_cube> bogusCube;
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
};

#endif//WIDGET_H
