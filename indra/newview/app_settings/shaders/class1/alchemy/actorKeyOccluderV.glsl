// [ActorKey] Virtual Prism set occluder; same agent-space corners as beauty.
uniform mat4 modelview_projection_matrix;
invariant gl_Position; // Match the Prism beauty vertex stage's depth contract.
in vec3 position;
void main() { gl_Position = modelview_projection_matrix * vec4(position, 1.0); }
