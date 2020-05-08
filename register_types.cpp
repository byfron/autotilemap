/* register_types.cpp */

#include "register_types.h"
#include "core/class_db.h"
#include "autotilemap.h" 

void register_autotilemap_types() {
    ClassDB::register_class<Autotilemap>();
}

void unregister_autotilemap_types() {
   // Nothing to do here in this example.
}
