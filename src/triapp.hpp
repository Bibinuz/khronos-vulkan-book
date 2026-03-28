#include <GLFW/glfw3.h>

constexpr auto WIDTH = 1920;
constexpr auto HEIGHT = 1920;

namespace vke {

class TriApp {
  public:
    void run();

  private:
    void initWindow();
    void initVulkan();
    void createInstance();
    void mainLoop();
    void cleanup();

    GLFWwindow *m_window{};
};
} // namespace vke
