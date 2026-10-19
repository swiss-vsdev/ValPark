#pragma once
#include "logic.h"
struct Zone { double south,north,west,east; const char* name; bool blue; };
// Aucun polygone officiel de Sion n'a ete importe. Cette liste est vide volontairement.
// Exemple de syntaxe a adapter : {46.23000,46.23010,7.35000,7.35010,"zone-test",true}
static const Zone zones[] = { {0,0,0,0,"DESACTIVEE",false} };
inline const Zone* locate(const Fix& f) {
 for(const auto& z:zones)if(z.south!=0&&f.lat>=z.south&&f.lat<=z.north&&f.lon>=z.west&&f.lon<=z.east)return &z;
 return nullptr;
}
