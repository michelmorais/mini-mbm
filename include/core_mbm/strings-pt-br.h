#ifndef STRINGS_PT_BR_H
#define STRINGS_PT_BR_H

// Portuguese (BR) strings for Windows dialogs. Accented characters are encoded as
// Windows-1252 (CP1252) hex escapes so they display correctly regardless of
// source file encoding (e.g. UTF-8). Do not replace with literal accented chars.

// Screen options dialog
#define STR_PT_BR_SCREEN_OPTIONS       "Op" "\xE7" "\xF5" "es de Tela"
#define STR_PT_BR_MONITOR_SELECT       "Selecione um monitor:"
#define STR_PT_BR_RESOLUTION_SELECT    "Selecione uma Resolu" "\xE7" "\xE3" "o:"
#define STR_PT_BR_FULL_SCREEN          "Tela cheia"
#define STR_PT_BR_START                "INICIAR"

// Monitor format string for sprintf: "%d: %ld x %ld, frequência:%lu, posição:%ld x %ld"
#define STR_PT_BR_MONITOR_FORMAT       "%d: %ld x %ld, frequ" "\xEA" "ncia:%lu, posi" "\xE7" "\xE3" "o:%ld x %ld"

// Application selection
#define STR_PT_BR_APPLICATION          "Aplicativo:"
#define STR_PT_BR_CUSTOM_SCRIPT        "Aplicativo Personalizado..."
#define STR_PT_BR_NO_NAME              "Sem nome"

// APP_RUN name_pt_br entries
#define STR_PT_BR_ASSET_PACKAGER       "Empacotador de ativos"
#define STR_PT_BR_FONT_MAKER           "Criador de fontes"
#define STR_PT_BR_MESH_EDITOR          "Editor de Mesh"
#define STR_PT_BR_MESH_DEBUG_EDITOR    "Editor de Mesh (debug)"
#define STR_PT_BR_SKELETAL_ANIMATION_EDITOR "Editor de Animação Esquelética"
#define STR_PT_BR_PARTICLE_EDITOR      "Editor de Part" "\xED" "culas"
#define STR_PT_BR_PHYSICS_EDITOR       "Editor de F" "\xED" "sica"
#define STR_PT_BR_SCENE_2D_EDITOR      "Editor de Cena 2D"
#define STR_PT_BR_SCENE_3D_EDITOR      "Editor de Cena 3D"
#define STR_PT_BR_SHADER_EDITOR        "Editor de Shader"
#define STR_PT_BR_SPRITE_MAKER         "Editor de Sprite"
#define STR_PT_BR_TEXTURE_PACKER       "Empacotador de texturas"
#define STR_PT_BR_TILEMAP_EDITOR       "Editor de mapa de blocos"
#define STR_PT_BR_USER_SPECIFIED       "Script do usu" "\xE1" "rio"

#endif
