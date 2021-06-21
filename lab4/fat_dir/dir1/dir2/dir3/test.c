#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"

#define LOGNAME "mount_fat16.log"


static FILE *logfile;

void log_open()
{
  logfile = fopen(LOGNAME, "w");

  if (logfile == NULL) {
    perror("[ERRO] Could not open log file.");
    exit(EXIT_FAILURE);
  }

  // Set logfile to line buffering
  setvbuf(logfile, NULL, _IONBF, 0);
}

void log_msg(const char *format, ...)
{
  va_list ap;
  va_start(ap, format);
  vfprintf(logfile, format, ap);
}
