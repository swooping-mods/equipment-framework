/* Native inventory integration; providers keep ownership and actual item use. */
#include "../../runtime/native/gameplay.h"
#include "../../runtime/native/services.h"
#include "runtime.h"
#include "registry.h"
#include "catalog_context.h"
int _fltused=0;
static PTR me_log,me_instantiate,me_alive,me_destroy,me_hide_flags,me_set_hide_flags,me_component,me_player,me_selected,me_slot_of,me_select,me_refresh,me_shop_open,me_shop_inventory,me_tool_id;
static PTR (*me_array_new)(PTR,U64);static int (*me_offset)(PTR);
static void (*me_set)(PTR,PTR,void *),(*me_barrier)(PTR,PTR *,PTR);
static const unsigned short *(*me_chars)(PTR);static int (*me_length)(PTR);
static DRoot me_world,me_catalog,me_original_catalog,me_walker,me_brush;
static DWORD me_thread,me_owned_bits;static int me_started,me_busy,me_failed,me_owned_busy,me_brush_prior,me_brush_override;
static void me_event(const char *name){if(!me_log)return;char b[256],*p=cat(b,"{\"event\":\"");p=cat(p,name);p=cat(p,"\"}\n");DWORD n=0;WriteFile(me_log,b,(DWORD)(p-b),&n,0);FlushFileBuffers(me_log);}
static PTR me_field(PTR obj,const char *name){PTR out=0;PTR f=obj?get_field(object_class(obj),name):0;if(f)field_value(obj,f,&out);return out;}
static int me_reference(PTR obj,const char *name,PTR value){
    PTR f=obj?get_field(object_class(obj),name):0;int off=f?me_offset(f):0;if(off<16)return 0;
    me_barrier(obj,(PTR *)((unsigned char *)obj+off),value);return 1;
}
static int me_scalar(PTR obj,const char *name,void *value){PTR f=obj?get_field(object_class(obj),name):0;if(!f)return 0;me_set(obj,f,value);return 1;}
static int me_zero(PTR obj,const char *name){unsigned char zero[32]={0};PTR f=get_field(object_class(obj),name);if(!f)return 0;me_set(obj,f,zero);return 1;}
static int me_live(PTR obj){int ok=1;void *a[]={obj};PTR v=obj?call(me_alive,0,a,&ok):0;return ok&&v&&*(unsigned char *)unbox(v);}
static void me_delete(PTR obj){if(obj){int ok=1;void *a[]={obj};call(me_destroy,0,a,&ok);}}
static int me_match(PTR value,const char *key){if(!value||!key)return 0;int n=me_length(value),i=0;const unsigned short *s=me_chars(value);if(n<0||n>95||!s)return 0;while(key[i]&&i<n){if(s[i]!=(unsigned char)key[i])return 0;i++;}return i==n&&!key[i];}
static MEEntry *me_managed_entry(PTR id){for(int i=0;i<ME_LIMIT;i++)if(me_entries[i].used&&me_match(id,me_entries[i].def.key))return me_entries+i;return 0;}
static PTR me_clone(PTR obj){
    int ok=1;void *a[]={obj};PTR clone=call(me_instantiate,0,a,&ok);if(!ok||!clone)return 0;
    /* GC handles retain managed wrappers, but Unity's load-time asset sweep
     * also needs a native lifetime pin for our otherwise unreferenced clones.
     * me_drop_world explicitly destroys every pinned clone on world changes. */
    int flags=integer(me_hide_flags,clone,&ok)|32; /* HideFlags.DontUnloadUnusedAsset */
    void *b[]={&flags};if(ok)call(me_set_hide_flags,clone,b,&ok);
    if(!ok){me_delete(clone);return 0;}return clone;
}
static DRoot me_clone_root(PTR obj){
    PTR clone=me_clone(obj);DRoot root=dhold(clone);
    if(clone&&!root)me_delete(clone);
    return root;
}
static int me_bind_catalog_context(PTR game){
    static const struct {const char *type,*name;DWORD token;unsigned offsets[2];} sites[]={
        {"MoonSupplyWall","AddMissingTools",100670833,{0x25f,0x27a}},
        {"MoonSupplyWallScreen","BuildSpots",100670884,{0xfa,0x144}},
        {"MoonSupplyWallScreen","BuildShelf",100670920,{0x320,0x352}}
    };
    PTR getter=method("MoonCleaners","MoonWorld","get_UpgradeCatalog",0);
    if(!getter||method_token(getter)!=100665736)return 0;
    U64 callers[ME_SUPPLY_CATALOG_CALLS];unsigned at=0;
    for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);i++){
        PTR m=method("MoonCleaners",sites[i].type,sites[i].name,i==0?1:0);
        if(!m||method_token(m)!=sites[i].token)return 0;
        PTR body=*(PTR *)m;
        for(unsigned j=0;j<2;j++){
            U64 address=(U64)body+sites[i].offsets[j];
            if(address<(U64)game+0x1000||address+5>(U64)game+31917844||
               !me_catalog_callsite(body,sites[i].offsets[j],*(PTR *)getter,callers+at++))return 0;
        }
    }
    for(unsigned i=0;i<ME_SUPPLY_CATALOG_CALLS;i++)me_supply_catalog_callers[i]=callers[i];
    return 1;
}
static void me_release_brush(void){
    PTR brush=dget(me_brush);if(me_brush_override&&me_live(brush))me_scalar(brush,"<PrototypeActionOnly>k__BackingField",&me_brush_prior);
    me_brush_override=0;ddrop(&me_brush);
}
static void me_drop_world(void){
    me_release_brush();
    /* Only our private clone is destroyed. The original asset never changes. */
    me_delete(dget(me_catalog));ddrop(&me_catalog);ddrop(&me_original_catalog);
    for(int i=0;i<ME_LIMIT;i++){me_delete(dget(me_entries[i].definition));ddrop(&me_entries[i].definition);}
    ddrop(&me_world);ddrop(&me_walker);me_owned_bits=0;
}
static void me_collect_world(void){
    /* The walker need not update again after scene destruction. Keep assets
     * through the world's final save, then collect its dead native shell from
     * the menu callback without detouring MarsSweepers' OnDestroy hook. */
    if(me_world&&!me_live(dget(me_world))){me_drop_world();me_event("native-equipment-world-released");}
}
static int me_setup(PTR world){
    if(!world||me_failed||me_busy||GetCurrentThreadId()!=me_thread)return 0;
    if(world==dget(me_world)&&me_catalog)return 1;
    PTR base=me_field(me_field(world,"settings"),"upgradeCatalog"),old=me_field(base,"tools");
    if(!base||!old)return 0; /* Native settings can be absent during startup. */
    U64 count=da.array_length(old);if(!count)return 0;
    me_busy=1;me_drop_world();me_world=dhold(world);
    if(!me_world||count>512)goto fail;
    me_original_catalog=dhold(base);unsigned added=0;for(int i=0;i<ME_LIMIT;i++)added+=me_entries[i].used!=0;
    PTR array=me_array_new(find_class("MoonCleaners","MoonToolOffer"),count+added);DRoot array_root=dhold(array);if(!array_root)goto fail;
    for(U64 i=0;i<count;i++)me_barrier(array,(PTR *)darray(array,i),*(PTR *)darray(old,i));
    U64 at=count;
    for(int i=0;i<ME_LIMIT;i++)if(me_entries[i].used){
        MEEntry *e=me_entries+i;PTR donor=0;
        for(U64 j=0;j<count;j++){
            PTR def=me_field(*(PTR *)darray(old,j),"tool");if(!def)continue;int ok=1;PTR native_id=call(me_tool_id,def,0,&ok);if(!ok){ddrop(&array_root);goto fail;}
            if(me_match(native_id,e->def.key)){me_event("native-equipment-key-occupied");ddrop(&array_root);goto fail;}
            if(!donor&&(!e->def.template_id||me_match(native_id,e->def.template_id)))donor=def;
        }
        if(!donor){me_event("equipment-template-missing");ddrop(&array_root);goto fail;}
        e->definition=me_clone_root(donor);PTR def=dget(e->definition);if(!def){ddrop(&array_root);goto fail;}
        if(!me_reference(def,"toolId",da.string(e->def.key))||!me_reference(def,"displayName",da.string(e->def.name))){ddrop(&array_root);goto fail;}
        /* Borrowed art/grips cannot silently grant the template's sweeper or
         * hammer behavior. Providers own use and can replace visuals only. */
        const char *zero[]={"buildTool","containerKg","strokeTravel","cleaningWidth","cleaningDepth","edgeWidth","clearInOnePass","removalRate","reach"};
        for(unsigned j=0;j<sizeof(zero)/sizeof(zero[0]);j++)if(!me_zero(def,zero[j])){ddrop(&array_root);goto fail;}
        if(e->def.configure&&!e->def.configure(def,world)){ddrop(&array_root);goto fail;}
        PTR offer=da.new_object(find_class("MoonCleaners","MoonToolOffer"));DRoot offer_root=dhold(offer);int ok=1;
        PTR ctor=method("MoonCleaners","MoonToolOffer",".ctor",0);if(offer_root)call(ctor,offer,0,&ok);else ok=0;
        if(!offer_root||!ok||!me_reference(offer,"tool",def)){ddrop(&offer_root);ddrop(&array_root);goto fail;}
        /* Native purchase is blocked below: payment stays in the shop provider. */
        me_barrier(array,(PTR *)darray(array,at++),offer);ddrop(&offer_root);
    }
    me_catalog=me_clone_root(base);
    if(!me_catalog||!me_reference(dget(me_catalog),"tools",array)){ddrop(&array_root);goto fail;}
    ddrop(&array_root);me_sealed=1;me_busy=0;me_event("native-equipment-catalog-ready");return 1;
fail:
    me_drop_world();me_busy=0;me_failed=1;me_event("native-equipment-registration-failed");return 0;
}
static PTR me_belt(PTR walker){int ok=1;void *a[]={dtype("MoonCleaners","MoonToolbelt")};PTR belt=call(me_component,walker,a,&ok);return ok?belt:0;}
static PTR me_player_id(PTR walker){int ok=1;PTR id=walker?call(me_player,walker,0,&ok):0;return ok?id:0;}
static int me_owned(MEEntry *e,PTR world,PTR walker){
    if(!e||!world||!walker||me_owned_busy||me_failed)return 0;
    me_owned_busy=1;int owned=e->def.owned(world,walker)>0;me_owned_busy=0;return owned;
}
static int me_equipped(const char *key,void *walker){
    MEEntry *e=me_entry(key);if(!e||!walker||!me_thread||GetCurrentThreadId()!=me_thread)return 0;
    PTR world=me_field(walker,"world");if(!world||!me_setup(world)||!me_owned(e,world,walker))return 0;
    PTR belt=me_belt(walker);if(!belt||me_live(me_field(belt,"prototypeTool")))return 0;
    int ok=1;PTR selected=call(me_selected,belt,0,&ok);return ok&&selected==dget(e->definition);
}
static int me_equip(const char *key,void *walker){
    MEEntry *e=me_entry(key);if(!e||!walker||GetCurrentThreadId()!=me_thread)return 0;
    PTR world=me_field(walker,"world"),player=me_player_id(walker);if(!player||!me_setup(world)||!me_owned(e,world,walker))return 0;
    int ok=1;void *args[]={player,da.string(key)};PTR slot_value=call(me_slot_of,world,args,&ok);int slot=slot_value?*(int *)unbox(slot_value):-1;
    if(!ok||slot<0||slot>5)return 0;
    PTR belt=me_belt(walker);void *a[]={&slot};if(!belt)return 0;call(me_select,belt,a,&ok);return ok&&me_equipped(key,walker);
}
static MEEntry *me_requested_entry(int id,Context *c){
    return me_managed_entry((PTR)(id==4?c->r8:c->rdx));
}
static int me_custom_brush(PTR brush){
    PTR belt=me_field(brush,"toolbelt");int ok=1;PTR selected=belt?call(me_selected,belt,0,&ok):0;
    if(!ok||!selected)return 0;
    for(int i=0;i<ME_LIMIT;i++)if(me_entries[i].used&&selected==dget(me_entries[i].definition))return 1;
    return 0;
}
static int me_before(int id,Context *c){
    if(me_failed)return 0;
    if(!me_thread)me_thread=GetCurrentThreadId();if(GetCurrentThreadId()!=me_thread)return 0;
    if(id==7){me_collect_world();return 0;}
    if(id==6)return me_custom_brush((PTR)c->rcx);
    if(id==0){
        PTR world=me_field((PTR)c->rcx,"world");if(!world){if(me_world)me_drop_world();return 0;}
        if(me_setup(world)){
            if(dget(me_walker)!=(PTR)c->rcx){ddrop(&me_walker);me_walker=dhold((PTR)c->rcx);}
            DWORD bits=0;for(int i=0;i<ME_LIMIT;i++)if(me_entries[i].used&&me_owned(me_entries+i,world,(PTR)c->rcx))bits|=1u<<i;
            if(bits!=me_owned_bits){
                me_owned_bits=bits;int ok=1;PTR belt=me_belt((PTR)c->rcx);if(belt)call(me_refresh,belt,0,&ok);
                void *args[]={dtype("MoonCleaners","MoonUpgradeShop")};PTR shop=call(me_component,(PTR)c->rcx,args,&ok);
                if(shop&&boolean(me_shop_open,shop,&ok))call(me_shop_inventory,shop,0,&ok);
                me_event("equipment-ownership-refreshed");
            }
        }return 0;
    }
    if(id==5){
        PTR brush=(PTR)c->rcx;int custom=me_custom_brush(brush);
        if(!custom){if(dget(me_brush)==brush)me_release_brush();return 0;}
        if(dget(me_brush)!=brush){
            me_release_brush();me_brush=dhold(brush);PTR f=get_field(object_class(brush),"<PrototypeActionOnly>k__BackingField");unsigned char prior=0;
            if(!f||!me_brush){ddrop(&me_brush);return 0;}field_value(brush,f,&prior);me_brush_prior=prior;me_brush_override=1;
        }
        int one=1;me_scalar(brush,"<PrototypeActionOnly>k__BackingField",&one);
        me_zero(brush,"hasPreviousStroke");me_zero(brush,"accumulated");me_zero(brush,"toolUseBlend");
        me_reference(brush,"<BrushedCrewmate>k__BackingField",0);
        return 0;
    }
    PTR world=(PTR)c->rcx;
    if(id==1){
        if(me_original_catalog_for_caller(*(U64 *)c->rsp))return 0;
        if(me_setup(world)){c->rax=(U64)dget(me_catalog);return 1;}return 0;
    }
    MEEntry *e=me_requested_entry(id,c);if(!e)return 0;
    if(id==2){
        PTR walker=dget(me_walker),player=me_player_id(walker);int same=player&&(PTR)c->r8;
        if(same){int n=me_length(player);const unsigned short *a=me_chars(player),*b=me_chars((PTR)c->r8);if(n!=me_length((PTR)c->r8))same=0;else for(int i=0;i<n;i++)if(a[i]!=b[i]){same=0;break;}}
        c->rax=(U64)(long long)(same&&world==dget(me_world)&&me_owned(e,world,walker)?0:-1);return 1;
    }
    /* Offers are inventory descriptors, not a second payment authority. The
     * Modded Shop's purchase callback is the sole owner of debit/persistence. */
    if(id==3&&c->r9)dzero((PTR)c->r9,16);
    c->rax=0;return 1;
}
static const MoonEquipment me_api={ME_ABI,sizeof(MoonEquipment),me_register,me_equipped,me_equip};
__declspec(dllexport) const MoonEquipment *MoonLab_Equipment_v1(unsigned int abi,unsigned int size){return me_started&&abi==ME_ABI&&size==sizeof(MoonEquipment)?&me_api:0;}
__declspec(dllexport) int MoonLab_ModStart(PTR domain,PTR game){
    if(me_started)return 1;char folder[1000],path[1200];DWORD n=GetEnvironmentVariableA("MOONLAB_EVIDENCE",folder,1000);if(!n||n>=1000)return 0;
    cat(cat(path,folder),"\\equipment-framework.jsonl");me_log=CreateFileA(path,0x40000000,1,0,1,0x80,0);if(me_log==(PTR)(U64)-1)return 0;
    if(!bind_runtime(domain,game)||!dapi(game))return 0;
#define ME_BIND(F,N,T) F=(T)GetProcAddress(game,"il2cpp_" N);if(!F)return 0
    ME_BIND(me_array_new,"array_new",PTR(*)(PTR,U64));ME_BIND(me_offset,"field_get_offset",int(*)(PTR));
    ME_BIND(me_set,"field_set_value",void(*)(PTR,PTR,void *));ME_BIND(me_barrier,"gc_wbarrier_set_field",void(*)(PTR,PTR *,PTR));
    ME_BIND(me_chars,"string_chars",const unsigned short *(*)(PTR));ME_BIND(me_length,"string_length",int(*)(PTR));
#undef ME_BIND
    me_instantiate=dmethod("UnityEngine","Object","Instantiate","UnityEngine.Object");me_alive=dmethod("UnityEngine","Object","op_Implicit","UnityEngine.Object");me_destroy=dmethod("UnityEngine","Object","Destroy","UnityEngine.Object");
    me_hide_flags=dmethod("UnityEngine","Object","get_hideFlags","");me_set_hide_flags=dmethod("UnityEngine","Object","set_hideFlags","UnityEngine.HideFlags");
    me_component=dmethod("UnityEngine","Component","GetComponent","System.Type");me_player=dmethod("MoonCleaners","MoonWalker","get_UpgradePlayer","");
    me_selected=dmethod("MoonCleaners","MoonToolbelt","get_SelectedTool","");me_slot_of=dmethod("MoonCleaners","MoonWorld","LoadoutSlotOf","System.String;System.String");me_select=dmethod("MoonCleaners","MoonToolbelt","SelectSlot","System.Int32");
    me_tool_id=dmethod("MoonCleaners","MoonToolDefinition","get_Id","");
    me_refresh=dmethod("MoonCleaners","MoonToolbelt","RefreshOwnership","");me_shop_open=dmethod("MoonCleaners","MoonUpgradeShop","get_IsOpen","");me_shop_inventory=dmethod("MoonCleaners","MoonUpgradeShop","BuildEquipmentInventory","");
    (void)num;(void)integer;(void)boolean;(void)dzero;(void)dcopy;(void)dnumber;(void)dvector;(void)me_live;
    if(!me_instantiate||!me_alive||!me_destroy||!me_hide_flags||!me_set_hide_flags||!me_component||!me_player||!me_selected||!me_slot_of||!me_select||!me_refresh||!me_shop_open||!me_shop_inventory||!me_tool_id)return 0;
    if(!me_bind_catalog_context(game)){me_event("supply-catalog-callsite-mismatch");return 0;}
    if(!prepare_hook(method("MoonCleaners","MoonWalker","Update",0),100671733,0,game)||
       !prepare_hook(method("MoonCleaners","MoonWorld","get_UpgradeCatalog",0),100665736,1,game)||
       !prepare_hook(method("MoonCleaners","MoonWorld","ToolLevel",2),100665739,2,game)||
       !prepare_hook(method("MoonCleaners","MoonWorld","TryGetToolPrice",3),100665740,3,game)||
       !prepare_hook(method("MoonCleaners","MoonWorld","TryPurchaseTool",4),100665742,4,game)||
       !prepare_hook(method("MoonCleaners","MoonBrush","LateUpdate",0),100663720,5,game)||
       !prepare_hook(method("MoonCleaners","MoonBrush","ToggleToolRotationLock",0),100663726,6,game)||
       !prepare_hook(method("MoonCleaners","MoonMainMenu","Update",0),100668088,7,game)){me_event("framework-hook-shape-failed");return 0;}
    before_hook=me_before;if(!install_hooks())return 0;if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        const MoonServiceLimitV1 limits[]={{"max_items",ME_LIMIT}};
        MoonServicePublicationLimitsV1 publication={.base={.size=sizeof(publication),.service_id="equipment",.abi=ME_ABI,.api_size=sizeof(me_api),.api=&me_api},.limit_count=sizeof(limits)/sizeof(limits[0]),.reserved=0,.limits=limits};
        if(!runtime||!runtime->publish(&publication.base))return 0;
    }
    me_started=1;me_event("ready");return 1;
}
