/* GNU m4 -- A simple macro processor
   Copyright 1989-1994, 1998, 1999, 2002, 2003 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307  USA
*/

#include "m4.h"
#include "pathconf.h"
#include "ltdl.h"
#include "m4private.h"

#define DEBUG_MODULES  /* Define this to see runtime debug info. */
#undef DEBUG_MODULES

/*
 * This file implements dynamic modules in GNU M4.  A module is a
 * compiled shared object, that can be loaded into GNU M4 at run
 * time. Information about creating modules is in ../modules/README.
 *
  * This implementation uses libltdl, which is in turn can open modules
 * using either dlopen(3) (exists on GNU/Linux, OSF, Solaris, SunOS and
 * others), shl_load(3) (exists on HPUX), LoadLibrary(3) (exists on
 * Windows, cygwin, OS/2), load_add_on(3) (exists on BeOS), NSAddImage
 * (exists on MacOS) and can also fall back to dld_link(3) from GNU
 * libdld or lt_dlpreload from libtool if shared libraries are not
 * available on the host machine.
 *
 * An M4 module will usually define an external symbol called
 * `m4_builtin_table'.  This symbol points to a table of `m4_builtin'.
 * The table is saved as libltdl caller data and each definition therein
 * is added to the symbol table.
 *
 * To load a module, call m4_module_load(), which uses the libltdl
 * API to find the module in the module search path.  The search
 * path is initialised from the environment variable M4MODPATH, followed
 * by the configuration time default where the modules shipped with M4
 * itself are installed.  Libltdl reads the libtool .la file to
 * get the real library name (which can be system dependent), returning
 * NULL on failure or else a libtool module handle for the newly mapped
 * vm segment containing the module code.  If the module is not already
 * loaded, m4_module_load() retrieves its value for the symbol
 * `m4_builtin_table', which is installed using set_module_builtin_table().
 *
 * In addition to builtin functions, you can also define static macro
 * expansions in the `m4_macro_table' symbol.  If you define this symbol
 * in your modules, it should be an array of `m4_macro's, mapping macro
 * names to the expansion text.  Any macros defined in `m4_macro_table'
 * are installed into the M4 symbol table with set_module_macro_table().
 *
 * Each time a module is loaded, the module function prototyped as
 * "M4INIT_HANDLER (<module name>)" is called, if defined.  Any value
 * stored in OBS by this function becomes the expansion of the macro
 * which called it.  Before M4 exits, all modules are unloaded and the
 * function prototyped as "M4FINISH_HANDLER (<module name>)" is called,
 * if defined.  It is safe to load the same module several times: the
 * init and finish functions will also be called multiple times in this
 * case.
 *
 * To unload a module, use m4_module_unload(). which uses
 * m4__symtab_remove_module_references() to remove the builtins defined by
 * the unloaded module from the symbol table.  If the module has been
 * loaded several times with calls to m4_module_load, then the module will
 * not be unloaded until the same number of calls to m4_module_unload()
 * have been made (nor will the symbol table be purged).
 **/

#define MODULE_SELF_NAME	"!myself!"

typedef struct {
  m4_builtin    *builtin_table;
  m4_macro	*macro_table;
} module_data;

static const char*  module_dlerror (void);
static int	    module_remove  (m4 *context, lt_dlhandle handle,
				    m4_obstack *obs);
static void	    module_close   (m4 *context, lt_dlhandle handle,
				    m4_obstack *obs);

static void set_module_macro_table   (m4*, lt_dlhandle, const m4_macro*);
static void set_module_builtin_table (m4*, lt_dlhandle, const m4_builtin*);

static lt_dlcaller_id caller_id = 0;

const char *
m4_get_module_name (lt_dlhandle handle)
{
  const lt_dlinfo *info;

  assert (handle);

  info = lt_dlgetinfo (handle);

  return info ? info->name : 0;
}

m4_builtin *
m4_get_module_builtin_table (lt_dlhandle handle)
{
  module_data *data;

  assert (handle);

  data = lt_dlcaller_get_data (caller_id, handle);

  return data ? data->builtin_table : 0;
}

static void
set_module_builtin_table (m4 *context, lt_dlhandle handle,
			  const m4_builtin *table)
{
  const m4_builtin *bp;

  assert (context);
  assert (handle);
  assert (table);

  for (bp = table; bp->name != NULL; bp++)
    {
      m4_symbol_value *value = m4_symbol_value_create ();
      char *	name;

      m4_set_symbol_value_func (value, bp->func);
      VALUE_HANDLE (value)	= handle;
      VALUE_MIN_ARGS (value)	= bp->min_args;
      VALUE_MAX_ARGS (value)	= bp->max_args;

      if (bp->groks_macro_args)
	BIT_SET (VALUE_FLAGS (value), VALUE_MACRO_ARGS_BIT);
      if (bp->blind_if_no_args)
	BIT_SET (VALUE_FLAGS (value), VALUE_BLIND_ARGS_BIT);

      if (m4_get_prefix_builtins_opt (context))
	{
	  static const char prefix[] = "m4_";
	  size_t len = strlen (prefix) + strlen (bp->name);

	  name = (char *) xmalloc (1+ len);
	  snprintf (name, 1+ len, "%s%s", prefix, bp->name);
	}
      else
	name = (char *) bp->name;


      m4_symbol_pushdef (M4SYMTAB, name, value);

      if (m4_get_prefix_builtins_opt (context))
	xfree (name);
    }
}

m4_macro *
m4_get_module_macro_table (lt_dlhandle handle)
{
  module_data *data;

  assert (handle);

  data = lt_dlcaller_get_data (caller_id, handle);

  return data ? data->macro_table : 0;
}

static void
set_module_macro_table (m4 *context, lt_dlhandle handle,
			   const m4_macro *table)
{
  const m4_macro *mp;

  assert (context);
  assert (handle);
  assert (table);

  for (mp = table; mp->name != NULL; mp++)
    {
      m4_symbol_value *value = m4_symbol_value_create ();

      m4_set_symbol_value_text (value, xstrdup (mp->value));
      VALUE_HANDLE (value) = handle;

      m4_symbol_pushdef (M4SYMTAB, mp->name, value);
    }
}

lt_dlhandle
m4_module_load (m4 *context, const char *name, m4_obstack *obs)
{
  const lt_dlhandle handle = m4__module_open (context, name, obs);

  /* If name is not set we are getting a reflective handle, but we
     might need to display an error message later so we set an appropriate
     value here.  */
  if (!name)
    name = MODULE_SELF_NAME;

  if (handle)
    {
      const lt_dlinfo  *info	= lt_dlgetinfo (handle);

      if (!info)
	{
	  M4ERROR ((m4_get_warning_status_opt (context), 0,
		    _("Warning: cannot load module `%s': %s"),
		    name, module_dlerror ()));
	}
      else if (info->ref_count == 1)
	{
	  const m4_builtin *builtin_table
	    = m4_get_module_builtin_table (handle);
	  const m4_macro   *macro_table
	    = m4_get_module_macro_table (handle);

	  /* Install the macro functions.  */
	  if (builtin_table)
	    {
	      set_module_builtin_table (context, handle, builtin_table);
#ifdef DEBUG_MODULES
	      M4_DEBUG_MESSAGE1("module %s: builtins loaded", name);
#endif /* DEBUG_MODULES */
	    }

	  /* Install the user macros. */
	  if (macro_table)
	    {
	      set_module_macro_table (context, handle, macro_table);
#ifdef DEBUG_MODULES
	      M4_DEBUG_MESSAGE1("module %s: macros loaded", name);
#endif /* DEBUG_MODULES */
	    }
	}
    }

  return handle;
}

/* Unload a module.  */
void
m4_module_unload (m4 *context, const char *name, m4_obstack *obs)
{
  lt_dlhandle	handle  = 0;
  int		errors	= 0;

  /* Scan the list for the first module with a matching name.  */
  while ((handle = lt_dlhandle_next (handle)))
    {
      if (name && (strcmp (name, m4_get_module_name (handle)) == 0))
	break;
    }

  if (!handle)
    {
      const char *error_msg = _("module not loaded");

      lt_dlseterror (lt_dladderror (error_msg));
      ++errors;
    }
  else
    errors = module_remove (context, handle, obs);

  if (errors)
    {
      M4ERROR ((EXIT_FAILURE, 0,
		_("cannot unload module `%s': %s"),
		name ? name : MODULE_SELF_NAME, module_dlerror ()));
    }
}



/* Initialisation.  Currently the module search path in path.c is
   initialised from M4MODPATH.  Only absolute path names are accepted to
   prevent the path search of the dlopen library from finding wrong
   files. */
void
m4__module_init (m4 *context)
{
  int errors = 0;

  /* Do this only once!  If we already have a caller_id, then the
     module system has already been initialised.  */
  if (caller_id)
    {
      M4ERROR ((m4_get_warning_status_opt (context), 0,
		_("Warning: multiple module loader initialisations")));
      return;
    }

#if !WITH_DMALLOC
  /* initialise libltdl's memory management. */
  lt_dlmalloc = xmalloc;
  lt_dlfree   = (void (*)(void*)) xfree;
#endif

  errors      = lt_dlinit ();

  /* Register with libltdl for a key to store client data against
     ltdl module handles.  */
  if (!errors)
    {
      caller_id = lt_dlcaller_register ();

      if (!caller_id)
	{
	  const char *error_msg = _("libltdl client registration failed");

	  lt_dlseterror (lt_dladderror (error_msg));

	  /* No need to check error statuses from the calls above -- If
	     either fails for some reason, a diagnostic will be set for
	     lt_dlerror() anyway.  */
	  ++errors;
	}
    }

  if (!errors)
    errors = lt_dlsetsearchpath (MODULE_PATH);

  /* If the user set M4MODPATH, then use that as the start of the module
     search path.  */
  if (!errors)
    {
      char *path = getenv (USER_MODULE_PATH_ENV);

      if (path)
	errors = lt_dlinsertsearchdir (lt_dlgetsearchpath (), path);
    }

  /* Couldn't initialise the module system; diagnose and exit.  */
  if (errors)
    M4ERROR ((EXIT_FAILURE, 0,
	      _("failed to initialise module loader: %s"),
	      module_dlerror ()));

#ifdef DEBUG_MODULES
  M4_DEBUG_MESSAGE ("Module loader initialised.");
#endif /* DEBUG_MODULES */
}


/* Load a module.  NAME can be a absolute file name or, if relative,
   it is searched for in the module path.  The module is unloaded in
   case of error.  */
lt_dlhandle
m4__module_open (m4 *context, const char *name, m4_obstack *obs)
{
  lt_dlhandle		handle		= lt_dlopenext (name);
  m4_module_init_func  *init_func	= 0;
  m4_builtin	       *builtin_table		= 0;
  m4_macro	       *macro_table		= 0;

  if (handle)
    {
#ifdef DEBUG_MODULES
      const lt_dlinfo *info = lt_dlgetinfo (handle);
      M4_DEBUG_MESSAGE2("module %s: opening at %s", name, info->filename);
#endif

      /* Find the builtin table in the opened module. */
      builtin_table = (m4_builtin *) lt_dlsym (handle, BUILTIN_SYMBOL);

      /* Find the macro table in the opened module. */
      macro_table = (m4_macro *) lt_dlsym (handle, MACRO_SYMBOL);

      /* Find and run any initialising function in the opened module,
	 each time the module is opened.  */
      init_func = (m4_module_init_func *) lt_dlsym (handle, INIT_SYMBOL);
      if (init_func)
	{
	  (*init_func) (context, handle, obs);

#ifdef DEBUG_MODULES
	  M4_DEBUG_MESSAGE1("module %s: init hook called", name);
#endif /* DEBUG_MODULES */
	}
    }
  else
    {
      /* Couldn't open the module; diagnose and exit. */
      M4ERROR ((EXIT_FAILURE, 0,
		_("cannot open module `%s': %s"),
		name, module_dlerror ()));
    }

  if (!builtin_table && !macro_table && !init_func)
    {
      /* Since we don't use it here, only check for the finish hook
	 if we were about to diagnose a module with no entry points.  */
      if (!lt_dlsym (handle, FINISH_SYMBOL))
	M4ERROR ((EXIT_FAILURE, 0,
		  _("module `%s' has no entry points"),
		  name));
    }

  /* If the module was correctly opened and has the necessary
     symbols, then store some client data for the new module
     on the first open only.  */
  if (handle)
    {
      const lt_dlinfo  *info	= lt_dlgetinfo (handle);

      if (info && (info->ref_count == 1))
	{
	  module_data *data	= XMALLOC (module_data, 1);
	  module_data *stale	= 0;

	  data->builtin_table	= builtin_table;
	  data->macro_table	= macro_table;

	  stale = lt_dlcaller_set_data (caller_id, handle, data);

	  if (stale)
	    {
	      xfree (stale);

	      M4ERROR ((m4_get_warning_status_opt (context), 0,
			_("Warning: overiding stale caller data in module `%s'"),
			name));
	    }

#ifdef DEBUG_MODULES
	  M4_DEBUG_MESSAGE1("module %s: opened", name);
#endif /* DEBUG_MODULES */
	}
    }

  return handle;
}

void
m4__module_exit (m4 *context)
{
  lt_dlhandle	handle	= lt_dlhandle_next (0);
  int		errors	= 0;

  while (handle && !errors)
    {
      lt_dlhandle      pending	= handle;
      const lt_dlinfo *info	= lt_dlgetinfo (pending);

      /* We are *really* shutting down here, so freeing the module
	 data is required.  */
      if (info)
	{
	  module_data *stale
	    = lt_dlcaller_set_data (caller_id, handle, 0);
	  XFREE (stale);
	}

      /* If we are about to unload the final reference, move on to the
	 next handle before we unload the current one.  */
      if (info->ref_count <= 1)
	handle = lt_dlhandle_next (pending);

      errors = module_remove (context, pending, 0);
    }

  if (!errors)
    errors = lt_dlexit();

  if (errors)
    {
      M4ERROR ((EXIT_FAILURE, 0,
		_("cannot unload all modules: %s"),
		module_dlerror ()));
    }
}



static const char *
module_dlerror (void)
{
  const char *dlerror = lt_dlerror ();

  if (!dlerror)
    dlerror = _("unknown error");

  return dlerror;
}

static void
module_close (m4 *context, lt_dlhandle handle, m4_obstack *obs)
{
  m4_module_finish_func *finish_func;
  const char		*name;
  int			 errors		= 0;

  assert (handle);

  /* Be careful when closing myself.  */
  name = m4_get_module_name (handle);
  name = xstrdup (name ? name : MODULE_SELF_NAME);

  /* Run any finishing function for the opened module. */
  finish_func = (m4_module_finish_func *) lt_dlsym (handle, FINISH_SYMBOL);

  if (finish_func)
    {
      (*finish_func) (context, handle, obs);

#ifdef DEBUG_MODULES
      M4_DEBUG_MESSAGE1("module %s: finish hook called", name);
#endif /* DEBUG_MODULES */
    }

  if (!lt_dlisresident (handle))
    {
      const lt_dlinfo  *info	= lt_dlgetinfo (handle);

      /* When we are about to unload the module for the final
	 time, be sure to release any client data memory.  */
      if (info && (info->ref_count == 1))
	{
	  module_data *stale
	    = lt_dlcaller_set_data (caller_id, handle, 0);
	  XFREE (stale);
	}

      errors = lt_dlclose (handle);
      if (!errors)
	{
#ifdef DEBUG_MODULES
	  M4_DEBUG_MESSAGE1("module %s: closed", name);
#endif /* DEBUG_MODULES */
	}
    }
#ifdef DEBUG_MODULES
  else
    M4_DEBUG_MESSAGE1("module %s: resident module not closed", name);
#endif /* DEBUG_MODULES */

  if (errors)
    {
      M4ERROR ((EXIT_FAILURE, 0,
		_("cannot close module `%s': %s"),
		name, module_dlerror ()));
    }

  xfree ((void *) name);
}

static int
module_remove (m4 *context, lt_dlhandle handle, m4_obstack *obs)
{
  const lt_dlinfo *info;
  int		   errors	= 0;

#ifdef DEBUG_MODULES
  char *	   name;

  /* Be careful when closing myself.  */
  if (handle)
    {
      name = m4_get_module_name (handle);
      name = xstrdup (name ? name : MODULE_SELF_NAME);
    }
#endif /* DEBUG_MODULES */

  assert (handle);

  info = lt_dlgetinfo (handle);

  /* Only do the actual close when the number of calls to close this
     module is equal to the number of times it was opened. */
#ifdef DEBUG_MODULES
  if (info->ref_count > 1)
    {
      M4_DEBUG_MESSAGE2("module %s: now has %d references.",
			name, info->ref_count -1);
    }
#endif /* DEBUG_MODULES */

  if (info->ref_count == 1)
    {
      /* Remove the table references only when ref_count is *exactly*
	 equal to 1.  If module_close is called again on a
	 resident module after the references have already been
	 removed, we needn't try to remove them again!  */
      m4__symtab_remove_module_references (M4SYMTAB, handle);

#ifdef DEBUG_MODULES
      M4_DEBUG_MESSAGE1("module %s: symbols unloaded", name);
#endif /* DEBUG_MODULES */
    }

  if (!errors)
    module_close (context, handle, obs);

  return errors;
}
