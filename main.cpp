#include "rendering_nodes.h"

#include "../tt_cpplib/windont.h"
#include "../tt_cpplib/tt_window.h"
#include "../tt_rendering/gl/tt_glcontext.h"

struct RenderGraph {
    std::vector<Node*> nodes;
    std::vector<Node*> sinkNodes;
    std::vector<CreateRenderPassNode*> renderPassNodes;

    void destroy() { for (Node* node : nodes) delete node; }
};

RenderGraph generateTestGraph() {
    RenderGraph graph;

    auto& cbo = *new CreateImageNode("cbo");
    cbo.width.setValue(0);
    cbo.height.setValue(0);
    cbo.factor.setValue(1);

    auto& fbo = *new CreateFramebufferNode("fbo");
    fbo.colorBuffers.appendNew().setInput(cbo.result);

    auto& intermediate = *new CreateRenderPassNode("intermediate");
    intermediate.clearColor.setValue(TT::Vec4(0.0f, 0.0f, 1.0f, 1.0f));
    intermediate.framebuffer.setInput(fbo.result);

    auto& generate = *new CreateMaterialNode("generate");
    generate.shaderPaths.appendNew().setValue("noop.vert.glsl");
    generate.shaderPaths.appendNew().setValue("generate.frag.glsl");
    generate.blendMode.setValue(TTRendering::MaterialBlendMode::Additive);

    auto& generatePass = *new DrawQuadNode("generatePass");
    generatePass.material.setInput(generate.result);
    generatePass.renderPass.setInput(intermediate.result);

    auto& present = *new CreateRenderPassNode("present");
    present.clearColor.setValue(TT::Vec4(0.0f, 0.5f, 0.0f, 1.0f));

    auto& blit = *new CreateMaterialNode("blit");
    blit.shaderPaths.appendNew().setValue("noop.vert.glsl");
    blit.shaderPaths.appendNew().setValue("blit.frag.glsl");
    blit.blendMode.setValue(TTRendering::MaterialBlendMode::Opaque);

    auto& forward = *new MaterialSetImageNode("forward");
    forward.material.setInput(blit.result);
    forward.image.setInput(cbo.result);

    auto& presentPass = *new DrawQuadNode("presentPass");
    presentPass.material.setInput(blit.result);
    presentPass.renderPass.setInput(present.result);

    // We can probably just compute all nodes instead of tracking only specific node types or nodes without outputs.
    graph.nodes = { &cbo, &fbo, &intermediate, &generate, &generatePass, &present, &blit, &forward, &presentPass };
    graph.sinkNodes = { &generatePass, &forward, &presentPass };
    graph.renderPassNodes = { &intermediate, &present };
    
    return graph;
}

#if 0
#include "dg.h"

class ConstF32 final : public Node {
public:
    std::string debugStr() const override { 
        return "\"ConstF32\":" + label(); 
    }

    Socket<float>& value;

    ConstF32(const std::string& label = "") 
        : Node(label)
        , value(addInput("value", 0.0f))  {
        _initializing = false;
    }

private:
};

class MulF32 final : public Node {
public:
    std::string debugStr() const override { 
        return "\"MulF32\":" + label(); 
    }

    Socket<float>& lhs;
    Socket<float>& rhs;
    Socket<float>& result;

    MulF32(const std::string& label = "")
        : Node(label)
        , lhs(addInput("lhs", 0.0f))
        , rhs(addInput("rhs", 0.0f))
        , result(addOutput("result", 0.0f)) {
        _initializing = false;
    }

private:
    void _compute() override {
        result.setValue(lhs.value() * rhs.value());
    }
};

int main() {
    MulF32 x;
    x.lhs.setValue(2.0f);
    x.rhs.setValue(3.0f);
    TT::assert(x.result.value() == 6.0f);

    ConstF32 a;
    a.value.setValue(2.0f);
    ConstF32 b;
    b.value.setValue(3.0f);
    ConstF32 c;
    c.value.setValue(4.0f);
    MulF32 d;
    d.lhs.setInput(b.value);
    d.rhs.setInput(c.value);
    MulF32 e;
    e.lhs.setInput(d.result);
    e.rhs.setValue(5.0f);
    MulF32 f;
    f.lhs.setInput(d.result);
    f.rhs.setInput(e.result);
    TT::assert(f.result.value() == 720.0f);

    generateTestGraph();

    return (int)f.result.value();
}


/*
class _State:
    def __init__(self, w: int, h: int) -> None:
        self.ctx = OpenGLContext()
        self.ctx.windowResized(w, h)

        quadVerts = (ctypes.c_float * 8)(0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0)
        quadVbo = self.ctx.createBuffer(ctypes.sizeof(quadVerts), quadVerts)
        quadMesh = self.ctx.createMesh(4, quadVbo, (MeshAttribute(MeshAttribute.Dimensions.D2, MeshAttribute.ElementType.F32, 0),), None, PrimitiveType.TriangleFan)

        # Obtain a graph that describes the rendering pipeline
        graph = list(generateTestGraph())

        # For testing we can shuffle the graph to verify that
        # our render pass ordering always comes out valid.
        # random.shuffle(graph)

        # Make sure the rendering nodes are ready to evaluate graphs
        initializeRenderGraphState(RenderGraphState(self.ctx, quadMesh))

        # Then make sure all endpoints are evaluated to generate the actual GPU pipeline
        renderPasses: list[RenderPass] = []
        for node in graph:
            if isinstance(node, (MaterialSetImageNode, DrawQuadNode)):
                node.compute()
            if isinstance(node, CreateRenderPassNode):
                renderPasses.append(node.result.value())

        # Find which passes are responsible for generating which images
        imageGenerators = {}
        for renderPass in renderPasses:
            fbo = renderPass.framebuffer()
            if not fbo:
                continue

            for cbo in fbo.iterColorAttachments():
                imageGenerators.setdefault(cbo, set()).add(renderPass)
            dbo = fbo.depthStencilAttachment()
            if dbo:
                imageGenerators.setdefault(dbo, set()).add(renderPass)

        # In turn, sort the passes based on which images they use,
        # and thus after which passes they have to go.
        self.renderPasses = []
        for renderPass in renderPasses:
            # this pass needs these images
            mtls = set()
            imgs = set()
            for q in renderPass.drawQueue().queues:
                for q2 in q.queues:
                    mtls |= set(q2.keys)
            for mtl in mtls:
                imgs |= set(mtl.images().values())

            # and thus it comes after the passes that generate these images
            insertAt = 0
            for img in imgs:
                if img in imageGenerators:
                    previous = imageGenerators[img]
                    if previous in self.renderPasses:
                        insertAt = max(insertAt, self.renderPasses.index(previous) + 1)
            self.renderPasses.insert(insertAt, renderPass)

    def draw(self, defaultFramebuffer: int = 0) -> None:
        for renderPass in self.renderPasses:
            self.ctx.drawPass(renderPass, defaultFramebuffer)
*/

#endif

// TODO: Just because I can't hash or compare handles I have to resort to this bullshit
// HandleBase should implement an identifier based hash
template<typename T>
class HandleSet {
    std::vector<size_t> identifiers;
    std::vector<T> contents;

    size_t index(size_t identifier) const {
        const auto& it = std::find(identifiers.begin(), identifiers.end(), identifier);
        if (it == identifiers.end()) return identifiers.size();
        return it - identifiers.begin();
    }

public:
    void insert(const T& t, size_t identifier) {
        size_t i = index(identifier);
        if (i == identifiers.size()) {
            contents.push_back(t);
            identifiers.push_back(identifier);
        }
    }

    bool contains(const T& t, size_t identifier) {
        return index(t, identifier) != contents.size();
    }

    auto begin() const { return contents.begin(); }
    auto end() const { return contents.end(); }
};

class App : public TT::Window {
    TTRendering::OpenGLContext context;
    TTRendering::MeshHandle quadMesh;
    bool sizeKnown = false;
    RenderGraph graph;
    std::vector<const TTRendering::RenderPass*> orderedRenderPasses;

public:
    App() : TT::Window(), context(*this) {
        show();
    }

    virtual ~App() {
        for (const TTRendering::RenderPass* renderPass : orderedRenderPasses) 
            delete renderPass;
        graph.destroy();
    }

    void initRenderingResources() {
        // Obtain a graph that describes the rendering pipeline
        graph = generateTestGraph();

        // Make sure the rendering nodes are ready to evaluate graphs
        RenderGraphGlobals::gContext = &context;

        float quadVerts[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
        TTRendering::BufferHandle quadVbo = context.createBuffer(sizeof(float) * 8, (unsigned char*)quadVerts);
        quadMesh = context.createMesh(4, quadVbo, { {TTRendering::MeshAttribute::Dimensions::D2, TTRendering::MeshAttribute::ElementType::F32, 0} }, nullptr, TTRendering::PrimitiveType::TriangleFan);
        RenderGraphGlobals::gQuadMesh = &quadMesh;

        // Then make sure all endpoints are evaluated to generate the actual GPU pipeline
        for(const auto& node : graph.sinkNodes)
            node->compute();
        
        // Gather the passes that were generated
        std::vector<TTRendering::RenderPass*> renderPasses;
        for (const auto& node : graph.renderPassNodes)
            renderPasses.push_back(node->result.value());

        // Find which passes are responsible for generating which images
        std::map</*ImageHandle*/size_t, std::set<TTRendering::RenderPass*>> imageGenerators = {};
        for(const auto& renderPass : renderPasses) {
            const auto& fbo = renderPass->framebuffer();
            if (!fbo) continue;
            for (const auto& cbo : fbo->colorAttachments())
                imageGenerators[cbo.identifier()].insert(renderPass);
            const auto& dbo = fbo->depthStencilAttachment();
            if(dbo)
                imageGenerators[dbo->identifier()].insert(renderPass);
        }

        // In turn, sort the passes based on which images they use,
        // and thus after which passes they have to go.
        for(const TTRendering::RenderPass* renderPass : renderPasses) {
            // This pass needs these images
            std::set<TTRendering::MaterialHandle> mtls;
            HandleSet<TTRendering::ImageHandle> imgs;
            for(const auto& q : renderPass->drawQueue().queues)
                for(const auto& q2 : q.queues)
                    for(const auto& q2e : q2.keys)
                        mtls.insert(q2e);
            for (const auto& mtl : mtls) {
                const auto& mtlImages = mtl.images();
                for(const auto& img : mtlImages) {
                    size_t sz = img.second;
                    const auto& ref = mtlImages.handle(sz);
                    imgs.insert(ref, ref.identifier());
                }
            }

            // and thus it comes after the passes that generate these images
            size_t insertAt = 0;
            for(const auto& img : imgs) {
                const auto& it = imageGenerators.find(img.identifier());
                if(it != imageGenerators.end()) {
                    for(const auto& previous : it->second) {
                        const auto& it2 = std::find(orderedRenderPasses.begin(), orderedRenderPasses.end(), previous);
                        if(it2 != orderedRenderPasses.end()) {
                            size_t alternative = it2 - orderedRenderPasses.begin() + 1;
                            if(alternative > insertAt)
                                insertAt = alternative;
                        }
                    }
                }
            }
            if (insertAt == orderedRenderPasses.size())
                orderedRenderPasses.push_back(renderPass);
            else
                orderedRenderPasses.insert(orderedRenderPasses.begin() + insertAt, renderPass);
        }
    }

private:
    void onResizeEvent(const TT::ResizeEvent& event) override {
        context.windowResized(event.width, event.height);

        if (!sizeKnown) { 
            sizeKnown = true;
            initRenderingResources();
        }
    }

    void onPaintEvent(const TT::PaintEvent& event) override {
        context.beginFrame();
        for (const TTRendering::RenderPass* renderPass : orderedRenderPasses)
            context.drawPass(*renderPass);
        context.endFrame();
    }
};

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    App window;
    MSG msg;
    bool quit = false;

    // Track frame timings
    ULONGLONG a = GetTickCount64();
    ULONGLONG b;
    constexpr ULONGLONG FPS = 30;
    constexpr ULONGLONG t = 1000 / FPS;

    do {
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                quit = true;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        // Force UI repaints as fast as possible
        window.repaint();
        // Detect when all windows are closed and exit this loop.
        if (!TT::Window::hasVisibleWindows())
            PostQuitMessage(0);

        // Sleep for remaining frame time
        b = a;
        a = GetTickCount64();
        b = a - b;
        if (b < t) Sleep((DWORD)(t - b));
    } while (!quit);
    return (int)msg.wParam;
}