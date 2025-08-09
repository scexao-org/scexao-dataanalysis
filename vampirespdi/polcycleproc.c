#include "CommandLineInterface/CLIcore.h"

#include "read_asciiconf.h"
#include "scanFITSfiles.h"







// Configuration file
static char *confname;




// List of arguments to function
static CLICMDARGDEF farg[] =
{
    {
        CLIARG_STR,
        ".confname",
        "configuration file",
        "vamppdi.conf",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &confname,
        NULL
    }
};

// CLI function initialization data
static CLICMDDATA CLIcmddata =
{
    "procWPcycle",               // keyword to call function in CLI
    "process WP cycle",          // description of what the function does
    CLICMD_FIELDS_NOFPS
};






/**
 * @brief Wrapper function, used by all CLI calls
 *
 * Defines how local variables are fed to computation code.
 * Always local to this translation unit.
 *
 * @return errno_t
 */
static errno_t compute_function()
{
    DEBUG_TRACE_FSTART();

    printf("Reading configuration file %s\n", confname);

    int pair_count = 0;
    KeyValuePair* config = parse_config(confname, &pair_count);

    if (config == NULL) {
        fprintf(stderr, "Failed to parse the configuration file.\n");
        return 1;
    }

    printf("Successfully parsed %d key-value pairs:\n", pair_count);
    printf("------------------------------------------------------------------\n");
    for (int i = 0; i < pair_count; i++) {
        printf("Pair %d:  Key='%-20s' Value='%-25s'", i + 1, config[i].key, config[i].value);
        if (config[i].comment) {
            printf(" Comment='%s'", config[i].comment);
        }
        printf("\n");
    }
    printf("------------------------------------------------------------------\n\n");

    // look for rawdatadir entry
    char *rawdatadir = NULL;
    for (int i = 0; i < pair_count; i++) {
        if (strcmp(config[i].key, "rawdatadir") == 0) {
            rawdatadir = config[i].value;
            break;
        }
    }

    // list of FILE filenames, dynamically allocated
    char **FITSfilelist;
    int file_count = 0;

    // Scan for .fits.fz and .fits files in directory rawdatadir, write filenames in FITSfilelist array
    file_count = scanFITSfiles(rawdatadir, FITSfilelist);

    // Collect header info and aux info
    for (int fileidx = 0; fileidx < file_count; fileidx++) {
        printf("[%4d] File %s/%s\n", fileidx, rawdatadir, FITSfilelist[fileidx]);
    }



    // Free the allocated memory when done.
    printf("Cleaning up allocated memory...\n");

    // Clean up already allocated filenames
    for (int i = 0; i < file_count; i++) {
        free(FITSfilelist[i]);
    }
    free(FITSfilelist);

    free_config(config, pair_count);
    printf("Cleanup complete.\n");



    DEBUG_TRACE_FEXIT();
    return RETURN_SUCCESS;
}


INSERT_STD_CLIfunction



/** @brief Register CLI command
*/
errno_t
CLIADDCMD_vampires_pdi__polcycleproc()
{
    INSERT_STD_CLIREGISTERFUNC

    return RETURN_SUCCESS;
}
