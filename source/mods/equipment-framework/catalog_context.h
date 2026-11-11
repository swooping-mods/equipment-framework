/* Pinned Supply Wall constructors must enumerate vanilla offers. Inventory,
 * saved loadouts and already-equipped mod tools still use the extended catalog. */
#ifndef MOON_EQUIPMENT_CATALOG_CONTEXT_H
#define MOON_EQUIPMENT_CATALOG_CONTEXT_H
#define ME_SUPPLY_CATALOG_CALLS 6u
static U64 me_supply_catalog_callers[ME_SUPPLY_CATALOG_CALLS];

static int me_catalog_callsite(PTR body,unsigned offset,PTR getter,U64 *caller){
    const unsigned char *p=(const unsigned char *)body+offset;
    if(p[0]!=0xe8)return 0;
    unsigned relative=(unsigned)p[1]|((unsigned)p[2]<<8)|((unsigned)p[3]<<16)|((unsigned)p[4]<<24);
    U64 next=(U64)(p+5);
    if(next+(long long)(int)relative!=(U64)getter)return 0;
    *caller=next;return 1;
}
static int me_original_catalog_for_caller(U64 caller){
    if(!caller)return 0;
    for(unsigned i=0;i<ME_SUPPLY_CATALOG_CALLS;i++)if(me_supply_catalog_callers[i]==caller)return 1;
    return 0;
}
#endif
