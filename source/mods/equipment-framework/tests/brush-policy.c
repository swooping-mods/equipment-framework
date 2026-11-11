#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef void *PTR;
typedef void *DRoot;
#define ME_LIMIT 32
static struct {int used;DRoot definition;} me_entries[ME_LIMIT];
static int brush,belt,native,custom,error,no_belt,selected_tag;
static PTR selected,me_selected=&selected_tag;
static PTR dget(DRoot root){return root;}
static PTR me_field(PTR object,const char *name){assert(object==&brush&&!strcmp(name,"toolbelt"));return no_belt?0:&belt;}
static PTR call(PTR method,PTR self,void **args,int *ok){assert(method==me_selected&&self==&belt&&!args);if(error)*ok=0;return selected;}

#include "brush-runtime.h"

int main(void){
    me_entries[0].used=1;me_entries[0].definition=&custom;
    selected=&custom;assert(me_custom_brush(&brush));
    selected=&native;assert(!me_custom_brush(&brush));
    selected=0;assert(!me_custom_brush(&brush));
    selected=&custom;error=1;assert(!me_custom_brush(&brush));error=0;
    no_belt=1;assert(!me_custom_brush(&brush));no_belt=0;
    me_entries[0].used=0;assert(!me_custom_brush(&brush));
    puts("equipment brush rotation selection policy: passed");
}
