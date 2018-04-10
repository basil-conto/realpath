/* Realpath - Emacs dynamic module interface to canonicalize_file_name(3).

Written in 2017 by Basil L. Contovounesios <basil.conto@gmail.com>

This file is NOT part of Emacs.

To the extent possible under law, the author has dedicated all
copyright and related and neighbouring rights to this software to the
public domain worldwide.  This software is distributed without any
warranty.

You should have received a copy of the CC0 Public Domain Dedication
along with this software.  If not, see
<https://creativecommons.org/publicdomain/zero/1.0/>.  */

#define _GNU_SOURCE             /* For canonicalize_file_name.                */

#include "emacs-module.h"       /* For emacs_*.                               */

#include <errno.h>              /* For errno.                                 */
#include <stddef.h>             /* For NULL, ptrdiff_t.                       */
#include <stdlib.h>             /* For canonicalize_file_name, free, malloc.  */
#include <string.h>             /* For strerror, strlen.                      */

int plugin_is_GPL_compatible;

/* Frequently used symbols.  */

static emacs_value Qdirectory_name_p;
static emacs_value Qexpand_file_name;
static emacs_value Qfile_name_as_directory;
static emacs_value Qnil;

/* Return GC-protected global reference to interned NAME.  */

static emacs_value
realpath_intern (emacs_env *env, const char *name)
{
  return env->make_global_ref (env, env->intern (env, name));
}

/* Wrap module_make_string using strlen(3).  */

static emacs_value
realpath_make_string (emacs_env *env, const char *contents)
{
  return env->make_string (env, contents, strlen (contents));
}

/* Signal error with NAME and message describing errno(3).  */

static void
realpath_signal (emacs_env *env, const char *name)
{
  emacs_value msg  = realpath_make_string (env, strerror (errno));
  emacs_value data = env->funcall (env, env->intern (env, "list"), 1, &msg);
  env->non_local_exit_signal (env, env->intern (env, name), data);
}

/* Wrap module_copy_string_contents for arbitrary-length strings.  */

static char *
realpath_copy_string (emacs_env *env, emacs_value value)
{
  ptrdiff_t len;
  char *buffer = NULL;

  /* Measure, allocate, copy.  */
  if (env->copy_string_contents (env, value, buffer, &len)
      && (buffer = malloc (len))
      && env->copy_string_contents (env, value, buffer, &len))
    return buffer;

  /* Check for malloc error; module_copy_string_contents should handle
     its own.  */
  if (! buffer)
    realpath_signal (env, "error");

  free (buffer);
  return NULL;
}

#define REALPATH_TRUENAME_DOC                                      \
  "Like `file-truename', but using `canonicalize_file_name(3)'.\n" \
  "\n"                                                             \
  "(fn FILENAME)"

static emacs_value
Frealpath_truename (emacs_env *env, ptrdiff_t nargs, emacs_value *args,
                    void *ptr)
{
  (void) nargs;
  (void) ptr;

  /* Falsepath, truepath.  */
  char *fp, *tp;
  emacs_value fpath, tpath;

  tpath = Qnil;
  fpath = env->funcall (env, Qexpand_file_name, 1, args);
  fp    = realpath_copy_string (env, fpath);

  if ((tp = canonicalize_file_name (fp)))
  {
    tpath = realpath_make_string (env, tp);
    /* Return directory name when given one à la Ffile_truename.  */
    if (env->is_not_nil (env, env->funcall (env, Qdirectory_name_p, 1, &fpath)))
      tpath = env->funcall (env, Qfile_name_as_directory, 1, &tpath);
  }
  /* Allow non-existent expanded filename à la Ffile_truename.  */
  else if (errno == ENOENT)
    tpath = fpath;
  else
    realpath_signal (env, "file-error");

  free (fp);
  free (tp);

  return tpath;
}

/* Bind `realpath-truename' and provide `realpath'.  */

int
emacs_module_init (struct emacs_runtime *ert)
{
  emacs_value Qdefalias, Qprovide;
  emacs_value Qrealpath, Qrealpath_truename, Srealpath_truename;

  emacs_env *env          = ert->get_environment (ert);
  Qdirectory_name_p       = realpath_intern (env, "directory-name-p");
  Qexpand_file_name       = realpath_intern (env, "expand-file-name");
  Qfile_name_as_directory = realpath_intern (env, "file-name-as-directory");
  Qnil                    = realpath_intern (env, "nil");
  Qdefalias               = env->intern     (env, "defalias");
  Qprovide                = env->intern     (env, "provide");
  Qrealpath               = env->intern     (env, "realpath");
  Qrealpath_truename      = env->intern     (env, "realpath-truename");
  Srealpath_truename      = env->make_function (env, 1, 1, Frealpath_truename,
                                                REALPATH_TRUENAME_DOC, NULL);

  emacs_value args[] = {Qrealpath_truename, Srealpath_truename};

  env->funcall (env, Qdefalias, 2, args);
  env->funcall (env, Qprovide,  1, &Qrealpath);

  return EXIT_SUCCESS;
}
