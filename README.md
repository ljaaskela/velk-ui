<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://github.com/ljaaskela/velk/blob/main/docs/logos/logo/velk-logo-standard-dark%402x.png">
  <source media="(prefers-color-scheme: light)" srcset="https://github.com/ljaaskela/velk/blob/main/docs/logos/logo/velk-logo-standard-light%402x.png">
  <img alt="Velk" src="https://github.com/ljaaskela/velk/blob/main/docs/logos/logo/velk-logo-standard-dark%402x.png" width="200">
</picture>

Application platform built on the [Velk](https://github.com/ljaaskela/velk) component object model. Provides GPU rendering, a UI framework, text rendering, and application infrastructure.

## Modules

### Application runtime ([velk-runtime](velk-runtime/))

The user-facing entry point. Wraps render context creation, window management, plugin loading, and the frame loop behind a single `Application` class. Supports both desktop (managed window via GLFW) and framework-driven (wrapped native surface, Android / embedded) modes.

### Rendering foundation ([velk-render](velk-render/))

Pointer-based GPU rendering abstraction:
* Minimal backend interface relying on buffer device addresses, bindless textures, push-constant-driven draw calls.
* Includes a Vulkan 1.2 backend (`velk::vk`) with BDA and bindless descriptors.

### UI framework ([velk-ui](velk-ui/))

Declarative UI framework:
* Scene graphs, element composition via traits (constraints, visuals, transforms, input), JSON scene loading.
* A scene renderer using velk-render that walks the visual tree and submits draw calls to the render backend.
* Text rendering plugin (`velk_text`) using analytic Bezier glyph coverage adapted from Eric Lengyel's [Slug](https://github.com/EricLengyel/Slug) reference shaders. No glyph atlas: outlines are baked once with FreeType, packed into GPU curve and band buffers, and shaded per-pixel with exact analytic coverage. Scale-independent: one font instance renders at any pixel size with no re-baking.
* Image plugin (`velk_image`) decoding raster images via stb_image into bindless GPU textures.

## Documentation

User documentation lives at [`docs/`](docs/) — see [`docs/README.md`](docs/README.md) for the index. Quick links:

* **[Getting started](docs/getting-started.md)** Minimal example walkthrough
* **[Runtime](docs/runtime/runtime.md)** Application setup, frame loop, both creation modes
* **[Scene](docs/ui/scene.md)** / **[Traits](docs/ui/traits.md)** / **[Input](docs/ui/input.md)** UI authoring
* **[Rendering](docs/render/rendering.md)** / **[Render backend](docs/render/render-backend.md)** Internals
* **[Text](docs/plugins/text.md)** / **[Image](docs/plugins/image.md)** Bundled plugins

## Quick start

A minimal application which shows a fixed 64px*64px velk logo on screen.

```cpp
#include <velk-runtime/api/application.h>
#include <velk-ui/api/scene.h>

int main()
{
    // Initialize velk
    auto app = velk::create_app({});
    // Create a window
    auto window = app.create_window({.width = 1280, .height = 720, .title = "demo"});
    // Load a scene from a file
    auto scene = velk::ui::create_scene("app://scenes/velk_logo.json");
    // Find the first Element with a Trait implementing ICamera and bind it to the window
    if (auto camera = scene.find_first<velk::ui::ICamera>()) {
        app.add_view(window, camera);
    }
    // Application loop
    while (app.poll()) {
        app.update();
        app.present();
    }
}
```

velk_logo.json:
```json
{
  "version": 1,
  "objects": [
    { "id": "root", "class": "velk-ui.Element" },
    { "id": "camera", "class": "velk-ui.Element" },
    { "id": "logo", "class": "velk-ui.Element" }
  ],
  "hierarchies": {
    "scene": {
      "root": ["logo", "camera"]
    }
  },
  "attachments": [
    { "targets": ["camera"],  "class": "velk-ui.Camera" },
    { "targets": ["logo"],    "class": "velk-ui.FixedSize", "properties": { "width": "64px", "height": "64px" } },
    { "targets": ["logo"],    "class": "velk_image.ImageVisual", "properties": { "uri": "image:app://assets/logo.png" } }
  ]
}
```

Or, equivalently, build the same scene programmatically in C++ instead of loading from JSON:

```cpp
#include <velk-ui/api/camera.h>
#include <velk-ui/api/element.h>
#include <velk-ui/api/scene.h>
#include <velk-ui/api/trait/fixed_size.h>
#include <velk-ui/plugins/image/api/image_visual.h>

using namespace velk::ui;

// Empty scene; no JSON file
auto scene = create_scene();

// Three elements ("objects")
auto root   = create_element();
auto camera = create_element();
auto logo   = create_element();

// Hierarchy: root has children ("hierarchies")
scene.set_root(root);
scene.add(root, logo);
scene.add(root, camera);

// Camera: Camera trait ("attachments": velk-ui.Camera)
camera.add_trait(trait::render::create_camera());

// logo: FixedSize 64x64 ("attachments": velk-ui.FixedSize)
logo.add_trait(trait::layout::create_fixed_size(dim::px(64.f), dim::px(64.f));

// logo: Visual trait to show the image ("attachments": velk-ui.ImageVisual)
logo.add_trait(trait::visual::create_image("image:app://assets/logo.png"));
```

Build & run:

![Application](./docs/minimal_app.png)

## Building

Requires CMake 3.14+, MSVC 2019 (C++17), and the Vulkan SDK (for shaderc).

Velk is built from source automatically. The `VELK_SOURCE_DIR` cache variable defaults to `../velk`.

```bash
cmake -B build -G "Visual Studio 16 2019" -A x64 -T v142
cmake --build build --config Release
```

## Running

```bash
./build/bin/Release/velk_ui_app.exe
```

## Dependencies

* [Velk](https://github.com/ljaaskela/velk) (`../velk/` by default)
* GLFW 3.4 (vendored)
* Vulkan SDK (shaderc for runtime GLSL to SPIR-V compilation)
* volk, VMA (Vulkan function loader and memory allocator, header-only via Vulkan SDK)
* FreeType 2.13, HarfBuzz 10.2 (vendored in `plugins/text/third_party/`)
* CMake 3.14+
