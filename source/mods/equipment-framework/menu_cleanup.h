#ifndef MOON_EQUIPMENT_MENU_CLEANUP_HOST_H
#define MOON_EQUIPMENT_MENU_CLEANUP_HOST_H
#include "sdk/menu_cleanup.h"
static struct {char key[96];void (*callback)(void);} me_cleanups[ME_CLEANUP_LIMIT];
static unsigned me_cleanup_registration_lock;
static int me_cleanup_busy;
static DWORD me_cleanup_startup_thread;
static int me_register_cleanup(const char *key,void (*callback)(void)){
    if(GetCurrentThreadId()!=__atomic_load_n(&me_cleanup_startup_thread,__ATOMIC_ACQUIRE))return 0;
    if(!callback||!key||!*key)return 0;
    unsigned n=0;for(;n<96&&key[n];n++){
        char c=key[n];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='.'))return 0;
    }
    if(n==96)return 0;
    unsigned expected=0;
    if(!__atomic_compare_exchange_n(&me_cleanup_registration_lock,&expected,1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED))return 0;
    unsigned free_slot=ME_CLEANUP_LIMIT;int result=0;
    for(unsigned i=0;i<ME_CLEANUP_LIMIT;i++){
        void (*registered)(void)=__atomic_load_n(&me_cleanups[i].callback,__ATOMIC_ACQUIRE);
        if(registered&&deq(me_cleanups[i].key,key))goto done;
        if(!registered&&free_slot==ME_CLEANUP_LIMIT)free_slot=i;
    }
    if(free_slot==ME_CLEANUP_LIMIT)goto done;
    for(unsigned i=0;i<=n;i++)me_cleanups[free_slot].key[i]=key[i];
    /* Menu can run during startup even under the loader: only shared gameplay
     * hooks are deferred. Publish each immutable entry after its copied key. */
    __atomic_store_n(&me_cleanups[free_slot].callback,callback,__ATOMIC_RELEASE);result=1;
done:
    __atomic_store_n(&me_cleanup_registration_lock,0,__ATOMIC_RELEASE);return result;
}
static void me_dispatch_menu_cleanup(void){
    /* Registration never establishes a Unity owner. Only the existing native
     * menu callback dispatches; provider readiness gates cover startup overlap. */
    if(!me_thread||me_thread!=GetCurrentThreadId()||me_cleanup_busy)return;
    me_cleanup_busy=1;
    for(unsigned i=0;i<ME_CLEANUP_LIMIT;i++){
        void (*callback)(void)=__atomic_load_n(&me_cleanups[i].callback,__ATOMIC_ACQUIRE);
        if(callback)callback();
    }
    me_cleanup_busy=0;
}
static const MoonEquipmentMenuCleanupV1 me_cleanup_api={ME_CLEANUP_ABI,sizeof(MoonEquipmentMenuCleanupV1),me_register_cleanup};
#endif
