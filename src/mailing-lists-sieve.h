#ifndef INCLUDES_H
#define INCLUDES_H 1

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int load_generators();
char** get_scripts(const char* const listname);
void unload_generators();
GPtrArray* get_supported_lists();

char** load_uploaders();
void install(const char* const address, const char* const data);
void delete(const char* const address);
void unload_uploaders();

#endif
