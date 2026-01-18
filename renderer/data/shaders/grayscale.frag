#version 150

uniform sampler2DRect tex0;
uniform float brightness;
uniform float alpha;

in vec2 vTexCoord;
out vec4 fragColor;

void main() {
    vec4 texColor = texture(tex0, vTexCoord);

    // Convert to grayscale using luminance formula (ITU-R BT.709)
    // This weights colors by human perception: green is brightest, blue is darkest
    float luminance = dot(texColor.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Apply brightness and alpha
    vec3 gray = vec3(luminance * brightness);

    fragColor = vec4(gray, texColor.a * alpha);
}
