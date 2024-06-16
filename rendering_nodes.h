#pragma once

#include "dg.h"

#include "../tt_rendering/tt_rendering.h"
#include "../tt_cpplib/tt_cgmath.h"

// TODO: This is clearly not good. The parent application must set this before computing anything in the graph.
namespace RenderGraphGlobals {
    extern TTRendering::RenderingContext* gContext;
    extern TTRendering::MeshHandle* gQuadMesh;

    // TODO: Move these into tt_rendering so we can make these constructors private again.
    // TODO: We use those for comparisons, and we can safely compare just the identifier, we should implement that in an == operator in tt_rendering instead.
    extern TTRendering::ImageHandle NULL_IMAGE_HANDLE;
    extern TTRendering::FramebufferHandle NULL_FRAMEBUFFER_HANDLE;
    extern TTRendering::ShaderHandle NULL_SHADER_HANDLE;
    extern TTRendering::MaterialHandle NULL_MATERIAL_HANDLE;
}

// Hahah kut C++ waarom is typeid(T).name() niet gewoon dit en waarom kan ik niet gewoon een string in een template stoppen. Generate dit zelf ofzo.
namespace {
    char F32[] = "F32";
    char U16[] = "U16";
    char ImageFormat[] = "ImageFormat";
    char ImageInterpolation[] = "ImageInterpolation";
    char ImageTiling[] = "ImageTiling";
    char MaterialBlendMode[] = "MaterialBlendMode";
    char ImageHandle[] = "ImageHandle";
    char FramebufferHandle[] = "FramebufferHandle";
    char MaterialHandle[] = "MaterialHandle";
    char RenderPass[] = "RenderPass";
    char String[] = "String";
    char Vec4[] = "Vec4";
}

// Note: all of these nodes allocate GPU resources without cleaning up after themselves.
// Pipeline graphs are intended to be computed only once, in their entirety, and then never touched again.

// These sockets are serializable:
template<typename T, const char* NAME> class NumericSocket : public Socket<T, NumericSocket<T, NAME>, NAME> {
public:
    using Socket<T, NumericSocket<T, NAME>, NAME>::Socket; 

protected:
    bool deserializeValue(const TTJson::Value& value) override {
        if (value.isDouble()) {
            if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
                // WTF MSVC?
                Socket<T, NumericSocket<T, NAME>, NAME>::setValue((T)value.asDouble());
            else
                // WTF MSVC?
                Socket<T, NumericSocket<T, NAME>, NAME>::setValue((T)(long long)value.asDouble());
        } else if(value.isInt())
            // WTF MSVC?
            Socket<T, NumericSocket<T, NAME>, NAME>::setValue((T)value.asInt());
        else 
            return false;
        return true;
    }

    TTJson::Value serializeValue() const override {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
            return (double)Socket<T, NumericSocket<T, NAME>, NAME>::_value;
        return (long long)Socket<T, NumericSocket<T, NAME>, NAME>::_value;
    }
};

class StringSocket : public Socket<std::string, StringSocket, String> {
    using Socket::Socket; 

protected:
    bool deserializeValue(const TTJson::Value& value) override {
        if (!value.isString())
            return false;
        setValue(value.asString());
        return true;
    }

    TTJson::Value serializeValue() const override {
        return (TTJson::str_t)_value;
    }
};

class Vec4Socket : public Socket<TT::Vec4, Vec4Socket, Vec4> {
    using Socket::Socket; 

protected:
    bool deserializeValue(const TTJson::Value& value) override {
        TT::Vec4 dst;
        if (!value.isArray()) return false;
        const auto& array = value.asArray();
        if (array.size() != 4) return false;
        
        const auto& x = array[0];
        if (x.isDouble()) dst.x = (float)x.asDouble();
        if (x.isInt()) dst.x = (float)x.asInt();

        const auto& y = array[1];
        if (y.isDouble()) dst.y = (float)y.asDouble();
        if (y.isInt()) dst.y = (float)y.asInt();

        const auto& z = array[2];
        if (y.isDouble()) dst.y = (float)y.asDouble();
        if (y.isInt()) dst.y = (float)y.asInt();

        const auto& w = array[3];
        if (w.isDouble()) dst.w = (float)w.asDouble();
        if (w.isInt()) dst.w = (float)w.asInt();

        setValue(dst);
        return true;
    }

    TTJson::Value serializeValue() const override {
        TTJson::Array result;
        result.push_back((double)_value.x);
        result.push_back((double)_value.y);
        result.push_back((double)_value.z);
        result.push_back((double)_value.w);
        return result;
    }
};

typedef NumericSocket<float, F32> F32Socket;
typedef NumericSocket<unsigned short, U16> U16Socket;
typedef NumericSocket<TTRendering::ImageFormat, ImageFormat> ImageFormatSocket;
typedef NumericSocket<TTRendering::ImageInterpolation, ImageInterpolation> ImageInterpolationSocket;
typedef NumericSocket<TTRendering::ImageTiling, ImageTiling> ImageTilingSocket;
typedef NumericSocket<TTRendering::MaterialBlendMode, MaterialBlendMode> MaterialBlendModeSocket;

// These sockets are NOT serializable:
class ImageHandleSocket : public Socket<TTRendering::ImageHandle, ImageHandleSocket, ImageHandle> { using Socket::Socket; };
class FramebufferHandleSocket : public Socket<TTRendering::FramebufferHandle, FramebufferHandleSocket, FramebufferHandle> { using Socket::Socket; };
class MaterialHandleSocket : public Socket<TTRendering::MaterialHandle, MaterialHandleSocket, MaterialHandle> { using Socket::Socket; };
class RenderPassSocket : public Socket<TTRendering::RenderPass*, RenderPassSocket, RenderPass> { using Socket::Socket; };

class CreateImageNode final : public Node {
public:
    std::string typeName() const override { return "CreateImageNode"; }

    // TODO: Can we delete the copy and move constructors to avoid accidentally declaring these by value"?
    U16Socket& width;
    U16Socket& height;
    ImageFormatSocket& format;
    ImageInterpolationSocket& interpolation;
    ImageTilingSocket& tiling;
    U16Socket& factor;
    ImageHandleSocket& result;

    CreateImageNode(const std::string& label = "");

private:
    void _compute() override;
};

class CreateFramebufferNode final : public Node {
public:
    std::string typeName() const override { return "CreateFramebufferNode"; }

    SocketArray<ImageHandleSocket>& colorBuffers;
    ImageHandleSocket& depthBuffer;
    FramebufferHandleSocket& result;

    CreateFramebufferNode(const std::string& label = "");

private:
    void _compute() override;
};

class CreateMaterialNode final : public Node {
public:
    std::string typeName() const override { return "CreateMaterialNode"; }

    SocketArray<StringSocket>& shaderPaths;
    MaterialBlendModeSocket& blendMode;
    MaterialHandleSocket& result;

    CreateMaterialNode(const std::string& label = "");

private:
    void _compute() override;
};

class MaterialSetImageNode final : public Node {
public:
    std::string typeName() const override { return "MaterialSetImageNode"; }

    MaterialHandleSocket& material;
    ImageHandleSocket& image;
    StringSocket& uniformName;

    MaterialSetImageNode(const std::string& label = "");

private:
    void _compute() override;
};

class CreateRenderPassNode final : public Node {
public:
    std::string typeName() const override { return "CreateRenderPassNode"; }

    Vec4Socket& clearColor;
    FramebufferHandleSocket& framebuffer;
    RenderPassSocket& result;

    CreateRenderPassNode(const std::string& label = "");

private:
    void _compute() override;
};

class DrawQuadNode final : public Node {
public:
    std::string typeName() const override { return "DrawQuadNode"; }

    MaterialHandleSocket& material;
    RenderPassSocket& renderPass;

    DrawQuadNode(const std::string& label = "");

private:
    void _compute() override;
};
