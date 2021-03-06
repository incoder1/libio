#version 420 compatibility

#pragma optimize(on)

#ifdef GL_ES
precision mediump float;
#else
precision highp float;
#endif

uniform mat4 light_pads;
uniform mat4 material_adse;
uniform	float material_shininess;

in vec4 tess_position;
in vec4 tess_normal;


invariant out vec4 frag_color;

vec4 phong_shading(vec4 eye_pos, vec4 norm) {
	vec4 s;
	if(0.0 == light_pads[0].w)
		s = normalize( light_pads[0] );
	else
		s = normalize( light_pads[0] - eye_pos );
	vec4 v = normalize( -eye_pos );
	vec4 r = reflect( -s, norm );
	vec4 ambient = light_pads[1] * material_adse[0];
	float cos_theta = max( dot(s,norm), 0.0 );
	vec4  diffuse = light_pads[2] * material_adse[1] * cos_theta;
	vec4 specular;
	if( cos_theta > 0.0 ) {
		float shininess = pow( max( dot(r,v), 0.0 ), material_shininess);
		vec4 specular = (light_pads[3] * material_adse[2]) * shininess;
		return ambient + clamp(diffuse,0.0, 1.0) +  clamp(specular, 0.0, 1.0);
	}
	return ambient + clamp(diffuse,0.0, 1.0);
}

void main(void) {
	if( gl_FrontFacing ) {
		frag_color = phong_shading( tess_position,tess_normal);
	} else {
		frag_color = phong_shading( tess_position,-tess_normal);
	}
}
