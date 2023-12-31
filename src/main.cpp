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

using Vector3DI32 = Vector3D<int32_t>;
using Vector3DFloat = Vector3D<float>;
using Matrix3DI32 = Matrix3D<int32_t>;
using PointInt32 = Point<int32_t>;

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

std::vector<Vector3DFloat> loadObjFile(const std::string &filename)
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

    std::vector<Vector3DFloat> res;

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

std::vector<Vector3DFloat> getTranslated(const Vector3DFloat &transl, const std::vector<Vector3DFloat> &vertices)
{
    std::vector<Vector3DFloat> res = vertices;

    for (auto &vertice : res)
    {
        vertice.x += transl.x;
        vertice.y += transl.y;
        vertice.z += transl.z;
    }

    return res;
}

std::vector<Vector3DFloat> getRotatedZ(float angle, const std::vector<Vector3DFloat> &vertices)
{
    const auto cosangle = cosf(angle);
    const auto sinangle = sinf(angle);

    std::vector<Vector3DFloat> res = vertices;

    for (auto &vertice : res)
    {
        const auto nx = cosangle * vertice.x - sinangle * vertice.y;
        const auto ny = sinangle * vertice.x + cosangle * vertice.y;

        vertice.x = nx;
        vertice.y = ny;
    }

    return res;
}

std::vector<Vector3DFloat> getRotatedX(float angle, const std::vector<Vector3DFloat> &vertices)
{
    const auto cosangle = cosf(angle);
    const auto sinangle = sinf(angle);

    std::vector<Vector3DFloat> res = vertices;

    for (auto &vertice : res)
    {
        const auto ny = cosangle * vertice.y - sinangle * vertice.z;
        const auto nz = sinangle * vertice.y + cosangle * vertice.z;

        vertice.y = ny;
        vertice.z = nz;
    }

    return res;
}

std::vector<Vector3DFloat> getRotatedY(float angle, const std::vector<Vector3DFloat> &vertices)
{
    const auto cosangle = cosf(angle);
    const auto sinangle = sinf(angle);

    std::vector<Vector3DFloat> res = vertices;

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

    std::vector<Vector3DFloat> vertices = {};
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

    std::vector<Vector3DFloat> vertices = {
        {-0.7f, -0.5f, 6.0f},
        {0.7f, -0.25f, 6.0f},
        {0.0f, 0.5f, 6.0f},

        {-0.5f, -0.5f, 5.0f},
        {0.5f, -0.45f, 5.0f},
        {0.0f, 0.5f, 5.0f},

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

    std::vector<Vector3DFloat> vertices = {
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

    void drawLine(const PointInt32 &point1, const PointInt32 &point2, int32_t argb)
    {
        drawLine(point1.x, point1.y, point2.x, point2.y, argb);
    }

    void drawLine(const Vector3DFloat &point1, const Vector3DFloat &point2, int32_t argb)
    {
        drawLine(point1.x, point1.y, point2.x, point2.y, argb, point1.z, point2.z);
    }

    void drawTriangle(Vector3DFloat p1, Vector3DFloat p2, Vector3DFloat p3, int32_t argb)
    {
        if (p1.y < p2.y)
        {
            std::swap(p1, p2);
        }

        if (p2.y < p3.y)
        {
            std::swap(p2, p3);
        }

        if (p1.y < p2.y)
        {
            std::swap(p1, p2);
        }

        if (p1.y == p2.y)
        {
            drawFlatTriangle(p1, p2, p3, argb);
            return;
        }

        const auto ay = (p1.x - p3.x) / (p1.y - p3.y);
        const auto b = p3.x - ay * p3.y;

        const auto x = p2.y * ay + b;

        const auto az = (p1.z - p3.z) / (p1.y - p3.y);
        const auto bz = p3.z - az * p3.y;

        const auto z = p2.y * az + bz;

        const auto midPoint = Vector3DFloat{x, p2.y, z};

        drawFlatTriangle(p2, midPoint, p1, argb);
        drawFlatTriangle(p2, midPoint, p3, argb);
    }

    void drawFlatTriangle(Vector3DFloat p1, Vector3DFloat p2, Vector3DFloat p3, int32_t argb)
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

        if (p1.y < p3.y)
        {
            for (auto y = p1.y; y < p3.y; y++)
            {
                const auto x1 = (ay1 * y + b1);
                const auto x2 = (ay2 * y + b2);
                const auto z1 = (az1 * y + bb1);
                const auto z2 = (az2 * y + bb2);

                drawLine({x1, y, z1}, {x2, y, z2}, argb);
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

                drawLine({x1, y, z1}, {x2, y, z2}, argb);
            }
        }
    }

    void drawLine(float x1, float y1, float x2, float y2, float argb, float z1 = 0, float z2 = 0)
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
            assert(z1 == z2 && "DOT?");

            return z1;
        };
        if (x1 == x2)
        {
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            for (auto y = y1; y < y2; y++)
            {
                putPixel(x1, y, getZ(x1, y), argb);
            }
            return;
        }

        if (y1 == y2)
        {
            if (x1 > x2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            for (auto x = x1; x < x2; x++)
            {
                putPixel(x, y1, getZ(x, y1), argb);
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
                std::swap(y1, y2);
            }
            const auto b = y2 - ax * x2;

            for (auto x = x1; x <= x2; x++)
            {
                const auto y = static_cast<int32_t>(ax * x + b);
                putPixel(x, y, getZ(x, y), argb);
            }
        }
        else
        {
            if (y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            const auto b = x2 - ay * y2;

            for (auto y = y1; y <= y2; y++)
            {
                const auto x = static_cast<int32_t>(ay * y + b);
                putPixel(x, y, getZ(x, y), argb);
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
void drawModel(Model_T model, float anglez, float anglex, float angley, Vector3DFloat pos, int32_t color, bool wireframe = false, bool backfaceCulling = true)
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

        if (backfaceCulling)
        {
            const auto cameraToTriangle = Vector3D{v1.x - 0, v1.y - 0, v1.z - 0};
            const auto triangleNormal = calculateTriangleNormal(v1, v2, v3);
            const auto dotProd = dotProduct(cameraToTriangle, triangleNormal);

            if (dotProd > 0)
            {
                i++;
                continue;
            }
        }

        const auto p1 = toScreenSpace(applyNonOrthoProj(v1),
                                      g_screenBuffer.getWidth(), g_screenBuffer.getHeight());

        const auto p2 = toScreenSpace(applyNonOrthoProj(v2),
                                      g_screenBuffer.getWidth(), g_screenBuffer.getHeight());

        const auto p3 = toScreenSpace(applyNonOrthoProj(v3),
                                      g_screenBuffer.getWidth(), g_screenBuffer.getHeight());

        if (!wireframe)
        {
            g_screenBuffer.drawTriangle(p1, p2, p3, colorIndices[i % colorIndices.size()]);
        }
        else
        {
            g_screenBuffer.drawLine(p1, p2, colorIndices[i % colorIndices.size()]);
            g_screenBuffer.drawLine(p2, p3, colorIndices[i % colorIndices.size()]);
            g_screenBuffer.drawLine(p3, p1, colorIndices[i % colorIndices.size()]);
        }
        i++;
        std::ignore = color;
    }
}

void drawCube(float anglez, float anglex, float angley, Vector3DFloat pos, int32_t color)
{
    auto model = CubeModel{};
    // model = model.getTranslated({-0.5f, -0.5f, -0.5f});
    drawModel(model, anglez, anglex, angley, pos, color);
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

    const auto triangleModel = SimpleTriangleModel{};

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
        static float anglex = 0.0f;
        // anglex += 0.02f;
        static float angley = 0.0f;
        angley += 0.01f;

        angley = clampAngle(angley);
        anglex = clampAngle(anglex);
        anglez = clampAngle(anglez);

        // drawCube(anglez, anglex, angley, {1.5f, 0.0f, 3.0f}, blueColor);
        // drawCube(anglez, anglex, angley, {0.0f, 0.0f, 3.0f}, blueColor);
        drawModel(utahTeaPot, anglez, anglex, angley, {-2.0f, -1.5f, 9.0f}, redColor);
        // drawModel(triangleModel, anglez, anglex, angley, {0.0f, 0.0f, 0.0f}, redColor,false, false );

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

        g_screenBuffer.cleanScreen();
        g_screenBuffer.clearZBuffer();

        pixelData[200 + (50 * g_screenBuffer.getWidth())] = redColor;
        std::ignore = XPutImage(display, window, gc, pImg, 0, 0, 0, 0, imgWidth * 4, imgHeight * 4);
    }

    XDestroyImage(pImg);

    XCloseDisplay(display);

    return 0;
}