#version 120

#extension GL_ARB_texture_rectangle : enable

uniform sampler2DRect tex0;
uniform float brightness;
uniform float alpha;

varying vec2 vTexCoord;

void main() {
    vec4 texColor = texture2DRect(tex0, vTexCoord);

    // Convert to grayscale using luminance formula (ITU-R BT.709)
    // This weights colors by human perception: green is brightest, blue is darkest
    float luminance = dot(texColor.rgb, vec3(0.2126, 0.7152, 0.0722));

    // Apply brightness and alpha
    vec3 gray = vec3(luminance * brightness);

    gl_FragColor = vec4(gray, texColor.a * alpha);
}
