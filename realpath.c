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

#include <emacs-module.h>       /* For emacs_*.                               */

#include <errno.h>              /* For errno.                                 */
#include <stdbool.h>            /* For bool.                                  */
#include <stddef.h>             /* For NULL, ptrdiff_t.                       */
#include <stdlib.h>             /* For canonicalize_file_name, free, malloc.  */
#include <string.h>             /* For strerror, strlen.                      */

int plugin_is_GPL_compatible;

/* Return true if the last module function exited normally;
   otherwise signal an error and return false.  */

static bool
rp_check (emacs_env *env)
{
  if (env->non_local_exit_check (env) == emacs_funcall_exit_return)
    return true;

  emacs_value err, data;
  env->non_local_exit_get (env, &err, &data);
  env->non_local_exit_signal (env, err, data);
  return false;
}

/* Like module_make_string, but store result in STRING and determine
   length of CONTENTS using strlen(3).  Return true on success;
   otherwise signal an error and return false.  */

static bool
rp_lisp_string (emacs_env *env, emacs_value *string, const char *contents)
{
  *string = env->make_string (env, contents, strlen (contents));
  return rp_check (env);
}

/* Like module_funcall, but call the function whose name is NAME and
   store result in VALUE.  Return true on sucess; otherwise signal an
   error and return false.  */

static bool
rp_funcall (emacs_env *env, emacs_value *value, const char *name,
            ptrdiff_t nargs, emacs_value *args)
{
  *value = env->funcall (env, env->intern (env, name), nargs, args);
  return rp_check (env);
}

/* Signal ERROR with description of ERRNUM if non-zero;
   otherwise with description of errno(3).  */

static void
rp_signal (emacs_env *env, const char *error, int errnum)
{
  emacs_value err, data;
  if (rp_lisp_string (env, &data, strerror (errnum ? errnum : errno))
      && rp_funcall (env, &data, "list", 1, &data)
      && (err = env->intern (env, error), rp_check (env)))
    env->non_local_exit_signal (env, err, data);
}

/* Wrap module_copy_string_contents for arbitrary-length strings.  */

static char *
rp_char_string (emacs_env *env, emacs_value value)
{
  ptrdiff_t len;
  char *buffer = NULL;

  /* Measure, allocate, copy.  */
  if (env->copy_string_contents (env, value, buffer, &len)
      && (buffer = malloc (len))
      && env->copy_string_contents (env, value, buffer, &len))
    return buffer;

  free (buffer);
  if (rp_check (env))
    /* malloc returned NULL.  */
    rp_signal (env, "error", ENOMEM);
  return NULL;
}

#define REALPATH_TRUENAME_DOC                                      \
  "Like `file-truename', but using `canonicalize_file_name(3)'.\n" \
  "\n"                                                             \
  "(fn FILENAME)"

static emacs_value
Frealpath_truename (emacs_env *env, ptrdiff_t nargs, emacs_value *args,
                    void *data)
{
  (void) nargs;
  (void) data;

  emacs_value file, dir;
  char *obuf, *nbuf;
  obuf = nbuf = NULL;

  if (! (rp_funcall (env, &file, "expand-file-name", 1, args)
         && (obuf = rp_char_string (env, file))
         && rp_lisp_string (env, &dir, obuf)))
    return file;

  if ((nbuf = canonicalize_file_name (obuf)))
    {
      if (rp_lisp_string (env, &file, nbuf)
          && rp_funcall (env, &dir, "directory-name-p", 1, &dir)
          && env->is_not_nil (env, dir))
        /* Return directory name when given one à la Ffile_truename.  */
        rp_funcall (env, &file, "file-name-as-directory", 1, &file);
    }
  /* Allow non-existent expanded filename à la Ffile_truename.
     FIXME: Handle ENOTDIR.  */
  else if (errno != ENOENT)
    rp_signal (env, "file-error", 0);

  free (obuf);
  free (nbuf);
  return file;
}

/* Bind `realpath-truename' and provide `realpath'.  */

int
emacs_module_init (struct emacs_runtime *ert)
{
  emacs_env *env = ert->get_environment (ert);

  emacs_value args[] = {env->intern (env, "realpath-truename"),
                        env->make_function (env, 1, 1, Frealpath_truename,
                                            REALPATH_TRUENAME_DOC, NULL)};

  return rp_funcall (env, args, "defalias", 2, args)
    && (args[0] = env->intern (env, "realpath"),
        rp_funcall (env, args, "provide", 1, args))
    ? EXIT_SUCCESS : EXIT_FAILURE;
}
