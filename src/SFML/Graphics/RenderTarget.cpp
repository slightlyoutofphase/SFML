////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2026 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Drawable.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Graphics/GLExtensions.hpp>
#include <SFML/Graphics/Glsl.hpp>
#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexBuffer.hpp>

#include <SFML/Window/Context.hpp>
#include <SFML/Window/GlResource.hpp>

#include <SFML/System/EnumArray.hpp>
#include <SFML/System/Err.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <ostream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <cassert>
#include <cmath>
#include <cstddef>


namespace
{
// A nested named namespace is used here to allow unity builds of SFML.
namespace RenderTargetImpl
{
struct RendererResources : sf::GlResource
{
    sf::Shader               shader;
    sf::VertexBuffer         vertexBuffer{sf::VertexBuffer::Usage::Stream};
    PFNGLGENVERTEXARRAYSPROC genVertexArrays{};
    PFNGLBINDVERTEXARRAYPROC bindVertexArray{};
    GLuint                   vertexArray{};
    bool                     coreProfile{};
    bool                     initialized{};

    void registerWithContext(const std::shared_ptr<RendererResources>& self)
    {
        registerUnsharedGlObject(self);
    }
};

struct VertexAttributeState
{
    GLint enabled{};
    GLint size{};
    GLint type{};
    GLint normalized{};
    GLint stride{};
    GLint buffer{};
    void* pointer{};
};

struct GLStateSnapshot
{
    GLint                               activeTexture{};
    GLint                               program{};
    GLint                               arrayBuffer{};
    GLint                               vertexArray{};
    PFNGLBINDVERTEXARRAYPROC            bindVertexArray{};
    std::vector<GLint>                  textures;
    std::array<GLint, 4>                viewport{};
    std::array<GLint, 4>                scissorBox{};
    std::array<GLfloat, 4>              clearColor{};
    GLint                               clearStencil{};
    std::array<GLboolean, 4>            colorMask{};
    std::array<GLint, 2>                blendFactors{};
    std::array<GLint, 2>                blendFactorsAlpha{};
    std::array<GLint, 2>                blendEquations{};
    std::array<GLint, 2>                stencilFunctions{};
    std::array<GLint, 2>                stencilReferences{};
    std::array<GLint, 2>                stencilValueMasks{};
    std::array<GLint, 2>                stencilWriteMasks{};
    std::array<std::array<GLint, 3>, 2> stencilOperations{};
    std::array<VertexAttributeState, 5> vertexAttributes{};
    bool                                blendEnabled{};
    bool                                cullEnabled{};
    bool                                depthEnabled{};
    bool                                stencilEnabled{};
    bool                                scissorEnabled{};
    bool                                framebufferSrgbEnabled{};
    bool                                coreProfile{};
};

using GLStateSnapshotStackMap = std::unordered_map<std::uint64_t, std::vector<GLStateSnapshot>>;

GLStateSnapshotStackMap& getGLStateSnapshotStacks()
{
    static GLStateSnapshotStackMap stacks;
    return stacks;
}

std::mutex& getGLStateSnapshotMutex()
{
    static std::mutex mutex;
    return mutex;
}

bool isCoreProfile()
{
    return sf::priv::isCoreProfile();
}

RendererResources& getRendererResources()
{
    static std::mutex                                                          mutex;
    static std::unordered_map<std::uint64_t, std::weak_ptr<RendererResources>> resources;

    const std::uint64_t   contextId = sf::Context::getActiveContextId();
    const std::lock_guard lock(mutex);
    auto                  renderer = resources[contextId].lock();

    if (!renderer)
    {
        renderer             = std::make_shared<RendererResources>();
        resources[contextId] = renderer;
        renderer->registerWithContext(renderer);
    }

    if (!renderer->initialized)
    {
        sf::priv::ensureExtensionsInit();
        renderer->coreProfile = isCoreProfile();

#ifndef SFML_OPENGL_ES
        if (renderer->coreProfile)
        {
            renderer->genVertexArrays = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(
                sf::Context::getFunction("glGenVertexArrays"));
            renderer->bindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(
                sf::Context::getFunction("glBindVertexArray"));

            if (!renderer->genVertexArrays || !renderer->bindVertexArray)
            {
                sf::err() << "Failed to initialize OpenGL core-profile vertex array entry points" << std::endl;
                renderer->initialized = true;
                return *renderer;
            }
        }
#endif

#if defined(SFML_OPENGL_ES)
        static constexpr std::string_view vertexShader = R"(
#version 100
attribute vec2 position;
attribute vec4 color;
attribute vec2 texCoords;
uniform mat4 matrix;
uniform vec4 textureTransform;
varying vec4 vertexColor;
varying vec2 vertexTexCoords;
void main()
{
    gl_Position = matrix * vec4(position, 0.0, 1.0);
    vertexColor = color;
    vertexTexCoords = texCoords * textureTransform.xy + textureTransform.zw;
}
)";

        static constexpr std::string_view fragmentShader = R"(
#version 100
precision mediump float;
uniform sampler2D sfmlTexture;
uniform int hasTexture;
varying vec4 vertexColor;
varying vec2 vertexTexCoords;
void main()
{
    gl_FragColor = vertexColor * (hasTexture != 0 ? texture2D(sfmlTexture, vertexTexCoords) : vec4(1.0));
}
)";
#else
        static constexpr std::string_view compatibilityVertexShader = R"(
#version 110
attribute vec2 position;
attribute vec4 color;
attribute vec2 texCoords;
uniform mat4 matrix;
uniform vec4 textureTransform;
varying vec4 vertexColor;
varying vec2 vertexTexCoords;
void main()
{
    gl_Position = matrix * vec4(position, 0.0, 1.0);
    vertexColor = color;
    vertexTexCoords = texCoords * textureTransform.xy + textureTransform.zw;
}
)";

        static constexpr std::string_view compatibilityFragmentShader = R"(
#version 110
uniform sampler2D sfmlTexture;
uniform int hasTexture;
varying vec4 vertexColor;
varying vec2 vertexTexCoords;
void main()
{
    gl_FragColor = vertexColor * (hasTexture != 0 ? texture2D(sfmlTexture, vertexTexCoords) : vec4(1.0));
}
)";

        static constexpr std::string_view coreVertexShader = R"(
#version 140
in vec2 position;
in vec4 color;
in vec2 texCoords;
uniform mat4 matrix;
uniform vec4 textureTransform;
out vec4 vertexColor;
out vec2 vertexTexCoords;
void main()
{
    gl_Position = matrix * vec4(position, 0.0, 1.0);
    vertexColor = color;
    vertexTexCoords = texCoords * textureTransform.xy + textureTransform.zw;
}
)";

        static constexpr std::string_view coreFragmentShader = R"(
#version 140
uniform sampler2D sfmlTexture;
uniform int hasTexture;
in vec4 vertexColor;
in vec2 vertexTexCoords;
out vec4 fragmentColor;
void main()
{
    fragmentColor = vertexColor * (hasTexture != 0 ? texture(sfmlTexture, vertexTexCoords) : vec4(1.0));
}
)";

        const std::string_view vertexShader = renderer->coreProfile ? coreVertexShader : compatibilityVertexShader;
        const std::string_view fragmentShader = renderer->coreProfile ? coreFragmentShader : compatibilityFragmentShader;
#endif

        const bool shaderLoaded = renderer->shader.loadFromMemory(vertexShader, fragmentShader);

        if (!shaderLoaded)
        {
            sf::err() << "Failed to initialize SFML's internal renderer shader" << std::endl;
            renderer->initialized = true;
            return *renderer;
        }

        if (!renderer->vertexBuffer.create(4))
        {
            sf::err() << "Failed to initialize SFML's internal renderer vertex buffer" << std::endl;
            renderer->initialized = true;
            return *renderer;
        }

        renderer->shader.setUniform("sfmlTexture", sf::Shader::CurrentTexture);
        renderer->initialized = true;
    }

    return *renderer;
}

void pushGLStateSnapshot()
{
    GLStateSnapshot snapshot;

    snapshot.coreProfile = isCoreProfile();
    glCheck(glGetIntegerv(GL_ACTIVE_TEXTURE, &snapshot.activeTexture));

    GLint textureUnitCount = 1;
    glCheck(glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &textureUnitCount));
    snapshot.textures.resize(static_cast<std::size_t>(std::max(textureUnitCount, 1)));
    for (std::size_t i = 0; i < snapshot.textures.size(); ++i)
    {
        glCheck(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i)));
        glCheck(glGetIntegerv(GL_TEXTURE_BINDING_2D, &snapshot.textures[i]));
    }

    glCheck(glActiveTexture(static_cast<GLenum>(snapshot.activeTexture)));
    glCheck(glGetIntegerv(GL_CURRENT_PROGRAM, &snapshot.program));
    glCheck(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &snapshot.arrayBuffer));
    glCheck(glGetIntegerv(GL_VIEWPORT, snapshot.viewport.data()));
    glCheck(glGetIntegerv(GL_SCISSOR_BOX, snapshot.scissorBox.data()));
    glCheck(glGetFloatv(GL_COLOR_CLEAR_VALUE, snapshot.clearColor.data()));
    glCheck(glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &snapshot.clearStencil));
    glCheck(glGetBooleanv(GL_COLOR_WRITEMASK, snapshot.colorMask.data()));

    snapshot.blendEnabled   = glIsEnabled(GL_BLEND) == GL_TRUE;
    snapshot.cullEnabled    = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    snapshot.depthEnabled   = glIsEnabled(GL_DEPTH_TEST) == GL_TRUE;
    snapshot.stencilEnabled = glIsEnabled(GL_STENCIL_TEST) == GL_TRUE;
    snapshot.scissorEnabled = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;

    glCheck(glGetIntegerv(GL_BLEND_SRC_RGB, &snapshot.blendFactors[0]));
    glCheck(glGetIntegerv(GL_BLEND_DST_RGB, &snapshot.blendFactors[1]));
    glCheck(glGetIntegerv(GL_BLEND_SRC_ALPHA, &snapshot.blendFactorsAlpha[0]));
    glCheck(glGetIntegerv(GL_BLEND_DST_ALPHA, &snapshot.blendFactorsAlpha[1]));
    glCheck(glGetIntegerv(GL_BLEND_EQUATION_RGB, &snapshot.blendEquations[0]));
    glCheck(glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &snapshot.blendEquations[1]));

    constexpr std::array<GLenum, 2>                stencilFunctions{GL_STENCIL_FUNC, GL_STENCIL_BACK_FUNC};
    constexpr std::array<GLenum, 2>                stencilReferences{GL_STENCIL_REF, GL_STENCIL_BACK_REF};
    constexpr std::array<GLenum, 2>                stencilValueMasks{GL_STENCIL_VALUE_MASK, GL_STENCIL_BACK_VALUE_MASK};
    constexpr std::array<GLenum, 2>                stencilWriteMasks{GL_STENCIL_WRITEMASK, GL_STENCIL_BACK_WRITEMASK};
    constexpr std::array<std::array<GLenum, 3>, 2> stencilOperations{
        {{GL_STENCIL_FAIL, GL_STENCIL_PASS_DEPTH_FAIL, GL_STENCIL_PASS_DEPTH_PASS},
         {GL_STENCIL_BACK_FAIL, GL_STENCIL_BACK_PASS_DEPTH_FAIL, GL_STENCIL_BACK_PASS_DEPTH_PASS}}};

    for (std::size_t face = 0; face < stencilFunctions.size(); ++face)
    {
        glCheck(glGetIntegerv(stencilFunctions[face], &snapshot.stencilFunctions[face]));
        glCheck(glGetIntegerv(stencilReferences[face], &snapshot.stencilReferences[face]));
        glCheck(glGetIntegerv(stencilValueMasks[face], &snapshot.stencilValueMasks[face]));
        glCheck(glGetIntegerv(stencilWriteMasks[face], &snapshot.stencilWriteMasks[face]));

        for (std::size_t operation = 0; operation < stencilOperations[face].size(); ++operation)
            glCheck(glGetIntegerv(stencilOperations[face][operation], &snapshot.stencilOperations[face][operation]));
    }

#ifndef SFML_OPENGL_ES
    snapshot.framebufferSrgbEnabled = GLEXT_framebuffer_sRGB && glIsEnabled(GL_FRAMEBUFFER_SRGB) == GL_TRUE;

    if (snapshot.coreProfile)
    {
        snapshot.bindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(
            sf::Context::getFunction("glBindVertexArray"));
        glCheck(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &snapshot.vertexArray));
    }
#endif

    if (!snapshot.coreProfile)
    {
#if defined(SFML_OPENGL_ES)
        constexpr std::array<GLuint, 3> attributeIndices{0, 1, 2};
#else
        constexpr std::array<GLuint, 5> attributeIndices{0, 1, 2, 3, 8};
#endif

        for (std::size_t i = 0; i < attributeIndices.size(); ++i)
        {
            auto& attribute = snapshot.vertexAttributes[i];
            glCheck(glGetVertexAttribiv(attributeIndices[i], GL_VERTEX_ATTRIB_ARRAY_ENABLED, &attribute.enabled));
            glCheck(glGetVertexAttribiv(attributeIndices[i], GL_VERTEX_ATTRIB_ARRAY_SIZE, &attribute.size));
            glCheck(glGetVertexAttribiv(attributeIndices[i], GL_VERTEX_ATTRIB_ARRAY_TYPE, &attribute.type));
            glCheck(glGetVertexAttribiv(attributeIndices[i], GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &attribute.normalized));
            glCheck(glGetVertexAttribiv(attributeIndices[i], GL_VERTEX_ATTRIB_ARRAY_STRIDE, &attribute.stride));
            glCheck(glGetVertexAttribiv(attributeIndices[i], GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &attribute.buffer));
            glCheck(glGetVertexAttribPointerv(attributeIndices[i], GL_VERTEX_ATTRIB_ARRAY_POINTER, &attribute.pointer));
        }
    }

    const std::lock_guard lock(getGLStateSnapshotMutex());
    getGLStateSnapshotStacks()[sf::Context::getActiveContextId()].push_back(snapshot);
}

bool popGLStateSnapshot()
{
    GLStateSnapshot snapshot;
    {
        const std::lock_guard lock(getGLStateSnapshotMutex());
        auto&                 stack = getGLStateSnapshotStacks()[sf::Context::getActiveContextId()];

        if (stack.empty())
            return false;

        snapshot = stack.back();
        stack.pop_back();
    }

    if (snapshot.coreProfile && snapshot.bindVertexArray)
        glCheck(snapshot.bindVertexArray(static_cast<GLuint>(snapshot.vertexArray)));

    glCheck(glUseProgram(static_cast<GLuint>(snapshot.program)));
    for (std::size_t i = 0; i < snapshot.textures.size(); ++i)
    {
        glCheck(glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i)));
        glCheck(glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(snapshot.textures[i])));
    }

    glCheck(glActiveTexture(static_cast<GLenum>(snapshot.activeTexture)));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(snapshot.arrayBuffer)));

    if (!snapshot.coreProfile)
    {
#if defined(SFML_OPENGL_ES)
        constexpr std::array<GLuint, 3> attributeIndices{0, 1, 2};
#else
        constexpr std::array<GLuint, 5> attributeIndices{0, 1, 2, 3, 8};
#endif

        for (std::size_t i = 0; i < attributeIndices.size(); ++i)
        {
            const auto& attribute = snapshot.vertexAttributes[i];
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(attribute.buffer)));
            glCheck(glVertexAttribPointer(attributeIndices[i],
                                          attribute.size,
                                          static_cast<GLenum>(attribute.type),
                                          static_cast<GLboolean>(attribute.normalized),
                                          attribute.stride,
                                          attribute.pointer));
            if (attribute.enabled)
                glCheck(glEnableVertexAttribArray(attributeIndices[i]));
            else
                glCheck(glDisableVertexAttribArray(attributeIndices[i]));
        }

        glCheck(glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(snapshot.arrayBuffer)));
    }

    glCheck(glViewport(snapshot.viewport[0], snapshot.viewport[1], snapshot.viewport[2], snapshot.viewport[3]));
    glCheck(glScissor(snapshot.scissorBox[0], snapshot.scissorBox[1], snapshot.scissorBox[2], snapshot.scissorBox[3]));
    glCheck(glClearColor(snapshot.clearColor[0], snapshot.clearColor[1], snapshot.clearColor[2], snapshot.clearColor[3]));
    glCheck(glClearStencil(snapshot.clearStencil));
    glCheck(glColorMask(snapshot.colorMask[0], snapshot.colorMask[1], snapshot.colorMask[2], snapshot.colorMask[3]));
    glCheck(glBlendFuncSeparate(static_cast<GLenum>(snapshot.blendFactors[0]),
                                static_cast<GLenum>(snapshot.blendFactors[1]),
                                static_cast<GLenum>(snapshot.blendFactorsAlpha[0]),
                                static_cast<GLenum>(snapshot.blendFactorsAlpha[1])));
    glCheck(glBlendEquationSeparate(static_cast<GLenum>(snapshot.blendEquations[0]),
                                    static_cast<GLenum>(snapshot.blendEquations[1])));

    for (std::size_t face = 0; face < 2; ++face)
    {
        const GLenum faceEnum = face == 0 ? GL_FRONT : GL_BACK;
        glCheck(glStencilFuncSeparate(faceEnum,
                                      static_cast<GLenum>(snapshot.stencilFunctions[face]),
                                      snapshot.stencilReferences[face],
                                      static_cast<GLuint>(snapshot.stencilValueMasks[face])));
        glCheck(glStencilMaskSeparate(faceEnum, static_cast<GLuint>(snapshot.stencilWriteMasks[face])));
        glCheck(glStencilOpSeparate(faceEnum,
                                    static_cast<GLenum>(snapshot.stencilOperations[face][0]),
                                    static_cast<GLenum>(snapshot.stencilOperations[face][1]),
                                    static_cast<GLenum>(snapshot.stencilOperations[face][2])));
    }

    const auto setEnabled = [](GLenum capability, bool enabled)
    {
        if (enabled)
            glCheck(glEnable(capability));
        else
            glCheck(glDisable(capability));
    };

    setEnabled(GL_BLEND, snapshot.blendEnabled);
    setEnabled(GL_CULL_FACE, snapshot.cullEnabled);
    setEnabled(GL_DEPTH_TEST, snapshot.depthEnabled);
    setEnabled(GL_STENCIL_TEST, snapshot.stencilEnabled);
    setEnabled(GL_SCISSOR_TEST, snapshot.scissorEnabled);

#ifndef SFML_OPENGL_ES
    if (GLEXT_framebuffer_sRGB)
        setEnabled(GL_FRAMEBUFFER_SRGB, snapshot.framebufferSrgbEnabled);
#endif

    return true;
}

// Mutex to protect ID generation and our context-RenderTarget-map
std::recursive_mutex& getMutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}

// Unique identifier, used for identifying RenderTargets when
// tracking the currently active RenderTarget within a given context
std::uint64_t getUniqueId()
{
    const std::lock_guard lock(getMutex());
    static std::uint64_t  id = 1; // start at 1, zero is "no RenderTarget"
    return id++;
}

// Map to help us detect whether a different RenderTarget
// has been activated within a single context
using ContextRenderTargetMap = std::unordered_map<std::uint64_t, std::uint64_t>;
ContextRenderTargetMap& getContextRenderTargetMap()
{
    static ContextRenderTargetMap contextRenderTargetMap;
    return contextRenderTargetMap;
}

// Check if a RenderTarget with the given ID is active in the current context
bool isActive(std::uint64_t id)
{
    const auto it = getContextRenderTargetMap().find(sf::Context::getActiveContextId());
    return (it != getContextRenderTargetMap().end()) && (it->second == id);
}

// Convert an sf::BlendMode::Factor constant to the corresponding OpenGL constant.
std::uint32_t factorToGlConstant(sf::BlendMode::Factor blendFactor)
{
    // clang-format off
    switch (blendFactor)
    {
        case sf::BlendMode::Factor::Zero:             return GL_ZERO;
        case sf::BlendMode::Factor::One:              return GL_ONE;
        case sf::BlendMode::Factor::SrcColor:         return GL_SRC_COLOR;
        case sf::BlendMode::Factor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case sf::BlendMode::Factor::DstColor:         return GL_DST_COLOR;
        case sf::BlendMode::Factor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
        case sf::BlendMode::Factor::SrcAlpha:         return GL_SRC_ALPHA;
        case sf::BlendMode::Factor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case sf::BlendMode::Factor::DstAlpha:         return GL_DST_ALPHA;
        case sf::BlendMode::Factor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
    }
    // clang-format on

    sf::err() << "Invalid value for sf::BlendMode::Factor! Fallback to sf::BlendMode::Factor::Zero." << std::endl;
    assert(false);
    return GL_ZERO;
}


// Convert an sf::BlendMode::Equation constant to the corresponding OpenGL constant.
std::uint32_t equationToGlConstant(sf::BlendMode::Equation blendEquation)
{
    switch (blendEquation)
    {
        case sf::BlendMode::Equation::Add:
            return GLEXT_GL_FUNC_ADD;
        case sf::BlendMode::Equation::Subtract:
            if (GLEXT_blend_subtract)
                return GLEXT_GL_FUNC_SUBTRACT;
            break;
        case sf::BlendMode::Equation::ReverseSubtract:
            if (GLEXT_blend_subtract)
                return GLEXT_GL_FUNC_REVERSE_SUBTRACT;
            break;
        case sf::BlendMode::Equation::Min:
            if (GLEXT_blend_minmax)
                return GLEXT_GL_MIN;
            break;
        case sf::BlendMode::Equation::Max:
            if (GLEXT_blend_minmax)
                return GLEXT_GL_MAX;
            break;
    }

    static bool warned = false;
    if (!warned)
    {
        sf::err() << "OpenGL extension EXT_blend_minmax or EXT_blend_subtract unavailable" << '\n'
                  << "Some blending equations will fallback to sf::BlendMode::Equation::Add" << '\n'
                  << "Ensure that hardware acceleration is enabled if available" << std::endl;

        warned = true;
    }

    return GLEXT_GL_FUNC_ADD;
}


// Convert an UpdateOperation constant to the corresponding OpenGL constant.
std::uint32_t stencilOperationToGlConstant(sf::StencilUpdateOperation operation)
{
    // clang-format off
    switch (operation)
    {
        case sf::StencilUpdateOperation::Keep:      return GL_KEEP;
        case sf::StencilUpdateOperation::Zero:      return GL_ZERO;
        case sf::StencilUpdateOperation::Replace:   return GL_REPLACE;
        case sf::StencilUpdateOperation::Increment: return GL_INCR;
        case sf::StencilUpdateOperation::Decrement: return GL_DECR;
        case sf::StencilUpdateOperation::Invert:    return GL_INVERT;
    }
    // clang-format on

    sf::err() << "Invalid value for sf::StencilUpdateOperation! Fallback to sf::StencilMode::Keep." << std::endl;
    assert(false);
    return GL_KEEP;
}


// Convert a Comparison constant to the corresponding OpenGL constant.
std::uint32_t stencilFunctionToGlConstant(sf::StencilComparison comparison)
{
    // clang-format off
    switch (comparison)
    {
        case sf::StencilComparison::Never:        return GL_NEVER;
        case sf::StencilComparison::Less:         return GL_LESS;
        case sf::StencilComparison::LessEqual:    return GL_LEQUAL;
        case sf::StencilComparison::Greater:      return GL_GREATER;
        case sf::StencilComparison::GreaterEqual: return GL_GEQUAL;
        case sf::StencilComparison::Equal:        return GL_EQUAL;
        case sf::StencilComparison::NotEqual:     return GL_NOTEQUAL;
        case sf::StencilComparison::Always:       return GL_ALWAYS;
    }
    // clang-format on

    sf::err() << "Invalid value for sf::StencilComparison! Fallback to sf::StencilMode::Always." << std::endl;
    assert(false);
    return GL_ALWAYS;
}
} // namespace RenderTargetImpl
} // namespace


namespace sf
{
////////////////////////////////////////////////////////////
void RenderTarget::clear(Color color)
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        // Unbind texture to fix RenderTexture preventing clear
        applyTexture(nullptr);

        // Apply the view (scissor testing can affect clearing)
        if (!m_cache.enable || m_cache.viewChanged)
            applyCurrentView();

        glCheck(glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
        glCheck(glClear(GL_COLOR_BUFFER_BIT));
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::clearStencil(StencilValue stencilValue)
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        // Unbind texture to fix RenderTexture preventing clear
        applyTexture(nullptr);

        // Apply the view (scissor testing can affect clearing)
        if (!m_cache.enable || m_cache.viewChanged)
            applyCurrentView();

        glCheck(glClearStencil(static_cast<int>(stencilValue.value)));
        glCheck(glClear(GL_STENCIL_BUFFER_BIT));
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::clear(Color color, StencilValue stencilValue)
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        // Unbind texture to fix RenderTexture preventing clear
        applyTexture(nullptr);

        // Apply the view (scissor testing can affect clearing)
        if (!m_cache.enable || m_cache.viewChanged)
            applyCurrentView();

        glCheck(glClearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f));
        glCheck(glClearStencil(static_cast<int>(stencilValue.value)));
        glCheck(glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT));
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::setView(const View& view)
{
    m_view              = view;
    m_cache.viewChanged = true;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getView() const
{
    return m_view;
}


////////////////////////////////////////////////////////////
const View& RenderTarget::getDefaultView() const
{
    return m_defaultView;
}


////////////////////////////////////////////////////////////
IntRect RenderTarget::getViewport(const View& view) const
{
    const auto [width, height] = Vector2f(getSize());
    const FloatRect& viewport  = view.getViewport();

    return IntRect(Rect<long>({std::lround(width * viewport.position.x), std::lround(height * viewport.position.y)},
                              {std::lround(width * viewport.size.x), std::lround(height * viewport.size.y)}));
}


////////////////////////////////////////////////////////////
IntRect RenderTarget::getScissor(const View& view) const
{
    const auto [width, height] = Vector2f(getSize());
    const FloatRect& scissor   = view.getScissor();

    return IntRect(Rect<long>({std::lround(width * scissor.position.x), std::lround(height * scissor.position.y)},
                              {std::lround(width * scissor.size.x), std::lround(height * scissor.size.y)}));
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(Vector2i point) const
{
    return mapPixelToCoords(point, getView());
}


////////////////////////////////////////////////////////////
Vector2f RenderTarget::mapPixelToCoords(Vector2i point, const View& view) const
{
    // First, convert from viewport coordinates to homogeneous coordinates
    const FloatRect viewport = FloatRect(getViewport(view));
    const Vector2f
        normalized = Vector2f(-1, 1) +
                     Vector2f(2, -2).componentWiseMul(Vector2f(point) - viewport.position).componentWiseDiv(viewport.size);

    // Then transform by the inverse of the view matrix
    return view.getInverseTransform().transformPoint(normalized);
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(Vector2f point) const
{
    return mapCoordsToPixel(point, getView());
}


////////////////////////////////////////////////////////////
Vector2i RenderTarget::mapCoordsToPixel(Vector2f point, const View& view) const
{
    // First, transform the point by the view matrix
    const Vector2f normalized = view.getTransform().transformPoint(point);

    // Then convert to viewport coordinates
    const FloatRect viewport = FloatRect(getViewport(view));
    return Vector2i(
        (normalized.componentWiseMul({1, -1}) + sf::Vector2f(1, 1)).componentWiseDiv({2, 2}).componentWiseMul(viewport.size) +
        viewport.position);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Drawable& drawable, const RenderStates& states)
{
    drawable.draw(*this, states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const Vertex* vertices, std::size_t vertexCount, PrimitiveType type, const RenderStates& states)
{
    // Nothing to draw?
    if (!vertices || (vertexCount == 0))
        return;

    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        auto& renderer = RenderTargetImpl::getRendererResources();
        if (!setupDraw(states))
            return;

        if (!renderer.vertexBuffer.update(vertices, vertexCount, 0))
        {
            err() << "Failed to stream vertices to SFML's internal renderer buffer" << std::endl;
            return;
        }

        VertexBuffer::bind(&renderer.vertexBuffer);

#ifndef SFML_OPENGL_ES
        if (renderer.coreProfile)
        {
            if (!renderer.vertexArray)
                glCheck(renderer.genVertexArrays(1, &renderer.vertexArray));

            glCheck(renderer.bindVertexArray(renderer.vertexArray));
        }
#endif

        glCheck(glEnableVertexAttribArray(0));
        glCheck(glEnableVertexAttribArray(1));
        glCheck(glEnableVertexAttribArray(2));
        glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(0)));
        glCheck(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<const void*>(8)));
        glCheck(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(12)));

#if !defined(SFML_OPENGL_ES)
        if (!renderer.coreProfile)
        {
            glCheck(glEnableVertexAttribArray(3));
            glCheck(glEnableVertexAttribArray(8));
            glCheck(
                glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<const void*>(8)));
            glCheck(glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(12)));
        }
#endif

        drawPrimitives(type, 0, vertexCount);

        VertexBuffer::bind(nullptr);
        cleanupDraw(states);
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const VertexBuffer& vertexBuffer, const RenderStates& states)
{
    draw(vertexBuffer, 0, vertexBuffer.getVertexCount(), states);
}


////////////////////////////////////////////////////////////
void RenderTarget::draw(const VertexBuffer& vertexBuffer, std::size_t firstVertex, std::size_t vertexCount, const RenderStates& states)
{
    // VertexBuffer not supported?
    if (!VertexBuffer::isAvailable())
    {
        err() << "sf::VertexBuffer is not available, drawing skipped" << std::endl;
        return;
    }

    // Sanity check
    if (firstVertex > vertexBuffer.getVertexCount())
        return;

    // Clamp vertexCount to something that makes sense
    vertexCount = std::min(vertexCount, vertexBuffer.getVertexCount() - firstVertex);

    // Nothing to draw?
    if (!vertexCount || !vertexBuffer.getNativeHandle())
        return;

    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        if (!setupDraw(states))
            return;

        // Bind vertex buffer
        VertexBuffer::bind(&vertexBuffer);

#ifndef SFML_OPENGL_ES
        auto& renderer = RenderTargetImpl::getRendererResources();
        if (renderer.coreProfile)
        {
            if (!renderer.vertexArray)
                glCheck(renderer.genVertexArrays(1, &renderer.vertexArray));

            glCheck(renderer.bindVertexArray(renderer.vertexArray));
        }
#endif

        glCheck(glEnableVertexAttribArray(0));
        glCheck(glEnableVertexAttribArray(1));
        glCheck(glEnableVertexAttribArray(2));
        glCheck(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(0)));
        glCheck(glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<const void*>(8)));
        glCheck(glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(12)));

#if !defined(SFML_OPENGL_ES)
        if (!renderer.coreProfile)
        {
            glCheck(glEnableVertexAttribArray(3));
            glCheck(glEnableVertexAttribArray(8));
            glCheck(
                glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<const void*>(8)));
            glCheck(glVertexAttribPointer(8, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(12)));
        }
#endif

        drawPrimitives(vertexBuffer.getPrimitiveType(), firstVertex, vertexCount);

        // Unbind vertex buffer
        VertexBuffer::bind(nullptr);

        cleanupDraw(states);
    }
}


////////////////////////////////////////////////////////////
bool RenderTarget::isSrgb() const
{
    // By default sRGB encoding is not enabled for an arbitrary RenderTarget
    return false;
}


////////////////////////////////////////////////////////////
bool RenderTarget::setActive(bool active)
{
    // Mark this RenderTarget as active or no longer active in the tracking map
    const std::lock_guard lock(RenderTargetImpl::getMutex());

    const std::uint64_t contextId = Context::getActiveContextId();

    using RenderTargetImpl::getContextRenderTargetMap;
    auto&      contextRenderTargetMap = getContextRenderTargetMap();
    const auto it                     = contextRenderTargetMap.find(contextId);

    if (active)
    {
        if (it == contextRenderTargetMap.end())
        {
            contextRenderTargetMap[contextId] = m_id;

            m_cache.glStatesSet = false;
            m_cache.enable      = false;
        }
        else if (it->second != m_id)
        {
            it->second = m_id;

            m_cache.enable = false;
        }
    }
    else
    {
        if (it != contextRenderTargetMap.end())
            contextRenderTargetMap.erase(it);

        m_cache.enable = false;
    }

    return true;
}


////////////////////////////////////////////////////////////
void RenderTarget::pushGLStates()
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
#ifdef SFML_DEBUG
        // make sure that the user didn't leave an unchecked OpenGL error
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR)
        {
            err() << "OpenGL error (" << error << ") detected in user code, "
                  << "you should check for errors with glGetError()" << std::endl;
        }
#endif

        RenderTargetImpl::pushGLStateSnapshot();
    }

    resetGLStates();
}


////////////////////////////////////////////////////////////
void RenderTarget::popGLStates()
{
    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        if (!RenderTargetImpl::popGLStateSnapshot())
            err() << "sf::RenderTarget::popGLStates called without a matching pushGLStates" << std::endl;
        else
        {
            m_cache.enable      = false;
            m_cache.glStatesSet = false;
        }
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::resetGLStates()
{
    // Check here to make sure a context change does not happen after activate(true)
    const bool shaderAvailable = Shader::isAvailable();

// Workaround for states not being properly reset on
// macOS unless a context switch really takes place
#if defined(SFML_SYSTEM_MACOS)
    if (!setActive(false))
    {
        err() << "Failed to set render target inactive" << std::endl;
    }
#endif

    if (RenderTargetImpl::isActive(m_id) || setActive(true))
    {
        // Make sure that extensions are initialized
        priv::ensureExtensionsInit();

        // Make sure that the texture unit which is active is the number 0
        if (GLEXT_multitexture)
            glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0));

        // Define the default OpenGL states
        glCheck(glDisable(GL_CULL_FACE));
        glCheck(glDisable(GL_STENCIL_TEST));
        glCheck(glDisable(GL_DEPTH_TEST));
        glCheck(glDisable(GL_SCISSOR_TEST));
        glCheck(glEnable(GL_BLEND));
        glCheck(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
        m_cache.scissorEnabled = false;
        m_cache.stencilEnabled = false;
        m_cache.glStatesSet    = true;

        // Apply the default SFML states
        applyBlendMode(BlendAlpha);
        applyStencilMode(StencilMode());
        applyTexture(nullptr);
        if (shaderAvailable)
            applyShader(nullptr);

        // Set the default view
        setView(getView());

        m_cache.enable = true;
    }
}


////////////////////////////////////////////////////////////
void RenderTarget::initialize()
{
    // Setup the default and current views
    m_defaultView = View(FloatRect({0, 0}, Vector2f(getSize())));
    m_view        = m_defaultView;

    // Set GL states only on first draw, so that we don't pollute user's states
    m_cache.glStatesSet = false;

    // Generate a unique ID for this RenderTarget to track
    // whether it is active within a specific context
    m_id = RenderTargetImpl::getUniqueId();
}


////////////////////////////////////////////////////////////
void RenderTarget::applyCurrentView()
{
    // Set the viewport
    const IntRect viewport    = getViewport(m_view);
    const int     viewportTop = static_cast<int>(getSize().y) - (viewport.position.y + viewport.size.y);
    glCheck(glViewport(viewport.position.x, viewportTop, viewport.size.x, viewport.size.y));

    // Set the scissor rectangle and enable/disable scissor testing
    if (m_view.getScissor() == FloatRect({0, 0}, {1, 1}))
    {
        if (!m_cache.enable || m_cache.scissorEnabled)
        {
            glCheck(glDisable(GL_SCISSOR_TEST));
            m_cache.scissorEnabled = false;
        }
    }
    else
    {
        const IntRect pixelScissor = getScissor(m_view);
        const int     scissorTop   = static_cast<int>(getSize().y) - (pixelScissor.position.y + pixelScissor.size.y);
        glCheck(glScissor(pixelScissor.position.x, scissorTop, pixelScissor.size.x, pixelScissor.size.y));

        if (!m_cache.enable || !m_cache.scissorEnabled)
        {
            glCheck(glEnable(GL_SCISSOR_TEST));
            m_cache.scissorEnabled = true;
        }
    }

    m_cache.viewChanged = false;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyBlendMode(const BlendMode& mode)
{
    using RenderTargetImpl::equationToGlConstant;
    using RenderTargetImpl::factorToGlConstant;

    // Apply the blend mode, falling back to the non-separate versions if necessary
    if (GLEXT_blend_func_separate)
    {
        glCheck(GLEXT_glBlendFuncSeparate(factorToGlConstant(mode.colorSrcFactor),
                                          factorToGlConstant(mode.colorDstFactor),
                                          factorToGlConstant(mode.alphaSrcFactor),
                                          factorToGlConstant(mode.alphaDstFactor)));
    }
    else
    {
        glCheck(glBlendFunc(factorToGlConstant(mode.colorSrcFactor), factorToGlConstant(mode.colorDstFactor)));
    }

    if (GLEXT_blend_minmax || GLEXT_blend_subtract)
    {
        if (GLEXT_blend_equation_separate)
        {
            glCheck(GLEXT_glBlendEquationSeparate(equationToGlConstant(mode.colorEquation),
                                                  equationToGlConstant(mode.alphaEquation)));
        }
        else
        {
            glCheck(GLEXT_glBlendEquation(equationToGlConstant(mode.colorEquation)));
        }
    }
    else if ((mode.colorEquation != BlendMode::Equation::Add) || (mode.alphaEquation != BlendMode::Equation::Add))
    {
        static bool warned = false;

        if (!warned)
        {
#ifdef SFML_OPENGL_ES
            err() << "OpenGL ES extension OES_blend_subtract unavailable" << std::endl;
#else
            err() << "OpenGL extension EXT_blend_minmax and EXT_blend_subtract unavailable" << std::endl;
#endif
            err() << "Selecting a blend equation not possible" << '\n'
                  << "Ensure that hardware acceleration is enabled if available" << std::endl;

            warned = true;
        }
    }

    m_cache.lastBlendMode = mode;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyStencilMode(const StencilMode& mode)
{
    using RenderTargetImpl::stencilFunctionToGlConstant;
    using RenderTargetImpl::stencilOperationToGlConstant;

    // Fast path if we have a default (disabled) stencil mode
    if (mode == StencilMode())
    {
        if (!m_cache.enable || m_cache.stencilEnabled)
        {
            glCheck(glDisable(GL_STENCIL_TEST));
            glCheck(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

            m_cache.stencilEnabled = false;
        }
    }
    else
    {
        // Apply the stencil mode
        if (!m_cache.enable || !m_cache.stencilEnabled)
            glCheck(glEnable(GL_STENCIL_TEST));

        glCheck(glStencilOp(GL_KEEP,
                            stencilOperationToGlConstant(mode.stencilUpdateOperation),
                            stencilOperationToGlConstant(mode.stencilUpdateOperation)));
        glCheck(glStencilFunc(stencilFunctionToGlConstant(mode.stencilComparison),
                              static_cast<int>(mode.stencilReference.value),
                              mode.stencilMask.value));

        m_cache.stencilEnabled = true;
    }

    m_cache.lastStencilMode = mode;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyTexture(const Texture* texture, CoordinateType coordinateType)
{
    Texture::bindForRendering(texture);

    m_cache.lastTextureId      = texture ? texture->m_cacheId : 0;
    m_cache.lastCoordinateType = coordinateType;
}


////////////////////////////////////////////////////////////
void RenderTarget::applyShader(const Shader* shader)
{
    Shader::bind(shader);
}


////////////////////////////////////////////////////////////
bool RenderTarget::setupDraw(const RenderStates& states)
{
    // GL_FRAMEBUFFER_SRGB is not available on OpenGL ES
    // If a framebuffer supports sRGB, it will always be enabled on OpenGL ES
#ifndef SFML_OPENGL_ES
    // Enable or disable sRGB encoding
    // This is needed for drivers that do not check the format of the surface drawn to before applying sRGB conversion
    if (!m_cache.enable)
    {
        if (isSrgb())
            glCheck(glEnable(GL_FRAMEBUFFER_SRGB));
        else if (GLEXT_framebuffer_sRGB)
            glCheck(glDisable(GL_FRAMEBUFFER_SRGB));
    }
#endif

    // First set the persistent OpenGL states if it's the very first call
    if (!m_cache.glStatesSet)
        resetGLStates();

    // Apply the view
    if (!m_cache.enable || m_cache.viewChanged)
        applyCurrentView();

    // Apply the blend mode
    if (!m_cache.enable || (states.blendMode != m_cache.lastBlendMode))
        applyBlendMode(states.blendMode);

    // Apply the stencil mode
    if (!m_cache.enable || (states.stencilMode != m_cache.lastStencilMode))
        applyStencilMode(states.stencilMode);

    // Mask the color buffer off if necessary
    if (states.stencilMode.stencilOnly)
        glCheck(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));

    // Apply the texture
    if (!m_cache.enable || (states.texture && states.texture->m_fboAttachment))
    {
        // If the texture is an FBO attachment, always rebind it
        // in order to inform the OpenGL driver that we want changes
        // made to it in other contexts to be visible here as well
        // This saves us from having to call glFlush() in
        // RenderTextureImplFBO which can be quite costly
        // See: https://www.khronos.org/opengl/wiki/Memory_Model
        applyTexture(states.texture, states.coordinateType);
    }
    else
    {
        const std::uint64_t textureId = states.texture ? states.texture->m_cacheId : 0;
        if (textureId != m_cache.lastTextureId || states.coordinateType != m_cache.lastCoordinateType)
            applyTexture(states.texture, states.coordinateType);
    }

    const Transform matrix = m_view.getTransform() * states.transform;

    Glsl::Vec4 textureTransform(1.f, 1.f, 0.f, 0.f);
    if (states.texture)
    {
        const Texture& texture = *states.texture;
        if (states.coordinateType == CoordinateType::Pixels)
        {
            textureTransform.x = 1.f / static_cast<float>(texture.m_actualSize.x);
            textureTransform.y = 1.f / static_cast<float>(texture.m_actualSize.y);
        }
        else if (texture.m_size != texture.m_actualSize)
        {
            textureTransform.x = static_cast<float>(texture.m_size.x) / static_cast<float>(texture.m_actualSize.x);
            textureTransform.y = static_cast<float>(texture.m_size.y) / static_cast<float>(texture.m_actualSize.y);
        }

        if (texture.m_pixelsFlipped)
        {
            textureTransform.y = -textureTransform.y;
            textureTransform.w = static_cast<float>(texture.m_size.y) / static_cast<float>(texture.m_actualSize.y);
        }
    }

    // Apply the shader
    if (states.shader)
    {
        applyShader(states.shader);
        states.shader->setUniformIfPresent("matrix", Glsl::Mat4(matrix));
        states.shader->setUniformIfPresent("textureTransform", textureTransform);
    }
    else
    {
        auto& renderer = RenderTargetImpl::getRendererResources();
        if (!renderer.shader.getNativeHandle())
            return false;

        renderer.shader.setUniform("matrix", Glsl::Mat4(matrix));
        renderer.shader.setUniform("hasTexture", states.texture ? 1 : 0);
        renderer.shader.setUniform("textureTransform", textureTransform);
        applyShader(&renderer.shader);
    }

    return true;
}


////////////////////////////////////////////////////////////
void RenderTarget::drawPrimitives(PrimitiveType type, std::size_t firstVertex, std::size_t vertexCount)
{
    // Find the OpenGL primitive type
    static constexpr priv::EnumArray<PrimitiveType, GLenum, 6> modes =
        {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
    const GLenum mode = modes[type];

    // Draw the primitives
    glCheck(glDrawArrays(mode, static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount)));
}


////////////////////////////////////////////////////////////
void RenderTarget::cleanupDraw(const RenderStates& states)
{
    // Unbind the shader
    applyShader(nullptr);

    // If the texture we used to draw belonged to a RenderTexture, then forcibly unbind that texture.
    // This prevents a bug where some drivers do not clear RenderTextures properly.
    if (states.texture && states.texture->m_fboAttachment)
        applyTexture(nullptr);

    // Mask the color buffer back on if necessary
    if (states.stencilMode.stencilOnly)
        glCheck(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

    // Re-enable the cache at the end of the draw if it was disabled
    m_cache.enable = true;
}

} // namespace sf


////////////////////////////////////////////////////////////
// Render states caching strategies
//
// * View
//   If SetView was called since last draw, the view uniform
//   is updated. We don't need more, the view doesn't change
//   frequently.
//
// * Transform
//   The transform matrix is usually expensive because each
//   entity will most likely use a different transform. This can
//   lead, in worst case, to changing it every 4 vertices.
//   To avoid that, when the vertex count is low enough, we
//   pre-transform them and therefore use an identity transform
//   in the shader.
//
// * Blending mode
//   Since it overloads the == operator, we can easily check
//   whether any of the 6 blending components changed and,
//   thus, whether we need to update the blend mode.
//
// * Texture
//   Storing the pointer or OpenGL ID of the last used texture
//   is not enough; if the sf::Texture instance is destroyed,
//   both the pointer and the OpenGL ID might be recycled in
//   a new texture instance. We need to use our own unique
//   identifier system to ensure consistent caching.
//
// * Shader
//   Shaders are very hard to optimize, because they have
//   parameters that can be hard (if not impossible) to track,
//   like matrices or textures. The only optimization that we
//   do is that we avoid setting a null shader if there was
//   already none for the previous draw.
//
////////////////////////////////////////////////////////////
