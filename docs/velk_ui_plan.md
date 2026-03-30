# velk-ui Plan

## Overall sequencing

| Step | Status |
|------|--------|
| 1. Importer plugin (velk repo) | **DONE** |
| 2. velk-ui repo | **In progress** |
| 3. Editor | Deferred |

## Step 2: velk-ui

### Completed rounds

#### Round 1: Foundation types DONE

- `mat4`, `aabb` in velk `math_types.h` with operators, translate/scale factories, min/max accessors
- Math type operators and named constants on all types
- `velk::size` extended to 3D (width, height, depth)
- `IElement` updated: position (vec3), size, local_transform, world_matrix, z_index
- Importer: struct type deserialization from JSON objects and arrays
- GL renderer updated for new IElement layout

#### Round 2a: Scene + Core Constraints + Solver + Importer DONE

- `dim`, `Constraint` structs in velk-ui; `register_type<dim>()` in plugin init
- `IConstraint` interface (with `IHierarchy*` parameter for layout constraints)
- `DirtyFlags` enum (Layout, Visual, DrawOrder)
- `ISceneObserver` interface (on_attached/on_detached, Element marks Layout+Visual dirty on attach)
- `IScene` interface (inherits IHierarchy) + `Scene` class (owns hierarchy, dirty vector, visual list)
- Element dirty tracking: `pending_dirty_` accumulator, pushes to Scene's vector on first dirty per frame
- `IStack`, `IFixedSize` interfaces and implementations
- `LayoutSolver` (internal to Scene): top-down recursion, measure/apply, world matrix accumulation
- `ConstraintImportHandler` for `"ui-constraints"` JSON section (later replaced by attachments)
- Per-config DLL copy (Debug copies Debug velk.dll, Release copies Release)

#### Round 2a.5: Visual Representation + Text Rendering DONE

- `DrawCommand` POD struct and `DrawCommandType` enum
- `IVisual` interface (color PROP, on_visual_changed EVT, get_draw_commands virtual)
- `ITextureProvider` interface (pixel data, dimensions, dirty tracking)
- `RectVisual` (single FillRect command), `TextVisual` (IVisual + ITextureProvider + ITextVisual)
- `ITextVisual` public interface in text plugin
- Visual import handler for `"ui-visuals"` section (later replaced by attachments)
- `color` removed from IElement; visual appearance exclusively via IVisual attachments
- Element subscribes to IVisual `on_visual_changed` events, marks self Visual-dirty
- GlRenderer: two pipelines (untextured FillRect, textured TexturedQuad), draw command caching
- `IFont::init_default()`, embedded Inter Regular font data
- Testable: 3 colored rects + "Hello, Velk!" text visible on screen

#### Renderer Refactor DONE

Split the monolithic GL renderer into a plugin-layered architecture:

```
velk_gl  -->  velk_render  -->  velk-ui  -->  velk
velk_vk  -->  velk_render  -->  velk-ui  -->  velk
```

- `IRenderContext` interface: owns the backend, loads backend plugin, creates renderers and surfaces
- `ISurface` interface: render target with width/height properties
- `IRenderer` interface: attach/detach surface+scene, render(), shutdown()
- `IRenderBackend` interface: init, surface/pipeline/texture management, begin/submit/end frame
- `velk_render` plugin: RenderContextImpl, Renderer (batching, material resolution, dirty tracking), Surface
- `velk_gl` plugin: GlBackend (glad loading, shader compilation, VAO/VBO per format, pipeline cache)
- Well-known keys: PipelineKey (Rect, Text, RoundedRect, CustomBase), VertexFormat (Untextured, Textured), TextureKey (Atlas)
- `RenderBatch`, `PipelineDesc`, `SurfaceDesc` structs
- Default shaders (rect, text, rounded rect) registered by context on init
- `SceneState` / `consume_state()` on IScene (scene decoupled from renderer)
- `set_geometry(aabb)` on IScene (replaces set_viewport/set_renderer)
- Batch caching with dirty tracking; clean frames re-submit cached batches
- App updated: no GL headers needed, GL loader passed via RenderConfig::backend_params

### Layout model (reference)

Single `Element` type (poolable/hiveable), behavior defined by traits.

**Defaults**: elements fill their parent. Siblings overlap (no implicit layout).

**Transforms**: each element has two 4x4 matrices:
- **World matrix**: computed by the solver. Accumulates parent chain: `parent.world * translate(position) * child.local_transform`
- **Local matrix**: user-specified offset/rotation/scale, applied on top of layout. Does not affect layout of siblings

**Traits**: layout constraints and visuals are separate objects attached via velk's attachment mechanism. The solver finds `IConstraint` traits on each element.

**Two-phase solve**: `measure` computes desired size, `apply` writes final bounds. Constraints that need multiple passes over children do so internally.

**Constraint struct**: `{ velk::aabb bounds; }` Input: available space. Output: used space.

**dim type**: `{ float value; DimUnit unit; }` with `none`, `px`, `pct` units. `none` = don't constrain this axis.

**Ordering**: solver collects all `IConstraint` traits, sorts by phase (Layout first, Constraint second), runs in attachment order within phase.

### Next: Round 2b: Additional Constraints

- `IMargin` / `Margin`: shrinks available rect by dim left/top/right/bottom
- `IPadding` / `Padding`: shrinks available rect for children (element size unchanged)
- `IAlignment` / `Alignment`: positions within available space (horizontal, vertical)
- `IMinMax` / `MinMax`: enforces dim min/max width/height bounds
- `IFitContent` / `FitContent`: opt-in shrink-wrap (Layout phase, measures children, takes bounding box as own size)
- Register new types, extend import/attachment handler
- Testable: scenes using the full constraint set

### Next: Round 3: Demo polish

- Additional test scenes
- Multiple visual types rendered together

### Next: Round 4: Unit tests

- Test framework setup
- Solver tests: verify element positions/sizes/world matrices for various constraint combinations
- Scene dirty tracking tests
- Constraint tests (Stack multi-pass, FixedSize, Margin, etc.)

### Ahead (no plan yet)

- **2D renderer features**: gradients, clipping, custom shaders
- **Text features**: line wrapping, multi-line, text alignment, font size/weight variants
- **Style system**: TBD
- **Event/input model**: hit testing, focus, keyboard/mouse
- **Vulkan backend**: velk_vk plugin

## Step 3: Editor (deferred)

- Editor for creating UI scene files in velk serialization format
- Could self-host with velk-ui or start with Dear ImGui
- Web target via Emscripten is a goal
- Exporter lives here (needs editorial intent: dirty tracking, default omission)

## Guiding principle

Keep velk (with plugins) small and embeddable. Heavy dependencies (renderer, text shaping, windowing) go in velk-ui, not velk.
