#version 300 es

precision mediump float;

// Uniforms for dynamic shader preferences
uniform vec4 ambient_light_color;      // RGB + factor (w component)
uniform vec4 directive_light_color;    // RGB + factor (w component)  
uniform vec3 directive_light_direction;

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

    vec3 norm = faceNormal();
    vec3 light_dir = normalize(directive_light_direction);
    
    // Use half-lambert lighting for softer shadows (wraps around more)
    float NdotL = dot(norm, light_dir);
    float diffuse = NdotL * 0.5 + 0.5;  // Maps [-1,1] to [0,1] for softer lighting
    
    vec3 ambient = ambient_light_color.rgb * ambient_light_color.a;
    vec3 directive = directive_light_color.rgb * directive_light_color.a * diffuse;
    vec3 color = ambient + directive;
    fragColor = vec4(color, 1.0);
}
