#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <iostream>

#include <cassert>

#include <chrono>
#include <memory>
#include <cstring>
#include <vector>
#include <array>
#include <cmath>
#include <ranges>
#include <cassert>
#include <fstream>
#include <limits>
bool quited = false;

static constexpr auto colorIndices = std::array{
    0xFFFF0000,
    0xFF00FF00,
    0xFF0000FF,
    0xFFFF00FF,
    0xFFFFFF00,
};

template <typename T>
struct Matrix3D
{
    T data[3][3];
};

template <typename T>
struct Point
{
    T x;
    T y;
};

template <typename T>
struct Vector3D
{
    Point<T> as2DPoint() const
    {
        return Point<T>{.x = x, .y = y};
    }

    T x;
    T y;
    T z;
};

template <typename T>
struct TexturedVertexType
{
    T x;
    T y;
    T z;

    TexturedVertexType(const Vector3D<T> &from)
        : x(from.x),
          y(from.y),
          z(from.z),
          u(0),
          v(0)
    {
    }

    TexturedVertexType(T x, T y, T z, T u, T v)
        : x(x),
          y(y),
          z(z),
          u(u),
          v(v)
    {
    }

    Vector3D<T> getPointVector() const
    {
        return {x, y, z};
    }

    T u;
    T v;
};

using Vector3DI32 = Vector3D<int32_t>;
using Vector3DFloat = Vector3D<float>;
using TexturedVertextFloat = TexturedVertexType<float>;
using Matrix3DI32 = Matrix3D<int32_t>;
using PointInt32 = Point<int32_t>;
using PointFloat = Point<float>;

Vector3DFloat readVector(const std::string_view &line)
{
    auto currSpacePos = line.find_first_of(' ');

    float v[4] = {};
    size_t i = 0;

    while (currSpacePos != std::string::npos)
    {
        auto nextSpacePos = line.find_first_of(' ', currSpacePos + 1);
        const auto end = nextSpacePos != std::string::npos ? line.begin() + nextSpacePos : line.end();
        std::string buffer{line.begin() + currSpacePos, end};
        float val = (float)atof(buffer.c_str());
        v[i] = val;
        i++;
        currSpacePos = nextSpacePos;
        assert(i < sizeof(v));
    }

    return {.x = v[0], .y = v[1], .z = v[2]};
}

std::tuple<int, int, int> readFace(const std::string_view &line)
{
    auto currSpacePos = line.find_first_of(' ');

    int f[4] = {};
    size_t i = 0;

    while (currSpacePos != std::string::npos)
    {
        auto nextSpacePos = line.find_first_of(' ', currSpacePos + 1);
        const auto end = nextSpacePos != std::string::npos ? line.begin() + nextSpacePos : line.end();
        std::string buffer{line.begin() + currSpacePos, end};
        int val = atoi(buffer.c_str());
        assert(val > 0);
        f[i] = val - 1;
        i++;
        currSpacePos = nextSpacePos;
        assert(i < sizeof(f));
    }

    return {f[0], f[1], f[2]};
}

std::vector<TexturedVertextFloat> loadObjFile(const std::string &filename)
{
    std::ifstream objFile{filename};

    if (!objFile.is_open())
    {
        std::cout << "Could not open file " << filename << std::endl;
        return {};
    }

    std::vector<Vector3DFloat> allVertices;
    std::vector<std::tuple<int, int, int>> allFaces;

    std::string linebuffer;
    while (std::getline(objFile, linebuffer))
    {
        if (linebuffer[0] == 'v')
        {
            allVertices.emplace_back(readVector(linebuffer));
        }
        else if (linebuffer[0] == 'f')
        {
            allFaces.emplace_back(readFace(linebuffer));
        }
        // empty line
        else if (linebuffer.size() < 2)
        {
            continue;
        }
        else
        {
            assert(0 && "Not implemented");
        }
    }

    std::vector<TexturedVertextFloat> res;

    for (const auto &[a, b, c] : allFaces)
    {
        assert(a < (int)allVertices.size());
        assert(b < (int)allVertices.size());
        assert(c < (int)allVertices.size());

        res.push_back(allVertices[a]);
        res.push_back(allVertices[b]);
        res.push_back(allVertices[c]);
    }

    return res;
}

Vector3DFloat toScreenSpace(const Vector3DFloat &vec, int32_t screenWidth, int32_t screenHeight)
{
    return {
        .x = (vec.x + 1.0f) * (screenWidth / 2.0f),
        .y = (-vec.y + 1.0f) * (screenHeight / 2.0f),
        .z = vec.z};
}

Vector3DFloat applyNonOrthoProj(const Vector3DFloat &vec)
{
    return {
        .x = vec.x / vec.z,
        .y = vec.y / vec.z,
        .z = vec.z};
}

std::vector<TexturedVertextFloat> getTranslated(const Vector3DFloat &transl, const std::vector<TexturedVertextFloat> &vertices)
{
    std::vector<TexturedVertextFloat> res = vertices;

    for (auto &vertice : res)
    {
        vertice.x += transl.x;
        vertice.y += transl.y;
        vertice.z += transl.z;
    }

    return res;
}

std::vector<TexturedVertextFloat> getRotatedZ(float angle, const std::vector<TexturedVertextFloat> &vertices)
{
    const auto cosangle = cosf(angle);
    const auto sinangle = sinf(angle);

    std::vector<TexturedVertextFloat> res = vertices;

    for (auto &vertice : res)
    {
        const auto nx = cosangle * vertice.x - sinangle * vertice.y;
        const auto ny = sinangle * vertice.x + cosangle * vertice.y;

        vertice.x = nx;
        vertice.y = ny;
    }

    return res;
}

std::vector<TexturedVertextFloat> getRotatedX(float angle, const std::vector<TexturedVertextFloat> &vertices)
{
    const auto cosangle = cosf(angle);
    const auto sinangle = sinf(angle);

    std::vector<TexturedVertextFloat> res = vertices;

    for (auto &vertice : res)
    {
        const auto ny = cosangle * vertice.y - sinangle * vertice.z;
        const auto nz = sinangle * vertice.y + cosangle * vertice.z;

        vertice.y = ny;
        vertice.z = nz;
    }

    return res;
}

std::vector<TexturedVertextFloat> getRotatedY(float angle, const std::vector<TexturedVertextFloat> &vertices)
{
    const auto cosangle = cosf(angle);
    const auto sinangle = sinf(angle);

    std::vector<TexturedVertextFloat> res = vertices;

    for (auto &vertice : res)
    {
        const auto nx = cosangle * vertice.x + sinangle * vertice.z;
        const auto nz = -sinangle * vertice.x + cosangle * vertice.z;

        vertice.x = nx;
        vertice.z = nz;
    }

    return res;
}

struct ObjModel
{
    static ObjModel fromObjFile(const std::string &filename)
    {
        return ObjModel{loadObjFile(filename)};
    }
    ObjModel getTranslated(const Vector3DFloat &transl) const
    {
        return ObjModel{::getTranslated(transl, vertices)};
    }

    ObjModel getRotatedZ(float angle) const
    {
        return ObjModel{::getRotatedZ(angle, vertices)};
    }

    ObjModel getRotatedX(float angle) const
    {
        return ObjModel{::getRotatedX(angle, vertices)};
    }

    ObjModel getRotatedY(float angle) const
    {
        return ObjModel{::getRotatedY(angle, vertices)};
    }

    std::vector<TexturedVertextFloat> vertices = {};
};

struct SimpleTriangleModel
{
    SimpleTriangleModel getTranslated(const Vector3DFloat &transl) const
    {
        return SimpleTriangleModel{::getTranslated(transl, vertices)};
    }

    SimpleTriangleModel getRotatedZ(float angle) const
    {
        return SimpleTriangleModel{::getRotatedZ(angle, vertices)};
    }

    SimpleTriangleModel getRotatedX(float angle) const
    {
        return SimpleTriangleModel{::getRotatedX(angle, vertices)};
    }

    SimpleTriangleModel getRotatedY(float angle) const
    {
        return SimpleTriangleModel{::getRotatedY(angle, vertices)};
    }

    std::vector<TexturedVertextFloat> vertices = {
        // {-0.7f, -0.5f, 6.0f, 0.0f, 0.0f},
        // {0.7f, -0.25f, 6.0f, 1.0f, 0.0f},
        // {0.0f, 0.5f, 6.0f, 1.0f, 1.0f},

        {-0.7f, -0.5f, 0.0f, 0.0f, 0.0f},
        {0.7f, -0.25f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.5f, 0.0f, 1.0f, 1.0f},

        // {-0.5f, -0.5f, 5.0f},
        // {0.5f, -0.45f, 5.0f},
        // {0.0f, 0.5f, 5.0f},

    };
};

struct SimpleQuadModel
{
    SimpleQuadModel getTranslated(const Vector3DFloat &transl) const
    {
        return SimpleQuadModel{::getTranslated(transl, vertices)};
    }

    SimpleQuadModel getRotatedZ(float angle) const
    {
        return SimpleQuadModel{::getRotatedZ(angle, vertices)};
    }

    SimpleQuadModel getRotatedX(float angle) const
    {
        return SimpleQuadModel{::getRotatedX(angle, vertices)};
    }

    SimpleQuadModel getRotatedY(float angle) const
    {
        return SimpleQuadModel{::getRotatedY(angle, vertices)};
    }

    std::vector<TexturedVertextFloat> vertices = {
        {-0.5f, -0.5f, 0.0f, 0.0f, 1.0f},
        {-0.5f, 0.5f, 0.0f, 0.0f, 0.0f},
        {0.5f, -0.5f, 0.0f, 1.0f, 1.0f},

        {0.5f, -0.5f, 0.0f, 1.0f, 1.0f},
        {0.5f, 0.5f, 0.0f, 1.0f, 0.0f},
        {-0.5f, 0.5f, 0.0f, 0.0f, 0.0f},
    };
};

struct CubeModel
{
    CubeModel getTranslated(const Vector3DFloat &transl) const
    {
        return CubeModel{::getTranslated(transl, vertices)};
    }

    CubeModel getRotatedZ(float angle) const
    {
        return CubeModel{::getRotatedZ(angle, vertices)};
    }

    CubeModel getRotatedX(float angle) const
    {
        return CubeModel{::getRotatedX(angle, vertices)};
    }

    CubeModel getRotatedY(float angle) const
    {
        return CubeModel{::getRotatedY(angle, vertices)};
    }

    std::vector<TexturedVertextFloat> vertices = {
        // bottom-front
        Vector3DFloat{0.0f, 0.0f, 0.0f},
        Vector3DFloat{1.0f, 0.0f, 1.0f},
        Vector3DFloat{1.0f, 0.0f, 0.0f},

        // top-front
        Vector3DFloat{0.0f, 0.0f, 0.0f},
        Vector3DFloat{0.0f, 0.0f, 1.0f},
        Vector3DFloat{1.0f, 0.0f, 1.0f},

        // top-front
        Vector3DFloat{0.0f, 0.0f, 1.0f},
        Vector3DFloat{0.0f, 1.0f, 1.0f},
        Vector3DFloat{1.0f, 0.0f, 1.0f},

        // top-back
        Vector3DFloat{0.0f, 1.0f, 1.0f},
        Vector3DFloat{1.0f, 1.0f, 1.0f},
        Vector3DFloat{1.0f, 0.0f, 1.0f},

        // left-front
        Vector3DFloat{0.0f, 0.0f, 0.0f},
        Vector3DFloat{0.0f, 1.0f, 0.0f},
        Vector3DFloat{0.0f, 0.0f, 1.0f},

        // left-back
        Vector3DFloat{0.0f, 0.0f, 1.0f},
        Vector3DFloat{0.0f, 1.0f, 1.0f},
        Vector3DFloat{0.0f, 1.0f, 0.0f},

        // right-front
        Vector3DFloat{1.0f, 0.0f, 0.0f},
        Vector3DFloat{1.0f, 0.0f, 1.0f},
        Vector3DFloat{1.0f, 1.0f, 0.0f},

        // right-back
        Vector3DFloat{1.0f, 0.0f, 1.0f},
        Vector3DFloat{1.0f, 1.0f, 1.0f},
        Vector3DFloat{1.0f, 1.0f, 0.0f},

        // under-front
        Vector3DFloat{1.0f, 0.0f, 0.0f},
        Vector3DFloat{0.0f, 1.0f, 0.0f},
        Vector3DFloat{0.0f, 0.0f, 0.0f},

        // under-back
        Vector3DFloat{1.0f, 0.0f, 0.0f},
        Vector3DFloat{1.0f, 1.0f, 0.0f},
        Vector3DFloat{0.0f, 1.0f, 0.0f},

        // back-top
        Vector3DFloat{1.0f, 1.0f, 1.0f},
        Vector3DFloat{0.0f, 1.0f, 1.0f},
        Vector3DFloat{0.0f, 1.0f, 0.0f},

        // back-bottom
        Vector3DFloat{1.0f, 1.0f, 0.0f},
        Vector3DFloat{1.0f, 1.0f, 1.0f},
        Vector3DFloat{0.0f, 1.0f, 0.0f},
    };
};

struct Texture
{
    uint32_t m_width{0U};
    uint32_t m_height{0U};
    std::vector<int32_t> m_pixels;

    Texture(const std::string &)
    {
        // TODO: load image
        // m_pixels
        // m_pixels = PixelBuffer{width, height};
    }

    template <typename T>
    Texture(int32_t width, int32_t height, const T &colorProvider)
        : m_width(width),
          m_height(height)
    {
        m_pixels.resize(m_width * m_height);

        for (auto x = 0u; x < m_width; x++)
        {
            for (auto y = 0u; y < m_height; y++)
            {
                putPixel(x, y, colorProvider(x, y));
            }
        }
    }

    void putPixel(uint32_t x, uint32_t y, int32_t argb)
    {
        assert(x < m_width);
        assert(y < m_height);

        m_pixels[x + (y * m_width)] = argb;
    }

    int32_t getPixel(float u, float v) const
    {
        const auto u_clamped = std::max(std::min(1.0f, u), 0.0f);
        const auto v_clamped = std::max(std::min(1.0f, v), 0.0f);

        const auto x = static_cast<uint32_t>(u_clamped * (m_width - 1));
        const auto y = static_cast<uint32_t>(v_clamped * (m_height - 1));

        assert(x < m_width);
        assert(y < m_height);

        return m_pixels[x + (y * m_width)];
    }
};

struct PixelBuffer
{
    static constexpr auto MAX_FLOAT = std::numeric_limits<float>::max();

    PixelBuffer(int32_t width, int32_t height)
        : m_data(),
          m_zbuffer(),
          m_width(width),
          m_height(height)
    {
        m_data.resize(width * height);
        m_zbuffer.resize(width * height);
    }

    PixelBuffer()
        : m_width(0),
          m_height(0)
    {
    }

    virtual ~PixelBuffer() = default;

    void fillOutBuffer(void *outbuffer) const
    {
        memcpy(outbuffer, m_data.data(), m_width * m_height * sizeof(int32_t));
    }

    void cleanScreen(int32_t color = 0xFF000000)
    {
        for (auto &p : m_data)
        {
            p = color;
        }
    }

    void drawLine(const PointInt32 &point1, const PointInt32 &point2, const Texture &texture)
    {
        drawLine(point1.x, point1.y, point2.x, point2.y, texture);
    }

    void drawLine(const Vector3DFloat &point1, const Vector3DFloat &point2, const Texture &texture, const PointFloat &uv1, const PointFloat &uv2)
    {
        drawLine(point1.x, point1.y, point2.x, point2.y, texture, point1.z, point2.z, uv1.x, uv2.x, uv1.y, uv2.y);
    }

    void drawTriangle(Vector3DFloat p1, Vector3DFloat p2, Vector3DFloat p3, const Texture &texture, PointFloat uv1, PointFloat uv2, PointFloat uv3)
    {
        if (p1.y < p2.y)
        {
            std::swap(p1, p2);
            std::swap(uv1, uv2);
        }

        if (p2.y < p3.y)
        {
            std::swap(p2, p3);
            std::swap(uv2, uv3);
        }

        if (p1.y < p2.y)
        {
            std::swap(p1, p2);
            std::swap(uv1, uv2);
        }

        if (p1.y == p2.y)
        {
            drawFlatTriangle(p1, p2, p3, texture, uv1, uv2, uv3);
            return;
        }

        const auto ay = (p1.x - p3.x) / (p1.y - p3.y);
        const auto b = p3.x - ay * p3.y;

        const auto x = p2.y * ay + b;

        const auto az = (p1.z - p3.z) / (p1.y - p3.y);
        const auto bz = p3.z - az * p3.y;

        const auto z = p2.y * az + bz;

        const auto midPoint = Vector3DFloat{x, p2.y, z};

        const auto au1 = float(uv1.x - uv3.x) / (p1.y - p3.y);
        const auto bu1 = uv3.x - au1 * p3.y;

        const auto av1 = float(uv1.y - uv3.y) / (p1.y - p3.y);
        const auto bv1 = uv3.y - av1 * p3.y;

        const auto u1 = au1 * p2.y + bu1;
        const auto v1 = av1 * p2.y + bv1;

        const auto midPointUv = PointFloat{u1, v1};

        drawFlatTriangle(p2, midPoint, p1, texture, uv2, midPointUv, uv1);
        drawFlatTriangle(p2, midPoint, p3, texture, uv2, midPointUv, uv3);
    }

    void drawFlatTriangle(Vector3DFloat p1, Vector3DFloat p2, Vector3DFloat p3, const Texture &texture, const PointFloat &uv1, const PointFloat &uv2, const PointFloat &uv3)
    {
        assert(p1.y == p2.y);

        const auto ay1 = float(p1.x - p3.x) / (p1.y - p3.y);
        const auto b1 = p3.x - ay1 * p3.y;

        const auto ay2 = float(p2.x - p3.x) / (p2.y - p3.y);
        const auto b2 = p3.x - ay2 * p3.y;

        const auto az1 = float(p1.z - p3.z) / (p1.y - p3.y);
        const auto bb1 = p3.z - az1 * p3.y;

        const auto az2 = float(p2.z - p3.z) / (p2.y - p3.y);
        const auto bb2 = p3.z - az2 * p3.y;

        const auto au1 = float(uv1.x - uv3.x) / (p1.y - p3.y);
        const auto bu1 = uv3.x - au1 * p3.y;

        const auto au2 = float(uv2.x - uv3.x) / (p2.y - p3.y);
        const auto bu2 = uv3.x - au2 * p3.y;

        const auto av1 = float(uv1.y - uv3.y) / (p1.y - p3.y);
        const auto bv1 = uv3.y - av1 * p3.y;

        const auto av2 = float(uv2.y - uv3.y) / (p2.y - p3.y);
        const auto bv2 = uv3.y - av2 * p3.y;

        if (p1.y < p3.y)
        {
            for (auto y = p1.y; y < p3.y; y++)
            {
                const auto x1 = (ay1 * y + b1);
                const auto x2 = (ay2 * y + b2);
                const auto z1 = (az1 * y + bb1);
                const auto z2 = (az2 * y + bb2);

                const auto u1 = (au1 * y + bu1);
                const auto u2 = (au2 * y + bu2);

                const auto v1 = (av1 * y + bv1);
                const auto v2 = (av2 * y + bv2);

                drawLine({x1, y, z1}, {x2, y, z2}, texture, {u1, v1}, {u2, v2});
            }
        }
        else
        {
            for (auto y = p3.y; y < p1.y; y++)
            {
                const auto x1 = (ay1 * y + b1);
                const auto x2 = (ay2 * y + b2);
                const auto z1 = (az1 * y + bb1);
                const auto z2 = (az2 * y + bb2);

                const auto u1 = (au1 * y + bu1);
                const auto u2 = (au2 * y + bu2);

                const auto v1 = (av1 * y + bv1);
                const auto v2 = (av2 * y + bv2);

                drawLine({x1, y, z1}, {x2, y, z2}, texture, {u1, v1}, {u2, v2});
            }
        }
    }

    void drawLine(float x1, float y1, float x2, float y2, const Texture &texture, float z1 = 0, float z2 = 0, float u1 = 0.0f, float u2 = 0.0f, float v1 = 0.0f, float v2 = 0.0f)
    {
        const auto getZ = [z1, z2, x1, x2, y1, y2](float x, float y)
        {
            if (z1 == z2)
            {
                return z1;
            }

            if (x1 != x2)
            {
                const auto dzdx = (z2 - z1) / static_cast<float>(x2 - x1);
                const auto b = z1 - (dzdx * x1);
                return ((dzdx * x) + b);
            }

            if (y1 != y2)
            {
                const auto dzdy = (z2 - z1) / static_cast<float>(y2 - y1);
                const auto b = z1 - (dzdy * y1);
                return ((dzdy * y) + b);
            }
            // assert(z1 == z2 && "DOT?");

            return z1;
        };

        const auto getU = [z1, z2, x1, x2, y1, y2, u1, u2](float x, float y, float z)
        {
            if (u1 == u2)
            {
                return u1;
            }

            if (x1 != x2)
            {
                const auto dudx = (u2 - u1) / static_cast<float>(x2 - x1);
                const auto b = u1 - (dudx * x1);
                return ((dudx * x) + b);
            }

            if (y1 != y2)
            {
                const auto dudy = (u2 - u1) / static_cast<float>(y2 - y1);
                const auto b = u1 - (dudy * y1);
                return ((dudy * y) + b);
            }

            if (z1 != z2)
            {
                const auto dudz = (u2 - u1) / static_cast<float>(z2 - z1);
                const auto b = u1 - (dudz * z1);
                return ((dudz * z) + b);
            }

            return u1;
        };

        const auto getV = [z1, z2, x1, x2, y1, y2, v1, v2](float x, float y, float z)
        {
            if (v1 == v2)
            {
                return v1;
            }


            if (x1 != x2)
            {
                const auto dudx = (v2 - v1) / static_cast<float>(x2 - x1);
                const auto b = v1 - (dudx * x1);
                return ((dudx * x) + b);
            }

            if (y1 != y2)
            {
                const auto dudy = (v2 - v1) / static_cast<float>(y2 - y1);
                const auto b = v1 - (dudy * y1);
                return ((dudy * y) + b);
            }

            if (z1 != z2)
            {
                const auto dudz = (v2 - v1) / static_cast<float>(z2 - z1);
                const auto b = v1 - (dudz * z1);
                return ((dudz * z) + b);
            }

            return v1;
        };

        const auto getColor = [getU, getV, &texture](float x, float y, float z)
        {
            const auto u = getU(x, y, z);
            const auto v = getV(x, y, z);

            return texture.getPixel(u, v);
        };

        if (x1 == x2)
        {
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(u1, u2);
                std::swap(y1, y2);
                std::swap(v1, v2);
            }
            for (auto y = y1; y < y2; y++)
            {
                const auto z = getZ(x1, y);
                putPixel(x1, y, z, getColor(x1, y, z));
            }
            return;
        }

        if (y1 == y2)
        {
            if (x1 > x2)
            {
                std::swap(x1, x2);
                std::swap(u1, u2);
                std::swap(y1, y2);
                std::swap(v1, v2);
            }
            for (auto x = x1; x < x2; x++)
            {
                const auto z = getZ(x, y1);
                putPixel(x, y1, z, getColor(x, y1, z));
            }
            return;
        }

        const auto ax = (y1 - y2) / (x1 - x2);
        const auto ay = (x1 - x2) / (y1 - y2);

        if (fabs(ax) < fabs(ay))
        {
            if (x1 > x2)
            {
                std::swap(x1, x2);
                std::swap(u1, u2);
                std::swap(y1, y2);
                std::swap(v1, v2);
            }
            const auto b = y2 - ax * x2;
            
            for (auto x = x1; x <= x2; x++)
            {
                const auto y = static_cast<int32_t>(ax * x + b);
                const auto z = getZ(x, y);
                putPixel(x, y, z, getColor(x, y, z));
            }
        }
        else
        {
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(u1, u2);
                std::swap(y1, y2);
                std::swap(v1, v2);
            }
            const auto b = x2 - ay * y2;

            for (auto y = y1; y <= y2; y++)
            {
                const auto x = static_cast<int32_t>(ay * y + b);
                const auto z = getZ(x, y);
                putPixel(x, y, z, getColor(x, y, z));
            }
        }
    }

    void putPixel(const PointInt32 &point, int32_t color)
    {
        putPixel(point.x, point.y, color);
    }

    void putPixel(int32_t x, int32_t y, int32_t argb)
    {
        assert(x >= 0);
        assert(x < m_width);
        assert(y >= 0);
        assert(y < m_height);

        m_data[x + (y * m_width)] = argb;
    }

    void putPixel(int32_t x, int32_t y, float z, int32_t color)
    {
        const auto index = x + (y * m_width);
        if (z < m_zbuffer[index])
        {
            putPixel(x, y, color);
            m_zbuffer[index] = z;
        }
    }

    void clearZBuffer()
    {
        for (auto &zvalue : m_zbuffer)
        {
            zvalue = MAX_FLOAT;
        }
    }

    int32_t getPixel(const PointInt32 &point) const
    {
        return getPixel(point.x, point.y);
    }

    int32_t getPixel(int32_t x, int32_t y) const
    {
        assert(x >= 0);
        assert(x <= m_width);
        assert(y >= 0);
        assert(y <= m_height);
        return m_data[x + (y * m_height)];
    }

    int32_t getWidth() const
    {
        return m_width;
    }

    int32_t getHeight() const
    {
        return m_height;
    }

    void drawBuffer(const PixelBuffer &buffer, int32_t posX, int32_t posY)
    {
        for (auto i = 0; i < buffer.getWidth(); i++)
        {
            for (auto j = 0; j < buffer.getHeight(); j++)
            {
                putPixel(i + posX, j + posY, buffer.getPixel(i, j));
            }
        }
    }

protected:
    std::vector<int32_t> m_data;
    std::vector<float> m_zbuffer;
    int32_t m_width;
    int32_t m_height;
};

struct ScreenBuffer : public PixelBuffer
{
    ScreenBuffer(int32_t width, int32_t height)
        : PixelBuffer(width, height)
    {
    }

    using PixelBuffer::drawLine;
    using PixelBuffer::drawTriangle;
    using PixelBuffer::putPixel;
};

ScreenBuffer g_screenBuffer{720, 720};

void on_delete(Display *display, Window window)
{
    XDestroyWindow(display, window);
    quited = true;
}

template <typename T>
Vector3D<T> crossProduct(const Vector3D<T> &v1, const Vector3D<T> &v2)
{
    return Vector3D<T>{
        .x = (v1.y * v2.z) - (v1.z * v2.y),
        .y = (v1.z * v2.x) - (v1.x * v2.z),
        .z = (v1.x * v2.y) - (v1.y * v2.x)};
}

template <typename T>
T dotProduct(const Vector3D<T> &v1, const Vector3D<T> &v2)
{
    return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);
}

template <typename T>
Vector3D<T> calculateTriangleNormal(const Vector3D<T> &v1, const Vector3D<T> &v2, const Vector3D<T> &v3)
{
    const auto vv1 = Vector3D<T>{v2.x - v1.x, v2.y - v1.y, v2.z - v1.z};
    const auto vv2 = Vector3D<T>{v3.x - v1.x, v3.y - v1.y, v3.z - v1.z};

    return crossProduct(vv1, vv2);
}

template <typename Model_T>
void drawModel(Model_T model, float anglez, float anglex, float angley, Vector3DFloat pos, const Texture &texture, bool wireframe = false, bool backfaceCulling = true)
{
    model = model.getRotatedZ(anglez);
    model = model.getRotatedX(anglex);
    model = model.getRotatedY(angley);

    model = model.getTranslated(pos);

    assert((model.vertices.size() % 3) == 0);

    int32_t i = 0;

    for (auto itr = model.vertices.begin();
         itr != model.vertices.end(); std::advance(itr, 3))
    {
        const auto &v1 = *itr;
        const auto &v2 = *(itr + 1);
        const auto &v3 = *(itr + 2);

        const Vector3D vv1 = {v1.x, v1.y, v1.z};
        const Vector3D vv2 = {v2.x, v2.y, v2.z};
        const Vector3D vv3 = {v3.x, v3.y, v3.z};

        if (backfaceCulling)
        {
            const auto cameraToTriangle = Vector3D{v1.x - 0, v1.y - 0, v1.z - 0};
            const auto triangleNormal = calculateTriangleNormal(vv1, vv2, vv3);
            const auto dotProd = dotProduct(cameraToTriangle, triangleNormal);

            if (dotProd > 0)
            {
                i++;
                continue;
            }
        }

        const auto p1 = toScreenSpace(applyNonOrthoProj(vv1),
                                      g_screenBuffer.getWidth(), g_screenBuffer.getHeight());

        const auto p2 = toScreenSpace(applyNonOrthoProj(vv2),
                                      g_screenBuffer.getWidth(), g_screenBuffer.getHeight());

        const auto p3 = toScreenSpace(applyNonOrthoProj(vv3),
                                      g_screenBuffer.getWidth(), g_screenBuffer.getHeight());

        if (!wireframe)
        {
            g_screenBuffer.drawTriangle(p1, p2, p3, texture, {v1.u, v1.v}, {v2.u, v2.v}, {v3.u, v3.v});
        }
        // else
        // {
        //     g_screenBuffer.drawLine(p1, p2, colorIndices[i % colorIndices.size()]);
        //     g_screenBuffer.drawLine(p2, p3, colorIndices[i % colorIndices.size()]);
        //     g_screenBuffer.drawLine(p3, p1, colorIndices[i % colorIndices.size()]);
        // }
        i++;
    }
}

void drawCube(float anglez, float anglex, float angley, Vector3DFloat pos, const Texture &texture)
{
    auto model = CubeModel{};
    // model = model.getTranslated({-0.5f, -0.5f, -0.5f});
    drawModel(model, anglez, anglex, angley, pos, texture);
}

float clampAngle(float angle)
{
    if (angle > M_PI)
    {
        angle = -M_PI;
    }
    else if (angle < -M_PI)
    {
        angle = M_PI;
    }

    return angle;
}

int main(int, char **)
{
    Display *display = XOpenDisplay(NULL);
    if (NULL == display)
    {
        fprintf(stderr, "Failed to initialize display");
        return EXIT_FAILURE;
    }

    Window root = DefaultRootWindow(display);
    if (None == root)
    {
        fprintf(stderr, "No root window found");
        XCloseDisplay(display);
        return EXIT_FAILURE;
    }

    Window window = XCreateSimpleWindow(display, root, 0, 0, 800, 600, 0, 0, 0xffffffff);
    if (None == window)
    {
        fprintf(stderr, "Failed to create window");
        XCloseDisplay(display);
        return EXIT_FAILURE;
    }

    XMapWindow(display, window);

    auto gc = XCreateGC(display, window, 0, 0);

    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    const auto inputTypes = StructureNotifyMask | KeyPressMask | KeyRelease | ButtonPressMask | ButtonReleaseMask;
    XSelectInput(display, window, inputTypes);

    XEvent event;
    while (1)
    {
        XNextEvent(display, &event);
        if (event.type == MapNotify)
            break;
    }
    XFlush(display);

    const auto imgWidth = g_screenBuffer.getWidth();
    const auto imgHeight = g_screenBuffer.getHeight();

    auto pixelData = new int32_t[imgHeight * imgWidth];

    for (auto i = 0; i < imgWidth * imgHeight; i++)
    {
        pixelData[i] = 0x00000000;
    }

    XImage *pImg = XCreateImage(display, CopyFromParent, DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0,
                                (char *)pixelData, imgWidth, imgHeight, 32, imgWidth * sizeof(int32_t));

    int frames = 0;
    const int32_t greenColor = 0xFF00FF00;
    const int32_t redColor = 0xFFFF0000;
    const int32_t blueColor = 0xFF0000FF;

    int currColor = 0;

    const auto utahTeaPot = ObjModel::fromObjFile("assets/teapot.obj");

    const auto texture_width = 26.0;
    const auto texture_height = 26.0;
    const auto texture = Texture{int32_t(texture_width), int32_t(texture_height), [texture_width, texture_height](int32_t x, int32_t y)
                                 {
                                    (void)x;
                                    (void)y;
                                      const auto max_color = uint8_t{255};
                                    //   const auto r = static_cast<uint8_t>((x / texture_width) * max_color);
                                    //   const auto g = static_cast<uint8_t>((y / texture_height) * max_color);

                                    //   const auto max_mod = sqrt((texture_width * texture_width) + (texture_height * texture_height));

                                    //   const auto b = static_cast<uint8_t>(((x * x + y * y) / max_mod) * max_color);

                                    // const auto dist_x = x -(texture_width/2);
                                    // const auto dist_y = y -(texture_width / 2);

                                    // const auto dist = sqrtf((dist_x*dist_x) + (dist_y*dist_y));
                                    // const auto radius = 0.25f*texture_width;
                                    // const uint8_t r = dist < radius ? 255 : 0;
                                    const uint8_t r = (x + y) % 2 == 0 ? 255 : 0;//(y > 0.45*texture_width && y < 0.55*texture_width) || (x >= 0.45*texture_width && x < 0.55*texture_width) || (y < 0.05*texture_height) ? 255 : 0;
                                    const uint8_t g = 0;
                                    const uint8_t b = 0;

                                      const auto a = max_color;

                                      const int32_t argb = (a << 24) | (r << 16) | (g << 8) | (b << 0);

                                     return argb;
                                 }};

    const auto triangleModel = SimpleQuadModel{};

    while (!quited)
    {
        while (XPending(display) > 0)
        {
            XNextEvent(display, &event);

            switch (event.type)
            {
            case ClientMessage:
            {
                if (event.xclient.data.l[0] == (long)wm_delete_window)
                {
                    on_delete(event.xclient.display, event.xclient.window);
                }
            }
            break;
            case ButtonPress:
            case ButtonRelease:
            case EnterNotify:
            case MotionNotify:
            case LeaveNotify:
                // if(_mouseHandler)
                //     _mouseHandler->HandleInput(lDisplay, &xEvent);
                break;
            case KeyPress:
            case KeyRelease:
                // if(_keyboardHandler)
                //     _keyboardHandler->HandleInput(lDisplay, &xEvent);
                break;
            default:
                // if(_keyboardHandler)
                //     _keyboardHandler->HandleInput(lDisplay, &xEvent);
                break;
            }
            std::cout << "Got event: " << event.type << std::endl;
        }

        const auto x = imgWidth / 2;
        const auto y = imgHeight / 2;

        if (currColor == 0)
        {
            g_screenBuffer.putPixel(x, y, greenColor);
        }
        else if (currColor == 1)
        {
            g_screenBuffer.putPixel(x, y, redColor);
        }
        else if (currColor == 2)
        {
            g_screenBuffer.putPixel(x, y, blueColor);
        }

        static float anglez = 0.0f;
        // anglez += 0.01f;
        static float anglex = 3.14159f *0.0;
        anglex += 0.002f;
        static float angley = 0.0f;
        // angley += 0.01f;

        angley = clampAngle(angley);
        anglex = clampAngle(anglex);
        anglez = clampAngle(anglez);

        // drawCube(anglez, anglex, angley, {1.5f, 0.0f, 3.0f}, blueColor);
        // drawCube(anglez, anglex, angley, {0.0f, 0.0f, 3.0f}, blueColor);
        // drawModel(utahTeaPot, anglez, anglex, angley, {-2.0f, -1.5f, 9.0f}, texture);
        drawModel(triangleModel, anglez, anglex, angley, {0.2f, 0.0f, 1.2f}, texture, false, false);

        for(auto y = 0ull; y < texture.m_height; y++)
        {
            for(auto x = 0ull; x < texture.m_width; x++)
            {
                g_screenBuffer.putPixel({(int)x, (int)y}, texture.m_pixels[x + (y*texture.m_width)]);
            }
        }

        // g_screenBuffer.drawBuffer(texture.getBuffer(), 0, 0);

        if (frames > 500)
        {
            frames = 0;
            currColor++;

            if (currColor > 2)
            {
                currColor = 0;
            }
        }
        frames++;

        g_screenBuffer.fillOutBuffer(pixelData);

        g_screenBuffer.cleanScreen(0xFFFFFFFF);
        g_screenBuffer.clearZBuffer();

        pixelData[200 + (50 * g_screenBuffer.getWidth())] = redColor;
        std::ignore = XPutImage(display, window, gc, pImg, 0, 0, 0, 0, imgWidth * 4, imgHeight * 4);
    }

    XDestroyImage(pImg);

    XCloseDisplay(display);

    return 0;
}