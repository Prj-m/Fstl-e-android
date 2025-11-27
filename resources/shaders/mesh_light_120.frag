#version 300 es

precision mediump float;

uniform float zoom;
uniform vec4 ambient_light_color;
uniform vec4 directive_light_color;
uniform vec3 directive_light_direction;

// Layer-peeling clip plane (object-space Z)
uniform bool layerClipEnabled;
uniform float layerClipZ;

in vec3 ec_pos;
in vec3 world_normal;
in vec3 vObjPos;
out vec4 fragColor;

void main() {
    if (layerClipEnabled && vObjPos.z > layerClipZ) {
        discard;
    }
    // Normalize light direction
    vec3 dir = normalize(directive_light_direction);

    // Use pre-computed normals from vertex data for proper flat shading
    vec3 norm = normalize(world_normal);
    
    // Use raw dot product (no Half-Lambert) for minimal shadows only at edges
    float NdotL = dot(norm, dir);

    vec3 ambient = ambient_light_color.rgb * ambient_light_color.a;
    vec3 directive = directive_light_color.rgb * directive_light_color.a * NdotL;
    vec3 color = ambient + directive;

    fragColor = vec4(color, 1.0);
}
