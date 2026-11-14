#version 330 core

out vec4 FragColor;

in vec2 TexCoord;

uniform vec3 color;        // Boja pravougaonika ako nema teksture
uniform sampler2D tex;     // Tekstura
uniform bool useTexture;   // Da li koristiti teksturu ili boju
uniform bool highlight;    // Da li dugme treba da svetli

void main()
{
    vec4 base = useTexture ? texture(tex, TexCoord) : vec4(color,1.0);
    if(highlight){
        // Pojacamo boje + doda se beli sjaj
        base.rgb = base.rgb * 0.5 + vec3(1.0, 1.0, 1.0) * 1.0;
    }

    FragColor = base;
}
