#include <stdlib.h>

#include "storage.h"
#include "mongodb.h"

struct storage_module *
storage_module_create()
{
    struct storage_module *module = malloc(sizeof(struct storage_module));
    
    if (!module)
        return NULL;

    return module;
}

void
storage_module_destroy(struct storage_module *module)
{
    if (!module)
        return;

    free(module);
}

