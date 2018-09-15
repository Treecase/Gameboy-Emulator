/*
 * Error logging
 *
 */

#ifndef __LOGGING_H
#define __LOGGING_H


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>


static bool LOG_DOLOG = false;

#define die()               exit (EXIT_FAILURE)

/* FIXME: debug calls can _REALLY_ slow things down -- not sure if there's a way around that... */
#define debug(format, ...)  ({  if (LOG_DOLOG)  printf (format "\n", ##__VA_ARGS__);          \
                                else            snprintf (NULL, 0, format, ##__VA_ARGS__);    })
#define debugl(format, ...) ({  if (LOG_DOLOG)  printf (format, ##__VA_ARGS__);   \
                                else            debug (format, ##__VA_ARGS__);    })

#define error(format, ...)  ({  debug ("%s:%i Error: " format, __FILE__, __LINE__, ##__VA_ARGS__);  })
#define fatal(format, ...)  ({  error (format, ##__VA_ARGS__); die();   })


#endif

