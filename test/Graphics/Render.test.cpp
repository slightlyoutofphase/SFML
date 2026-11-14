#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/StencilMode.hpp>
#include <SFML/Graphics/Texture.hpp>

#include <SFML/Window/Context.hpp>

#include <catch2/catch_test_macros.hpp>

#include <GraphicsUtil.hpp>
#include <SFML/OpenGL.hpp>
#include <WindowUtil.hpp>
#include <array>
#include <string_view>

TEST_CASE("[Graphics] Render Tests", runDisplayTests())
{
#if !defined(SFML_OPENGL_ES)
    SECTION("Compatibility rendering leaves fixed-function matrices untouched")
    {
        sf::ContextSettings settings;
        settings.majorVersion = 2;
        settings.minorVersion = 1;

        sf::RenderTexture renderTexture;
        if (!renderTexture.resize({32, 32}, settings))
        {
            WARN("OpenGL 2.1 compatibility context is unavailable");
            return;
        }

        REQUIRE(renderTexture.setActive(true));

        constexpr std::array<GLenum, 3> matrixModes{GL_MODELVIEW, GL_PROJECTION, GL_TEXTURE};
        constexpr std::array<GLenum, 3> matrixDepths{GL_MODELVIEW_STACK_DEPTH, GL_PROJECTION_STACK_DEPTH, GL_TEXTURE_STACK_DEPTH};
        std::array<GLint, 3>                   originalStackDepths{};
        std::array<std::array<GLfloat, 16>, 3> originalMatrices{};

        GLint originalMatrixMode{};
        glGetIntegerv(GL_MATRIX_MODE, &originalMatrixMode);
        for (std::size_t i = 0; i < matrixModes.size(); ++i)
        {
            glGetIntegerv(matrixDepths[i], &originalStackDepths[i]);
            glMatrixMode(matrixModes[i]);
            glGetFloatv(matrixModes[i] == GL_MODELVIEW    ? GL_MODELVIEW_MATRIX
                        : matrixModes[i] == GL_PROJECTION ? GL_PROJECTION_MATRIX
                                                          : GL_TEXTURE_MATRIX,
                        originalMatrices[i].data());
        }
        glMatrixMode(static_cast<GLenum>(originalMatrixMode));

        sf::Image   image({2, 2}, sf::Color::White);
        sf::Texture texture(image);
        glMatrixMode(GL_TEXTURE);
        sf::Texture::bind(&texture, sf::CoordinateType::Pixels);
        GLint matrixModeAfterBind{};
        glGetIntegerv(GL_MATRIX_MODE, &matrixModeAfterBind);
        CHECK(matrixModeAfterBind == GL_TEXTURE);
        glMatrixMode(static_cast<GLenum>(originalMatrixMode));

        GLint textureBinding{};
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &textureBinding);
        CHECK(textureBinding == static_cast<GLint>(texture.getNativeHandle()));

        sf::RectangleShape shape({32, 32});
        renderTexture.pushGLStates();

        std::array<GLint, 3> pushedStackDepths{};
        glGetIntegerv(GL_MODELVIEW_STACK_DEPTH, &pushedStackDepths[0]);
        glGetIntegerv(GL_PROJECTION_STACK_DEPTH, &pushedStackDepths[1]);
        glGetIntegerv(GL_TEXTURE_STACK_DEPTH, &pushedStackDepths[2]);
        CHECK(pushedStackDepths == originalStackDepths);

        renderTexture.draw(shape);
        renderTexture.popGLStates();

        std::array<GLint, 3>                   finalStackDepths{};
        std::array<std::array<GLfloat, 16>, 3> finalMatrices{};
        glGetIntegerv(GL_MATRIX_MODE, &originalMatrixMode);
        for (std::size_t i = 0; i < matrixModes.size(); ++i)
        {
            glGetIntegerv(matrixDepths[i], &finalStackDepths[i]);
            glMatrixMode(matrixModes[i]);
            glGetFloatv(matrixModes[i] == GL_MODELVIEW    ? GL_MODELVIEW_MATRIX
                        : matrixModes[i] == GL_PROJECTION ? GL_PROJECTION_MATRIX
                                                          : GL_TEXTURE_MATRIX,
                        finalMatrices[i].data());
        }
        glMatrixMode(static_cast<GLenum>(originalMatrixMode));

        CHECK(finalStackDepths == originalStackDepths);
        CHECK(finalMatrices == originalMatrices);

        sf::Image   blueImage({1, 1}, sf::Color::Blue);
        sf::Texture blueTexture(blueImage);
        blueTexture.setRepeated(true);
        blueTexture.setRepeated(false);

        glBindTexture(GL_TEXTURE_2D, blueTexture.getNativeHandle());
        GLint textureWrap{};
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &textureWrap);
        CHECK(textureWrap == 0x812F); // GL_CLAMP_TO_EDGE

        const std::string_view fragmentShader = R"(
uniform sampler2D image;
void main()
{
    gl_FragColor = texture2D(image, gl_TexCoord[0].xy) * gl_Color;
}
)";
        sf::Shader             shader(fragmentShader, sf::Shader::Type::Fragment);
        shader.setUniform("image", sf::Shader::CurrentTexture);

        shape.setSize({8, 8});
        shape.setPosition({8, 8});
        shape.setTexture(&blueTexture);
        sf::RenderStates states;
        states.shader = &shader;

        renderTexture.clear(sf::Color::Black);
        renderTexture.draw(shape, states);
        renderTexture.display();
        CHECK(renderTexture.getTexture().copyToImage().getPixel({12, 12}) == sf::Color::Blue);
        CHECK(renderTexture.getTexture().copyToImage().getPixel({4, 4}) == sf::Color::Black);
    }
#endif

    SECTION("Core-profile rendering")
    {
        sf::Context compatibilityContext;
        REQUIRE(compatibilityContext.setActive(true));
        REQUIRE(sf::Shader::isAvailable());

        sf::ContextSettings settings;
        settings.majorVersion   = 3;
        settings.minorVersion   = 2;
        settings.attributeFlags = sf::ContextSettings::Core;

        sf::RenderTexture renderTexture;
        if (!renderTexture.resize({32, 32}, settings))
        {
            WARN("Core-profile OpenGL 3.2 context is unavailable");
            return;
        }

        renderTexture.clear(sf::Color::Black);

        sf::RectangleShape shape({32, 32});
        shape.setFillColor(sf::Color::Green);
        renderTexture.draw(shape);
        renderTexture.display();

        CHECK(renderTexture.getTexture().copyToImage().getPixel({16, 16}) == sf::Color::Green);

        sf::Image   image({1, 1}, sf::Color::Blue);
        sf::Texture texture(image);
        shape.setFillColor(sf::Color::White);
        shape.setTexture(&texture);

        renderTexture.clear(sf::Color::Black);
        renderTexture.draw(shape);
        renderTexture.display();
        CHECK(renderTexture.getTexture().copyToImage().getPixel({16, 16}) == sf::Color::Blue);

        const std::string_view vertexShader   = R"(
#version 150
in vec2 position;
in vec4 color;
in vec2 texCoords;
uniform mat4 matrix;
uniform vec4 textureTransform;
out vec2 textureCoordinate;
void main()
{
    gl_Position = matrix * vec4(position, 0.0, 1.0);
    textureCoordinate = texCoords * textureTransform.xy + textureTransform.zw;
}
)";
        const std::string_view fragmentShader = R"(
#version 150
uniform sampler2D image;
in vec2 textureCoordinate;
out vec4 fragmentColor;
void main()
{
    fragmentColor = texture(image, textureCoordinate);
}
)";
        sf::Shader             shader(vertexShader, fragmentShader);
        shader.setUniform("image", texture);

        sf::RenderStates states;
        states.shader = &shader;
        renderTexture.clear(sf::Color::Black);
        renderTexture.draw(shape, states);
        renderTexture.display();

        CHECK(renderTexture.getTexture().copyToImage().getPixel({16, 16}) == sf::Color::Blue);

        glDisable(GL_BLEND);
        glEnable(GL_CULL_FACE);
        glEnable(GL_SCISSOR_TEST);
        glViewport(2, 3, 8, 9);
        glScissor(4, 5, 6, 7);
        glClearColor(0.25f, 0.5f, 0.75f, 0.125f);
        glClearStencil(42);

        renderTexture.pushGLStates();
        renderTexture.clear(sf::Color::Red, 127);
        renderTexture.draw(shape);
        renderTexture.popGLStates();

        CHECK(glIsEnabled(GL_BLEND) == GL_FALSE);
        CHECK(glIsEnabled(GL_CULL_FACE) == GL_TRUE);
        CHECK(glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE);

        std::array<GLint, 4> viewport{};
        std::array<GLint, 4> scissorBox{};
        glGetIntegerv(GL_VIEWPORT, viewport.data());
        glGetIntegerv(GL_SCISSOR_BOX, scissorBox.data());
        CHECK((viewport == std::array<GLint, 4>{2, 3, 8, 9}));
        CHECK((scissorBox == std::array<GLint, 4>{4, 5, 6, 7}));

        std::array<GLfloat, 4> clearColor{};
        GLint                  clearStencil{};
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor.data());
        glGetIntegerv(GL_STENCIL_CLEAR_VALUE, &clearStencil);
        CHECK(clearColor == std::array<GLfloat, 4>{0.25f, 0.5f, 0.75f, 0.125f});
        CHECK(clearStencil == 42);
    }

    SECTION("Renderer context resources follow context lifetime")
    {
        for (int i = 0; i < 3; ++i)
        {
            sf::RenderTexture renderTexture({16, 16});
            renderTexture.clear(sf::Color::Black);

            sf::RectangleShape shape({16, 16});
            shape.setFillColor(sf::Color::Green);
            renderTexture.draw(shape);
            renderTexture.display();

            CHECK(renderTexture.getTexture().copyToImage().getPixel({8, 8}) == sf::Color::Green);
        }
    }

    SECTION("Stencil Tests")
    {
        sf::RenderTexture renderTexture({100, 100}, sf::ContextSettings{0 /* depthBits */, 8 /* stencilBits */});
        renderTexture.clear(sf::Color::Red, 127);

        sf::RectangleShape shape1({100, 100});
        shape1.setFillColor(sf::Color::Green);
        sf::RectangleShape shape2({100, 100});
        shape2.setFillColor(sf::Color::Blue);

        SECTION("Stencil-Only")
        {
            renderTexture.draw(shape1,
                               sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Keep, 1, 0xFF, true});
            renderTexture.display();
            CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
        }

        SECTION("Comparisons")
        {
            SECTION("Always")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Keep, 1, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("Equal")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Keep, 126, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Keep, 127, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("Greater")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Greater, sf::StencilUpdateOperation::Keep, 126, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Greater, sf::StencilUpdateOperation::Keep, 127, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Greater, sf::StencilUpdateOperation::Keep, 128, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("GreaterEqual")
            {
                renderTexture.draw(shape1,
                                   sf::StencilMode{sf::StencilComparison::GreaterEqual,
                                                   sf::StencilUpdateOperation::Keep,
                                                   126,
                                                   0xFF,
                                                   false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
                renderTexture.draw(shape1,
                                   sf::StencilMode{sf::StencilComparison::GreaterEqual,
                                                   sf::StencilUpdateOperation::Keep,
                                                   127,
                                                   0xFF,
                                                   false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
                renderTexture.draw(shape2,
                                   sf::StencilMode{sf::StencilComparison::GreaterEqual,
                                                   sf::StencilUpdateOperation::Keep,
                                                   128,
                                                   0xFF,
                                                   false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Blue);
            }

            SECTION("Less")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Less, sf::StencilUpdateOperation::Keep, 128, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Less, sf::StencilUpdateOperation::Keep, 127, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Less, sf::StencilUpdateOperation::Keep, 126, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("LessEqual")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::LessEqual, sf::StencilUpdateOperation::Keep, 128, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::LessEqual, sf::StencilUpdateOperation::Keep, 127, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
                renderTexture
                    .draw(shape2,
                          sf::StencilMode{sf::StencilComparison::LessEqual, sf::StencilUpdateOperation::Keep, 126, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Blue);
            }

            SECTION("Never")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Never, sf::StencilUpdateOperation::Keep, 127, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
            }

            SECTION("NotEqual")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::NotEqual, sf::StencilUpdateOperation::Keep, 127, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Red);
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::NotEqual, sf::StencilUpdateOperation::Keep, 128, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }
        }

        SECTION("Updating")
        {
            SECTION("Decrement")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Decrement, 127, 0xFF, true});
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Decrement, 126, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("Increment")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Increment, 127, 0xFF, true});
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Increment, 128, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("Invert")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Invert, 127, 0xFF, true});
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Invert, 0x80, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("Keep")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Keep, 127, 0xFF, true});
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Keep, 127, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("Replace")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Replace, 255, 0xFF, true});
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Replace, 255, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }

            SECTION("Zero")
            {
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Always, sf::StencilUpdateOperation::Zero, 127, 0xFF, true});
                renderTexture
                    .draw(shape1,
                          sf::StencilMode{sf::StencilComparison::Equal, sf::StencilUpdateOperation::Zero, 0, 0xFF, false});
                renderTexture.display();
                CHECK(renderTexture.getTexture().copyToImage().getPixel({50, 50}) == sf::Color::Green);
            }
        }
    }
}
