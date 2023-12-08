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

bool quited = false;

template<typename T>
struct Matrix3D
{
    T data[3][3];
};

template<typename T>
struct Point
{
    T x;
    T y;
};

template<typename T>
struct Vector3D
{
    Point<T> as2DPoint() const
    {
        return Point<T>{.x=x, .y=y};
    }

    T x;
    T y;
    T z;
};

using Vector3DI32 = Vector3D<int32_t>;
using Vector3DFloat = Vector3D<float>;
using Matrix3DI32 = Matrix3D<int32_t>;
using PointInt32 = Point<int32_t>;

Vector3DI32 toScreenSpace(const Vector3DFloat& vec, int32_t screenWidth, int32_t screenHeight)
{
    return {
        .x = static_cast<int32_t>((vec.x + 1.0f) * (screenWidth/2.0f)),
        .y = static_cast<int32_t>((-vec.y + 1.0f) * (screenHeight/2.0f)),
        .z = static_cast<int32_t>(vec.z)
    };
}

Vector3DFloat applyNonOrthoProj(const Vector3DFloat& vec)
{
    return {
        .x = vec.x / vec.z,
        .y = vec.y / vec.z,
        .z = vec.z
    };
}

struct CubeModel
{
    CubeModel getTranslated(const Vector3DFloat& transl)
    {
        CubeModel res{
            .vertices = this->vertices
        };

        for(auto& vertice : res.vertices)
        {
            vertice.x += transl.x;
            vertice.y += transl.y;
            vertice.z += transl.z;
        }

        return res;
    }

    CubeModel getRotatedZ(float angle)
    {
        const auto cosangle = cosf(angle);
        const auto sinangle = sinf(angle);

        CubeModel res {
            this->vertices
        };

        for(auto& vertice : res.vertices)
        {
            const auto nx = cosangle*vertice.x - sinangle * vertice.y;
            const auto ny = sinangle*vertice.x + cosangle * vertice.y;

            vertice.x = nx;
            vertice.y = ny;
        }

        return res;
    }

    CubeModel getRotatedX(float angle)
    {
        const auto cosangle = cosf(angle);
        const auto sinangle = sinf(angle);

        CubeModel res {
            this->vertices
        };

        for(auto& vertice : res.vertices)
        {
            const auto ny = cosangle*vertice.y - sinangle * vertice.z;
            const auto nz = sinangle*vertice.y + cosangle * vertice.z;

            vertice.y = ny;
            vertice.z = nz;
        }

        return res;
    }

    CubeModel getRotatedY(float angle)
    {
        const auto cosangle = cosf(angle);
        const auto sinangle = sinf(angle);

        CubeModel res {
            this->vertices
        };

        for(auto& vertice : res.vertices)
        {
            const auto nx = cosangle*vertice.x + sinangle * vertice.z;
            const auto nz = -sinangle*vertice.x + cosangle * vertice.z;

            vertice.x = nx;
            vertice.z = nz;
        }

        return res;
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

struct ScreenBuffer
{
    ScreenBuffer(int32_t width, int32_t height)
        :
        m_data(),
        m_width(width),
        m_height(height)
    {
        m_data.resize(width*height);
    }

    ~ScreenBuffer() = default;

    void fillOutBuffer(void* outbuffer) const
    {
        memcpy(outbuffer, m_data.data(), m_width * m_height * sizeof(int32_t));
    }
    
    void cleanScreen(int32_t color = 0xFF000000)
    {
        for(auto& p : m_data)
        {
            p = color;
        }
    }

    void drawLine(const PointInt32& point1, const PointInt32& point2, int32_t argb)
    {
        drawLine(point1.x, point1.y, point2.x, point2.y, argb);
    }

    void drawLine(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t argb)
    {
        if(x1 == x2)
        {
            if(y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            for(auto y = y1; y < y2; y++)
            {
                putPixel(x1, y, argb);
            }
            return;
        }

        if(y1 == y2)
        {
            if(x1 > x2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            for(auto x = x1; x < x2; x++)
            {
                putPixel(x, y1, argb);
            }
            return;
        }

        const auto ax = float(y1 - y2)/(x1 - x2);
        const auto ay = float(x1 - x2)/(y1 - y2);
        
        if(fabs(ax) < fabs(ay))
        {
            if(x1 > x2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            const auto b = y2 - ax*x2;

            for(auto x = x1; x <= x2; x++)
            {
                const auto y = static_cast<int32_t>(ax*x + b);
                putPixel(x, y, argb);
            }
        }
        else
        {
            if(y1 > y2)
            {
                std::swap(x1, x2);
                std::swap(y1, y2);
            }
            const auto b = x2 - ay*y2;

            for(auto y = y1; y <= y2; y++)
            {
                const auto x = static_cast<int32_t>(ay*y + b);
                putPixel(x, y, argb);
            }
        }
    }

    void putPixel(const PointInt32& point, int32_t color)
    {
        putPixel(point.x, point.y, color);
    }

    void putPixel(int32_t x, int32_t y, int32_t argb)
    {
        assert(x >= 0);
        assert(x < m_width);
        assert(y >= 0);
        assert(y < m_height);

        m_data[x + (y*m_width)] = argb;
    }

    int32_t getWidth() const
    {
        return m_width;
    }

    int32_t getHeight() const
    {
        return m_height;
    }

private:
    std::vector<int32_t> m_data;
    int32_t m_width;
    int32_t m_height;
};

ScreenBuffer g_screenBuffer{500, 500};

void on_delete(Display *display, Window window)
{
    XDestroyWindow(display, window);
    quited = true;
}

void drawCube(float anglez, float anglex, float angley, Vector3DFloat pos, int32_t color)
{
    CubeModel cb;
    cb = cb.getTranslated({-0.5f, -0.5f, -0.5f});

    cb = cb.getRotatedZ(anglez);
    cb = cb.getRotatedX(anglex);
    cb = cb.getRotatedY(angley);

    cb = cb.getTranslated(pos);

    assert((cb.vertices.size() % 3) == 0);

    for(auto itr = cb.vertices.begin();
        itr != cb.vertices.end(); std::advance(itr, 3))
    {
        const auto& v1 = *itr;
        const auto& v2 = *(itr + 1);
        const auto& v3 = *(itr + 2);

        const auto p1 = toScreenSpace(applyNonOrthoProj(v1),
            g_screenBuffer.getWidth(), g_screenBuffer.getHeight()).as2DPoint();
        
        const auto p2 = toScreenSpace(applyNonOrthoProj(v2),
            g_screenBuffer.getWidth(), g_screenBuffer.getHeight()).as2DPoint();

        const auto p3 = toScreenSpace(applyNonOrthoProj(v3),
            g_screenBuffer.getWidth(), g_screenBuffer.getHeight()).as2DPoint();


        g_screenBuffer.drawLine(p1, p2, color);
        g_screenBuffer.drawLine(p2, p3, color);
        g_screenBuffer.drawLine(p3, p1, color);
    }
}

float clampAngle(float angle)
{
    if(angle > M_PI)
    {
        angle = -M_PI;
    }
    else if(angle < -M_PI)
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

        // g_screenBuffer.drawLine({0, 0}, {100, 100}, greenColor);
        // g_screenBuffer.drawLine({0, 0}, {100, 20}, redColor);
        // g_screenBuffer.drawLine({0, 0}, {100, 400}, blueColor);

        static float anglez = 0.0f;
        anglez += 0.01f;
        static float anglex = 0.0f;
        // anglex += 0.02f;
        static float angley = 0.0f;
        // angley += 0.03f;

        angley = clampAngle(angley);
        anglex = clampAngle(anglex);
        anglez = clampAngle(anglez);

        drawCube(anglez, anglex, angley, {0.0f, 0.0f, 2.0f}, redColor);

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

        pixelData[200 + (50 * g_screenBuffer.getWidth())] = redColor;
        std::ignore = XPutImage(display, window, gc, pImg, 0, 0, 0, 0, imgWidth * 4, imgHeight * 4);
    }

    XDestroyImage(pImg);

    XCloseDisplay(display);

    return 0;
}