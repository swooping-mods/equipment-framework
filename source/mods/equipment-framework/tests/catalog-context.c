#include <assert.h>
#include <stdio.h>
typedef void *PTR;
typedef unsigned long long U64;
#include "../catalog_context.h"

static void direct_call(unsigned char *site,unsigned char *target){
    int relative=(int)(target-(site+5));unsigned bits=(unsigned)relative;
    site[0]=0xe8;
    for(unsigned i=0;i<4;i++)site[i+1]=(unsigned char)(bits>>(8*i));
}
int main(void){
    unsigned char arena[512]={0};
    for(unsigned i=0;i<ME_SUPPLY_CATALOG_CALLS;i++){
        unsigned char *body=arena+i*32;
        direct_call(body+4,arena+256);
        assert(me_catalog_callsite(body,4,arena+256,me_supply_catalog_callers+i));
        assert(me_original_catalog_for_caller((U64)(body+9)));
        assert(!me_original_catalog_for_caller((U64)(body+8)));
    }
    /* Native construction gets originals; all other callers (inventory,
     * loadout, drag and already-assigned mod tools) keep the extended catalog. */
    assert(!me_original_catalog_for_caller(0));
    assert(!me_original_catalog_for_caller((U64)(arena+300)));
    U64 result=123;
    assert(!me_catalog_callsite(arena,4,arena+257,&result)&&result==123);
    arena[4]=0xe9;
    assert(!me_catalog_callsite(arena,4,arena+256,&result)&&result==123);
    direct_call(arena+400,arena);
    assert(me_catalog_callsite(arena,400,arena,&result)&&result==(U64)(arena+405));
    puts("equipment Supply Wall catalog context: passed");
}
