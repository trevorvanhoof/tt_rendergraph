#pragma once

#include "dg.h"

#include "../tt_rendering/tt_rendering.h"
#include "../tt_cpplib/tt_cgmath.h"

// TODO: This is clearly not good. The parent application must set this before computing anything in the graph.
namespace RenderGraphGlobals {
    extern TTRendering::RenderingContext* gContext;
    extern TTRendering::MeshHandle* gQuadMesh;
}

// Note: all of these nodes allocate GPU resources without cleaning up after themselves.
// Pipeline graphs are intended to be computed only once, in their entirety, and then never touched again.

class CreateImageNode final : public Node {
public:
    std::string debugStr() const override { 
        return "\"CreateImageNode\":" + label(); 
    }

    Socket<unsigned short>& width;
    Socket<unsigned short>& height;
    Socket<TTRendering::ImageFormat>& format;
    Socket<TTRendering::ImageInterpolation>& interpolation;
    Socket<TTRendering::ImageTiling>& tiling;
    Socket<unsigned short>& factor;
    Socket<TTRendering::ImageHandle>& result;

    CreateImageNode(const std::string& label = "");

private:
    void _compute() override;
};

class CreateFramebufferNode final : public Node {
public:
    std::string debugStr() const override {
        return "\"CreateFramebufferNode\":" + label();
    }

    SocketArray<TTRendering::ImageHandle>& colorBuffers;
    Socket<TTRendering::ImageHandle>& depthBuffer;
    Socket<TTRendering::FramebufferHandle>& result;

    CreateFramebufferNode(const std::string& label = "");

private:
    void _compute() override;
};

class CreateMaterialNode final : public Node {
public:
    std::string debugStr() const override {
        return "\"CreateMaterialNode\":" + label();
    }

    SocketArray<std::string> shaderPaths;
    Socket<TTRendering::MaterialBlendMode> blendMode;
    Socket<TTRendering::MaterialHandle> result;

    CreateMaterialNode(const std::string& label = "");

private:
    void _compute() override;
};

class MaterialSetImageNode final : public Node {
public:
    std::string debugStr() const override {
        return "\"MaterialSetImageNode\":" + label();
    }

    Socket<TTRendering::MaterialHandle> material;
    Socket<TTRendering::ImageHandle> image;
    Socket<std::string> uniformName;

    MaterialSetImageNode(const std::string& label = "");

private:
    void _compute() override;
};

class CreateRenderPassNode final : public Node {
public:
    std::string debugStr() const override {
        return "\"CreateRenderPassNode\":" + label();
    }

    Socket<TT::Vec4> clearColor;
    Socket<TTRendering::FramebufferHandle> framebuffer;
    Socket<TTRendering::RenderPass*> result;

    CreateRenderPassNode(const std::string& label = "");

private:
    void _compute() override;
};

class DrawQuadNode final : public Node {
public:
    std::string debugStr() const override {
        return "\"DrawQuadNode\":" + label();
    }

    Socket<TTRendering::MaterialHandle> material;
    Socket<TTRendering::RenderPass*> renderPass;

    DrawQuadNode(const std::string& label = "");

private:
    void _compute() override;
};
