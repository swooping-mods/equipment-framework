#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned long long DRoot;
static int deq(const char *a,const char *b){return a&&b&&!strcmp(a,b);}
#include "../registry.h"
static int owned(void *world,void *walker){return world&&walker;}
static MoonEquipmentDefinition item(const char *key){return (MoonEquipmentDefinition){ME_ABI,sizeof(MoonEquipmentDefinition),key,"Item","Description",0,owned,0};}
int main(void){
    MoonEquipmentDefinition a=item("test.vacuum"),b=item("test.sticker");
    assert(me_register(&a)&&me_register(&b));assert(me_entry(a.key)->def.owned==(owned));assert(!me_entry("native.broom"));
    assert(!me_register(&a));b.abi++;assert(!me_register(&b));b=item("third");b.size--;assert(!me_register(&b));
    b=item("bad key");assert(!me_register(&b));b=item("third");b.name="";assert(!me_register(&b));
    b=item("third");b.owned=0;assert(!me_register(&b));b=item("third");b.template_id="";assert(!me_register(&b));
    b=item("third");me_sealed=1;assert(!me_register(&b));me_sealed=0;
    char keys[ME_LIMIT][24];for(int i=2;i<ME_LIMIT;i++){snprintf(keys[i],sizeof(keys[i]),"test.tool-%d",i);b=item(keys[i]);assert(me_register(&b));}
    b=item("overflow");assert(!me_register(&b));assert(me_entry("test.vacuum")&&me_entry("test.sticker"));
    puts("Equipment registry: ABI, keys, ownership, duplicates, seal and capacity passed.");return 0;
}
