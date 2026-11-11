#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef void *PTR;
typedef unsigned long long U64;
typedef struct {U64 rdx,r8;} Context;
typedef struct {int value;} MEEntry;
static MEEntry numeric,custom;
static MEEntry *me_managed_entry(PTR text){
    if(!strcmp(text,"12345678901234567"))return &numeric;
    if(!strcmp(text,"portable-map"))return &custom;
    return 0;
}
#include "request-runtime.h"
int main(void){
    const char *player="12345678901234567",*vanilla="toothbrush",*mod="portable-map";
    Context c={(U64)vanilla,(U64)player};
    assert(!me_requested_entry(2,&c)&&!me_requested_entry(3,&c));
    c=(Context){(U64)player,(U64)vanilla};assert(!me_requested_entry(4,&c));
    c=(Context){(U64)mod,(U64)player};
    assert(me_requested_entry(2,&c)==&custom&&me_requested_entry(3,&c)==&custom);
    c=(Context){(U64)player,(U64)mod};assert(me_requested_entry(4,&c)==&custom);
    c=(Context){(U64)player,(U64)"another-player"};assert(me_requested_entry(3,&c)==&numeric);
    puts("equipment request ABI: passed");
}
