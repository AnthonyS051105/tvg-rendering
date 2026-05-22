#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

struct Material {
    vec3  ambient;
    vec3  specular;
    float shininess;
};

struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Material  material;
uniform Light     light;
uniform vec3      viewPos;
uniform sampler2D diffuseMap;
uniform int       preset;      // 1=texture, 2=wood, 3=marble, 4=plastic, 5=jade
uniform int       hasTexture;

// ── Procedural noise helpers ──────────────────────────────────────────────────

float hash(float n) { return fract(sin(n) * 43758.5453123); }

float hash2(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Smooth value noise in 2D
float noise2(vec2 p) {
    vec2  i = floor(p);
    vec2  f = fract(p);
    vec2  u = f * f * (3.0 - 2.0 * f);
    float a = hash2(i);
    float b = hash2(i + vec2(1.0, 0.0));
    float c = hash2(i + vec2(0.0, 1.0));
    float d = hash2(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// Fractal Brownian Motion (4 octaves)
float fbm(vec2 p) {
    float v = 0.0, amp = 0.5;
    for (int i = 0; i < 4; i++) {
        v   += amp * noise2(p);
        p   *= 2.1;
        amp *= 0.5;
    }
    return v;
}

// ── Procedural textures ───────────────────────────────────────────────────────

// Preset 2 – Oak wood: ring pattern + grain noise along X
vec3 woodColor(vec2 uv) {
    vec2  scaled = uv * vec2(6.0, 18.0);
    float grain  = fbm(scaled * 0.4);
    float rings  = fract((length(uv * vec2(4.0, 1.0)) + grain * 1.8) * 6.0);
    rings = smoothstep(0.0, 0.5, rings) - smoothstep(0.5, 1.0, rings);

    vec3 darkWood  = vec3(0.28, 0.15, 0.05);
    vec3 lightWood = vec3(0.65, 0.40, 0.18);
    return mix(darkWood, lightWood, rings);
}

// Preset 3 – White marble: sinusoidal veins + turbulence
vec3 marbleColor(vec2 uv) {
    float turb = fbm(uv * 5.0) * 2.0;
    float vein = 0.5 + 0.5 * sin((uv.x * 8.0 + uv.y * 4.0 + turb) * 3.14159);
    vein = pow(vein, 2.5);

    vec3 white  = vec3(0.92, 0.91, 0.90);
    vec3 veinC  = vec3(0.30, 0.28, 0.32);
    return mix(white, veinC, vein * 0.55);
}

// Preset 4 – Blue matte plastic: subtle surface variation + dirt
vec3 plasticColor(vec2 uv) {
    float n = fbm(uv * 20.0);
    vec3  base = vec3(0.12, 0.28, 0.72);
    vec3  dark = vec3(0.08, 0.18, 0.52);
    return mix(base, dark, n * 0.35);
}

// Preset 5 – Jade: layered green with translucent depth variation
vec3 jadeColor(vec2 uv) {
    float n1 = fbm(uv * 3.0);
    float n2 = fbm(uv * 8.0 + vec2(3.7, 1.4));
    float t  = 0.5 * n1 + 0.5 * n2;

    vec3 jade0 = vec3(0.10, 0.38, 0.22);   // deep green
    vec3 jade1 = vec3(0.45, 0.72, 0.50);   // light green
    vec3 jade2 = vec3(0.20, 0.55, 0.38);   // mid
    vec3 col   = mix(jade0, jade1, t);
    col        = mix(col, jade2, fbm(uv * 15.0) * 0.3);
    return col;
}

// ─────────────────────────────────────────────────────────────────────────────

void main()
{
    vec3 baseDiffuse;
    if (preset == 1) {
        baseDiffuse = (hasTexture == 1)
            ? texture(diffuseMap, TexCoord).rgb
            : vec3(0.8);
    } else if (preset == 2) {
        baseDiffuse = woodColor(TexCoord);
    } else if (preset == 3) {
        baseDiffuse = marbleColor(TexCoord);
    } else if (preset == 4) {
        baseDiffuse = plasticColor(TexCoord);
    } else {
        baseDiffuse = jadeColor(TexCoord);
    }

    // Ambient
    vec3 ambient = light.ambient * material.ambient;

    // Diffuse
    vec3  norm     = normalize(Normal);
    vec3  lightDir = normalize(light.position - FragPos);
    float diff     = max(dot(norm, lightDir), 0.0);
    vec3  diffuse  = light.diffuse * diff * baseDiffuse;

    // Specular
    vec3  viewDir    = normalize(viewPos - FragPos);
    vec3  reflectDir = reflect(-lightDir, norm);
    float spec       = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3  specular   = light.specular * spec * material.specular;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
