# Sera C++ And Vulkan Based Application Framework

Template for fast start up for vulkan based application. It provides:
- Clang format
- Clang tidy
- [glfw](https://github.com/glfw/glfw)
- [Imgui](https://github.com/ocornut/imgui)
- [glm](https://github.com/g-truc/glm)
- [spdlog](https://github.com/gabime/spdlog)

>[!TIP]
>There is `Diligent` branch for same project but uses diligent engine for rendering

Clone this repository with: 
```bash
git clone --recursive --depth=1 https://github.com/ruchan-o7/sera
```

## Build
1. Go to `./Scripts` folder and execute `setup.bat`. It does: `cmake -S . -O ./build`. basically generates cmake builds
2. Run `build.bat` for building
3. Run `run.bat` for running app

