#version 300 es

precision mediump float;

uniform float zoom;

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
    // Use world normal
    vec3 ec_normal = normalize(world_normal);
    
    //rotated 10deg around the red axis for better color match
    float x = dot(ec_normal, vec3(1.0, 0.0, 0.0));
    float y = dot(ec_normal, vec3(0.0, 0.985, 0.174));
    float z = dot(ec_normal, vec3(0.0, -0.174, 0.985));

    fragColor = vec4(0.5-0.5*x, 0.5-0.5*y, 0.5+0.5*z, 1.0);
}
