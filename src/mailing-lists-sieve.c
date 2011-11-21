#include "mailing-lists-sieve.h"
#include <unistd.h>
#include <glib/gprintf.h>

GKeyFile *gfile;
char *configuration_file = NULL;
static gint all_lists = 0, show_list_names = 0, _stdout = 0, _delete = 0,
  delay = 0, version = 0;
//help, version;

static GOptionEntry entries[] = {
  {"all-lists", 'a', 0, G_OPTION_ARG_NONE, &all_lists,
   "Generates and uploads the scripts for all supported lists", ""},
  {"show-list-names", 's', 0, G_OPTION_ARG_NONE,
   &show_list_names, "Prints the names of the supported mailing lists", ""},
  {"stdout", 'o', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &_stdout,
   "Does not upload scripts but sends them to the standard output", ""},
  {"delete", 'd', G_OPTION_FLAG_HIDDEN , G_OPTION_ARG_NONE, &_delete,
   "Deletes the scripts for the given lists", ""},
  {"delay" , 'f', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &delay,
   "seconds.  Forks in the background, waits for that seconds and then uploads the scripts.", ""},
  {"config-file", 'c', 0, G_OPTION_ARG_FILENAME, &configuration_file,
   "where to look for the configuration file", ""},
  {"version", 'v', 0, G_OPTION_ARG_NONE, &version,
   "prints version and exists", ""},
  {NULL, 0, 0, 0, NULL, NULL, NULL}
};

static void
put_scripts_for_list (const char* const list)
{
  char **scripts = get_scripts (list);
  if (scripts == NULL) return;
  //  printf ("%35s\n", list);
  unsigned int i = 0;
  while (scripts[i]) {
    if (strcmp (scripts[i+1], ""))
      install (scripts[i],
	       scripts[i+1]);
    //    printf ("%s\n", scripts[i]);
    i+=2;
  };
  g_strfreev(scripts);
}

int
main (int argc, char* argv[])
{
  int listserv_exit;
  char *listname = NULL;
  if (0 == strcmp (g_get_user_name(), "listserv") && argc > 3) {
    /*
     * char* exit_input = g_strconcat (g_get_home_dir(),
     * 		 	 	       "/home/exit.input", NULL);
     * FILE* f = fopen (exit_input, "r");
     * g_free (exit_input);
     * if (f == NULL) exit (0);
     * fscanf (f, "%as %as", &exitname, &listname);
     * fclose (f);
     */
    if (strcmp (argv[1], "SUB_NEW") == 0
	|| strcmp (argv[1], "ADD_NEW") == 0
	|| strcmp (argv[1], "DEL_SIGNOFF") == 0
	|| strcmp (argv[1], "DEL_DELETE") == 0
	|| strcmp (argv[1], "CHG_REQ")== 0) {
      listname = argv[2];
      pid_t pid = fork ();
      if (pid < 0)
	printf ("fork in main() failed.\n");
      if (pid > 0)
	exit (0);
      listserv_exit = 1;
      if (setsid () < 0)
	printf ("setsid() in main() failed.\n");
      sleep(3);
    } else
      exit (0);
  } else
    listserv_exit = 0;


  GError *gerror = NULL;
  GOptionContext* gopt = g_option_context_new("tralala");
  g_option_context_add_main_entries(gopt, entries, NULL);
  g_option_context_set_summary (gopt, "mailing-lists-sieve 1.0 GLP3");
  g_option_context_set_description (gopt,
		    "report bugs to mailing-lists-sieve@aegee.org");
  g_option_context_set_help_enabled(gopt, TRUE);
  //  if (!g_option_context_parse(gopt, &argc, &argv, &gerror))
  //    g_fprintf(stderr, "option parsing failed: %s\n", gerror->message);
  configuration_file = g_strconcat (g_get_home_dir(),
				    "/.mailing-lists-sieve.conf", NULL);
  g_option_context_free(gopt);
  FILE* f = fopen (configuration_file, "r");
  if (f)
    fclose (f);
  else {
    g_free (configuration_file);
    configuration_file = g_strconcat (SYSCONFDIR,
				      "/mailing-lists-sieve.conf", NULL);
  }
  gfile = g_key_file_new ();
  g_key_file_set_list_separator (gfile, ',');
  if (g_key_file_load_from_file (gfile, configuration_file,
				 G_KEY_FILE_NONE, NULL) == 0) {
    fprintf (stderr, "Configuration file \"%s\" cannon be loaded.\n",
	    configuration_file);
    exit (2);
  }
  if (load_uploaders () == NULL)
    printf ("No uploaders configured.\n");
  if (load_generators () == -1)
    printf ("No generators configured.\n");
  g_free (configuration_file);
  g_key_file_free (gfile);
  if (listserv_exit) {
    put_scripts_for_list (listname);
  } else if (argc > 1) {
    if (strcmp (argv[1], "-a") == 0 ) { //update the scripts for all lists
      all_lists = 1;
      GPtrArray* p = get_supported_lists ();
      unsigned int i;
      for (i = 0; i < p->len; i++)
	put_scripts_for_list ((char*)g_ptr_array_index (p, i));
      g_ptr_array_free (p, 1);
    } else {
      listname = g_ascii_strup(argv[1], -1);
      //      printf ("single list %s\n", listname);
      put_scripts_for_list (listname);
      g_free(listname);
    }
  }
  unload_generators ();
  unload_uploaders ();
  return 0;  
}
