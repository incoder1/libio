#version 330 compatibility

#pragma optimize(on)

#ifdef GL_ES
precision mediump float;
#else
precision highp float;
#endif

invariant gl_Position;

uniform mat4 mvp;
uniform mat4 mv;
uniform mat4 nm;

layout(location = 0) in vec3 vertex_coord;
layout(location = 1) in vec3 vertex_normal;
layout(location = 2) in vec2 vertex_uv;

out vec4 eye_position;
out vec4 frag_normal;
out vec2 frag_uv;

void main(void) {
	vec4 vtx_pos = vec4(vertex_coord,1.0);
	eye_position = mv * vtx_pos;
	frag_normal =  normalize( nm * vec4(vertex_normal, 0.0 ) );
	frag_uv = vertex_uv;
	gl_Position = mvp * vtx_pos;
}
