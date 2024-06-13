#version 450

layout (location = 0) in vec2 vUv;

layout(location = 0) out vec4 cd0;

uniform sampler2D uImage;

void main() {
	cd0 = texture(uImage, vUv);
    cd0.z = 1.0;
}
