#version 300 es

precision mediump float;

// Layer-peeling clip plane (object-space Z)
uniform bool layerClipEnabled;
uniform float layerClipZ;

in vec3 world_normal;
in vec3 vObjPos;
out vec4 fragColor;

void main() {
    if (layerClipEnabled && vObjPos.z > layerClipZ) {
        discard;
    }
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
