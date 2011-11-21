#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <gsasl.h>
#include <glib.h>

#define extensions managesieve_LTX_extensions
#define install managesieve_LTX_install
#define delete managesieve_LTX_delete
#define close_ managesieve_LTX_close

extern GKeyFile *gfile;
struct server {
  char *host;
  char *user;
  char *service;
  char *realm;
  char *password;
  unsigned short port;
  const char* sasl_mech;
  char bits; //xxxxxxx0 - unauthenticate not supported
             //xxxxxxx1 - unauthenticate supported
             //xxxxxx0x - connection closed
             //xxxxxx1x - connection opened
  int socket;
  char** extensions;
};

static Gsasl *sasl = NULL;
static GPtrArray *servers;
static char hostname[300];

static inline char*
send_string (char* str)
{
  size_t k = strlen(str);
  GString* g = g_string_sized_new (k + 20);
  g_string_printf (g, "{%zu+}\r\n%s\r\n", k, str);
  return g_string_free (g, 0);
}

static inline char*
parse_response (char *str)
{
  switch (str[0]) {
    case 'O':
      if (strstr (str, "OK") == str) return NULL;
      //	printf("OK with reason %s\n", str+2);
      return NULL;
    case 'B':
      if (strstr (str, "BYE") == str) return NULL;
	//printf("closing connection with reason %s.\n", str+3);
      return NULL;
    case 'N':
      if (strstr (str, "NO") == str)
	return str;
  }
  return NULL;
}

static inline char*
receive_string (char* str)
{
  char* ret = NULL;
  switch (str[0]) {
  case '"':
    if (str[1] == '"') return NULL;
    ret = strdup (str+1);
    ret[strlen (ret)-1] = '\0';
    break;
  case '{':
    if (str[1] == '0') return NULL;
    char* x;
    int length = strtol (str + 1, &x, 10);
    ret = g_malloc (length + 3);
    strcpy (ret, x + 3);
    ret[strlen (ret) - 2] = '\0';
  }
  return ret;
}

static inline GString*
request (struct server *s, char* data)
{
  if (data != NULL)
    write (s->socket, data, strlen (data));
  GString *gstr = g_string_new ("");
  char b[120];
  int i;
  do {
    i = read ( s->socket, b, 119);
    b[i] = '\0';
    g_string_append (gstr, b);
  } while (i == 119);
  return gstr;
}

static inline void
server_connect (struct server *s)
{
    struct addrinfo *res, *pointer;
    getaddrinfo (s->host, NULL , NULL, &res);
    pointer = res;
    s->socket = 0;
    while (res) {
      s->socket = socket (res->ai_family, SOCK_STREAM, 0);
      switch (res->ai_family) {
      case AF_INET:
	((struct sockaddr_in*)res->ai_addr)->sin_port = htons (s->port);
	break;
      case AF_INET6:
	((struct sockaddr_in6*)res->ai_addr)->sin6_port = htons (s->port);
      }
      if ( 0 == connect (s->socket, (struct sockaddr *)(res->ai_addr),
			res->ai_addrlen))
	break;
      res = res->ai_next;
    };
    freeaddrinfo (pointer);
}

static inline int
client_authenticate (struct server *s, const char* const user)
{
  int rc;
  Gsasl_session* session;
  if ((rc = gsasl_client_start (sasl, s->sasl_mech, &session)) != GSASL_OK)
    printf ("Cannot initalize client (%d): %s\n", rc, gsasl_strerror (rc));
  gsasl_property_set (session, GSASL_AUTHID, s->user);
  gsasl_property_set (session, GSASL_PASSWORD, s->password);
  gsasl_property_set (session, GSASL_SERVICE,
		      (s->service)?(s->service):"sieve");
  if (s->realm) {
    gsasl_property_set (session, GSASL_REALM, s->realm);
  };
  gsasl_property_set (session, GSASL_HOSTNAME, hostname);
  if (user) gsasl_property_set (session, GSASL_AUTHZID, user);
  char* str = g_strconcat ("AUTHENTICATE \"", s->sasl_mech, "\"\r\n", NULL);
  GString* data = request (s, str);
  g_free (str);
  do {
    char *resp = receive_string (data->str);
    g_string_free (data, 1);
    rc = gsasl_step64 (session, resp, &str);
    g_free (resp);
    if (rc == GSASL_NEEDS_MORE || rc == GSASL_OK) {
      resp = send_string (str);
      data = request (s, resp);
      g_free (resp);
      gsasl_free (str);
    } else
      printf ("SASL negotiation failure (%d): %s\n",
	      rc, gsasl_strerror (rc));
  } while (rc == GSASL_NEEDS_MORE);
  str = parse_response (data->str);
  g_string_free(data, 1);
  if (rc != GSASL_OK || str != NULL) {
    printf ("Authenticating with ManageSieve to server %s, port %i, user %s, password %s, service %s, realm %s, mechanism %s failed: %s\n",
	    s->host, s->port, s->user, s->password,
	    (s->service)?s->service:"sieve", s->realm, s->sasl_mech, str);
    if (str) g_free (str);
    return -1;
  }
  gsasl_finish (session);
  return 0;
}

static inline void parse_config_file(GKeyFile* fi)
{
  char **items =  g_key_file_get_keys (fi, "managesieve", NULL, NULL);
  if (items == NULL) return;
  struct server *s = g_malloc0 (sizeof (struct server));
  int j = 0;
  while (items[j]) {
    if (g_ascii_strcasecmp (items[j], "host") == 0) {
      s->host = g_strstrip (g_key_file_get_string (fi, "managesieve",
						   items[j], NULL));
      char* a = strchr (s->host, ':');
      if (a) {
	a[0] = '\0';
	s->port = strtol (a + 1, NULL, 10);
      }
    } else if (g_ascii_strcasecmp (items[j], "port") == 0) {
      if (s->port)
	fprintf (stderr,
		 "port already specified in section \"managesieve\".\n");
      else
	s->port = g_key_file_get_integer (fi, "managesieve", items[j], NULL);
    } else if (g_ascii_strcasecmp (items[j], "user") == 0)
      s->user = g_strstrip (g_key_file_get_string (fi, "managesieve",
						   items[j], NULL));
    else if (g_ascii_strcasecmp (items[j], "password") == 0)
      s->password = g_strstrip(g_key_file_get_string (fi, "managesieve",
						      items[j], NULL));
    else if (g_ascii_strcasecmp (items[j], "service") == 0)
      s->service = g_strstrip (g_key_file_get_string (fi, "managesieve", items[j], NULL));
    else if (g_ascii_strcasecmp (items[j], "realm") == 0)
      s->realm = g_strstrip (g_key_file_get_string (fi, "managesieve", items[j], NULL));
    j++;
  }
  g_strfreev (items);
  if (s->port == 0)
    s->port = 4192;
  g_ptr_array_add (servers, s);
}

static inline char**
split_string_in_words (char* string)
{
  int num_words = 1;
  int i;
  for (i = 0; string[i] != '\0'; i++)
    if (string[i] == ' ') num_words++;
  char **res = g_malloc (sizeof(char*) * (num_words+1));
  res[num_words] = NULL;
  num_words = 0;
  res[0] = string;
  for (i = 0; string[i] != '\0'; i++)
    if (string[i] == ' ') {
      num_words++;
      string[i] = '\0';
      res[num_words] = string + i + 1;
    }
  return res;
}

char**
extensions ()
{
  if (sasl == NULL) {
    int rc = gsasl_init (&sasl);
    if (rc != GSASL_OK) {
      printf ("SASL initialization failure (%d): %s\n",
	      rc, gsasl_strerror (rc));
      return NULL;
    };
  };
  gethostname (hostname, 299);
  servers = g_ptr_array_new ();
  parse_config_file (gfile);
  //connect
  if (servers->len == 0) {
    g_ptr_array_free (servers, 1);
    return NULL;
  }
  unsigned int j = 0;
  while ( j < servers->len) {
    struct server *s = g_ptr_array_index (servers, j++);
    server_connect (s);
    //receive capabilities
    GString *gstr = request (s, NULL);
    g_string_ascii_up (gstr);
    //check for UNAUTHENTICATE
    if (g_strstr_len (gstr->str, gstr->len, "\"UNAUTHENTICATE\"")) {
      s->bits |= 1;
    }
    //check for SASL mechanisms, temp contains the list provided by the server
    char *temp = g_strdup (g_strstr_len (gstr->str, gstr->len, "\"SASL\" \"") 
			   + 8);
    temp[strchr (temp, '"') - temp] = '\0';
    char** supported_mechs_by_server = split_string_in_words (temp);
    char* excluded_mechs[] = {"LOGIN", "GSSAPI", "CRAM-MD5", NULL};
    int g, h, len = 0;
    for (h = 0; excluded_mechs[h]; h++)
      for (g = 0; supported_mechs_by_server[g] != NULL; g++)
	if (strcmp (supported_mechs_by_server[g], excluded_mechs[h]) == 0) {
	  int z;
	  for (z = g; supported_mechs_by_server[z]; z++)
	    supported_mechs_by_server[z] = supported_mechs_by_server[z+1];
	  continue;
	} else 
	  len += 1 + strlen (supported_mechs_by_server[g]);
    char* final_list_sasl = g_malloc (len + 2);
    final_list_sasl[0] = '\0'; h =0;
    for (g = 0; supported_mechs_by_server[g] != NULL; g++) {
      strcat (final_list_sasl+h, supported_mechs_by_server[g]);
      h += strlen(supported_mechs_by_server[g]) + 1;
      final_list_sasl[h-1] = ' ';
      final_list_sasl[h] = '\0';
    }
    final_list_sasl[h-1] = '\0';
    g_free (supported_mechs_by_server);

    s->sasl_mech = gsasl_client_suggest_mechanism (sasl, final_list_sasl);
    g_free (final_list_sasl);
    client_authenticate (s, NULL);
    //check for extensions in "SIEVE"
    temp = g_strdup (g_strstr_len (gstr->str, gstr->len, "\"SIEVE\" \"")+9);
    temp[strchr (temp, '"') - temp] = '\0';
    s->extensions = g_strsplit (temp, " ",-1);
    g_free (temp);
    if ((s->bits && 1) == 0) {
      shutdown (s->socket, 2);
      close (s->socket);
    } else {
      g_string_free (request (s, "UNAUTHENTICATE\r\n"), 1);
    }
  }
  struct server* s = g_ptr_array_index (servers, 0);
  return s->extensions;
}

static inline char*
curdate ()
{
  static const char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
  static const char *wday[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  time_t _time = time (NULL);
  struct tm lt;
  long gmtoff = -0;
  int gmtneg = 0;
  localtime_r (&_time, &lt);
  char* t = g_malloc (149);
  sprintf(t, "# generated on %s, %02d %s %4d %02d:%02d:%02d %c%.2lu%.2lu\r\n",
          wday[lt.tm_wday], lt.tm_mday, month[lt.tm_mon], lt.tm_year + 1900, 
          lt.tm_hour, lt.tm_min, lt.tm_sec, 
	  gmtneg ? '-' : '+', gmtoff / 60, gmtoff % 60);
  return t;
}


void
install (const char* const address, const char* const data)
{
  unsigned int j = 0;
  while (j < servers->len) {
    struct server *s = g_ptr_array_index (servers, j++);
    if ((s->bits && 1) == 0) { 
      server_connect (s);
      g_string_free (request (s, NULL), 1);
    }
    client_authenticate (s, address);
    char* date = curdate ();
    char *string = g_strconcat (date, data, NULL);
    g_free (date);
    char* script = send_string (string);
    g_free (string);
    char* str = g_strconcat ("PUTSCRIPT \"mailing-lists.sieve\" ",  script, "\r\n", NULL);
    GString *out = request (s, str);
    g_free (str);
    str = parse_response (out->str);
    if (str)
      printf ("error for script user %s, script %s\n", address, str);
    g_free (script);
    g_string_free (out, 1);
    g_string_free (request (s, "SETACTIVE \"mailing-lists.sieve\"\r\n"), 1);
    if (s->bits && 1) {
      g_string_free (request (s, "UNAUTHENTICATE\r\n"), 1);
    } else {
      g_string_free (request (s, "LOGOUT\r\n"), 1);
      shutdown (s->socket, 2);
      close (s->socket);
    }
  }
  return;
}

/*
void delete(char* address) {
}
*/
void
close_ ()
{
  //
  //unsigned int a;
  //for (a = 0; a < servers->len; a++) {
    struct server *s = g_ptr_array_index(servers, 0);
    if ((s->bits && 1) == 0) {
      shutdown (s->socket, 2);
      close (s->socket);
    }
    g_free(s->user);
    if (s->service) g_free(s->service);
    g_strfreev(s->extensions);
    g_free(s->password);
    g_free(s->host);
    g_free(s);
  //}
  g_ptr_array_free(servers, 1);
}
