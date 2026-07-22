cmake_minimum_required(VERSION 3.10)

# select the toolchain before project() initializes the compiler.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
        message(STATUS "Using vcpkg toolchain from VCPKG_ROOT: $ENV{VCPKG_ROOT}")
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
        message(STATUS "Using vcpkg toolchain from project directory")
    endif()
endif()

project(rift)

file(STRINGS "${CMAKE_SOURCE_DIR}/src/Version.hpp" _ver_major REGEX "#define RIFT_VERSION_MAJOR")
file(STRINGS "${CMAKE_SOURCE_DIR}/src/Version.hpp" _ver_minor REGEX "#define RIFT_VERSION_MINOR")
file(STRINGS "${CMAKE_SOURCE_DIR}/src/Version.hpp" _ver_patch REGEX "#define RIFT_VERSION_PATCH")
file(STRINGS "${CMAKE_SOURCE_DIR}/src/Version.hpp" _ver_release REGEX "#define RIFT_VERSION_RELEASE")
string(REGEX REPLACE ".*#define RIFT_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" RIFT_VERSION_MAJOR "${_ver_major}")
string(REGEX REPLACE ".*#define RIFT_VERSION_MINOR[ \t]+([0-9]+).*" "\\1" RIFT_VERSION_MINOR "${_ver_minor}")
string(REGEX REPLACE ".*#define RIFT_VERSION_PATCH[ \t]+([0-9]+).*" "\\1" RIFT_VERSION_PATCH "${_ver_patch}")
string(REGEX REPLACE ".*#define RIFT_VERSION_RELEASE[ \t]+([0-9]+).*" "\\1" RIFT_VERSION_RELEASE "${_ver_release}")
set(RIFT_VERSION_STRING "${RIFT_VERSION_MAJOR}.${RIFT_VERSION_MINOR}.${RIFT_VERSION_PATCH}.${RIFT_VERSION_RELEASE}")
message(STATUS "Rift version: ${RIFT_VERSION_STRING}")

configure_file("${CMAKE_SOURCE_DIR}/Doxyfile.in" "${CMAKE_BINARY_DIR}/Doxyfile" @ONLY)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# build.bat runs blocking clang-tidy against the Ninja compile-db sidecar.
# RIFT_ENABLE_CLANG_TIDY adds checks to each compilation. the standalone tidy
# target uses this tree and lacks the sidecar flags; use build.bat for validation.
option(RIFT_ENABLE_CLANG_TIDY "Run clang-tidy on every C++ compilation (slow)" OFF)
find_program(RIFT_CLANG_TIDY_EXE NAMES clang-tidy)

if(RIFT_CLANG_TIDY_EXE)
    if(RIFT_ENABLE_CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY "${RIFT_CLANG_TIDY_EXE}")
        message(STATUS "clang-tidy build-time integration enabled: ${RIFT_CLANG_TIDY_EXE}")
    else()
        message(STATUS "clang-tidy found: ${RIFT_CLANG_TIDY_EXE}"
                       " (run `cmake --build ${CMAKE_BINARY_DIR} --target tidy`)")
    endif()
else()
    message(STATUS "clang-tidy not found - static analysis target will no-op."
                   " Install LLVM to enable.")
endif()

# enable SEH translation through asynchronous exception handling.
if(MSVC)
    string(REPLACE "/EHsc" "/EHa" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    # keep __cplusplus accurate for diagnostics and bundled headers.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zc:__cplusplus")
endif()

message(STATUS "Building with both OpenGL and Vulkan support (runtime switchable)")

find_package(OpenGL REQUIRED)
add_definitions(-DUSE_OPENGL)

find_package(Vulkan REQUIRED)
message(STATUS "Vulkan SDK found: ${Vulkan_INCLUDE_DIRS}")

find_package(glm CONFIG QUIET)
if(glm_FOUND)
    message(STATUS "GLM found via vcpkg/find_package")
    set(GLM_FROM_VCPKG TRUE)
else()
    if(EXISTS "${CMAKE_SOURCE_DIR}/external/glm/glm/glm.hpp")
        include_directories(${CMAKE_SOURCE_DIR}/external/glm)
        message(STATUS "Using bundled GLM from external/")
    else()
        message(FATAL_ERROR "GLM not found!\n"
                "Install via vcpkg: vcpkg install glm\n"
                "Or run setup-dependencies.ps1 to download to external/")
    endif()
endif()

find_package(glfw3 CONFIG QUIET)
if(glfw3_FOUND)
    message(STATUS "GLFW found via vcpkg/find_package")
    set(GLFW_FROM_VCPKG TRUE)
else()
    if(EXISTS "${CMAKE_SOURCE_DIR}/external/glfw/CMakeLists.txt")
        add_subdirectory(external/glfw)
        message(STATUS "Using bundled GLFW from external/")
    else()
        message(FATAL_ERROR "GLFW not found!\n"
                "Install via vcpkg: vcpkg install glfw3\n"
                "Or run setup-dependencies.ps1 to download to external/")
    endif()
endif()

find_package(glad CONFIG QUIET)
if(glad_FOUND)
    message(STATUS "GLAD found via vcpkg/find_package")
    set(GLAD_FROM_VCPKG TRUE)
else()
    if(EXISTS "${CMAKE_SOURCE_DIR}/external/glad/src/glad.c")
        add_subdirectory(external/glad)
        message(STATUS "Using bundled GLAD from external/")
    else()
        message(FATAL_ERROR "GLAD not found!\n"
                "Install via vcpkg: vcpkg install glad[gl-api-33-core,loader]\n"
                "Or run setup-dependencies.ps1 to download to external/")
    endif()
endif()

find_package(Stb QUIET)
if(Stb_FOUND)
    message(STATUS "stb found via vcpkg/find_package")
    set(STB_FROM_VCPKG TRUE)
else()
    if(EXISTS "${CMAKE_SOURCE_DIR}/external/stb/stb_image.h")
        include_directories(${CMAKE_SOURCE_DIR}/external/stb)
        message(STATUS "Using bundled stb from external/")
    else()
        message(FATAL_ERROR "stb not found!\n"
                "Install via vcpkg: vcpkg install stb\n"
                "Or run setup-dependencies.ps1 to download to external/")
    endif()
endif()

find_package(nlohmann_json CONFIG QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "nlohmann/json found via vcpkg/find_package")
    set(JSON_FROM_VCPKG TRUE)
else()
    if(EXISTS "${CMAKE_SOURCE_DIR}/external/nlohmann/json.hpp")
        include_directories(${CMAKE_SOURCE_DIR}/external/nlohmann)
        message(STATUS "Using bundled nlohmann/json from external/")
    else()
        message(FATAL_ERROR "nlohmann/json not found!\n"
                "Install via vcpkg: vcpkg install nlohmann-json\n"
                "Or run setup-dependencies.ps1 to download to external/")
    endif()
endif()

# pinned EnTT header: 85c6bba014049b5de8fad49d25424df2f1f6a8c1.
set(ENTT_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/external/entt")
if(EXISTS "${ENTT_INCLUDE_DIR}/entt/entt.hpp")
    message(STATUS "Using bundled EnTT v4.0.0 from external/")
else()
    message(FATAL_ERROR "EnTT not found!\n"
            "Run setup.ps1 to restore external/entt/entt/entt.hpp")
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/include")
    include_directories(${CMAKE_SOURCE_DIR}/include)
endif()
if(NOT GLAD_FROM_VCPKG AND EXISTS "${CMAKE_SOURCE_DIR}/external/glad/include")
    include_directories(${CMAKE_SOURCE_DIR}/external/glad/include)
endif()
if(NOT GLFW_FROM_VCPKG AND EXISTS "${CMAKE_SOURCE_DIR}/external/glfw/include")
    include_directories(${CMAKE_SOURCE_DIR}/external/glfw/include)
endif()

find_package(Freetype QUIET)
if(Freetype_FOUND)
    message(STATUS "FreeType found: ${FREETYPE_INCLUDE_DIRS}")
    add_definitions(-DUSE_FREETYPE)
else()
    message(WARNING "FreeType not found! Text rendering will not work.\n"
                    "To install FreeType:\n"
                    "  Windows (vcpkg): vcpkg install freetype\n"
                    "  Linux: sudo apt-get install libfreetype6-dev\n"
                    "  macOS: brew install freetype\n"
                    "\n"
                    "If using vcpkg, configure CMake with:\n"
                    "  cmake -B build -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake")

endif()

file(GLOB_RECURSE SOURCES "src/*.cpp")

# this target uses the main build tree; build.bat supplies the required sidecar flags.
if(RIFT_CLANG_TIDY_EXE)
    file(GLOB RIFT_TIDY_SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")
    add_custom_target(tidy
        COMMAND ${RIFT_CLANG_TIDY_EXE}
                -p ${CMAKE_BINARY_DIR}
                --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
                ${RIFT_TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running clang-tidy on src/*.cpp (project headers via HeaderFilterRegex)"
        VERBATIM)
else()
    add_custom_target(tidy
        COMMAND ${CMAKE_COMMAND} -E echo
                "clang-tidy not found - skipping static analysis."
        COMMENT "clang-tidy target (no-op: tool missing)")
endif()

# Texture.cpp defines STB_IMAGE_IMPLEMENTATION; do not compile a second stb implementation.
set(STB_SOURCES "")

add_executable(${PROJECT_NAME} ${SOURCES} ${STB_SOURCES})

target_include_directories(${PROJECT_NAME}
    SYSTEM PRIVATE ${ENTT_INCLUDE_DIR}
)
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/external/entt-ext
)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()

target_link_libraries(${PROJECT_NAME} OpenGL::GL)

target_link_libraries(${PROJECT_NAME} glfw)

if(GLAD_FROM_VCPKG)
    target_link_libraries(${PROJECT_NAME} glad::glad)
else()
    target_link_libraries(${PROJECT_NAME} glad)
endif()

if(GLM_FROM_VCPKG)
    target_link_libraries(${PROJECT_NAME} glm::glm)
endif()

if(JSON_FROM_VCPKG)
    target_link_libraries(${PROJECT_NAME} nlohmann_json::nlohmann_json)
endif()

if(STB_FROM_VCPKG)
    target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})
endif()

target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES})
target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})

if(Freetype_FOUND)
    target_link_libraries(${PROJECT_NAME} ${FREETYPE_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${FREETYPE_INCLUDE_DIRS})
endif()

set(ASSETS_DIR "${CMAKE_SOURCE_DIR}/assets")
set(SHADERS_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(PROJECT_MANIFEST_FILE "${CMAKE_SOURCE_DIR}/rift.project.json")

foreach(subdir "" "Debug" "Release")
    if(subdir STREQUAL "")
        set(OUT_DIR "${CMAKE_BINARY_DIR}")
    else()
        set(OUT_DIR "${CMAKE_BINARY_DIR}/${subdir}")
    endif()

    file(MAKE_DIRECTORY "${OUT_DIR}")

    if(EXISTS "${ASSETS_DIR}")
        file(COPY "${ASSETS_DIR}" DESTINATION "${OUT_DIR}")
    endif()
    if(EXISTS "${SHADERS_DIR}")
        file(COPY "${SHADERS_DIR}" DESTINATION "${OUT_DIR}")
    endif()
    if(EXISTS "${PROJECT_MANIFEST_FILE}")
        file(COPY "${PROJECT_MANIFEST_FILE}" DESTINATION "${OUT_DIR}")
    endif()
endforeach()

find_program(GLSLANG_VALIDATOR glslangValidator)

set(SHADER_SOURCES
    "${SHADERS_DIR}/Geometry.vert"
    "${SHADERS_DIR}/Geometry.frag"
    # Geometry3D uses world-space MVP and depth; Geometry handles screen-space UI.
    "${SHADERS_DIR}/Geometry3D.vert"
    "${SHADERS_DIR}/Geometry3D.frag"
)

if(GLSLANG_VALIDATOR)
    message(STATUS "glslangValidator found: ${GLSLANG_VALIDATOR}")
    message(STATUS "-> Shaders will be compiled to SPIR-V during build")

    set(SHADER_OUTPUTS "")
    foreach(SHADER_SRC ${SHADER_SOURCES})
        get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
        set(SHADER_SPV "${SHADERS_DIR}/${SHADER_NAME}.spv")
        list(APPEND SHADER_OUTPUTS ${SHADER_SPV})

        add_custom_command(
            OUTPUT ${SHADER_SPV}
            COMMAND ${GLSLANG_VALIDATOR} -V -DUSE_VULKAN "${SHADER_SRC}" -o "${SHADER_SPV}"
            DEPENDS ${SHADER_SRC}
            COMMENT "Compiling ${SHADER_NAME} -> ${SHADER_NAME}.spv"
            VERBATIM
        )
    endforeach()

    add_custom_target(compile_shaders DEPENDS ${SHADER_OUTPUTS})
else()
    message(STATUS "glslangValidator not found - shader compilation skipped")
    message(STATUS "  Pre-compiled .spv files will be copied if present")
    # keep add_dependencies valid when the shader compiler is unavailable.
    add_custom_target(compile_shaders)
endif()

file(GLOB SHADER_FILES "${SHADERS_DIR}/*.vert" "${SHADERS_DIR}/*.frag" "${SHADERS_DIR}/*.glsl")
if(SHADER_FILES)
    target_sources(${PROJECT_NAME} PRIVATE ${SHADER_FILES})
    source_group("Shaders" FILES ${SHADER_FILES})
endif()

add_dependencies(${PROJECT_NAME} compile_shaders)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${SHADERS_DIR}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_MANIFEST_FILE}" "$<TARGET_FILE_DIR:${PROJECT_NAME}>/rift.project.json"
    COMMENT "Copying assets and shaders to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

file(GLOB SAVE_FILES "${CMAKE_SOURCE_DIR}/rift.save*.json")
if(SAVE_FILES)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Copying save files to $<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SAVE_FILES} "$<TARGET_FILE_DIR:${PROJECT_NAME}>/"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
endif()

# game and tests share the configured build tree; select a target to build either.
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    enable_testing()

    find_package(GTest CONFIG QUIET)
    if(GTest_FOUND)
        message(STATUS "Google Test found via vcpkg/find_package")
        set(GTEST_FROM_VCPKG TRUE)
    else()
        message(STATUS "Google Test not found via vcpkg, fetching from GitHub...")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
        )
        # match the parent project's Windows runtime linkage.
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        message(STATUS "Using Google Test from FetchContent")
    endif()

    file(GLOB TEST_SOURCES "tests/*.cpp")

    # tests link graphics libraries without creating a GL or Vulkan context.
    # exercise draw paths only through MockRenderer and avoid GPU resource calls.
    set(TEST_LIB_SOURCES
        "${CMAKE_SOURCE_DIR}/src/Logger.cpp"
        "${CMAKE_SOURCE_DIR}/src/TimeManager.cpp"

        "${CMAKE_SOURCE_DIR}/src/WeatherDefinitions.cpp"

        "${CMAKE_SOURCE_DIR}/src/WeatherBlend.cpp"

        "${CMAKE_SOURCE_DIR}/src/WeatherDirector.cpp"

        "${CMAKE_SOURCE_DIR}/src/DrawTracer.cpp"
        "${CMAKE_SOURCE_DIR}/src/CollisionSystem.cpp"

        "${CMAKE_SOURCE_DIR}/src/MotionSystem.cpp"

        "${CMAKE_SOURCE_DIR}/src/PlayerMovementSystem.cpp"

        "${CMAKE_SOURCE_DIR}/src/PlayerSystem.cpp"

        "${CMAKE_SOURCE_DIR}/src/NpcAiSystem.cpp"

        "${CMAKE_SOURCE_DIR}/src/NpcRender.cpp"
        "${CMAKE_SOURCE_DIR}/src/PlayerRender.cpp"

        "${CMAKE_SOURCE_DIR}/src/RenderDrawable.cpp"

        "${CMAKE_SOURCE_DIR}/src/EntityStore.cpp"
        "${CMAKE_SOURCE_DIR}/src/PatrolRoute.cpp"

        "${CMAKE_SOURCE_DIR}/src/Pathfinding.cpp"

        "${CMAKE_SOURCE_DIR}/src/CharacterKinematics.cpp"

        "${CMAKE_SOURCE_DIR}/src/SurfaceSystem.cpp"
        "${CMAKE_SOURCE_DIR}/src/Tilemap.cpp"
        "${CMAKE_SOURCE_DIR}/src/Texture.cpp"

        "${CMAKE_SOURCE_DIR}/src/TextureStore.cpp"

        "${CMAKE_SOURCE_DIR}/src/DialogueStore.cpp"

        "${CMAKE_SOURCE_DIR}/src/CharacterRender.cpp"

        "${CMAKE_SOURCE_DIR}/src/AssetRegistry.cpp"
        "${CMAKE_SOURCE_DIR}/src/IRenderer.cpp"
        "${CMAKE_SOURCE_DIR}/src/EditorCommands.cpp"
        "${CMAKE_SOURCE_DIR}/src/NavigationRecalc.cpp"
        "${CMAKE_SOURCE_DIR}/src/ProjectManifest.cpp"
        # MockRenderer records particle draws without a context. tests must not call LoadTextures.
        "${CMAKE_SOURCE_DIR}/src/ParticleSystem.cpp"
        # tests construct CommandContext directly; RegisterDefaultCommands depends on Game.
        "${CMAKE_SOURCE_DIR}/src/Console.cpp"
        "${CMAKE_SOURCE_DIR}/src/ConsoleCommands.cpp"

        "${CMAKE_SOURCE_DIR}/src/DialogueManager.cpp"
        "${CMAKE_SOURCE_DIR}/src/GameStateManager.cpp"
        # EditorInput.cpp pulls in dialogue builders and stays outside this target.
        # TestStubs.cpp supplies link-only geometry stubs; avoid overlays that call them.
        "${CMAKE_SOURCE_DIR}/src/Editor.cpp"
        "${CMAKE_SOURCE_DIR}/src/EditorRendering.cpp"
        "${CMAKE_SOURCE_DIR}/src/CameraController.cpp"

        "${CMAKE_SOURCE_DIR}/src/CameraRig.cpp"

        "${CMAKE_SOURCE_DIR}/src/AuroraTextures.cpp"
        # Initialize builds CPU textures; LoadFromData skips GL without a context.
        # MockRenderer records both sky submission paths.
        "${CMAKE_SOURCE_DIR}/src/SkyRenderer.cpp"
    )

    add_executable(rift_tests ${TEST_SOURCES} ${TEST_LIB_SOURCES})

    target_include_directories(rift_tests
        SYSTEM PRIVATE ${ENTT_INCLUDE_DIR}
    )
    target_include_directories(rift_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
    )

    if(MSVC)
        target_compile_options(rift_tests PRIVATE /bigobj)
    endif()

    if(GTEST_FROM_VCPKG)
        target_link_libraries(rift_tests PRIVATE GTest::gtest GTest::gtest_main)
    else()
        target_link_libraries(rift_tests PRIVATE gtest gtest_main)
    endif()

    # resolve graphics symbols at link time; tests never create a graphics context.
    target_link_libraries(rift_tests PRIVATE OpenGL::GL glfw)

    if(GLAD_FROM_VCPKG)
        target_link_libraries(rift_tests PRIVATE glad::glad)
    else()
        target_link_libraries(rift_tests PRIVATE glad)
    endif()

    if(GLM_FROM_VCPKG)
        target_link_libraries(rift_tests PRIVATE glm::glm)
    endif()

    if(JSON_FROM_VCPKG)
        target_link_libraries(rift_tests PRIVATE nlohmann_json::nlohmann_json)
    endif()

    if(STB_FROM_VCPKG)
        target_include_directories(rift_tests PRIVATE ${Stb_INCLUDE_DIR})
    endif()

    target_link_libraries(rift_tests PRIVATE ${Vulkan_LIBRARIES})
    target_include_directories(rift_tests PRIVATE ${Vulkan_INCLUDE_DIRS})

    if(Freetype_FOUND)
        target_link_libraries(rift_tests PRIVATE ${FREETYPE_LIBRARIES})
        target_include_directories(rift_tests PRIVATE ${FREETYPE_INCLUDE_DIRS})
    endif()

    include(GoogleTest)
    gtest_discover_tests(rift_tests)

    message(STATUS "Tests enabled - build target: rift_tests")
endif()
