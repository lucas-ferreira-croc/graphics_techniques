#version 450

layout(binding = 1) uniform sampler2D samplerColorMap;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inViewVec;
layout(location = 3) in vec3 inLightVec;

layout(location = 0) out vec4 outFragColor;

void main()
{
	vec3 color = vec3(mix(inColor, vec3(dot(vec3(0.2126, 0.7152, 0.0722), inColor)), 0.65));

	vec3 ambient = color * vec3(1.0);
	vec3 Normal = normalize(inNormal);
	vec3 Light = normalize(inLightVec);
	vec3 View = normalize(inViewVec);
	vec3 Reflection = reflect(-Light, Normal);
	vec3 diffuse = max(dot(Normal, Light), 0.0) * color;
	vec3 specular = pow(max(dot(Reflection, View), 0.0), 16.0) * vec3(0.75);
	outFragColor = vec4(ambient + diffuse * 1.75 + specular, 1.0);

	float intensity = dot(Normal, Light);
	float shade = 1.0;

	shade = intensity < 0.5 ? 0.75 : shade;
	shade = intensity < 0.35 ? 0.6 : shade;
	shade = intensity < 0.25 ? 0.5 : shade;
	shade = intensity < 0.1 ? 0.25 : shade;

	outFragColor.rgb = inColor * 3.0 * shade;
}