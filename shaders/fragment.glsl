#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

out vec4 FragColor;

// matID: 0=qita(meja+kaki), 1=lvzhi(taplak), 2=zhuoyi(kursi),
//        3=material(bunga),  4=boli(mangkuk), 5=niunai(piring/vas)
uniform int       preset;
uniform int       matID;
uniform int       hasTexture;
uniform sampler2D diffuseMap;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightAmb;
uniform vec3 lightDiff;
uniform vec3 lightSpec;

// ── Noise helpers ─────────────────────────────────────────────────────────────

float hash2(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float noise2(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i),           hash2(i+vec2(1,0)), u.x),
               mix(hash2(i+vec2(0,1)), hash2(i+vec2(1,1)), u.x), u.y);
}
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 4; i++) { v += a * noise2(p); p *= 2.1; a *= 0.5; }
    return v;
}
float fbm6(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 6; i++) { v += a * noise2(p); p *= 2.0; a *= 0.5; }
    return v;
}

// ── Shared procedural textures ────────────────────────────────────────────────

// Walnut wood (preset 2)
vec3 tex_walnut(vec2 uv) {
    float grain  = fbm6(uv * vec2(2.0, 14.0));
    float rings  = fract((length(uv * vec2(3.5, 0.8)) + grain * 2.2) * 8.0);
    rings = smoothstep(0.0, 0.45, rings) - smoothstep(0.45, 1.0, rings);
    float xgrain = fbm(uv * vec2(60.0, 1.5)) * 0.12;
    return mix(vec3(0.16,0.09,0.03), mix(vec3(0.38,0.22,0.09), vec3(0.52,0.32,0.14), rings), rings) + xgrain;
}

// Aged saddle leather (preset 2)
vec3 tex_leather_aged(vec2 uv, vec3 baseCol) {
    float swell  = fbm(uv * 2.5) * 0.18;
    float pores  = 1.0 - pow(fbm(uv * 38.0), 0.6) * 0.4;
    float scratch = fbm(vec2(uv.x * 90.0, uv.y * 3.0)) * 0.08;
    float wear   = fbm(uv * 5.0) * 0.15;
    return clamp(baseCol * pores * (0.82 + swell) - wear * 0.5 + scratch, 0.0, 1.0);
}

// Linen weave (preset 2)
vec3 tex_linen(vec2 uv, vec3 colA, vec3 colB) {
    vec2  sc   = uv * 40.0;
    float weft = smoothstep(0.38,0.50,fract(sc.x)) - smoothstep(0.50,0.62,fract(sc.x));
    float warp = smoothstep(0.38,0.50,fract(sc.y)) - smoothstep(0.50,0.62,fract(sc.y));
    return mix(colA, colB, clamp(max(weft,warp) + fbm(uv*10.0)*0.10, 0.0, 1.0));
}

// Organic leaf with vein network (preset 2, 3, 5)
// Vein berbasis noise agar tidak bergantung range UV model
vec3 tex_leaf_organic(vec2 uv) {
    float mainVein = fbm(uv * vec2(0.5, 6.0));
    float sideVein = pow(abs(sin((uv.y * 14.0 + fbm(uv * 3.0) * 2.0))), 8.0) * 0.5;
    float vein     = max(mainVein * 0.5, sideVein) * 0.35;
    float depth    = fbm(uv * 4.0);
    vec3  col      = mix(vec3(0.06,0.24,0.06), mix(vec3(0.18,0.48,0.10), vec3(0.38,0.68,0.18), depth), depth);
    return clamp(mix(col, vec3(0.38,0.68,0.18)*1.05, vein), vec3(0.04,0.18,0.04), vec3(1.0));
}

// Matte stoneware (preset 2)
vec3 tex_stoneware(vec2 uv, vec3 base) {
    float speck  = fbm6(uv * 22.0);
    float coarse = fbm(uv * 5.0);
    return mix(mix(base*0.70, base*1.10, speck), base*0.85, coarse*0.3);
}

// Aged bone china (preset 2)
vec3 tex_bone_china(vec2 uv) {
    float turb  = fbm(uv * 7.0) * 1.5;
    float crack = pow(0.5 + 0.5*sin((uv.x*6.0 + uv.y*3.0 + turb)*3.14159), 5.0);
    float warm  = fbm(uv * 12.0) * 0.06;
    return mix(vec3(0.94,0.92,0.87) + warm, vec3(0.70,0.68,0.62), crack * 0.18);
}

// ── Preset 3 textures: Scandinavian Modern ────────────────────────────────────

// Polished concrete: large pour variation + fine aggregate speckle
vec3 tex_concrete(vec2 uv) {
    float macro  = fbm(uv * 1.2);                     // large pour patterns
    float micro  = fbm6(uv * 28.0) * 0.5;             // fine aggregate
    float streak = fbm(vec2(uv.x*0.8, uv.y*15.0))*0.08; // vertical form streaks
    vec3  light  = vec3(0.78, 0.76, 0.74);
    vec3  dark   = vec3(0.52, 0.50, 0.49);
    return clamp(mix(light, dark, macro*0.6 + micro*0.3) - streak, 0.0, 1.0);
}

// Cotton/poplin: tight plain weave, bright white
vec3 tex_cotton(vec2 uv, vec3 base) {
    vec2  sc   = uv * 60.0;
    float weft = smoothstep(0.40,0.50,fract(sc.x)) - smoothstep(0.50,0.60,fract(sc.x));
    float warp = smoothstep(0.40,0.50,fract(sc.y)) - smoothstep(0.50,0.60,fract(sc.y));
    float fiber = noise2(uv * 18.0) * 0.04;
    return clamp(base - max(weft,warp)*0.06 - fiber, 0.0, 1.0);
}

// Velvet: short-pile directional sheen
vec3 tex_velvet(vec2 uv, vec3 baseCol) {
    // Pile direction variation — velvet changes color with viewing angle approximated via UV gradient
    float pile   = fbm(uv * 12.0);                    // pile compression variation
    float sheen  = fbm(vec2(uv.x*3.0, uv.y*60.0));   // directional highlight along pile
    vec3  shadow = baseCol * 0.55;
    vec3  bright = baseCol * 1.15;
    return clamp(mix(shadow, bright, pile*0.5 + sheen*0.35), 0.0, 1.0);
}

// Matte white ceramic: fine glaze crazing + subtle tonal variation
vec3 tex_white_ceramic(vec2 uv) {
    float base_n = fbm(uv * 8.0) * 0.04;
    float craze  = pow(noise2(uv * 45.0), 3.0) * 0.05; // very fine crazing
    return vec3(0.96, 0.95, 0.94) - base_n - craze;
}

// Raku pottery: dramatic black/grey crackle glaze
vec3 tex_raku(vec2 uv) {
    float turb   = fbm(uv * 4.0) * 2.0;
    float crackA = pow(0.5+0.5*sin((uv.x*12.0+uv.y*8.0+turb)*3.14159), 4.0);
    float crackB = pow(0.5+0.5*sin((uv.x*7.0-uv.y*14.0+turb*0.8)*3.14159), 4.0);
    float carbon = fbm6(uv * 18.0);                   // carbon deposit from smoke firing
    vec3  clay   = vec3(0.18, 0.16, 0.14);
    vec3  glaze  = vec3(0.08, 0.08, 0.09);
    vec3  bright = vec3(0.45, 0.42, 0.38);            // glaze flash
    vec3  col    = mix(clay, glaze, max(crackA,crackB)*0.8);
    return mix(col, bright, carbon * 0.25);
}

// ── Preset 4 textures: Contemporary Luxury ────────────────────────────────────

// Anisotropic brushed stainless steel: tight directional streaks with cross-hatch
vec3 tex_brushed_steel(vec2 uv) {
    float streak1 = fbm(vec2(uv.x * 120.0, uv.y * 1.5));   // primary brush direction
    float streak2 = fbm(vec2(uv.x * 2.0,   uv.y * 80.0)) * 0.15; // cross brush
    float macro   = fbm(uv * 3.0) * 0.08;                   // large-scale panel variation
    float base    = 0.72 + streak1 * 0.18 + streak2 - macro;
    return clamp(vec3(base, base, base*1.02), 0.0, 1.0);    // very slight cool tint
}

// Polished copper: warm orange with oxidation patches
vec3 tex_copper(vec2 uv) {
    float streak  = fbm(vec2(uv.x * 50.0, uv.y * 2.0));    // lathe marks
    float oxide   = fbm(uv * 6.0);                           // oxidation patina
    float highlight = fbm(vec2(uv.x * 8.0, uv.y * 0.5));
    vec3  bright  = vec3(0.82, 0.52, 0.25);                 // polished copper
    vec3  dark    = vec3(0.55, 0.32, 0.14);
    vec3  patina  = vec3(0.28, 0.52, 0.40);                 // green oxidation
    vec3  col     = mix(dark, bright, streak * 0.6 + highlight * 0.3);
    return mix(col, patina, oxide * oxide * 0.22);           // subtle patina patches
}

// Black vinyl / faux leather: tight pebble grain
vec3 tex_vinyl(vec2 uv) {
    float grain   = fbm6(uv * 35.0);
    float pebble  = pow(noise2(uv * 18.0), 1.5);
    float gloss   = fbm(vec2(uv.x*5.0, uv.y*40.0)) * 0.12; // directional gloss streaks
    float dark    = 0.05 + grain * 0.06 + pebble * 0.04 + gloss;
    return vec3(dark, dark, dark + 0.01);
}

// Pewter: warm grey, semi-matte cast texture
vec3 tex_pewter(vec2 uv) {
    float mold    = fbm(uv * 7.0);                           // casting variation
    float sprue   = fbm(vec2(uv.x*2.0, uv.y*20.0)) * 0.08; // mold flow lines
    float pit     = pow(noise2(uv * 30.0), 2.5) * 0.06;    // micro pitting
    float val     = 0.52 + mold * 0.14 - pit + sprue;
    return clamp(vec3(val*0.98, val*0.97, val), 0.0, 1.0);  // slightly cool
}

// ── Preset 5 textures: East Asian Lacquerware ────────────────────────────────

// Lacquered black: deep black with subtle depth shimmer
vec3 tex_lacquer_black(vec2 uv) {
    float depth   = fbm(uv * 5.0) * 0.05;
    float shimmer = fbm6(uv * 20.0) * 0.03;
    return vec3(0.03 + depth + shimmer);
}

// Crimson silk: warp/weft twill weave + iridescent sheen variation
vec3 tex_silk(vec2 uv, vec3 baseCol, vec3 sheenCol) {
    // Twill diagonal
    float diag   = fract((uv.x + uv.y) * 25.0);
    float twill  = smoothstep(0.35,0.50,diag) - smoothstep(0.50,0.65,diag);
    float sheen  = fbm(vec2(uv.x*4.0, uv.y*50.0));         // warp highlight
    float fiber  = noise2(uv * 30.0) * 0.03;
    return clamp(mix(baseCol, sheenCol, twill*0.35 + sheen*0.25) + fiber, 0.0, 1.0);
}

// Gold satin fabric: lustrous warp sheen
vec3 tex_satin_gold(vec2 uv) {
    float warp   = fbm(vec2(uv.x*3.0, uv.y*55.0));
    float weft   = fbm(vec2(uv.x*50.0, uv.y*3.0)) * 0.4;
    float fiber  = noise2(uv * 25.0) * 0.03;
    vec3  dark   = vec3(0.55, 0.40, 0.08);
    vec3  bright = vec3(0.88, 0.72, 0.22);
    return clamp(mix(dark, bright, warp*0.6 + weft*0.3) + fiber, 0.0, 1.0);
}

// Celadon porcelain: pale green-grey glaze with faint crackle
vec3 tex_celadon(vec2 uv) {
    float turb    = fbm(uv * 6.0) * 1.2;
    float crackle = pow(0.5+0.5*sin((uv.x*9.0+uv.y*5.0+turb)*3.14159), 6.0);
    float tonal   = fbm(uv * 4.0) * 0.06;
    vec3  base    = vec3(0.72, 0.80, 0.72);
    vec3  vein    = vec3(0.55, 0.64, 0.57);   // vein lebih terang, tidak gelap
    return clamp(mix(base + tonal, vein, crackle * 0.20), vec3(0.55, 0.62, 0.55), vec3(1.0));
}

// Hand-carved jade bowl: translucent banding + inclusion spots
vec3 tex_carved_jade(vec2 uv) {
    float band   = clamp(fbm(uv * vec2(1.5, 8.0)), 0.0, 1.0);
    float incl   = pow(noise2(uv * 12.0), 3.0) * 0.25;
    float micro  = fbm6(uv * 25.0) * 0.06;
    vec3  light  = vec3(0.50, 0.82, 0.60);
    vec3  dark   = vec3(0.22, 0.52, 0.34);   // minimum cukup terang, tidak hitam
    vec3  white  = vec3(0.72, 0.88, 0.75);
    vec3  col    = mix(dark, light, band);
    col          = mix(col, white, incl);
    return clamp(col + micro, vec3(0.18, 0.42, 0.28), vec3(1.0));
}

// ── Material resolver ─────────────────────────────────────────────────────────

struct MatResult {
    vec3  diffuse;
    vec3  specular;
    float shininess;
    float ambScale;  // per-material ambient multiplier
};

MatResult resolveMaterial() {
    MatResult r;
    r.diffuse = vec3(0.8); r.specular = vec3(0.3); r.shininess = 32.0; r.ambScale = 1.0;

    // ── Preset 1: Mixed PNG per-material ─────────────────────────────────────
    if (preset == 1) {
        vec3 texCol = (hasTexture == 1) ? texture(diffuseMap, TexCoord).rgb : vec3(0.75);
        if (matID == 0) {
            r.diffuse = texCol * vec3(1.05,0.98,0.90); r.specular = vec3(0.20,0.18,0.14); r.shininess = 24.0;
        } else if (matID == 1) {
            r.diffuse = texCol * vec3(1.0,0.95,0.85)*1.4; r.specular = vec3(0.04,0.04,0.03); r.shininess = 5.0;
        } else if (matID == 2) {
            r.diffuse = texCol * vec3(0.82,0.82,0.85); r.specular = vec3(0.45,0.45,0.48); r.shininess = 80.0;
        } else if (matID == 3) {
            r.diffuse = texCol; r.specular = vec3(0.06,0.10,0.05); r.shininess = 10.0;
        } else if (matID == 4) {
            r.diffuse = texCol * vec3(0.50,0.42,0.36); r.specular = vec3(0.55,0.50,0.44); r.shininess = 120.0;
        } else {
            r.diffuse = texCol; r.specular = vec3(0.70,0.70,0.68); r.shininess = 160.0;
        }
        return r;
    }

    // ── Preset 2: Worn Leather & Walnut ──────────────────────────────────────
    if (preset == 2) {
        if (matID == 0) {
            r.diffuse = tex_walnut(TexCoord);
            r.specular = vec3(0.30,0.18,0.08); r.shininess = 55.0;
        } else if (matID == 1) {
            r.diffuse = tex_linen(TexCoord, vec3(0.86,0.80,0.68), vec3(0.74,0.68,0.56));
            r.specular = vec3(0.04,0.04,0.03); r.shininess = 5.0;
        } else if (matID == 2) {
            r.diffuse = tex_leather_aged(TexCoord, vec3(0.45,0.22,0.08));
            r.specular = vec3(0.38,0.20,0.08); r.shininess = 42.0;
        } else if (matID == 3) {
            r.diffuse = tex_leaf_organic(TexCoord);
            r.specular = vec3(0.07,0.14,0.04); r.shininess = 14.0;
        } else if (matID == 4) {
            r.diffuse = tex_stoneware(TexCoord, vec3(0.28,0.25,0.22));
            r.specular = vec3(0.10,0.10,0.10); r.shininess = 8.0;
        } else {
            r.diffuse = tex_bone_china(TexCoord);
            r.specular = vec3(0.60,0.58,0.54); r.shininess = 110.0;
        }
        return r;
    }

    // ── Preset 3: Scandinavian Modern ────────────────────────────────────────
    // meja=concrete abu, taplak=cotton putih, kursi=navy velvet,
    // material(bunga+daun+vas)=hijau segar, boli(gelas+mangkuk)=kaca bening frosted,
    // niunai(piring+alat makan)=matte white ceramic.
    if (preset == 3) {
        float n3 = fbm(TexCoord * 8.0);
        if (matID == 0) {
            // qita – meja: polished concrete abu-abu
            r.diffuse   = tex_concrete(TexCoord);
            r.specular  = vec3(0.18, 0.17, 0.16);
            r.shininess = 22.0;
            r.ambScale  = 0.9;
        } else if (matID == 1) {
            // lvzhi – taplak: white cotton
            r.diffuse   = tex_cotton(TexCoord, vec3(0.97, 0.96, 0.95));
            r.specular  = vec3(0.06, 0.06, 0.06);
            r.shininess = 7.0;
        } else if (matID == 2) {
            // zhuoyi – kursi: navy velvet
            r.diffuse   = tex_velvet(TexCoord, vec3(0.08, 0.12, 0.32));
            r.specular  = vec3(0.20, 0.24, 0.60);
            r.shininess = 28.0;
            r.ambScale  = 0.7;
        } else if (matID == 3) {
            // material – bunga+daun+vas: hijau segar organik, warna solid dijamin tidak putih/hitam
            r.diffuse   = vec3(0.18, 0.52, 0.12) + fbm(TexCoord * 5.0) * vec3(0.06, 0.10, 0.04);
            r.specular  = vec3(0.08, 0.18, 0.05);
            r.shininess = 14.0;
            r.ambScale  = 1.5;
        } else if (matID == 4) {
            // boli – gelas+mangkuk: frosted glass / clear crystal putih bening
            r.diffuse   = vec3(0.82, 0.88, 0.90) + n3 * 0.05;
            r.specular  = vec3(0.90, 0.92, 0.95);
            r.shininess = 180.0;
        } else {
            // niunai – piring+alat makan: matte white ceramic
            r.diffuse   = tex_white_ceramic(TexCoord);
            r.specular  = vec3(0.25, 0.25, 0.24);
            r.shininess = 18.0;
        }
        return r;
    }

    // ── Preset 4: Contemporary Luxury (Metal & High Gloss) ───────────────────
    // meja=brushed steel, taplak=charcoal cotton, kursi=black vinyl,
    // material(bunga+daun+vas)=hijau waxy terang, boli(gelas+mangkuk)=polished silver,
    // niunai(piring+alat makan)=brushed pewter terang.
    if (preset == 4) {
        float n4 = fbm(TexCoord * 8.0);
        if (matID == 0) {
            // qita – meja: brushed stainless steel
            r.diffuse   = tex_brushed_steel(TexCoord);
            r.specular  = vec3(0.90, 0.92, 0.95);
            r.shininess = 280.0;
            r.ambScale  = 0.8;
        } else if (matID == 1) {
            // lvzhi – taplak: charcoal matte cotton
            r.diffuse   = tex_cotton(TexCoord, vec3(0.12, 0.12, 0.13));
            r.specular  = vec3(0.03, 0.03, 0.03);
            r.shininess = 4.0;
        } else if (matID == 2) {
            // zhuoyi – kursi: black vinyl glossy
            r.diffuse   = tex_vinyl(TexCoord);
            r.specular  = vec3(0.75, 0.75, 0.78);
            r.shininess = 200.0;
            r.ambScale  = 0.6;
        } else if (matID == 3) {
            // material – bunga+daun+vas: hijau waxy, warna solid dijamin tidak hitam
            r.diffuse   = vec3(0.22, 0.58, 0.14) + fbm(TexCoord * 5.0) * vec3(0.08, 0.10, 0.05);
            r.specular  = vec3(0.35, 0.55, 0.18);
            r.shininess = 40.0;
            r.ambScale  = 1.5;
        } else if (matID == 4) {
            // boli – gelas+mangkuk: polished silver/chrome, terang dan reflektif
            float streak = fbm(vec2(TexCoord.x * 80.0, TexCoord.y * 2.0));
            r.diffuse   = vec3(0.78 + streak * 0.12, 0.80 + streak * 0.10, 0.82 + streak * 0.10);
            r.specular  = vec3(0.92, 0.93, 0.95);
            r.shininess = 250.0;
        } else {
            // niunai – piring+alat makan: brushed pewter, cukup terang
            r.diffuse   = clamp(tex_pewter(TexCoord), vec3(0.45, 0.44, 0.46), vec3(1.0));
            r.specular  = vec3(0.60, 0.59, 0.62);
            r.shininess = 90.0;
        }
        return r;
    }

    // ── Preset 5: East Asian Lacquerware ─────────────────────────────────────
    // meja=lacquer hitam glossy, taplak=gold satin, kursi=crimson silk,
    // material(bunga+daun+vas)=hijau tropis waxy,
    // boli(gelas+mangkuk)=celadon jade hijau muda TERANG,
    // niunai(piring+alat makan)=white porcelain krem TERANG.
    if (preset == 5) {
        float n5 = fbm(TexCoord * 6.0);
        if (matID == 0) {
            // qita – meja: lacquered black, deep gloss
            r.diffuse   = tex_lacquer_black(TexCoord);
            r.specular  = vec3(0.92, 0.90, 0.88);
            r.shininess = 350.0;
            r.ambScale  = 0.5;
        } else if (matID == 1) {
            // lvzhi – taplak: gold satin
            r.diffuse   = tex_satin_gold(TexCoord);
            r.specular  = vec3(0.80, 0.65, 0.20);
            r.shininess = 80.0;
        } else if (matID == 2) {
            // zhuoyi – kursi: crimson silk
            r.diffuse   = tex_silk(TexCoord, vec3(0.55,0.04,0.06), vec3(0.75,0.18,0.12));
            r.specular  = vec3(0.65, 0.20, 0.18);
            r.shininess = 95.0;
        } else if (matID == 3) {
            // material – bunga+daun+vas: hijau tropis, warna solid dijamin tidak hitam
            r.diffuse   = vec3(0.20, 0.55, 0.13) + fbm(TexCoord * 5.0) * vec3(0.07, 0.10, 0.04);
            r.specular  = vec3(0.28, 0.48, 0.18);
            r.shininess = 30.0;
            r.ambScale  = 1.5;
        } else if (matID == 4) {
            // boli – gelas+mangkuk: celadon jade hijau muda, warna solid dijamin tidak hitam
            r.diffuse   = vec3(0.62, 0.80, 0.66) + fbm(TexCoord * 4.0) * vec3(0.06, 0.06, 0.06);
            r.specular  = vec3(0.65, 0.80, 0.68);
            r.shininess = 160.0;
            r.ambScale  = 1.4;
        } else {
            // niunai – piring+alat makan: white porcelain krem, warna solid dijamin tidak hitam
            r.diffuse   = vec3(0.90, 0.88, 0.85) + fbm(TexCoord * 6.0) * vec3(0.04, 0.04, 0.03);
            r.specular  = vec3(0.75, 0.74, 0.72);
            r.shininess = 180.0;
            r.ambScale  = 1.4;
        }
        return r;
    }

    return r;
}

// ── Main ──────────────────────────────────────────────────────────────────────

void main()
{
    MatResult mat = resolveMaterial();

    vec3  norm     = normalize(Normal);
    vec3  lightDir = normalize(lightPos - FragPos);
    vec3  viewDir  = normalize(viewPos - FragPos);
    vec3  reflDir  = reflect(-lightDir, norm);

    // Ambient dengan per-material scale (velvet/lacquer lebih gelap di ambient)
    vec3  ambient  = lightAmb * mat.diffuse * 0.38 * mat.ambScale;

    // Diffuse
    float diff     = max(dot(norm, lightDir), 0.0);
    vec3  diffuse  = lightDiff * diff * mat.diffuse;

    // Specular — Blinn-Phong half-vector untuk hasil lebih natural
    vec3  halfDir  = normalize(lightDir + viewDir);
    float spec     = pow(max(dot(norm, halfDir), 0.0), mat.shininess);
    vec3  specular = lightSpec * spec * mat.specular;

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
