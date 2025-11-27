#version 300 es

precision highp float;

in vec3 vertex_position;
in vec3 vertex_color;  // Actually normal data

uniform mat4 transform_matrix;
uniform mat4 view_matrix;

out vec3 world_normal;
out vec3 ec_pos;      // Eye coordinate position for mesh_light shader
out vec3 vObjPos;     // Object-space position for layer-peeling clip plane

void main() {
    vec4 ec_position = view_matrix * transform_matrix * vec4(vertex_position, 1.0);
    gl_Position = ec_position;
    ec_pos = ec_position.xyz;
    vObjPos = vertex_position;
    // Transform normal to world space
    world_normal = mat3(transform_matrix) * vertex_color;
}
