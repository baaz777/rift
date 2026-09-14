# Setting Up the Development Environment

This guide covers installing dependencies and configuring your environment for developing Rift.

## Prerequisites Overview

\htmlonly
<pre class="mermaid">
graph LR
    classDef required fill:#134e3a,stroke:#10b981,color:#e2e8f0
    classDef optional fill:#4a3520,stroke:#f59e0b,color:#e2e8f0

    subgraph Required["Required Tools"]
        CMake["CMake 3.21+"]:::required
        Git["Git"]:::required
        Compiler["C++23 Compiler"]:::required
    end

    subgraph Libraries["Required Libraries"]
        GLFW["GLFW 3.3+"]:::required
        GLM["GLM"]:::required
        GLAD["GLAD"]:::required
        STB["stb_image"]:::required
        JSON["nlohmann/json"]:::required
        ENTT["EnTT v4.0.0"]:::required
    end

    subgraph RequiredSDK["Required SDK"]
        Vulkan["Vulkan SDK"]:::required
    end

    subgraph Optional["Optional"]
        FreeType["FreeType 2"]:::optional
    end
</pre>
\endhtmlonly

## Required Tools

### CMake

**Version:** 3.21 or higher. `CMakePresets.json` requires 3.21, and `build.bat` and `test.bat` both
configure through `cmake --preset default`. `CMakeLists.txt` on its own still declares 3.10, so a
preset-free `cmake ..` works with older CMake, but that path is not the supported one.

| Platform      | Installation                                                               |
|---------------|----------------------------------------------------------------------------|
| Windows       | [Download installer](https://cmake.org/download/) or `choco install cmake` |

Verify installation:
```cmd
cmake --version
```

### C++ Compiler

The project requires C++23 support.

| Platform | Compiler            | Notes                                           |
|----------|---------------------|-------------------------------------------------|
| Windows  | Visual Studio 2022+ | Install "Desktop development with C++" workload |

## Dependencies

### Dependency Overview

| Library       | Purpose                                                 | Required         |
|---------------|---------------------------------------------------------|------------------|
| GLFW          | Window creation, input handling                         | Yes              |
| GLM           | Mathematics (vectors, matrices)                         | Yes              |
| GLAD          | OpenGL function loading                                 | Yes (for OpenGL) |
| stb_image     | Image file loading                                      | Yes              |
| nlohmann/json | JSON parsing for maps, saves and the project manifest   | Yes              |
| EnTT          | single-header ECS, included as `<entt/entt.hpp>`        | Yes              |
| FreeType      | Font rendering                                          | Optional         |
| Vulkan SDK    | Vulkan graphics API                                     | Yes              |

CMake raises a `FATAL_ERROR` for each of the six required libraries it cannot resolve, whether from
vcpkg or from `external/`. FreeType is the only optional one: without it the build succeeds but
`USE_FREETYPE` stays undefined and text rendering is disabled.

### Automatic Setup (Windows)

We provide a PowerShell script to automatically download and configure dependencies:

```powershell
# Open PowerShell in project root
.\setup.ps1
```

This script will:
1. Clone GLFW into `external/glfw/` (or `git pull` it when already present)
2. Clone GLM into `external/glm/` (or `git pull` it when already present)
3. Download `nlohmann/json.hpp` into `external/nlohmann/`
4. download the pinned EnTT v4.0.0 amalgamated header and license into `external/entt/`
5. Report GLAD and stb_image as already present - both are committed to the repository
6. Report the status of the Vulkan SDK (required), vcpkg and Doxygen

You also need `VCPKG_ROOT` set to your vcpkg checkout before building: the `default` CMake preset
is conditional on it, and `cmake --preset default` fails without it. vcpkg manifest mode then
installs `vcpkg.json`'s dependencies (glm, glfw3, freetype, gtest) during configure.

### Manual Setup

If the automatic script doesn't work, follow these steps manually:

#### Project Structure

After setup, your `external/` directory should look like:

```
external/
|-- glfw/
|   |-- CMakeLists.txt
|   |-- include/
|   +-- src/
|-- glad/
|   |-- include/
|   |   |-- glad/
|   |   |   +-- glad.h
|   |   +-- KHR/
|   |       +-- khrplatform.h
|   +-- src/
|       +-- glad.c
|-- glm/
|   +-- glm/
|       |-- glm.hpp
|       +-- ...
|-- nlohmann/
|   +-- json.hpp
|-- entt/
|   |-- LICENSE
|   +-- entt/
|       +-- entt.hpp
|-- entt-ext/
|   +-- entt/
|       +-- ext/
|           +-- config.h
+-- stb/
    +-- stb_image.h
```

#### GLFW

Clone GLFW into `external/glfw/`:

```bash
git clone https://github.com/glfw/glfw.git external/glfw
```

Or download a release from [glfw.org](https://www.glfw.org/download.html).

#### GLM

**Option 1: Git clone**
```bash
git clone https://github.com/g-truc/glm.git external/glm
```

**Option 2: Download release**
1. Download from [GitHub Releases](https://github.com/g-truc/glm/releases)
2. Extract so headers are at `external/glm/glm/glm.hpp`

#### nlohmann/json

```powershell
mkdir external\nlohmann -Force
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp" -OutFile "external\nlohmann\json.hpp" -UseBasicParsing
```

#### EnTT

```powershell
mkdir external\entt\entt -Force
$enttCommit = "85c6bba014049b5de8fad49d25424df2f1f6a8c1"
$enttBaseUrl = "https://raw.githubusercontent.com/skypjack/entt/$enttCommit"
Invoke-WebRequest -Uri "$enttBaseUrl/single_include/entt/entt.hpp" -OutFile "external\entt\entt\entt.hpp" -UseBasicParsing
Invoke-WebRequest -Uri "$enttBaseUrl/LICENSE" -OutFile "external\entt\LICENSE" -UseBasicParsing
```

`external/entt-ext/entt/ext/config.h` is committed. It keeps game assertions active in Release
builds and routes failures through the Rift logger before aborting.

#### GLAD and stb_image

Both ship in the repository (`external/glad/`, `external/stb/`). Nothing to do.

Regenerate GLAD only if you deliberately target a different OpenGL version: generate at
[glad.dav1d.de](https://glad.dav1d.de/) for C/C++, OpenGL, gl 4.6, Core profile, "Generate a
loader", and extract over `external/glad/`. Doing this without a reason replaces working in-repo
files with a mismatched configuration.

## Required: Vulkan SDK

The Vulkan SDK is required to build the project. CMake configuration will fail without it because both rendering backends are linked into a single binary and selected at runtime.

### Windows

1. Download from [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
2. Run the installer
3. Verify installation:
   ```cmd
   glslangValidator --version
   ```

## Optional: FreeType

For text rendering support:

### Windows

FreeType is declared in `vcpkg.json` and installed automatically by vcpkg manifest mode when
`cmake --preset default` configures with `VCPKG_ROOT` set. No manual `vcpkg install` step is
needed, and there is no bundled copy under `external/`.

Without FreeType the build still succeeds: CMake warns, leaves `USE_FREETYPE` undefined, and text
rendering is disabled.

## Verifying Setup

After installing dependencies, verify your setup (run in Developer Command Prompt):

```cmd
:: Check CMake
cmake --version

:: Check compiler
cl

:: Check Vulkan (required)
vulkaninfo

:: Check GLSL compiler (required, ships with the Vulkan SDK)
glslangValidator --version
```

## Troubleshooting

### "Could not find GLFW"

CMake cannot find GLFW. Solutions:
1. Ensure GLFW is in `external/glfw/`
2. Check that `external/glfw/CMakeLists.txt` exists

### "Could not find GLM"

CMake cannot find GLM. Solutions:
1. Ensure GLM is in `external/glm/`
2. Check that `external/glm/glm/glm.hpp` exists

### "glad.h not found"

GLAD is committed to the repository, so this means the files were deleted or the checkout is
incomplete:
1. Ensure `external/glad/include/glad/glad.h` exists
2. Ensure `external/glad/src/glad.c` exists
3. Restore them from the repository rather than regenerating

### "stb_image.h not found"

stb_image is committed to the repository. Restore `external/stb/stb_image.h` from the checkout.

### "nlohmann/json not found" or "EnTT not found"

Both are downloaded by `setup.ps1` and are hard CMake errors when absent. Re-run `.\setup.ps1`, or
fetch `external/nlohmann/json.hpp` and `external/entt/entt/entt.hpp` manually as shown above.

### Vulkan Validation Layers Missing

If you see validation layer warnings:
1. Reinstall Vulkan SDK
2. Ensure `VK_LAYER_PATH` environment variable is set
3. Run `vulkaninfo` to check that validation layers are installed

## Next Steps

After setting up dependencies:

1. [Build the project](BUILDING.md)
2. [Configure project assets](PROJECT_MANIFEST.md)
3. [Learn the editor](EDITOR.md)
4. [Understand the architecture](ARCHITECTURE.md)

## Platform-Specific Notes

### Windows

- Use **x64 Native Tools Command Prompt** for command-line builds
- Visual Studio 2022 (17.x) required: the `default` preset pins the Visual Studio 17 2022 generator
  and the project is C++23
- PowerShell execution policy may need adjustment for scripts:
  ```powershell
  Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
  ```

## See Also

- [Building Guide](BUILDING.md) - Compiling the project
- [Project Manifest](PROJECT_MANIFEST.md) - Configuring startup assets
- [Architecture](ARCHITECTURE.md) - Understanding the codebase
