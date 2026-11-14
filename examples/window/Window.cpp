////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Window.hpp>

#include <SFML/System/Angle.hpp>

#include <OpenGLRenderer.hpp>

#include <cstdlib>

#ifdef SFML_SYSTEM_IOS
#include <SFML/Main.hpp>
#endif

#include <array>
#include <iostream>

////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Application exit code
///
////////////////////////////////////////////////////////////
int main()
{
    // Request a 24-bits depth buffer when creating the window
    sf::ContextSettings contextSettings;
    contextSettings.depthBits = 24;

    // Create the main window
    sf::Window window(sf::VideoMode({640, 480}), "SFML window with OpenGL", sf::Style::Default, sf::State::Windowed, contextSettings);

    // Make it the active window for OpenGL calls
    if (!window.setActive())
    {
        std::cerr << "Failed to set the window as active" << std::endl;
        return EXIT_FAILURE;
    }

    if (!loadOpenGLFunctions())
    {
        std::cerr << "Failed to load OpenGL functions" << std::endl;
        return EXIT_FAILURE;
    }

    // Set the color and depth clear values
#ifdef SFML_OPENGL_ES
    glClearDepthf(1.f);
#else
    glClearDepth(1.f);
#endif
    glClearColor(0.f, 0.f, 0.f, 1.f);

    // Enable Z-buffer read and write
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    // Configure the viewport (the same size as the window)
    glViewport(0, 0, static_cast<GLsizei>(window.getSize().x), static_cast<GLsizei>(window.getSize().y));

    // Define a 3D cube (6 faces made of 2 triangles composed by 3 vertices)
    // clang-format off
    constexpr std::array<GLfloat, 252> cube =
    {
        // positions    // colors (r, g, b, a)
        -50, -50, -50,  0, 0, 1, 1,
        -50,  50, -50,  0, 0, 1, 1,
        -50, -50,  50,  0, 0, 1, 1,
        -50, -50,  50,  0, 0, 1, 1,
        -50,  50, -50,  0, 0, 1, 1,
        -50,  50,  50,  0, 0, 1, 1,

         50, -50, -50,  0, 1, 0, 1,
         50,  50, -50,  0, 1, 0, 1,
         50, -50,  50,  0, 1, 0, 1,
         50, -50,  50,  0, 1, 0, 1,
         50,  50, -50,  0, 1, 0, 1,
         50,  50,  50,  0, 1, 0, 1,

        -50, -50, -50,  1, 0, 0, 1,
         50, -50, -50,  1, 0, 0, 1,
        -50, -50,  50,  1, 0, 0, 1,
        -50, -50,  50,  1, 0, 0, 1,
         50, -50, -50,  1, 0, 0, 1,
         50, -50,  50,  1, 0, 0, 1,

        -50,  50, -50,  0, 1, 1, 1,
         50,  50, -50,  0, 1, 1, 1,
        -50,  50,  50,  0, 1, 1, 1,
        -50,  50,  50,  0, 1, 1, 1,
         50,  50, -50,  0, 1, 1, 1,
         50,  50,  50,  0, 1, 1, 1,

        -50, -50, -50,  1, 0, 1, 1,
         50, -50, -50,  1, 0, 1, 1,
        -50,  50, -50,  1, 0, 1, 1,
        -50,  50, -50,  1, 0, 1, 1,
         50, -50, -50,  1, 0, 1, 1,
         50,  50, -50,  1, 0, 1, 1,

        -50, -50,  50,  1, 1, 0, 1,
         50, -50,  50,  1, 1, 0, 1,
        -50,  50,  50,  1, 1, 0, 1,
        -50,  50,  50,  1, 1, 0, 1,
         50, -50,  50,  1, 1, 0, 1,
         50,  50,  50,  1, 1, 0, 1,
    };
    // clang-format on

    OpenGLRenderer renderer;
    if (!renderer.create(cube.data(), 36, static_cast<GLsizei>(7 * sizeof(GLfloat)), 4, false))
    {
        std::cerr << "Failed to initialize the OpenGL renderer" << std::endl;
        return EXIT_FAILURE;
    }

    // Create a clock for measuring the time elapsed
    const sf::Clock clock;

    // Start the game loop
    while (window.isOpen())
    {
        // Process events
        while (const std::optional event = window.pollEvent())
        {
            // Window closed or escape key pressed: exit
            if (event->is<sf::Event::Closed>() ||
                (event->is<sf::Event::KeyPressed>() &&
                 event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape))
            {
                window.close();
            }

            // Resize event: adjust the viewport
            if (const auto* resized = event->getIf<sf::Event::Resized>())
            {
                const auto [width, height] = resized->size;
                glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
            }
        }

        // Clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const float elapsedTime = clock.getElapsedTime().asSeconds();
        renderer.draw(window.getSize(),
                      {0.f, 0.f, -200.f},
                      {sf::degrees(elapsedTime * 50.f).asRadians(),
                       sf::degrees(elapsedTime * 30.f).asRadians(),
                       sf::degrees(elapsedTime * 90.f).asRadians()},
                      false);

        // Finally, display the rendered frame on screen
        window.display();
    }

    return EXIT_SUCCESS;
}
