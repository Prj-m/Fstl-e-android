#version 300 es

precision mediump float;

uniform float zoom;

// Layer-peeling clip plane (object-space Z)
uniform bool layerClipEnabled;
uniform highp float layerClipZ;  // highp: mediump is fp16 on many GPUs

in highp vec3 world_pos;
in highp vec3 vObjPos;
out vec4 fragColor;

// Flat face normal from screen-space derivatives: no per-vertex normal buffer
// is needed, so meshes can share vertices. gl_FrontFacing restores the
// winding-based (outward) orientation. highp: per-pixel derivatives of the
// unit-scaled model are far below the mediump range.
vec3 faceNormal() {
    highp vec3 n = normalize(cross(dFdx(world_pos), dFdy(world_pos)));
    return gl_FrontFacing ? n : -n;
}

void main() {
    if (layerClipEnabled && vObjPos.z > layerClipZ) {
        discard;
    }
    // Use world normal
    vec3 ec_normal = faceNormal();
    
    //rotated 10deg around the red axis for better color match
    float x = dot(ec_normal, vec3(1.0, 0.0, 0.0));
    float y = dot(ec_normal, vec3(0.0, 0.985, 0.174));
    float z = dot(ec_normal, vec3(0.0, -0.174, 0.985));

    fragColor = vec4(0.5-0.5*x, 0.5-0.5*y, 0.5+0.5*z, 1.0);
}
