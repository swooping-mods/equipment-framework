#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef void *PTR;
typedef void *DRoot;
#define ME_LIMIT 32
typedef struct {int alive,destroyed;} Object;
static DRoot me_catalog,me_original_catalog,me_world,me_walker,me_brush;
static struct {DRoot definition;} me_entries[ME_LIMIT];
static unsigned me_owned_bits;
static int released,deleted,dropped,brush_released;
static PTR dget(DRoot root){return root;}
static int me_live(PTR object){return object&&((Object *)object)->alive;}
static void me_delete(PTR object){if(object){((Object *)object)->destroyed++;deleted++;}}
static void ddrop(DRoot *root){if(*root)dropped++;*root=0;}
static void me_release_brush(void){brush_released++;ddrop(&me_brush);}
static void me_event(const char *event){assert(!strcmp(event,"native-equipment-world-released"));released++;}

#include "world-runtime.h"

int main(void){
    Object world={1,0},next={1,0},donor={1,0},catalog={1,0},a={1,0},b={1,0},walker={1,0},brush={1,0};
    me_world=&world;me_catalog=&catalog;me_original_catalog=&donor;me_walker=&walker;me_brush=&brush;
    me_entries[0].definition=&a;me_entries[ME_LIMIT-1].definition=&b;me_owned_bits=7;
    /* Keep the catalog through native OnDestroy's final save. */
    me_collect_world();assert(!released&&!deleted&&me_catalog==&catalog&&me_owned_bits==7);
    world.alive=0;
    me_collect_world();
    assert(released==1&&deleted==3&&dropped==7&&brush_released==1);
    assert(catalog.destroyed==1&&a.destroyed==1&&b.destroyed==1&&!donor.destroyed);
    assert(!me_world&&!me_catalog&&!me_original_catalog&&!me_walker&&!me_brush&&!me_owned_bits);
    assert(!me_entries[0].definition&&!me_entries[ME_LIMIT-1].definition);
    me_collect_world();assert(released==1&&deleted==3);
    /* A delayed old-world death cannot collect a newer live world's assets. */
    me_world=&next;me_catalog=&catalog;me_collect_world();assert(released==1&&me_catalog==&catalog);
    next.alive=0;me_collect_world();assert(released==2&&!me_world&&!me_catalog&&deleted==4);
    puts("equipment world teardown: passed");
}
