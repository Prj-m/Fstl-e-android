#version 300 es

precision highp float;

in vec3 vertex_position;

uniform mat4 transform_matrix;
uniform mat4 view_matrix;

out vec3 world_pos;   // Model-rotation space; fragment shaders derive the face normal from it
out vec3 ec_pos;      // Eye coordinate position for mesh_light shader
out vec3 vObjPos;     // Object-space position for layer-peeling clip plane

void main() {
    vec4 world = transform_matrix * vec4(vertex_position, 1.0);
    vec4 ec_position = view_matrix * world;
    gl_Position = ec_position;
    ec_pos = ec_position.xyz;
    vObjPos = vertex_position;
    world_pos = world.xyz;
}
