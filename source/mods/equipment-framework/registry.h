#ifndef MOON_EQUIPMENT_REGISTRY_H
#define MOON_EQUIPMENT_REGISTRY_H
#include "sdk/equipment.h"
typedef struct {MoonEquipmentDefinition def;DRoot definition;int used;} MEEntry;
static MEEntry me_entries[ME_LIMIT];static int me_sealed;
static int me_text(const char *s,int max){if(!s||!*s)return 0;for(int i=0;i<max;i++)if(!s[i])return 1;return 0;}
static MEEntry *me_entry(const char *key){for(int i=0;i<ME_LIMIT;i++)if(me_entries[i].used&&deq(me_entries[i].def.key,key))return me_entries+i;return 0;}
static int me_register(const MoonEquipmentDefinition *d){
    if(!d||d->abi!=ME_ABI||d->size!=sizeof(*d)||me_sealed||!d->owned)return 0;
    if(!me_text(d->key,96)||!me_text(d->name,256)||!me_text(d->description,1024)||(d->template_id&&!me_text(d->template_id,96)))return 0;
    for(const char *p=d->key;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='.'))return 0;
    if(me_entry(d->key))return 0;
    for(int i=0;i<ME_LIMIT;i++)if(!me_entries[i].used){me_entries[i].def=*d;me_entries[i].used=1;return 1;}
    return 0;
}
#endif
