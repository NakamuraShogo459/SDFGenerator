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
    // 描画モード管理用
    enum Mode
    {
        SDFVis,
        SDFShadow,
    };
    // 描画モード管理用の変数
    Mode mode_ = SDFVis;
    // SDFのインスタンス
    d3d11_sdf::SDF       sdf_;
    // 頂点バッファ
    VertexBuffer<Vertex> mesh_;
    // SDF描画用のインスタンス
    SDFRenderer       sdfRenderer_;
    // SDFのシャドウ描画のインスタンス
    SDFShadowRenderer sdfShadowRenderer_;
    // カメラ
    Camera camera_;
    // SDF変換に使用するobjファイル名
    std::string objFilename_;
    // Rayのステップ数
    int         signRayCount_ = 12;
    // よくわからん
    // TODO:後で調べる
    int         sdfRes_       = 64;
    // objをSDFに変換する機能のインスタンス
    d3d11_sdf::SDFGenerator sdfGen_;
    // よくわからん
    // TODO:後で調べる
    int   maxTraceSteps_ = 64;
    // 絶対値のなにかのしきいち
    // TODO:後で調べる
    float absThreshold_  = 0.01f;
    // よくわからん
    // TODO:後で調べる
    float shadowK_         = 30;
    // RayのOffset 
    // TODO:どこで使われているか調べる
    float shadowRayOffset_ = 0.03f;
    // Gui.おそらくobjファイル変更時にファイル参照するタイミングで出てきたui
    ImGui::FileBrowser fileBrowser_;
    // 初期化処理
    void initialize() override
    {
        // ウィンドウの構築
        window_->setMaximized();

        // SDFRenderの初期化
        // TODO:後で読む
        sdfRenderer_.initilalize();
        sdfShadowRenderer_.initialize();
        // ウサギのモデルの初期化
        objFilename_ = "./asset/bunny.obj";
        loadMesh();
        // カメラの初期化
        camera_.setPosition(Float3(0, 0, -4));
        camera_.setDirection(3.1415926f / 2, 0);
        camera_.setPerspective(60.0f, 0.1f, 100.0f);
        // ファイル読み込み用の何か？uImGUIを使う物っぽい？
        // TODO:後で読む
        fileBrowser_.SetTitle("Select Obj");
        fileBrowser_.SetPwd("./asset/");
        // マウス情報の初期化
        mouse_->setCursorLock(
            true, window_->getClientWidth() / 2, window_->getClientHeight() / 2);
        mouse_->showCursor(false);
        // ウィンドウの何か
        // TODO:後で読む
        window_->doEvents();
    }
    // 毎フレーム行う処理？Update的な奴だと思う
    void frame() override
    {
        // ESCキーを押したら終了処理を行う
        if(keyboard_->isDown(KEY_ESCAPE))
            window_->setCloseFlag(true);
        // 左CTRLキーを押すとマウスが可視化されるようにするための処理
        // TOOD:Input関連の処理を後で参考にしておく
        if(keyboard_->isDown(KEY_LCTRL))
        {
            mouse_->showCursor(!mouse_->isVisible());
            mouse_->setCursorLock(
                !mouse_->isLocked(), mouse_->getLockX(), mouse_->getLockY());
        }
        // ImGUIの開始処理？
        // TODO:後で読む
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
        // カメラ関連の処理
        camera_.setWOverH(window_->getClientWOverH());
        // マウスが可視化されていなければ
        if(!mouse_->isVisible())
        {
            // カメラの情報を更新する
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
        // カメラの変換用マトリックスの生成
        camera_.recalculateMatrics();
        // 深度バッファのクリア
        window_->clearDefaultDepth(1);
        // RenderTargetのクリア
        window_->clearDefaultRenderTarget({ 0, 0, 0, 0 });
        // 描画モードの違いによるレンダリング処理の分岐
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
        // SDFのベイク処理を行っていると思われる
        // TODO:要チェック
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
