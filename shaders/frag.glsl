#version 330 core

uniform float maximumValue;
uniform float minimumValue;
uniform sampler2D fieldTexture;
uniform sampler2D obstacleTexture;
in vec2 TexCoord;
out vec4 FragColor;

vec3 applyColorMap(float t)
{
    if (t < 0.25)
    {
        return mix(
            vec3(0.0, 0.0, 0.5),
            vec3(0.0, 0.5, 1.0),
            t / 0.25
        );
    }
    else if (t < 0.5)
    {
        return mix(
            vec3(0.0, 0.5, 1.0),
            vec3(0.0, 1.0, 0.5),
            (t - 0.25) / 0.25
        );
    }
    else if (t < 0.75)
    {
        return mix(
            vec3(0.0, 1.0, 0.5),
            vec3(1.0, 1.0, 0.0),
            (t - 0.5) / 0.25
        );
    }
    else
    {
        return mix(
            vec3(1.0, 1.0, 0.0),
            vec3(1.0, 0.0, 0.0),
            (t - 0.75) / 0.25
        );
    }
}

void main()
{
    float obstacle = texture(obstacleTexture, TexCoord).r;

    if (obstacle > 0.5)
    {
        FragColor = vec4(0.15, 0.15, 0.15, 1.0);
        return;
    }

    float value = texture(fieldTexture, TexCoord).r;
    float range = maximumValue - minimumValue;
    float normalizedValue = (value - minimumValue) / range;
    normalizedValue = clamp(normalizedValue, 0.0, 1);
    vec3 color = applyColorMap(normalizedValue);
    FragColor = vec4(color, 1.0);
}