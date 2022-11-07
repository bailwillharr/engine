#version 330

layout (location = 0) in vec3 v_Position;
layout (location = 1) in vec3 v_Norm;
layout (location = 2) in vec2 v_UV;

uniform mat4 modelMat;
uniform mat4 viewMat;
uniform mat4 projMat;

uniform vec3 lightPos;

out vec3 f_Pos;
out vec3 f_Norm;
out vec2 f_UV;

out vec3 f_lightPos;

void main()
{
    gl_Position = projMat * viewMat * modelMat * vec4(v_Position, 1.0);
    f_Pos = vec3(viewMat * modelMat * vec4(v_Position, 1.0));
    f_Norm = mat3(transpose(inverse(viewMat * modelMat))) * v_Norm;
    f_UV = v_UV;
    f_lightPos = vec3(viewMat * vec4(lightPos, 1.0));
}
