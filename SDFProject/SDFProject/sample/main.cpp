#include <agz-utils/mesh.h>
#include <d3d11-sdf/sdf.h>

#include "camera.h"
#include "sdf_renderer.h"
#include "sdf_shadow.h"

class DirectX11SDFDemo : public Demo
{
public:

    using Demo::Demo;

private:

    enum Mode
    {
        SDFVis,
        SDFShadow,
    };

    Mode mode_ = SDFVis;

    d3d11_sdf::SDF       sdf_;
    VertexBuffer<Vertex> mesh_;

    SDFRenderer       sdfRenderer_;
    SDFShadowRenderer sdfShadowRenderer_;

    Camera camera_;

    std::string objFilename_;
    int         signRayCount_ = 12;
    int         sdfRes_       = 64;

    d3d11_sdf::SDFGenerator sdfGen_;

    int   maxTraceSteps_ = 64;
    float absThreshold_  = 0.01f;

    float shadowK_         = 30;
    float shadowRayOffset_ = 0.03f;

    ImGui::FileBrowser fileBrowser_;
    // ����������
    void initialize() override
    {
        // �E�B���h�E�̍\�z
        window_->setMaximized();

        // SDFRender�̏�����
        // TODO:��œǂ�
        sdfRenderer_.initilalize();
        sdfShadowRenderer_.initialize();
        // �E�T�M�̃��f���̏�����
        objFilename_ = "./asset/bunny.obj";
        loadMesh();
        // �J�����̏�����
        camera_.setPosition(Float3(0, 0, -4));
        camera_.setDirection(3.1415926f / 2, 0);
        camera_.setPerspective(60.0f, 0.1f, 100.0f);
        // �t�@�C���ǂݍ��ݗp�̉����HuImGUI���g�������ۂ��H
        // TODO:��œǂ�
        fileBrowser_.SetTitle("Select Obj");
        fileBrowser_.SetPwd("./asset/");
        // �}�E�X���̏�����
        mouse_->setCursorLock(
            true, window_->getClientWidth() / 2, window_->getClientHeight() / 2);
        mouse_->showCursor(false);
        // �E�B���h�E�̉���
        // TODO:��œǂ�
        window_->doEvents();
    }
    // ���t���[���s�������HUpdate�I�ȓz���Ǝv��
    void frame() override
    {
        // ESC�L�[����������I���������s��
        if(keyboard_->isDown(KEY_ESCAPE))
            window_->setCloseFlag(true);
        // ��CTRL�L�[�������ƃ}�E�X�����������悤�ɂ��邽�߂̏���
        // TOOD:Input�֘A�̏�������ŎQ�l�ɂ��Ă���
        if(keyboard_->isDown(KEY_LCTRL))
        {
            mouse_->showCursor(!mouse_->isVisible());
            mouse_->setCursorLock(
                !mouse_->isLocked(), mouse_->getLockX(), mouse_->getLockY());
        }
        // ImGUI�̊J�n�����H
        // TODO:��œǂ�
        if(ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static const char *modeNames[] =
            {
                "Visualizer", "Shadow"
            };
            if(ImGui::BeginCombo("Mode", modeNames[mode_]))
            {
                for(int i = 0; i < 2; ++i)
                {
                    const bool selected = mode_ == i;
                    if(ImGui::Selectable(modeNames[i], selected))
                        mode_ = static_cast<Mode>(i);
                }
                ImGui::End();
            }

            ImGui::InputInt("Max Steps", &maxTraceSteps_);
            ImGui::InputFloat("Abs Threshold", &absThreshold_);

            if(ImGui::Button("Select Scene"))
                fileBrowser_.Open();

            ImGui::SameLine();

            if(ImGui::Button("Regenerate"))
                loadMesh();

            ImGui::InputInt("SDF Res", &sdfRes_);
            sdfRes_ = (std::max)(sdfRes_, 1);

            ImGui::InputInt("Sign Ray Count", &signRayCount_);
            signRayCount_ = (std::max)(signRayCount_, 1);

            if(mode_ == SDFShadow)
            {
                ImGui::SliderFloat("Shadow K", &shadowK_, 0, 50);
                ImGui::InputFloat("Shadow Ray Offset", &shadowRayOffset_, 0, 0, 8);
            }
        }
        ImGui::End();

        fileBrowser_.Display();
        if(fileBrowser_.HasSelected())
        {
            objFilename_ = fileBrowser_.GetSelected().string();
            fileBrowser_.ClearSelected();

            loadMesh();
        }
        // �J�����֘A�̏���
        camera_.setWOverH(window_->getClientWOverH());
        // �}�E�X����������Ă��Ȃ����
        if(!mouse_->isVisible())
        {
            // �J�����̏����X�V����
            camera_.update({
                .front      = keyboard_->isPressed('W'),
                .left       = keyboard_->isPressed('A'),
                .right      = keyboard_->isPressed('D'),
                .back       = keyboard_->isPressed('S'),
                .up         = keyboard_->isPressed(KEY_SPACE),
                .down       = keyboard_->isPressed(KEY_LSHIFT),
                .cursorRelX = static_cast<float>(mouse_->getRelativePositionX()),
                .cursorRelY = static_cast<float>(mouse_->getRelativePositionY())
            });
        }
        // �J�����̕ϊ��p�}�g���b�N�X�̐���
        camera_.recalculateMatrics();
        // �[�x�o�b�t�@�̃N���A
        window_->clearDefaultDepth(1);
        // RenderTarget�̃N���A
        window_->clearDefaultRenderTarget({ 0, 0, 0, 0 });
        // �`�惂�[�h�̈Ⴂ�ɂ�郌���_�����O�����̕���
        if(mode_ == SDFVis)
        {
            sdfRenderer_.setCamera(camera_);
            sdfRenderer_.setSDF(sdf_.lower, sdf_.upper, sdf_.srv);
            sdfRenderer_.setTracer(maxTraceSteps_, absThreshold_);
            sdfRenderer_.render();
        }
        else if(mode_ == SDFShadow)
        {
            sdfShadowRenderer_.setCamera(camera_);
            sdfShadowRenderer_.setSDF(sdf_.lower, sdf_.upper, sdf_.srv);
            sdfShadowRenderer_.setLight(
                Float3(-1, -1, 1).normalize(), Float3(1));
            sdfShadowRenderer_.setTracer(
                shadowRayOffset_, shadowK_, maxTraceSteps_, absThreshold_);
            sdfShadowRenderer_.render(mesh_);
        }
    }

    void loadMesh()
    {
        const auto tris = agz::mesh::load_from_file(objFilename_);

        constexpr float InfFlt = std::numeric_limits<float>::infinity();
        agz::math::aabb3f bbox = { Float3(InfFlt), Float3(-InfFlt) };
        for(auto &t : tris)
        {
            for(auto &v : t.vertices)
                bbox |= v.position;
        }

        const float maxExtent = (bbox.high - bbox.low).max_elem();
        const Float3 offset = -0.5f * (bbox.high + bbox.low);
        const float scale = 2 / maxExtent;

        std::vector<Vertex> vertexData;
        std::vector<Float3> vertices, normals;
        for(auto &t : tris)
        {
            for(auto &v : t.vertices)
            {
                const Float3 vp = scale * (v.position + offset);
                vertexData.push_back({
                    vp, v.normal, Float3(0.5f)
                });
                vertices.push_back(vp);
                normals.push_back(v.normal);
            }
        }
        // SDF�̃x�C�N�������s���Ă���Ǝv����
        // TODO:�v�`�F�b�N
        sdfGen_.setSignRayCount(signRayCount_);
        sdf_ = sdfGen_.generateGPU(
            vertices.data(), normals.data(), tris.size(),
            Float3(-1.2f), Float3(1.2f), Int3(sdfRes_));

        mesh_.initialize(vertexData.size(), vertexData.data());
    }
};

int main()
{
    DirectX11SDFDemo(WindowDesc{
        .clientSize     = { 640, 480 },
        .title          = L"DirectX11-SDF",
        .disableTimeout = true,
        .sampleCount    = 4
    }).run();
}
