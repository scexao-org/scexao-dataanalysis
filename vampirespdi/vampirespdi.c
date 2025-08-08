#define _GNU_SOURCE


// module default short name
#define MODULE_SHORTNAME_DEFAULT "vamppdi"

// Module short description
#define MODULE_DESCRIPTION "VAMPIRES PDI"



#include "CommandLineInterface/CLIcore.h"

#include "polcycleproc.h"


// Module initialization macro in CLIcore.h
// macro argument defines module name for bindings
//
INIT_MODULE_LIB(vampirespdi)

/**
 * @brief Initialize module CLI
 *
 * CLI entries are registered: CLI call names are connected to CLI functions.\n
 * Any other initialization is performed\n
 *
 */
static errno_t init_module_CLI()
{

    CLIADDCMD_vampires_pdi__polcycleproc();

    // optional: add atexit functions here

    return RETURN_SUCCESS;
}
