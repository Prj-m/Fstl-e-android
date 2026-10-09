#version 300 es

precision highp float;

// Writes the fragment depth packed into RGBA8 so it can be read back with
// glReadPixels (OpenGL ES cannot read the depth buffer directly). Used to find
// the surface point under a finger, which becomes the rotation pivot.

uniform bool layerClipEnabled;
uniform highp float layerClipZ;

in highp vec3 vObjPos;
out vec4 fragColor;

void main() {
    if (layerClipEnabled && vObjPos.z > layerClipZ) {
        discard;
    }
    highp vec4 encoded = fract(gl_FragCoord.z * vec4(1.0, 255.0, 65025.0, 16581375.0));
    encoded -= encoded.yzww * vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0);
    fragColor = encoded;
}
