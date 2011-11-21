#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <glib.h>
#include <sys/stat.h>

#define extensions timsieved_LTX_extensions
#define install timsieved_LTX_install
#define delete timsieved_LTX_delete
#define close timsieved_LTX_close

extern GKeyFile *gfile;
extern char* configuration_file;

static char* sievedir;
static int virtdomain;
static char* defaultdomain;
static int owner, group;

char**
extensions ()
{
  char **items = g_key_file_get_keys (gfile, "timsieved_fs", NULL, NULL);
  if (items == NULL) return NULL;
  char **capabilities = NULL;
  unsigned int i = 0;
  while (items[i]) {
    if (g_strcmp0 (items[i], "sievedir") == 0)
      sievedir = g_strstrip (g_key_file_get_string (gfile, "timsieved_fs",
						    items[i], NULL));
    if (g_strcmp0 (items[i], "owner") == 0) {
      owner = g_key_file_get_integer (gfile, "timsieved_fs", items[i], NULL);
      umask (600);
    }
    if (g_strcmp0 (items[i], "group") == 0)
      group = g_key_file_get_integer (gfile, "timsieved_fs", items[i], NULL);
    if (g_strcmp0 (items[i], "virtdomain") == 0)
      virtdomain = g_key_file_get_boolean (gfile,
					   "timsieved_fs", items[i], NULL);
    if (g_strcmp0 (items[i], "defaultdomain") == 0)
      defaultdomain = g_strstrip(g_key_file_get_string (gfile, "timsieved_fs",
							items[i], NULL));
    if (g_strcmp0(items[i], "capabilities") == 0) {
      capabilities = g_key_file_get_string_list (gfile, "timsieved_fs",
						 items[i], NULL, NULL);
      int k = 0;
      while (capabilities[k]) g_strstrip(capabilities[k++]);
    }
    i++;
  }
  g_strfreev (items);
  return capabilities;
}

static inline char*
getPath (const char* const address)
{
  if (strcmp (address, "global") == 0)
    return g_strconcat (sievedir, "/global/", NULL);
  else 
    if (address[0] <= 'z' && address[0] >= 'a') {
      char temp[2];
      temp[0] = address[0];
      temp[1] = '\0';
      return g_strconcat (sievedir, "/",temp , "/", address, "/", NULL);
    }
    else
      return g_strconcat (sievedir, "/q/", address, "/", NULL);
}

void
install (const char* const address, const char* const data)
{
  char* const path = getPath (address);
  mkdir (path, 0);
  char *_default = g_strconcat (path, "defaultbc", NULL);
  char *bytecode = g_strconcat (path, "mailing-lists-sieve.siv.bc", NULL);
  char *script   = g_strconcat (path, "mailing-lists-sieve.siv.script", NULL);
  unlink (_default);
  symlink ("mailing-lists-sieve.siv.bc", _default);
  FILE* f = fopen (bytecode, "w");
  fclose (f);
  f = fopen (script, "w");
  fprintf (f, "%s", data);
  fclose (f);
  if (owner !=0 && group != 0) {
    chown (bytecode, owner, group);
    chown (script, owner, group);
    chmod (bytecode, 384);
    chmod (script, 384);
  }
  g_free (path);
  g_free (bytecode);
  g_free (_default);
  g_free (script);
}

void
delete (const char* const address)
{
  char* const path = getPath (address);
  char *_default = g_strconcat (path, "defaultbc", NULL);
  char *bytecode = g_strconcat (path, "mailing-lists-sieve.siv.bc", NULL);
  char *script   = g_strconcat (path, "mailing-lists-sieve.siv.script", NULL);
  unlink (_default);
  unlink (bytecode);
  unlink (script);
  rmdir (path);
  g_free (path);
  g_free (_default);
  g_free (bytecode);
  g_free (script);
}

void
close ()
{
  g_free (sievedir);
  if (defaultdomain) g_free (defaultdomain);
}
