#include "rendering_nodes.h"

namespace RenderGraphGlobals {
    TTRendering::RenderingContext* gContext;
    TTRendering::MeshHandle* gQuadMesh;

    // TODO: Move these into tt_rendering so we can make these constructors private again.
    // TODO: We use those for comparisons, and we can safely compare just the identifier, we should implement that in an == operator in tt_rendering instead.
    TTRendering::ImageHandle NULL_IMAGE_HANDLE(0, TTRendering::ImageFormat::RGBA32F, TTRendering::ImageInterpolation::Linear, TTRendering::ImageTiling::Clamp);
    TTRendering::FramebufferHandle NULL_FRAMEBUFFER_HANDLE(0, {}, nullptr);
    TTRendering::ShaderHandle NULL_SHADER_HANDLE(0);
    TTRendering::MaterialHandle NULL_MATERIAL_HANDLE(NULL_SHADER_HANDLE);
}

CreateImageNode::CreateImageNode(const std::string& label)
    : Node(label)
    , width(addInput<unsigned short>("width", 128))
    , height(addInput<unsigned short>("height", 128))
    , format(addInput("format", TTRendering::ImageFormat::RGBA32F))
    , interpolation(addInput("interpolation", TTRendering::ImageInterpolation::Linear))
    , tiling(addInput("tiling", TTRendering::ImageTiling::Clamp))
    , factor(addInput<unsigned short>("factor", 0))
    , result(addOutput("result", TTRendering::ImageHandle(0, TTRendering::ImageFormat::RGBA32F, TTRendering::ImageInterpolation::Linear, TTRendering::ImageTiling::Clamp))) {
    _initializing = false;
}

void CreateImageNode::_compute() {
    // Create new image with given settings.
    unsigned int w, h;
    RenderGraphGlobals::gContext->resolution(w, h);
    unsigned short f = factor.value();
    w = width.value() + (f ? (w / f) : 0);
    h = height.value() + (f ? (h / f) : 0);
    result.setValue(RenderGraphGlobals::gContext->createImage(w, h, format.value(), interpolation.value(), tiling.value()));
}

CreateFramebufferNode::CreateFramebufferNode(const std::string& label)
    : Node(label)
    , colorBuffers(addArrayInput("colorBuffers", RenderGraphGlobals::NULL_IMAGE_HANDLE))
    , depthBuffer(addInput("depthBuffer", RenderGraphGlobals::NULL_IMAGE_HANDLE))
    , result(addOutput("result", RenderGraphGlobals::NULL_FRAMEBUFFER_HANDLE)) {
    _initializing = false;
}

void CreateFramebufferNode::_compute() {
    std::vector<TTRendering::ImageHandle> cbos;
    for(const auto& child : colorBuffers.children()) {
        const auto& cbo = child->value();
        if (cbo.identifier() != RenderGraphGlobals::NULL_IMAGE_HANDLE.identifier())
            cbos.push_back(cbo);
    }

    const auto& dbo = depthBuffer.value();
    if (dbo.identifier() != RenderGraphGlobals::NULL_IMAGE_HANDLE.identifier())
        result.setValue(RenderGraphGlobals::gContext->createFramebuffer(cbos, &dbo));
    else if(cbos.size() > 0)
        result.setValue(RenderGraphGlobals::gContext->createFramebuffer(cbos));
    else
        result.setValue(RenderGraphGlobals::NULL_FRAMEBUFFER_HANDLE);
}

CreateMaterialNode::CreateMaterialNode(const std::string& label)
    : Node(label)
    , shaderPaths(addArrayInput<std::string>("shaderPaths", ""))
    , blendMode(addInput("blendMode", TTRendering::MaterialBlendMode::Opaque))
    , result(addOutput("result", RenderGraphGlobals::NULL_MATERIAL_HANDLE)) {
    _initializing = false;
}

void CreateMaterialNode::_compute() {
    std::vector<TTRendering::ShaderStageHandle> stages;
    for(const auto& shaderPath : shaderPaths.children())
        stages.push_back(RenderGraphGlobals::gContext->fetchShaderStage(shaderPath->value().data()));
    result.setValue(RenderGraphGlobals::gContext->createMaterial(RenderGraphGlobals::gContext->fetchShader(stages), blendMode.value()));
}

MaterialSetImageNode::MaterialSetImageNode(const std::string& label)
    : Node(label)
    , material(addInput("material", RenderGraphGlobals::NULL_MATERIAL_HANDLE))
    , image(addInput("image", RenderGraphGlobals::NULL_IMAGE_HANDLE))
    , uniformName(addInput<std::string>("uniformName", "")) {
    _initializing = false;
}

void MaterialSetImageNode::_compute() {
    auto& mtl = material.value();
    const auto& img = image.value();
    if (img.identifier() == RenderGraphGlobals::NULL_IMAGE_HANDLE.identifier() ||
        mtl.shader().identifier() == RenderGraphGlobals::NULL_SHADER_HANDLE.identifier())
        return;
    mtl.set(uniformName.value().data(), img);
}
    
CreateRenderPassNode::CreateRenderPassNode(const std::string& label)
    : Node(label)
    , clearColor(addInput("clearColor", TT::Vec4(0.0f, 0.0f, 0.0f, 0.0f)))
    , framebuffer(addInput("framebuffer", RenderGraphGlobals::NULL_FRAMEBUFFER_HANDLE))
    , result(addOutput<TTRendering::RenderPass*>("result", nullptr)) {
    _initializing = false;
}

void CreateRenderPassNode::_compute() {
    TTRendering::RenderPass* renderPass = new TTRendering::RenderPass;
    renderPass->clearColor = clearColor.value();
    const auto& fbo = framebuffer.value();
    if(fbo.identifier() == RenderGraphGlobals::NULL_FRAMEBUFFER_HANDLE.identifier())
        renderPass->clearFramebuffer();
    else
        renderPass->setFramebuffer(framebuffer.value());
    result.setValue(renderPass);
}

DrawQuadNode::DrawQuadNode(const std::string& label)
    : Node(label) 
    , material(addInput("material", RenderGraphGlobals::NULL_MATERIAL_HANDLE))
    , renderPass(addInput<TTRendering::RenderPass*>("renderPass", nullptr)) {
    _initializing = false;
}

void DrawQuadNode::_compute() {
    auto& renderPass_ = renderPass.value();
    if (!renderPass_) return;
    const auto& mtl = material.value();
    if (mtl.shader().identifier() == RenderGraphGlobals::NULL_MATERIAL_HANDLE.shader().identifier()) return;
    renderPass_->addToDrawQueue(*RenderGraphGlobals::gQuadMesh, mtl);
}