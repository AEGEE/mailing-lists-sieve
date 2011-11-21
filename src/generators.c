#include <ltdl.h>
#include "mailing-lists-sieve.h"

struct generator {
  lt_dlhandle module;
  char** (*get_scripts)(const char* const );
  void (*unload)();
};

static GPtrArray* generators;
static GHashTable* supported_lists; //key = listname, value = struct generator*

int load_generators ()
{
  extern const lt_dlsymlist lt_preloaded_symbols[];
  lt_dlpreload_default (lt_preloaded_symbols);
  generators = g_ptr_array_new ();
  int i = 1;
  supported_lists = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  while (lt_preloaded_symbols[i].name != NULL) {
      if (lt_preloaded_symbols[i++].address != NULL)
          continue;
      struct generator *mod = g_malloc0 (sizeof(struct generator));
      mod->module = lt_dlopen (lt_preloaded_symbols[i-1].name);
      char** (*lists)() = lt_dlsym (mod->module, "supported_lists");
      if (!lists) {
          lt_dlclose (mod->module);
          g_free (mod);
          continue;
      }
      mod->unload = lt_dlsym (mod->module, "unload");
      char** b = lists();
      if (b == NULL || b[0] == NULL) {//this module does not support any mailing list 
          lt_dlclose (mod->module);
          g_free (mod);
          continue;
      }
      int j =0;
      while (b[j])
          g_hash_table_insert (supported_lists, g_ascii_strup(b[j++], -1), mod);
    mod->get_scripts	= lt_dlsym (mod->module, "get_scripts");
    g_ptr_array_add (generators, mod);
  }
  if (generators->len == 0)
      return -1;
  else
      return 0;
}
 
void
unload_generators ()
{
  unsigned int i;
  g_hash_table_remove_all (supported_lists);
  for (i = 0; i < generators->len; i++) {
    struct generator *mod = g_ptr_array_index (generators, i);
    if (mod->unload) mod->unload ();
    lt_dlclose (mod->module);
    g_free (mod);
  }
  g_ptr_array_free (generators, 1);
  g_hash_table_destroy (supported_lists);
}

char**
get_scripts (const char* const listname)
{
  struct generator *module;
  module = (struct generator*)
      g_hash_table_lookup (supported_lists, listname);
  if (module == NULL) return NULL;
  else if (module->get_scripts == NULL) return NULL;
  else return module->get_scripts (listname);
}

GPtrArray*
get_supported_lists ()
{
  GPtrArray* g = g_ptr_array_new ();
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, supported_lists);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    g_ptr_array_add(g, key);
  }
  return g;
}
