#version 140
#pragma optimize(on)

precision highp float;

uniform mat4 light_pads;
uniform mat4 material_adse;
uniform	float material_shininess;

uniform sampler2D diffuse_texture;

in vec4 eye_position;
in vec3 frag_normal;
in vec2 frag_uv;

invariant out vec4 frag_color;

float dot(vec4 lsh, vec4 rhs) {
	return (lsh.x*rhs.x) + (lsh.y*rhs.y) + (lsh.z*rhs.z) + (lsh.w*rhs.w);
}

vec4 phong_shading(vec4 eye_pos, vec4 norm) {
	vec4 s = normalize( light_pads[0] - eye_pos );
	vec4 v = normalize( -eye_pos );
	vec4 r = reflect( -s, norm );
	vec4 ambient = light_pads[1] * material_adse[0];
	float s_dot_nrm = max( dot(s,norm), 0.0 );
	vec4  diffuse = light_pads[2] * material_adse[1] * s_dot_nrm;
	vec4 specular;
	if( s_dot_nrm > 0.0 ) {
		float shininess = pow( max( dot(r,v), 0.0 ), material_shininess);
		vec4 specular = (light_pads[3] * material_adse[2]) * shininess;
		return ambient + clamp(diffuse,0.0, 1.0) +  clamp(specular, 0.0, 1.0);
	}
	return ambient + clamp(diffuse,0.0, 1.0);
}

const vec4 GAMMA = vec4(1.0 / 2.2);

void main(void) {
	vec4 color = texture(diffuse_texture, frag_uv);
	frag_color = pow(color, GAMMA) + phong_shading(eye_position, vec4(frag_normal,0.0) );
}
