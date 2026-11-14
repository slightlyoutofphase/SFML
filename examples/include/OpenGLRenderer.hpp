#pragma once

#include <SFML/Window/Context.hpp>

#include <SFML/System/Vector2.hpp>
#include <SFML/System/Vector3.hpp>

#include <SFML/OpenGL.hpp>
#include <array>
#include <iostream>

#include <cstddef>
#include <cstdint>


namespace
{
constexpr GLenum VertexShader   = 0x8B31;
constexpr GLenum FragmentShader = 0x8B30;
constexpr GLenum CompileStatus  = 0x8B81;
constexpr GLenum LinkStatus     = 0x8B82;
constexpr GLenum ArrayBuffer    = 0x8892;
constexpr GLenum StaticDraw     = 0x88E4;
constexpr GLenum Texture0       = 0x84C0;

#if defined(SFML_SYSTEM_WINDOWS)
#define SFML_EXAMPLE_GL_APIENTRY APIENTRY
#else
#define SFML_EXAMPLE_GL_APIENTRY
#endif

struct OpenGLFunctions
{
#ifdef SFML_OPENGL_ES
    using ClearDepth = void(SFML_EXAMPLE_GL_APIENTRY*)(GLfloat);
#else
    using ClearDepth = void(SFML_EXAMPLE_GL_APIENTRY*)(GLdouble);
#endif
    using ClearColor               = void(SFML_EXAMPLE_GL_APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat);
    using Clear                    = void(SFML_EXAMPLE_GL_APIENTRY*)(GLbitfield);
    using Enable                   = void(SFML_EXAMPLE_GL_APIENTRY*)(GLenum);
    using DepthMask                = void(SFML_EXAMPLE_GL_APIENTRY*)(GLboolean);
    using Viewport                 = void(SFML_EXAMPLE_GL_APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
    using DrawArrays               = void(SFML_EXAMPLE_GL_APIENTRY*)(GLenum, GLint, GLsizei);
    using ActiveTexture            = void(SFML_EXAMPLE_GL_APIENTRY*)(GLenum);
    using CreateShader             = GLuint(SFML_EXAMPLE_GL_APIENTRY*)(GLenum);
    using ShaderSource             = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLsizei, const char* const*, const GLint*);
    using CompileShader            = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint);
    using GetShaderiv              = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLenum, GLint*);
    using GetShaderInfoLog         = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
    using DeleteShader             = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint);
    using CreateProgram            = GLuint(SFML_EXAMPLE_GL_APIENTRY*)();
    using AttachShader             = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLuint);
    using BindAttribLocation       = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLuint, const char*);
    using LinkProgram              = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint);
    using GetProgramiv             = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLenum, GLint*);
    using GetProgramInfoLog        = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLsizei, GLsizei*, char*);
    using DeleteProgram            = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint);
    using UseProgram               = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint);
    using GetUniformLocation       = GLint(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, const char*);
    using Uniform1i                = void(SFML_EXAMPLE_GL_APIENTRY*)(GLint, GLint);
    using Uniform2f                = void(SFML_EXAMPLE_GL_APIENTRY*)(GLint, GLfloat, GLfloat);
    using Uniform3f                = void(SFML_EXAMPLE_GL_APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat);
    using GenBuffers               = void(SFML_EXAMPLE_GL_APIENTRY*)(GLsizei, GLuint*);
    using BindBuffer               = void(SFML_EXAMPLE_GL_APIENTRY*)(GLenum, GLuint);
    using BufferData               = void(SFML_EXAMPLE_GL_APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum);
    using EnableVertexAttribArray  = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint);
    using DisableVertexAttribArray = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint);
    using VertexAttribPointer = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
    using VertexAttrib2f      = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLfloat, GLfloat);
    using VertexAttrib4f      = void(SFML_EXAMPLE_GL_APIENTRY*)(GLuint, GLfloat, GLfloat, GLfloat, GLfloat);

    ClearDepth               clearDepth{};
    ClearColor               clearColor{};
    Clear                    clear{};
    Enable                   enable{};
    DepthMask                depthMask{};
    Viewport                 viewport{};
    DrawArrays               drawArrays{};
    ActiveTexture            activeTexture{};
    CreateShader             createShader{};
    ShaderSource             shaderSource{};
    CompileShader            compileShader{};
    GetShaderiv              getShaderiv{};
    GetShaderInfoLog         getShaderInfoLog{};
    DeleteShader             deleteShader{};
    CreateProgram            createProgram{};
    AttachShader             attachShader{};
    BindAttribLocation       bindAttribLocation{};
    LinkProgram              linkProgram{};
    GetProgramiv             getProgramiv{};
    GetProgramInfoLog        getProgramInfoLog{};
    DeleteProgram            deleteProgram{};
    UseProgram               useProgram{};
    GetUniformLocation       getUniformLocation{};
    Uniform1i                uniform1i{};
    Uniform2f                uniform2f{};
    Uniform3f                uniform3f{};
    GenBuffers               genBuffers{};
    BindBuffer               bindBuffer{};
    BufferData               bufferData{};
    EnableVertexAttribArray  enableVertexAttribArray{};
    DisableVertexAttribArray disableVertexAttribArray{};
    VertexAttribPointer      vertexAttribPointer{};
    VertexAttrib2f           vertexAttrib2f{};
    VertexAttrib4f           vertexAttrib4f{};

    [[nodiscard]] bool load()
    {
        bool loaded = true;

#define SFML_EXAMPLE_LOAD_GL_FUNCTION(member, suffix) loaded = loadFunction(member, "gl" suffix) && loaded

#ifdef SFML_OPENGL_ES
        SFML_EXAMPLE_LOAD_GL_FUNCTION(clearDepth, "ClearDepthf");
#else
        SFML_EXAMPLE_LOAD_GL_FUNCTION(clearDepth, "ClearDepth");
#endif
        SFML_EXAMPLE_LOAD_GL_FUNCTION(clearColor, "ClearColor");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(clear, "Clear");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(enable, "Enable");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(depthMask, "DepthMask");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(viewport, "Viewport");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(drawArrays, "DrawArrays");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(activeTexture, "ActiveTexture");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(createShader, "CreateShader");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(shaderSource, "ShaderSource");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(compileShader, "CompileShader");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(getShaderiv, "GetShaderiv");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(getShaderInfoLog, "GetShaderInfoLog");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(deleteShader, "DeleteShader");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(createProgram, "CreateProgram");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(attachShader, "AttachShader");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(bindAttribLocation, "BindAttribLocation");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(linkProgram, "LinkProgram");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(getProgramiv, "GetProgramiv");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(getProgramInfoLog, "GetProgramInfoLog");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(deleteProgram, "DeleteProgram");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(useProgram, "UseProgram");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(getUniformLocation, "GetUniformLocation");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(uniform1i, "Uniform1i");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(uniform2f, "Uniform2f");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(uniform3f, "Uniform3f");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(genBuffers, "GenBuffers");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(bindBuffer, "BindBuffer");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(bufferData, "BufferData");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(enableVertexAttribArray, "EnableVertexAttribArray");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(disableVertexAttribArray, "DisableVertexAttribArray");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(vertexAttribPointer, "VertexAttribPointer");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(vertexAttrib2f, "VertexAttrib2f");
        SFML_EXAMPLE_LOAD_GL_FUNCTION(vertexAttrib4f, "VertexAttrib4f");

#undef SFML_EXAMPLE_LOAD_GL_FUNCTION

        return loaded;
    }

private:
    template <typename Function>
    static bool loadFunction(Function& function, const char* name)
    {
        function = reinterpret_cast<Function>(sf::Context::getFunction(name));
        return function != nullptr;
    }
};

OpenGLFunctions openGLFunctions;

bool loadOpenGLFunctions()
{
    return openGLFunctions.load();
}

GLuint compileShader(GLenum type, const char* source)
{
    const GLuint shader = openGLFunctions.createShader(type);
    if (!shader)
    {
        std::cerr << "Failed to create OpenGL example shader" << std::endl;
        return 0;
    }

    openGLFunctions.shaderSource(shader, 1, &source, nullptr);
    openGLFunctions.compileShader(shader);

    GLint compiled{};
    openGLFunctions.getShaderiv(shader, CompileStatus, &compiled);
    if (compiled)
        return shader;

    std::array<char, 1024> log{};
    openGLFunctions.getShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
    std::cerr << "Failed to compile OpenGL example shader: " << log.data() << std::endl;
    openGLFunctions.deleteShader(shader);
    return 0;
}

GLuint createProgram()
{
#ifdef SFML_OPENGL_ES
    static constexpr char vertexShaderSource[] = R"(
#version 100
attribute vec3 position;
attribute vec4 color;
attribute vec2 textureCoordinates;
uniform vec2 viewportSize;
uniform vec3 translation;
uniform vec3 rotation;
varying vec4 vertexColor;
varying vec2 vertexTextureCoordinates;
void main()
{
    vec3 p = position;
    float cz = cos(rotation.z);
    float sz = sin(rotation.z);
    p = vec3(cz * p.x - sz * p.y, sz * p.x + cz * p.y, p.z);
    float cy = cos(rotation.y);
    float sy = sin(rotation.y);
    p = vec3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);
    float cx = cos(rotation.x);
    float sx = sin(rotation.x);
    p = vec3(p.x, cx * p.y - sx * p.z, sx * p.y + cx * p.z) + translation;
    float depth = -p.z;
    gl_Position = vec4(p.x / (viewportSize.x / viewportSize.y),
                       p.y,
                       -1.004008 * p.z - 2.004008,
                       depth);
    vertexColor = color;
    vertexTextureCoordinates = textureCoordinates;
}
)";

    static constexpr char fragmentShaderSource[] = R"(
#version 100
precision mediump float;
uniform sampler2D image;
uniform int hasTexture;
varying vec4 vertexColor;
varying vec2 vertexTextureCoordinates;
void main()
{
    gl_FragColor = vertexColor * (hasTexture != 0 ? texture2D(image, vertexTextureCoordinates) : vec4(1.0));
}
)";
#else
    static constexpr char vertexShaderSource[] = R"(
#version 110
attribute vec3 position;
attribute vec4 color;
attribute vec2 textureCoordinates;
uniform vec2 viewportSize;
uniform vec3 translation;
uniform vec3 rotation;
varying vec4 vertexColor;
varying vec2 vertexTextureCoordinates;
void main()
{
    vec3 p = position;
    float cz = cos(rotation.z);
    float sz = sin(rotation.z);
    p = vec3(cz * p.x - sz * p.y, sz * p.x + cz * p.y, p.z);
    float cy = cos(rotation.y);
    float sy = sin(rotation.y);
    p = vec3(cy * p.x + sy * p.z, p.y, -sy * p.x + cy * p.z);
    float cx = cos(rotation.x);
    float sx = sin(rotation.x);
    p = vec3(p.x, cx * p.y - sx * p.z, sx * p.y + cx * p.z) + translation;
    float depth = -p.z;
    gl_Position = vec4(p.x / (viewportSize.x / viewportSize.y),
                       p.y,
                       -1.004008 * p.z - 2.004008,
                       depth);
    vertexColor = color;
    vertexTextureCoordinates = textureCoordinates;
}
)";

    static constexpr char fragmentShaderSource[] = R"(
#version 110
uniform sampler2D image;
uniform int hasTexture;
varying vec4 vertexColor;
varying vec2 vertexTextureCoordinates;
void main()
{
    gl_FragColor = vertexColor * (hasTexture != 0 ? texture2D(image, vertexTextureCoordinates) : vec4(1.0));
}
)";
#endif

    const GLuint vertexShader = compileShader(VertexShader, vertexShaderSource);
    if (!vertexShader)
        return 0;

    const GLuint fragmentShader = compileShader(FragmentShader, fragmentShaderSource);
    if (!fragmentShader)
    {
        openGLFunctions.deleteShader(vertexShader);
        return 0;
    }

    const GLuint program = openGLFunctions.createProgram();
    if (!program)
    {
        std::cerr << "Failed to create OpenGL example program" << std::endl;
        openGLFunctions.deleteShader(vertexShader);
        openGLFunctions.deleteShader(fragmentShader);
        return 0;
    }

    openGLFunctions.attachShader(program, vertexShader);
    openGLFunctions.attachShader(program, fragmentShader);
    openGLFunctions.bindAttribLocation(program, 0, "position");
    openGLFunctions.bindAttribLocation(program, 1, "color");
    openGLFunctions.bindAttribLocation(program, 2, "textureCoordinates");
    openGLFunctions.linkProgram(program);
    openGLFunctions.deleteShader(vertexShader);
    openGLFunctions.deleteShader(fragmentShader);

    GLint linked{};
    openGLFunctions.getProgramiv(program, LinkStatus, &linked);
    if (linked)
        return program;

    std::array<char, 1024> log{};
    openGLFunctions.getProgramInfoLog(program, static_cast<GLsizei>(log.size()), nullptr, log.data());
    std::cerr << "Failed to link OpenGL example shader: " << log.data() << std::endl;
    openGLFunctions.deleteProgram(program);
    return 0;
}

class OpenGLRenderer
{
public:
    [[nodiscard]] bool isCreated() const
    {
        return m_program != 0;
    }

    [[nodiscard]] bool create(const GLfloat* vertices, std::size_t vertexCount, GLsizei stride, GLint colorComponents, bool hasTextureCoordinates)
    {
        m_program = createProgram();
        if (!m_program)
            return false;

        m_viewportLocation      = openGLFunctions.getUniformLocation(m_program, "viewportSize");
        m_translationLocation   = openGLFunctions.getUniformLocation(m_program, "translation");
        m_rotationLocation      = openGLFunctions.getUniformLocation(m_program, "rotation");
        m_textureLocation       = openGLFunctions.getUniformLocation(m_program, "image");
        m_hasTextureLocation    = openGLFunctions.getUniformLocation(m_program, "hasTexture");
        m_vertexCount           = static_cast<GLsizei>(vertexCount);
        m_stride                = stride;
        m_colorComponents       = colorComponents;
        m_hasTextureCoordinates = hasTextureCoordinates;

        openGLFunctions.genBuffers(1, &m_vertexBuffer);
        if (!m_vertexBuffer)
        {
            std::cerr << "Failed to create OpenGL example vertex buffer" << std::endl;
            openGLFunctions.deleteProgram(m_program);
            m_program = 0;
            return false;
        }

        openGLFunctions.bindBuffer(ArrayBuffer, m_vertexBuffer);
        openGLFunctions.bufferData(ArrayBuffer,
                                   static_cast<std::ptrdiff_t>(vertexCount * static_cast<std::size_t>(stride)),
                                   vertices,
                                   StaticDraw);
        openGLFunctions.bindBuffer(ArrayBuffer, 0);

        return true;
    }

    void draw(sf::Vector2u viewportSize, sf::Vector3f translation, sf::Vector3f rotation, bool hasTexture) const
    {
        if (!viewportSize.x || !viewportSize.y)
            return;

        openGLFunctions.activeTexture(Texture0);
        openGLFunctions.useProgram(m_program);
        openGLFunctions.uniform2f(m_viewportLocation,
                                  static_cast<GLfloat>(viewportSize.x),
                                  static_cast<GLfloat>(viewportSize.y));
        openGLFunctions.uniform3f(m_translationLocation, translation.x, translation.y, translation.z);
        openGLFunctions.uniform3f(m_rotationLocation, rotation.x, rotation.y, rotation.z);
        openGLFunctions.uniform1i(m_textureLocation, 0);
        openGLFunctions.uniform1i(m_hasTextureLocation, hasTexture ? 1 : 0);

        openGLFunctions.bindBuffer(ArrayBuffer, m_vertexBuffer);
        openGLFunctions.enableVertexAttribArray(0);
        openGLFunctions.vertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, m_stride, nullptr);

        if (m_colorComponents > 0)
        {
            openGLFunctions.enableVertexAttribArray(1);
            openGLFunctions.vertexAttribPointer(1,
                                                m_colorComponents,
                                                GL_FLOAT,
                                                GL_FALSE,
                                                m_stride,
                                                reinterpret_cast<const void*>(3 * sizeof(GLfloat)));
        }
        else
        {
            openGLFunctions.disableVertexAttribArray(1);
            openGLFunctions.vertexAttrib4f(1, 1.f, 1.f, 1.f, 1.f);
        }

        if (m_hasTextureCoordinates)
        {
            openGLFunctions.enableVertexAttribArray(2);
            const auto offset = static_cast<std::uintptr_t>(3 + m_colorComponents) * sizeof(GLfloat);
            openGLFunctions.vertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, m_stride, reinterpret_cast<const void*>(offset));
        }
        else
        {
            openGLFunctions.disableVertexAttribArray(2);
            openGLFunctions.vertexAttrib2f(2, 0.f, 0.f);
        }

        openGLFunctions.drawArrays(GL_TRIANGLES, 0, m_vertexCount);
        openGLFunctions.disableVertexAttribArray(0);
        openGLFunctions.disableVertexAttribArray(1);
        openGLFunctions.disableVertexAttribArray(2);
        openGLFunctions.bindBuffer(ArrayBuffer, 0);
        openGLFunctions.useProgram(0);
    }

private:
    GLuint  m_program{};
    GLuint  m_vertexBuffer{};
    GLint   m_viewportLocation{};
    GLint   m_translationLocation{};
    GLint   m_rotationLocation{};
    GLint   m_textureLocation{};
    GLint   m_hasTextureLocation{};
    GLsizei m_vertexCount{};
    GLsizei m_stride{};
    GLint   m_colorComponents{};
    bool    m_hasTextureCoordinates{};
};
} // namespace
