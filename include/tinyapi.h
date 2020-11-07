
#include "tsval.h"

typedef TS_Val (*TS_ModuleEntry_t)(const uint8_t* referenced_name, TS_Val globals);

/*
typedef struct
{
    int type; // function, block, exception
    CodeLoc_t handler;
    size_t framebegin, frameend;
}
VMFrame_t;
*/