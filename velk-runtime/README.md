# velk-runtime

Application runtime and platform abstraction for [velk-platform](../README.md). Namespace: `velk::`.

The user-facing entry point. Wraps render context creation, window management, plugin loading, the frame loop, and platform input routing behind a single `Application` class. Hides GLFW, Vulkan, and the renderer setup so user code starts from `velk::create_app()` instead of dealing with raw native APIs.

Two execution modes are supported:

- **App-driven (desktop)**: velk creates the native window via the GLFW platform plugin and runs its own event loop. `app.poll() / update() / present()` in a while loop.
- **Framework-driven (Android, embedded)**: the host framework owns the native window. velk wraps an externally provided platform surface and the host calls `app.update() / prepare() / submit()` from its own callbacks.

## Source structure

| Directory | Description |
|-----------|-------------|
| `include/velk-runtime/interface/` | Core interfaces: IApplication, IWindow, IWindowProvider |
| `include/velk-runtime/api/` | Convenience wrappers: Application, Window, create_app() |
| `src/` | Application implementation, runtime plugin |
| `plugins/glfw/` | GLFW-backed platform plugin (desktop window provider) |

## Key concepts

- **Application**: owns the render context, default renderer, set of windows, frame loop. Constructed via `velk::create_app(config)`.
- **Window**: a platform window or wrapped native surface. Owns an ISurface and an IInputDispatcher. Created via `app.create_window()` or `app.wrap_native_surface()`.
- **WindowConfig** / **UpdateRate**: pacing options (VSync / Unlimited / Targeted) plus dimensions, title, resizable.
- **Performance overlay**: built-in FPS / CPU / frame-time text, enabled per window with `app.set_performance_overlay(window)`.

## Documentation

User-facing documentation lives at [`../docs/`](../docs/) at the repo root. The runtime topics:

| Document | Description |
|----------|-------------|
| [Runtime](../docs/runtime/runtime.md) | Application setup, window creation, frame loop, both modes, performance overlay |
