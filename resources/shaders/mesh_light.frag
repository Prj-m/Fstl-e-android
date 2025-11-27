#version 300 es

precision mediump float;

uniform float zoom;
uniform vec4 ambient_light_color;
uniform vec4 directive_light_color;
uniform vec3 directive_light_direction;
uniform bool useWire;
uniform vec3 wireColor;
uniform float wireWidth;

// Layer-peeling clip plane (object-space Z)
uniform bool layerClipEnabled;
uniform float layerClipZ;

in vec3 ec_pos;
in vec3 altitude;  // Note: noperspective not well supported in ES, removed
in vec3 gObjPos;

out vec4 fragColor;

void main() {
    if (layerClipEnabled && gObjPos.z > layerClipZ) {
        discard;
    }
    // Normalize light direction
    vec3 dir = normalize(directive_light_direction);

    // normal vector
    vec3 ec_normal = normalize(cross(dFdx(ec_pos), dFdy(ec_pos)));
    ec_normal.z *= zoom;
    ec_normal = normalize(ec_normal);


    vec3 color =  ambient_light_color.w * ambient_light_color.xyz + directive_light_color.w * dot(ec_normal,dir) * directive_light_color.xyz;

    if (useWire) {
        float d = min(min(altitude.x, altitude.y),altitude.z);
        float mixVal = smoothstep(wireWidth-1.0, wireWidth+1.0,d);
        color = mix(wireColor,color,mixVal);
    }

    fragColor = vec4(color, 1.0);
}
