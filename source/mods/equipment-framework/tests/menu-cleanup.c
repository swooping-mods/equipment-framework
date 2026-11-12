/* Production registry + actual me_before prefix through its id7 branch. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned DWORD;
typedef struct {unsigned long long rcx;} Context;
static DWORD me_thread,current_thread=77;
static int me_failed,calls,collected,interleave,extra_calls;
static DWORD GetCurrentThreadId(void){return current_thread;}
static int deq(const char *a,const char *b){return a&&b&&!strcmp(a,b);}
#include "../menu_cleanup.h"
static void me_collect_world(void){assert(!calls||!me_cleanup_busy);collected++;}
#include "menu-before-runtime.h"
static void cleanup(void){
    calls++;
    assert(!me_register_cleanup("during.callback",cleanup));
    if(interleave){
        /* Deterministic worker publication while the menu is between slots. */
        interleave=0;current_thread=77;
        extern void added_cleanup(void);
        assert(me_register_cleanup("added.during.menu",added_cleanup));current_thread=1;
    }
    me_dispatch_menu_cleanup(); /* recursion cannot run the registry twice */
}
void added_cleanup(void){extra_calls++;}
int main(void){
    Context context={0};
    me_cleanup_startup_thread=77;
    assert(me_cleanup_api.abi==ME_CLEANUP_ABI&&me_cleanup_api.size==sizeof(me_cleanup_api));
    assert(!me_register_cleanup(0,cleanup)&&!me_register_cleanup("",cleanup));
    assert(!me_register_cleanup("bad key",cleanup)&&!me_register_cleanup("missing.callback",0));
    assert(me_register_cleanup("zoom-module",cleanup));
    assert(!me_register_cleanup("zoom-module",cleanup));
    me_dispatch_menu_cleanup();assert(!calls&&!me_thread); /* worker startup is inert */
    /* A busy writer never blocks another registration or the menu dispatcher. */
    me_cleanup_registration_lock=1;assert(!me_register_cleanup("busy",cleanup));
    current_thread=1;interleave=0;
    assert(me_before(7,&context)==0&&calls==1&&collected==1&&me_thread==1);
    me_cleanup_registration_lock=0;
    assert(!me_register_cleanup("late",cleanup));
    current_thread=2;assert(me_before(7,&context)==0&&calls==1&&collected==1);
    current_thread=1;assert(me_before(7,&context)==0&&calls==2&&collected==2);
    me_failed=1;assert(me_before(7,&context)==0&&calls==2);me_failed=0;
    interleave=1;assert(me_before(7,&context)==0&&calls==3&&extra_calls==1);
    current_thread=77;
    char borrowed[]="copied.key";assert(me_register_cleanup(borrowed,added_cleanup));borrowed[0]='z';
    assert(!me_register_cleanup("copied.key",cleanup));
    memset(me_cleanups,0,sizeof(me_cleanups));
    char keys[ME_CLEANUP_LIMIT][24];
    for(unsigned i=0;i<ME_CLEANUP_LIMIT;i++){
        snprintf(keys[i],sizeof(keys[i]),"cleanup-%u",i);assert(me_register_cleanup(keys[i],cleanup));
    }
    assert(!me_register_cleanup("overflow",cleanup));
    current_thread=1;assert(me_before(7,&context)==0&&calls==3+(int)ME_CLEANUP_LIMIT);
    puts("Equipment menu cleanup registration, thread, continuation, recursion and capacity passed");
}
