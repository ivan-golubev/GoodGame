#include <SDL2/SDL.h>
#include <exception>
#include <format>
#include <cstdlib>
#include <chrono>
#include <thread>
#include <cassert>

import Application;
import ErrorHandling;
import Logging;
import ModelLoader;
import VulkanRenderer;

using gg::Application;
using gg::VulkanRenderer;
using gg::BreakIfFalse;
using gg::DebugLevel;

namespace {
    void MainLoop(std::shared_ptr<Application> app)
    {
        using namespace std::chrono_literals;

        BreakIfFalse(Application::IsInitialized());
        bool isRunning{ true };

        auto timeManager{ app->GetTimeManager() };
        uint64_t lastEventPollMs{ 0 };
        constexpr uint64_t EVENT_POLL_INTERVAL_MS{ 16ULL }; // Poll 60 times per sec.

        while (isRunning)
        {
            int64_t const currentTimeMs{ timeManager->GetCurrentTimeMs().count() };
            bool needToPollEvents{ currentTimeMs - lastEventPollMs > EVENT_POLL_INTERVAL_MS };

            if (needToPollEvents)
            {
                lastEventPollMs = currentTimeMs;

                SDL_Event event;
                // Poll for user input.
                while (SDL_PollEvent(&event))
                {
                    switch (event.type)
                    {
                    case SDL_QUIT:
                        isRunning = false;
                        break;
                    case SDL_WINDOWEVENT:
                    {
                        auto windowEvent{ event.window.event };
                        if (windowEvent == SDL_WINDOWEVENT_RESIZED)
                            app->OnWindowResized(event.window.data1, event.window.data2);
                        else if (windowEvent == SDL_WINDOWEVENT_MINIMIZED)
                            app->OnWindowMinimized();
                        else if (windowEvent == SDL_WINDOWEVENT_RESTORED)
                            app->OnWindowRestored();
                    }
                    break;
                    case SDL_KEYDOWN: 
                        [[fallthrough]];
                    case SDL_KEYUP:
                    {
                        SDL_Keycode key{ event.key.keysym.sym };
                        if (key == SDLK_ESCAPE)
                            isRunning = false;
                        else
                            app->OnKeyPressed(key, event.type == SDL_KEYDOWN);
                        break;
                    }
                    default:
                        // Do nothing.
                        break;
                    }
                }
            }
            app->Tick();
            /* Don't do the next tick immediately */
            // TODO: why we need this ? maybe just the input polling has to be slower ? 250ms is a lot
            std::this_thread::sleep_for(0.25ms);
        }
    }
} // namespace

int main()
{
    if(SDL_Init(SDL_INIT_VIDEO) != 0) 
    {
        DebugLog(DebugLevel::Error, "Could not initialize SDL");
        return EXIT_FAILURE;
    }
    atexit(SDL_Quit);

    uint32_t const width{ 1920 };
    uint32_t const height{ 1080 };

    SDL_Window* window = SDL_CreateWindow("Vulkan Renderer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if(nullptr == window) 
    {
        DebugLog(DebugLevel::Error, "Could not create SDL window");
        return EXIT_FAILURE;
    }

    try
    {
        auto app = Application::Init(std::make_unique<VulkanRenderer>(width, height, window));
        app->GetRenderer()->UploadGeometry(
            app->GetModelLoader()->LoadModel(
                "../../assets/runtime/models/textured_cube.glb",
                "shaders/textured_surface_VS.spv",
                "shaders/textured_surface_PS.spv"
            )
        );
        DebugLog(DebugLevel::Info, "Successfully initialized the Vulkan application");
        MainLoop(app);
    }
    catch (std::exception const& e)
    {
        DebugLog(DebugLevel::Error, std::format("Caught exception with message: {}", e.what()));
        return EXIT_FAILURE;
    }

    Application::Destroy();
    return EXIT_SUCCESS;
}
