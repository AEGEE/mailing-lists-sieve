#include <liblistserv.h>
#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

extern GKeyFile* gfile;
static FILE* file = NULL;
extern const char* configuration_file;
static struct listserv* l;
static int capabilities = 10;
static GHashTable *list_dependencies;
static GHashTable *created_scripts;

#define supported_lists		listserv_LTX_supported_lists
#define unload			listserv_LTX_unload
#define get_scripts		listserv_LTX_get_scripts

char **
supported_lists ()
{
  //parse configuration file
  file = fopen ("/var/log/mailing-lists-sieve", "a");
  char *user = NULL, *password = NULL, *host = NULL;
  char **items =  g_key_file_get_keys (gfile, "listserv", NULL, NULL);
  if (items) {
      int j = 0;
      while (items[j]) {
          if (g_ascii_strcasecmp (items[j], "host") == 0)
	    host = g_strstrip (g_key_file_get_string (gfile, "listserv",
						      items[j], NULL));
	  if (g_ascii_strcasecmp (items[j], "user") == 0)
	    user = g_strstrip (g_key_file_get_string (gfile, "listserv",
						      items[j], NULL));
	  if (g_ascii_strcasecmp (items[j], "password") == 0)
	    password = g_strstrip (g_key_file_get_string (gfile, "listserv", 
							  items[j], NULL));
	  if (g_ascii_strcasecmp (items[j], "capabilities") == 0)
	    capabilities = g_key_file_get_integer (gfile, "listserv", 
						   items[j], NULL);
	  j++;
      }
      g_strfreev (items);
  };
  l = listserv_init (user, password, host);
  //verify that connection to that server can be made
  char *str = listserv_command (l, "QUERY *");
  if (strcmp (str, "***BADPW***\n") == 0) {
    fprintf (stderr, "Password not accepted when connecting to listserv running on %s, with email %s and password \"%s\". Please edit section \"%s\" in the configuration file \"%s\".\n",
	     host, user, password, "listserv", configuration_file);
    listserv_destroy (l);
    exit (1);
  }
  char **results = listserv_getowned_lists (l);
  list_dependencies = g_hash_table_new_full (g_str_hash, g_str_equal,
					     g_free, NULL);
  created_scripts = g_hash_table_new_full (g_str_hash, g_str_equal,
					 g_free, NULL);
  if ((capabilities & 8) == 0) {//without extlists extension
    int i = 0;
    static const char *keywords[] = {"Send", "Owner", "Configuration-Owner",
				     "Editor", "Moderator"};
    while (results[i]) {
      int j = 0;
      int public = 0;
      char **kw = listserv_getlist_keyword (l, results[i], "Send");
      while (kw[j])
	if (g_ascii_strcasecmp (kw[j], "Public") == 0) {
	  public = 1;
	  break;
	} else j++; //end while (kw[j])
      if (public == 0) // if everybody can post to the list, the other keywords: Send, Owner, Configuration-Owner, Editor and Moderator do not matter when determining on which lists this one depends
	for (j = 0; j < 5; j++) {
	  int k = 0;
	  kw = listserv_getlist_keyword (l, results[i], keywords[j]);
	  while (kw[k]) {
	    if (strchr (kw[k], '(')) {
	      char *listname = g_ascii_strup (strchr (kw[k], '(') + 1, -1);
	      listname [ strlen (listname) -1] = '\0';
	      GHashTable *elem = g_hash_table_lookup (list_dependencies,
						      listname);
	      if (elem == NULL)
		elem = g_hash_table_new_full (g_str_hash, g_str_equal,
					      g_free, NULL);
	      g_hash_table_insert(elem, g_strdup (results[i]), NULL);
	      g_hash_table_insert(list_dependencies, listname, elem);
              //list_dependencies
	    }
	    k++;
	  }
	}//for & if (public == 1)
      char** sublists = listserv_getlist_keyword (l, results[i], "Sub-Lists");
      if (sublists[0]) {
	int m = 0;
	while (sublists[m]) {
	    GHashTable *elem = g_hash_table_lookup (list_dependencies,
						    sublists[m]);
	    if (elem == NULL)
	      elem = g_hash_table_new_full (g_str_hash, g_str_equal,
					    g_free, NULL);
	    g_hash_table_insert (elem, g_strdup (results[i]), NULL);
	    g_hash_table_insert (list_dependencies, g_strdup (sublists[m]),
				 elem);
	    m++;
	}
      }
      i++;
    }
  }
  if (user) free (user);
  if (host) free (host);
  if (password) free (password);
  return results;
}

static char**
get_scripts_recursive (struct listserv *l, char* const listname,
		       GHashTable *loop_protection)
{
  // loop protection - stop if this list is already proceeded
  if (g_hash_table_lookup_extended (loop_protection, listname, NULL, NULL)
      || g_hash_table_lookup_extended (created_scripts, listname, NULL, NULL)){
    char **x = g_malloc (sizeof(char*));
    x[0] = NULL;
    return x;
  }
  g_hash_table_insert (created_scripts, g_strdup(listname), NULL);
  g_hash_table_insert (loop_protection, listname, NULL);

  int scripts_pos = 0;
  int scripts_size = 5;
  char **scripts = g_malloc (scripts_size * sizeof(char*));
#define ADD_SCRIPT(x) if (scripts_size == scripts_pos) { 	              \
        scripts_size += 5;                                    	              \
        scripts = g_realloc (scripts, (scripts_size+1) * sizeof (char*));     \
    }                                                                         \
    scripts[scripts_pos++] = g_strdup (x);

  char **temp = listserv_getsieve_scripts (l, listname, capabilities);
  int j = 0;
  while (temp[j]) {
    ADD_SCRIPT(temp[j]);
    j++;
  }
  GHashTable *t = g_hash_table_lookup (list_dependencies, listname);
  if (t != NULL) {
    GHashTableIter iter;
    g_hash_table_iter_init (&iter, t);
    gpointer p1, always_null;
    while (g_hash_table_iter_next (&iter, &p1, &always_null)) {
      char **temp2 = get_scripts_recursive (l, (char*)p1, loop_protection);
      if (temp2[0]) {
	time_t _time = time(NULL);
	struct tm lt;
	localtime_r (&_time, &lt);
	fprintf (file,
		 "%04d/%02d/%02d %02d:%02d:%02d script upload %s (via %s)\n",
		 1900 + lt.tm_year, lt.tm_mon + 1, lt.tm_mday,
		 lt.tm_hour, lt.tm_min, lt.tm_sec, (char*)p1, listname);
	fflush (file);
	int k = 0;
	while (temp2[k]) {
	  ADD_SCRIPT (temp2[k]);
	  k++;
	}
      }
      g_strfreev (temp2);
    }
  }
  scripts[scripts_pos] = NULL;
  return scripts;
}

char **
get_scripts (char* const listname)
{
  if (g_hash_table_lookup_extended (created_scripts, listname, NULL, NULL)) {
    char **result = g_malloc (sizeof(char*));
    result[0] = NULL;
    return result;
  }
  time_t _time = time(NULL);
  struct tm lt;
  localtime_r (&_time, &lt);
  fprintf (file,
	   "%04d/%02d/%02d %02d:%02d:%02d script upload %s\n",
	   1900 + lt.tm_year, lt.tm_mon + 1, lt.tm_mday,
	   lt.tm_hour, lt.tm_min, lt.tm_sec, listname);
  fflush (file);
  char **results;
  if ((capabilities & 8) == 1) {
    char **temp = listserv_getsieve_scripts (l, listname, capabilities);
    int i = 0;
    while (temp[i]) i++;
    results = g_malloc((i+1) * sizeof (char*));
    results[i--] = NULL;
    for (; i >= 0; i--)
      results[i] = g_strdup(temp[i]);
  } else {
    GHashTable *loop_protection = g_hash_table_new (g_str_hash, g_str_equal);
    results = get_scripts_recursive(l, listname, loop_protection);
    g_hash_table_destroy(loop_protection);
  }
  return results;
  //ereject 4
  //reject  2
  //extlists 8
  //envelope 1
}

void
unload ()
{
  GHashTableIter iter;
  gpointer a, b;
  g_hash_table_iter_init(&iter, list_dependencies);
  while (g_hash_table_iter_next(&iter, &a, &b)) {
    g_hash_table_destroy((GHashTable*)b);
    g_hash_table_iter_remove(&iter);
  }
  g_hash_table_destroy (created_scripts);
  g_hash_table_destroy (list_dependencies);
  listserv_destroy (l);
  fclose (file);
}
