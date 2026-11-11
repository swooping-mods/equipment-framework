/* Unity's asset sweep ignores managed GC roots. Exercise the production clone
 * helper against that independent lifetime rule and failed native bindings. */
#include <assert.h>
#include <stdio.h>

typedef void *PTR;
typedef unsigned long long DRoot;
typedef struct {int native_alive,flags;} Object;
static Object donor,clone;
static int fail_read,fail_write,fail_root,destroyed;
static int instantiate_tag,read_tag,write_tag;
static PTR me_instantiate=&instantiate_tag,me_hide_flags=&read_tag,me_set_hide_flags=&write_tag;

static PTR call(PTR method,PTR self,void **args,int *ok){
    if(method==me_instantiate){
        assert(!self&&args[0]==&donor);
        clone=donor;return &clone;
    }
    assert(method==me_set_hide_flags&&self==&clone);
    if(fail_write)*ok=0;else clone.flags=*(int *)args[0];
    return 0;
}
static int integer(PTR method,PTR self,int *ok){
    assert(method==me_hide_flags&&self==&clone);
    if(fail_read)*ok=0;
    return clone.flags;
}
static void me_delete(PTR object){assert(object==&clone);clone.native_alive=0;destroyed++;}
static DRoot dhold(PTR object){return object&&!fail_root?1:0;}

#include "clone-runtime.h"

int main(void){
    donor=(Object){1,3};
    PTR managed_root=me_clone(&donor);
    assert(managed_root==&clone&&clone.flags==35&&donor.flags==3);
    /* Keep the managed wrapper; unload only native objects without bit 32. */
    if(!(clone.flags&32))clone.native_alive=0;
    assert(managed_root==&clone&&clone.native_alive&&donor.native_alive);
    me_delete(managed_root);assert(!clone.native_alive&&destroyed==1);

    fail_read=1;assert(!me_clone(&donor)&&destroyed==2&&!clone.native_alive);
    fail_read=0;fail_write=1;
    assert(!me_clone(&donor)&&destroyed==3&&!clone.native_alive);
    fail_write=0;fail_root=1;
    assert(!me_clone_root(&donor)&&destroyed==4&&!clone.native_alive);
    fail_root=0;
    assert(me_clone_root(&donor)==1&&clone.native_alive&&(clone.flags&32));
    me_delete(&clone);assert(destroyed==5);
    assert(donor.native_alive&&donor.flags==3);
    puts("equipment native asset lifetime: passed");
}
