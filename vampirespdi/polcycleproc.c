#include "CommandLineInterface/CLIcore.h"



// Within this translation unit, these point to the variables values
static char *inimname;

// float point variable should be double. single precision float not supported
static double *scoeff;





// List of arguments to function
// { CLItype, tag, description, initial value, flag, fptype, fpflag}
//
// A function variable is named by a tag, which is a hierarchical
// series of words separated by dot "."
// For example: .input.xsize (note that first dot is optional)
//
static CLICMDARGDEF farg[] =
{
    {
        CLIARG_IMG,
        ".in_name",
        "input image",
        "im1",
        CLIARG_VISIBLE_DEFAULT,
        (void **) &inimname,
        NULL
    },
    {
        // hidden argument is not part of CLI call, FPFLAG ignored
        CLIARG_FLOAT64,
        ".scaling",
        "scaling coefficient",
        "1.0",
        CLIARG_HIDDEN_DEFAULT,
        (void **) &scoeff,
        NULL
    }
};

// CLI function initialization data
static CLICMDDATA CLIcmddata =
{
    "imsum1",                          // keyword to call function in CLI
    "compute total of image example1", // description of what the function does
    CLICMD_FIELDS_NOFPS
};



/** @brief Compute function code
 *
 * Can be made non-static and called from outside this translation unit(TU)
 * Minimizes use of variables local to this TU.
 *
 * Functions should return error code of type errno_t (= int).
 * On success, return value is RETURN_SUCCESS (=0).
 */
static errno_t example_compute_2Dimage_total(IMGID img, double scalingcoeff)
{
    // entering function, updating trace accordingly
    DEBUG_TRACE_FSTART();

    // Resolve image if not already resolved
    resolveIMGID(&img, ERRMODE_ABORT);
    // abort if unable to resolve
    // Upon success, these are available for use:
    // img.name, img.naxis, img.ID, img.size, img.im


    uint32_t  xsize  = img.md->size[0];
    uint32_t  ysize  = img.md->size[1];
    uint64_t  xysize = xsize * ysize;

    double total = 0.0;
    for(uint64_t ii = 0; ii < xysize; ii++)
    {
        total += img.im->array.F[ii];
    }
    total *= scalingcoeff;

    printf("image %s total = %lf (scaling coeff %lf)\n",
           img.im->name,
           total,
           scalingcoeff);

    // normal successful return from function :
    DEBUG_TRACE_FEXIT();
    return RETURN_SUCCESS;
}

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

    example_compute_2Dimage_total(mkIMGID_from_name(inimname), *scoeff);

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
