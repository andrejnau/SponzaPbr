#include "ImGuiPass.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Geometry/IABuffer.h>
#include <Utilities/FormatHelper.h>

ImGuiPass::ImGuiPass(RenderDevice& device, RenderCommandList& command_list, const Input& input, int width, int height, GLFWwindow* window)
    : m_device(device)
    , m_input(input)
    , m_width(width)
    , m_height(height)
    , m_window(window)
    , m_program(device)
    , m_imgui_settings(m_input.root_scene, m_input.settings)
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height);

    InitKey();
    CreateFontsTexture(command_list);
    m_sampler = m_device.CreateSampler({
        SamplerFilter::kMinMagMipLinear,
        SamplerTextureAddressMode::kWrap,
        SamplerComparisonFunc::kAlways });
}

ImGuiPass::~ImGuiPass()
{
    ImGui::DestroyContext();
}

void ImGuiPass::OnUpdate()
{
}

void ImGuiPass::OnRender(RenderCommandList& command_list)
{
    if (glfwGetInputMode(m_window, GLFW_CURSOR) != GLFW_CURSOR_NORMAL)
        return;

    m_imgui_settings.NewFrame();

    ImGui::Render();

    ImDrawData* draw_data = ImGui::GetDrawData();

    std::vector<glm::vec2> positions;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec4> colors;
    std::vector<uint32_t> indices;

    for (int i = 0; i < draw_data->CmdListsCount; ++i)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[i];
        for (int j = 0; j < cmd_list->VtxBuffer.Size; ++j)
        {
            auto& pos = cmd_list->VtxBuffer.Data[j].pos;
            auto& uv = cmd_list->VtxBuffer.Data[j].uv;
            uint8_t* col = reinterpret_cast<uint8_t*>(&cmd_list->VtxBuffer.Data[j].col);
            positions.push_back({ pos.x, pos.y });
            texcoords.push_back({ uv.x, uv.y });
            colors.push_back({ col[0] / 255.0, col[1] / 255.0, col[2] / 255.0, col[3] / 255.0 });
        }
        for (int j = 0; j < cmd_list->IdxBuffer.Size; ++j)
        {
            indices.push_back(cmd_list->IdxBuffer.Data[j]);
        }
    }

    static uint32_t index = 0;
    m_positions_buffer[index].reset(new IAVertexBuffer(m_device, command_list, positions));
    m_texcoords_buffer[index].reset(new IAVertexBuffer(m_device, command_list, texcoords));
    m_colors_buffer[index].reset(new IAVertexBuffer(m_device, command_list, colors));
    m_indices_buffer[index].reset(new IAIndexBuffer(m_device, command_list, indices, gli::format::FORMAT_R32_UINT_PACK32));

    command_list.UseProgram(m_program);
    command_list.Attach(m_program.vs.cbv.vertexBuffer, m_program.vs.cbuffer.vertexBuffer);

    m_program.vs.cbuffer.vertexBuffer.ProjectionMatrix = glm::ortho(0.0f, 1.0f * m_width, 1.0f * m_height, 0.0f);

    RenderPassBeginDesc render_pass_desc = {};
    render_pass_desc.colors[m_program.ps.om.rtv0].texture = m_input.rtv;
    render_pass_desc.colors[m_program.ps.om.rtv0].load_op = RenderPassLoadOp::kLoad;

    m_indices_buffer[index]->Bind(command_list);
    m_positions_buffer[index]->BindToSlot(command_list, m_program.vs.ia.POSITION);
    m_texcoords_buffer[index]->BindToSlot(command_list, m_program.vs.ia.TEXCOORD);
    m_colors_buffer[index]->BindToSlot(command_list, m_program.vs.ia.COLOR);

    command_list.Attach(m_program.ps.sampler.sampler0, m_sampler);

    command_list.SetBlendState({
        true,
        Blend::kSrcAlpha,
        Blend::kInvSrcAlpha,
        BlendOp::kAdd,
        Blend::kInvSrcAlpha,
        Blend::kZero,
        BlendOp::kAdd
    });

    command_list.SetRasterizeState({ FillMode::kSolid, CullMode::kNone });
    command_list.SetDepthStencilState({ false, ComparisonFunc::kLessEqual });
    command_list.SetViewport(0, 0, ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

    int vtx_offset = 0;
    int idx_offset = 0;
    command_list.BeginRenderPass(render_pass_desc);
    for (int i = 0; i < draw_data->CmdListsCount; ++i)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[i];
        for (int j = 0; j < cmd_list->CmdBuffer.Size; ++j)
        {
            const ImDrawCmd& cmd = cmd_list->CmdBuffer[j];
            command_list.Attach(m_program.ps.srv.texture0, *(std::shared_ptr<Resource>*)cmd.TextureId);
            command_list.SetScissorRect(cmd.ClipRect.x, cmd.ClipRect.y, cmd.ClipRect.z, cmd.ClipRect.w);
            command_list.DrawIndexed(cmd.ElemCount, idx_offset, vtx_offset);
            idx_offset += cmd.ElemCount;
        }
        vtx_offset += cmd_list->VtxBuffer.Size;
    }
    command_list.EndRenderPass();

    index = (index + 1) % 3;
}

void ImGuiPass::OnResize(int width, int height)
{
    m_width = width;
    m_height = height;
    ImGui::GetIO().DisplaySize = ImVec2((float)m_width, (float)m_height);
}

void ImGuiPass::OnKey(int key, int action)
{
    if (glfwGetInputMode(m_window, GLFW_CURSOR) == GLFW_CURSOR_NORMAL)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (action == GLFW_PRESS)
            io.KeysDown[key] = true;
        if (action == GLFW_RELEASE)
            io.KeysDown[key] = false;

        io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
        io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
        io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
        io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
    }
    m_imgui_settings.OnKey(key, action);
}

void ImGuiPass::OnMouse(bool first, double xpos, double ypos)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MousePos = ImVec2(xpos, ypos);
}

void ImGuiPass::OnMouseButton(int button, int action)
{
    ImGuiIO& io = ImGui::GetIO();
    if (button >= 0 && button < 3)
        io.MouseDown[button] = action == GLFW_PRESS;
}

void ImGuiPass::OnScroll(double xoffset, double yoffset)
{
    ImGuiIO& io = ImGui::GetIO();
    io.MouseWheel += yoffset;
}

void ImGuiPass::OnInputChar(unsigned int ch)
{
    ImGuiIO& io = ImGui::GetIO();
    if (ch > 0 && ch < 0x10000)
        io.AddInputCharacter((unsigned short)ch);
}

void ImGuiPass::CreateFontsTexture(RenderCommandList& command_list)
{
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    m_font_texture_view = m_device.CreateTexture(BindFlag::kShaderResource | BindFlag::kCopyDest, gli::format::FORMAT_RGBA8_UNORM_PACK8, 1, width, height);
    size_t num_bytes = 0;
    size_t row_bytes = 0;
    GetFormatInfo(width, height, gli::format::FORMAT_RGBA8_UNORM_PACK8, num_bytes, row_bytes);
    command_list.UpdateSubresource(m_font_texture_view, 0, pixels, row_bytes, num_bytes);

    // Store our identifier
    io.Fonts->TexID = (void*)&m_font_texture_view;
}

void ImGuiPass::InitKey()
{
    ImGuiIO& io = ImGui::GetIO();
    io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
    io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
    io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
    io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
    io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
    io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
    io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
    io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
    io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
    io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
    io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
    io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
}
