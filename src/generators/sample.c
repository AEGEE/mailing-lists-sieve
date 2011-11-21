#define supported_lists		sample_LTX_supported_lists
#define get_scripts		sample_LTX_get_scripts
#define unload			sample_LTX_unload

#include <string.h>
#include <stdlib.h>


static char *sup_lists[] = {"SAMPLE4AEGEE-L", NULL};

char**
supported_lists ()
{
  return sup_lists;
}

char**
get_scripts (const char* const listname)
{
  char **results = malloc(3 * sizeof (char*));
  results[0] = strdup(listname);
  results[1] = strdup("require [\"envelope\", \"reject\"];\r\n if envelope :is \"from\" \"ivan.vasov@aegee.org\" \r\n	{\r\n		reject \"Sorry, you cannot post here.\r\nTry with a different address\";\r\n	}\r\n");
  results[2] = NULL;
  return results;
}
