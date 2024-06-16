#include "rendering_nodes.h"

namespace RenderGraphGlobals {
    TTRendering::RenderingContext* gContext;
    TTRendering::MeshHandle* gQuadMesh;
}

CreateImageNode::CreateImageNode(const std::string& label)
    : Node(label)
    , width(addInput<U16Socket>("width", 128))
    , height(addInput<U16Socket>("height", 128))
    , format(addInput<ImageFormatSocket>("format", TTRendering::ImageFormat::RGBA32F))
    , interpolation(addInput<ImageInterpolationSocket>("interpolation", TTRendering::ImageInterpolation::Linear))
    , tiling(addInput<ImageTilingSocket>("tiling", TTRendering::ImageTiling::Clamp))
    , factor(addInput<U16Socket>("factor", 0))
    , result(addOutput<ImageHandleSocket>("result", TTRendering::ImageHandle::Null)) {
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
    , colorBuffers(addArrayInput<ImageHandleSocket>("colorBuffers", TTRendering::ImageHandle::Null))
    , depthBuffer(addInput<ImageHandleSocket>("depthBuffer", TTRendering::ImageHandle::Null))
    , result(addOutput<FramebufferHandleSocket>("result", TTRendering::FramebufferHandle::Null)) {
    _initializing = false;
}

void CreateFramebufferNode::_compute() {
    std::vector<TTRendering::ImageHandle> cbos;
    for(const auto& child : colorBuffers.children()) {
        const auto& cbo = child->value();
        if (cbo != TTRendering::ImageHandle::Null)
            cbos.push_back(cbo);
    }

    const auto& dbo = depthBuffer.value();
    if (dbo != TTRendering::ImageHandle::Null)
        result.setValue(RenderGraphGlobals::gContext->createFramebuffer(cbos, &dbo));
    else if(cbos.size() > 0)
        result.setValue(RenderGraphGlobals::gContext->createFramebuffer(cbos));
    else
        result.setValue(TTRendering::FramebufferHandle::Null);
}

CreateMaterialNode::CreateMaterialNode(const std::string& label)
    : Node(label)
    , shaderPaths(addArrayInput<StringSocket>("shaderPaths", ""))
    , blendMode(addInput<MaterialBlendModeSocket>("blendMode", TTRendering::MaterialBlendMode::Opaque))
    , result(addOutput<MaterialHandleSocket>("result", TTRendering::MaterialHandle::Null)) {
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
    , material(addInput<MaterialHandleSocket>("material", TTRendering::MaterialHandle::Null))
    , image(addInput<ImageHandleSocket>("image", TTRendering::ImageHandle::Null))
    , uniformName(addInput<StringSocket>("uniformName", "")) {
    _initializing = false;
}

void MaterialSetImageNode::_compute() {
    TTRendering::MaterialHandle& mtl = material.value();
    const auto& img = image.value();
    if (img == TTRendering::ImageHandle::Null ||
        mtl == TTRendering::MaterialHandle::Null)
        return;
    mtl.set(uniformName.value().data(), img);
}
    
CreateRenderPassNode::CreateRenderPassNode(const std::string& label)
    : Node(label)
    , clearColor(addInput<Vec4Socket>("clearColor", TT::Vec4(0.0f, 0.0f, 0.0f, 0.0f)))
    , framebuffer(addInput<FramebufferHandleSocket>("framebuffer", TTRendering::FramebufferHandle::Null))
    , result(addOutput<RenderPassSocket>("result", nullptr)) {
    _initializing = false;
}

void CreateRenderPassNode::_compute() {
    TTRendering::RenderPass* renderPass = new TTRendering::RenderPass;
    renderPass->clearColor = clearColor.value();
    const auto& fbo = framebuffer.value();
    if(fbo == TTRendering::FramebufferHandle::Null)
        renderPass->clearFramebuffer();
    else
        renderPass->setFramebuffer(framebuffer.value());
    result.setValue(renderPass);
}

DrawQuadNode::DrawQuadNode(const std::string& label)
    : Node(label) 
    , material(addInput<MaterialHandleSocket>("material", TTRendering::MaterialHandle::Null))
    , renderPass(addInput<RenderPassSocket>("renderPass", nullptr)) {
    _initializing = false;
}

void DrawQuadNode::_compute() {
    auto& renderPass_ = renderPass.value();
    if (!renderPass_) return;
    const auto& mtl = material.value();
    if (mtl == TTRendering::MaterialHandle::Null) return;
    renderPass_->addToDrawQueue(*RenderGraphGlobals::gQuadMesh, mtl);
}
