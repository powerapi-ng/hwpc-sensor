#ifndef STORAGE_H
#define STORAGE_H

#include "payload.h"
#include "report.h"

/*
 * STORAGE_MODULE_CALL simplify the use of storage modules functions.
 */
#define STORAGE_MODULE_CALL(module, func, ...)  (*module->func)(module, ##__VA_ARGS__)

/*
 * storage_type enumeration allows to select a storage type to generate.
 */
enum storage_type
{
    STORAGE_NONE,
    STORAGE_MONGODB
};

/*
 * storage_module is a generic interface for storage modules.
 */
struct storage_module
{
    enum storage_type type;
    void *context;
    bool is_initialized;
    int (*initialize)(struct storage_module *self);
    int (*ping)(struct storage_module *self);
    int (*store_report)(struct storage_module *self, struct payload *payload);
    int (*deinitialize)(struct storage_module *self);
};

/*
 * storage_module_create allocate the required ressources for a storage module.
 */
struct storage_module *storage_module_create();

/*
 * storage_module_destroy free the allocated ressources for the storage module.
 */
void storage_module_destroy(struct storage_module *module);

#endif /* STORAGE_H */

