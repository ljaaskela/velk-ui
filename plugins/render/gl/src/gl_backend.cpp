#include "gl_backend.h"

#include <cstring>
#include <glad/gl.h>

namespace velk_ui {

namespace {

GLuint compile_shader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        VELK_LOG(E, "Shader compile error: %s", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint link_program(GLuint vert, GLuint frag)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        VELK_LOG(E, "Program link error: %s", log);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

GLuint build_program(const char* vert_src, const char* frag_src)
{
    GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_src);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return 0;
    }
    GLuint program = link_program(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);
    return program;
}

void setup_untextured_vao(GLuint vao, GLuint vbo)
{
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, VertexFormat::UntexturedStride, reinterpret_cast<void*>(0));
    glVertexAttribDivisor(0, 1);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, VertexFormat::UntexturedStride, reinterpret_cast<void*>(16));
    glVertexAttribDivisor(1, 1);

    glBindVertexArray(0);
}

void setup_textured_vao(GLuint vao, GLuint vbo)
{
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, VertexFormat::TexturedStride, reinterpret_cast<void*>(0));
    glVertexAttribDivisor(0, 1);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, VertexFormat::TexturedStride, reinterpret_cast<void*>(16));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, VertexFormat::TexturedStride, reinterpret_cast<void*>(32));
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);
}

velk::Uid gl_type_to_velk_uid(GLenum gl_type)
{
    switch (gl_type) {
    case GL_FLOAT:      return velk::type_uid<float>();
    case GL_FLOAT_VEC2: return velk::type_uid<velk::vec2>();
    case GL_FLOAT_VEC4: return velk::type_uid<velk::color>(); // vec4 and color are the same layout
    case GL_FLOAT_MAT4: return velk::type_uid<velk::mat4>();
    case GL_INT:        return velk::type_uid<int32_t>();
    case GL_SAMPLER_2D: return velk::type_uid<int32_t>(); // sampler bound as int
    default:            return {};
    }
}

} // namespace

GlBackend::~GlBackend()
{
    if (initialized_) {
        GlBackend::shutdown();
    }
}

bool GlBackend::init(void* params)
{
    if (params) {
        auto loader = reinterpret_cast<GLADloadfunc>(params);
        if (!gladLoadGL(loader)) {
            VELK_LOG(E, "GlBackend::init: failed to load GL functions");
            return false;
        }
        VELK_LOG(I, "OpenGL %s", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
    }

    // Untextured VAO + VBO
    glGenVertexArrays(1, &untextured_vao_);
    glGenBuffers(1, &untextured_vbo_);
    setup_untextured_vao(untextured_vao_, untextured_vbo_);

    // Textured VAO + VBO
    glGenVertexArrays(1, &textured_vao_);
    glGenBuffers(1, &textured_vbo_);
    setup_textured_vao(textured_vao_, textured_vbo_);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initialized_ = true;
    return true;
}

void GlBackend::shutdown()
{
    if (!initialized_) {
        return;
    }

    for (auto& [key, entry] : pipelines_) {
        if (entry.program) {
            glDeleteProgram(entry.program);
        }
    }
    pipelines_.clear();

    for (auto& [key, tex] : textures_) {
        if (tex) {
            glDeleteTextures(1, &tex);
        }
    }
    textures_.clear();

    if (untextured_vbo_) { glDeleteBuffers(1, &untextured_vbo_); untextured_vbo_ = 0; }
    if (textured_vbo_) { glDeleteBuffers(1, &textured_vbo_); textured_vbo_ = 0; }
    if (untextured_vao_) { glDeleteVertexArrays(1, &untextured_vao_); untextured_vao_ = 0; }
    if (textured_vao_) { glDeleteVertexArrays(1, &textured_vao_); textured_vao_ = 0; }

    surfaces_.clear();
    initialized_ = false;
}

bool GlBackend::create_surface(uint64_t surface_id, const SurfaceDesc& desc)
{
    surfaces_[surface_id] = {desc.width, desc.height};
    return true;
}

void GlBackend::destroy_surface(uint64_t surface_id)
{
    surfaces_.erase(surface_id);
}

void GlBackend::update_surface(uint64_t surface_id, const SurfaceDesc& desc)
{
    auto it = surfaces_.find(surface_id);
    if (it != surfaces_.end()) {
        it->second = {desc.width, desc.height};
    }
}

bool GlBackend::register_pipeline(uint64_t pipeline_key, const PipelineDesc& desc)
{
    if (pipelines_.count(pipeline_key)) {
        return true;
    }

    GLuint program = build_program(desc.vertex_source, desc.fragment_source);
    if (!program) {
        return false;
    }

    PipelineEntry entry;
    entry.program = program;

    // Introspect all active uniforms
    GLint uniform_count = 0;
    glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
    for (GLint i = 0; i < uniform_count; ++i) {
        char name_buf[128];
        GLsizei name_len = 0;
        GLint size = 0;
        GLenum gl_type = 0;
        glGetActiveUniform(program, static_cast<GLuint>(i), sizeof(name_buf),
                           &name_len, &size, &gl_type, name_buf);

        int location = glGetUniformLocation(program, name_buf);
        if (location < 0) {
            continue;
        }

        UniformInfo info;
        info.name = velk::string(name_buf, static_cast<size_t>(name_len));
        info.typeUid = gl_type_to_velk_uid(gl_type);
        info.location = location;
        entry.uniforms.push_back(std::move(info));
    }

    pipelines_[pipeline_key] = std::move(entry);
    return true;
}

velk::vector<UniformInfo> GlBackend::get_pipeline_uniforms(uint64_t pipeline_key) const
{
    auto it = pipelines_.find(pipeline_key);
    if (it != pipelines_.end()) {
        return it->second.uniforms;
    }
    return {};
}

void GlBackend::upload_texture(uint64_t texture_key,
                               const uint8_t* pixels, int width, int height)
{
    GLuint tex = 0;
    auto it = textures_.find(texture_key);
    if (it != textures_.end()) {
        tex = it->second;
    } else {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        textures_[texture_key] = tex;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
                 static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RED, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void GlBackend::begin_frame(uint64_t surface_id)
{
    current_surface_ = surface_id;

    auto it = surfaces_.find(surface_id);
    if (it == surfaces_.end()) {
        VELK_LOG(E, "GlBackend::begin_frame: unknown surface %llu", surface_id);
        return;
    }

    int w = it->second.width;
    int h = it->second.height;

    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    float L = 0.0f;
    float R = static_cast<float>(w);
    float T = 0.0f;
    float B = static_cast<float>(h);

    std::memset(projection_, 0, sizeof(projection_));
    projection_[0]  = 2.0f / (R - L);
    projection_[5]  = 2.0f / (T - B);
    projection_[10] = -1.0f;
    projection_[12] = -(R + L) / (R - L);
    projection_[13] = -(T + B) / (T - B);
    projection_[15] = 1.0f;
}

void GlBackend::submit(velk::array_view<const RenderBatch> batches)
{
    for (auto& batch : batches) {
        if (batch.instance_count == 0) {
            continue;
        }

        // Look up pipeline
        auto pit = pipelines_.find(batch.pipeline_key);
        if (pit == pipelines_.end()) {
            continue;
        }
        auto& pipeline = pit->second;

        // Determine VAO and VBO based on vertex format
        GLuint vao = 0;
        GLuint vbo = 0;
        if (batch.vertex_format_key == VertexFormat::Untextured) {
            vao = untextured_vao_;
            vbo = untextured_vbo_;
        } else if (batch.vertex_format_key == VertexFormat::Textured) {
            vao = textured_vao_;
            vbo = textured_vbo_;
        } else {
            continue;
        }

        glUseProgram(pipeline.program);

        // Set u_projection and u_atlas from introspected pipeline uniforms
        for (auto& info : pipeline.uniforms) {
            if (info.location < 0) continue;
            if (info.name == "u_projection") {
                glUniformMatrix4fv(info.location, 1, GL_FALSE, projection_);
            }
        }

        // Set per-batch element rect uniform (legacy path for backwards compat)
        if (batch.has_rect) {
            for (auto& info : pipeline.uniforms) {
                if (info.name == "u_rect" && info.location >= 0) {
                    glUniform4f(info.location,
                                batch.rect.x, batch.rect.y,
                                batch.rect.width, batch.rect.height);
                    break;
                }
            }
        }

        // Set all per-batch material uniforms
        for (auto& u : batch.uniforms) {
            if (u.location < 0) {
                continue;
            }
            if (u.typeUid == velk::type_uid<float>()) {
                glUniform1f(u.location, u.data[0]);
            } else if (u.typeUid == velk::type_uid<velk::color>()
                       || u.typeUid == velk::type_uid<velk::vec4>()) {
                glUniform4f(u.location, u.data[0], u.data[1], u.data[2], u.data[3]);
            } else if (u.typeUid == velk::type_uid<velk::mat4>()) {
                glUniformMatrix4fv(u.location, 1, GL_FALSE, u.data);
            } else if (u.typeUid == velk::type_uid<int32_t>()) {
                glUniform1i(u.location, static_cast<GLint>(u.data[0]));
            } else if (u.typeUid == velk::type_uid<velk::vec2>()) {
                glUniform2f(u.location, u.data[0], u.data[1]);
            }
        }

        // Bind texture if present
        if (batch.texture_key != 0) {
            auto tit = textures_.find(batch.texture_key);
            if (tit != textures_.end()) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tit->second);
                for (auto& info : pipeline.uniforms) {
                    if (info.name == "u_atlas" && info.location >= 0) {
                        glUniform1i(info.location, 0);
                        break;
                    }
                }
            }
        }

        // Upload instance data
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(batch.instance_data.size()),
                     batch.instance_data.data(),
                     GL_STREAM_DRAW);

        // Draw
        glBindVertexArray(vao);
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
                              static_cast<GLsizei>(batch.instance_count));
        glBindVertexArray(0);

        // Unbind texture
        if (batch.texture_key != 0) {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
    }

    glUseProgram(0);
}

void GlBackend::end_frame()
{
    current_surface_ = 0;
}

} // namespace velk_ui
