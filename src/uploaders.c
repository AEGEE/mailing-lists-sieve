#include <ltdl.h>
#include "mailing-lists-sieve.h"

struct uploader {
  lt_dlhandle module;
  char** extensions;  
  void (*install)(const char* const, const char* const);
  void (*delete)(const char* const );
  void (*close)();
};

static GPtrArray* uploaders;

char**
load_uploaders ()
{
  extern const lt_dlsymlist lt_preloaded_symbols[];
  lt_dlpreload_default (lt_preloaded_symbols);
  lt_dlinit();
  uploaders = g_ptr_array_new ();
  int i = 1;

  while (lt_preloaded_symbols[i].name != NULL) {
    if (lt_preloaded_symbols[i++].address != NULL)
      continue;
    struct uploader *mod = g_malloc0 (sizeof(struct uploader));
    mod->module = lt_dlopen (lt_preloaded_symbols[i-1].name);
    char** (*cap)() = lt_dlsym (mod->module, "extensions");
    if (cap == NULL) {
      lt_dlclose (mod->module);
      g_free (mod);
      continue;
    }
    if ((mod->extensions = cap()) == NULL) {
      lt_dlclose (mod->module);
      g_free (mod);
      continue;
    }
    mod->install = lt_dlsym (mod->module, "install");
    mod->delete = lt_dlsym (mod->module, "delete");
    mod->close = lt_dlsym (mod->module, "close");
    g_ptr_array_add (uploaders, mod);
  }
  if (uploaders->len > 0)
    return (char**)1;
  else
    return NULL;
}

void
install (const char* const address, const char* const script)
{
  unsigned int i;
  for (i = 0; i < uploaders->len; i++) {
    struct uploader *mod = g_ptr_array_index (uploaders, i);
    char *a =g_ascii_strdown (address, -1);
    mod->install (a, script);
    g_free (a);
  }
}

void
delete (const char* const address)
{
  unsigned int i;
  for (i = 0; i < uploaders->len; i++) {
    struct uploader *mod = g_ptr_array_index (uploaders, i);
    mod->delete (address);
  }
}

void
unload_uploaders ()
{
  unsigned int i;
  for (i = 0; i < uploaders->len; i++) {
    struct uploader *mod = g_ptr_array_index (uploaders, i);
    mod->close ();
    lt_dlclose (mod->module);
    g_free (mod);
  }
  g_ptr_array_free (uploaders, 1);
  lt_dlexit();
}
