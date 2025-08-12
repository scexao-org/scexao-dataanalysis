#include <stdio.h>
#include <stdlib.h>
#include <math.h>   // Required for fabs()
#include <float.h>  // Required for DBL_MAX

#include "CommandLineInterface/CLIcore.h"
#include "read_asciiconf.h"
#include "scanFITSfiles.h"
#include "COREMOD_tools/quicksort.h" // sort



#define MAXNBFILES 10000

// Maximum number of keywords in single FITS file
// Used for statistically allocated finfo to load one header at a time
#define FITSMAXNCARD 10000


typedef struct {
    int camindex;
    double WPangle;
} VAMPIRESFRAME_PDIINFO;


typedef struct {
    int cam;        // Camera index
    double WPangle;
    double MJD;     // Modified Julian Day
    int fileindex;  // Which FITS file is this frame from?
    int frameindex; // Which frame index within FITS file?
} PDIdetectorframe;




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
 * @brief Defines the output structure for an aligned point pair.
 * It holds the original index from each of the two streams.
 */
typedef struct {
    int index1;
    int index2;
} AlignedPoint;

/**
 * @brief Synchronizes two time-series streams based on a maximum time difference.
 *
 * This function finds the best one-to-one matches between two sorted streams of
 * time-stamped data. It uses an efficient two-pointer "greedy" algorithm to
 * iterate through both streams simultaneously. At each step, it decides whether
 * to pair the current points or advance one of the stream pointers to find a
 * better match. A pair is only considered if `abs(time1[i] - time2[j])` is
 * less than or equal to `max_time_diff`.
 *
 * @param time1 Pointer to the time array for stream 1 (must be sorted ascending).
 * @param nbpoint1 The number of points in stream 1.
 * @param time2 Pointer to the time array for stream 2 (must be sorted ascending).
 * @param nbpoint2 The number of points in stream 2.
 * @param max_time_diff The maximum allowed time difference for a valid match.
 * @param aligned_points_out An allocated array to store the resulting aligned pairs.
 * @param max_output_size The maximum capacity of the `aligned_points_out` array.
 * @return The total number of aligned pairs found and written to the output array.
 */
static int synchronize_timestreams2(
    const double* time1,
    int nbpoint1,
    const double* time2,
    int nbpoint2,
    double max_time_diff,
    AlignedPoint* aligned_points_out,
    int max_output_size)
{
    int i = 0; // Pointer for stream 1
    int j = 0; // Pointer for stream 2
    int aligned_count = 0;

    // Continue as long as there are points in both streams and space in the output array
    while (i < nbpoint1 && j < nbpoint2 && aligned_count < max_output_size) {
        double time_diff = time1[i] - time2[j];
        double abs_time_diff = fabs(time_diff);

        // Case 1: The points are within the synchronization window.
        if (abs_time_diff <= max_time_diff) {
            // Greedily decide if this is the best local match.
            // Look ahead: what's the time difference if we advance pointer i?
            double next_diff1 = (i + 1 < nbpoint1)
                                ? fabs(time1[i + 1] - time2[j])
                                : DBL_MAX;

            // Look ahead: what's the time difference if we advance pointer j?
            double next_diff2 = (j + 1 < nbpoint2)
                                ? fabs(time1[i] - time2[j + 1])
                                : DBL_MAX;

            // If the current diff is smaller than or equal to the next possible diffs,
            // we have found the best match for this pair.
            if (abs_time_diff <= next_diff1 && abs_time_diff <= next_diff2) {
                aligned_points_out[aligned_count].index1 = i;
                aligned_points_out[aligned_count].index2 = j;
                aligned_count++;
                // Advance both pointers to find the next unique pair.
                i++;
                j++;
            } else if (next_diff1 < next_diff2) {
                // Advancing pointer i will lead to a better match.
                i++;
            } else {
                // Advancing pointer j will lead to a better match.
                j++;
            }
        }
        // Case 2: Point in stream 1 is too "early". Advance pointer i to catch up.
        else if (time_diff < 0) { // This means time1[i] < time2[j]
            i++;
        }
        // Case 3: Point in stream 2 is too "early". Advance pointer j to catch up.
        else { // This means time1[i] > time2[j]
            j++;
        }
    }

    return aligned_count;
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


    // First we read the configuration file
    printf("Reading configuration file %s\n", confname);
    int pair_count = 0;
    KeyValuePair* config = parse_config(confname, &pair_count);
    if (config == NULL) {
        fprintf(stderr, "Failed to parse the configuration file.\n");
        return 1;
    }


    // Read rawdatadir entry from configuration
    char *rawdatadir = NULL;
    for (int i = 0; i < pair_count; i++) {
        if (strcmp(config[i].key, "rawdatadir") == 0) {
            rawdatadir = config[i].value;
            break;
        }
    }


    // Scan FITS files in directory

    int file_count = 0;

    // Temporary structure used to read a single header
    FITSfileinfo finfo;
    // Allow for max FITSMAXNCARD of keyword
    finfo.kw = (FITSkeyword *)malloc(sizeof(FITSkeyword) * FITSMAXNCARD);

    // Entries will then be copied to this array, one by one
    FITSfileinfo* fitsfileinfo = (FITSfileinfo *)malloc(sizeof(FITSfileinfo) * MAXNBFILES);

    int scanOK = 1;
    while (scanOK == 1)
    {
        int scanstatus = scan_nextFITSfiles(rawdatadir, &finfo);
        if (scanstatus == 1) // found FITS file
        {
            snprintf(fitsfileinfo[file_count].fname, FITSFNAMESTRLEN, "%s", finfo.fname);

            fitsfileinfo[file_count].bitpix = finfo.bitpix;
            fitsfileinfo[file_count].naxis = finfo.naxis;
            for(int i=0; i<finfo.naxis; i++)
            {
                fitsfileinfo[file_count].naxes[i] = finfo.naxes[i];
            }
            fitsfileinfo[file_count].nbkey = finfo.nbkey;
            fitsfileinfo[file_count].kw = (FITSkeyword *)malloc(sizeof(FITSkeyword) * finfo.nbkey);
            for(int kwi=0; kwi<finfo.nbkey; kwi++)
            {
                fitsfileinfo[file_count].kw[kwi].hdu = finfo.kw[kwi].hdu;
                snprintf(fitsfileinfo[file_count].kw[kwi].keyname, FLEN_KEYWORD, "%s", finfo.kw[kwi].keyname);
                snprintf(fitsfileinfo[file_count].kw[kwi].value, FLEN_VALUE, "%s", finfo.kw[kwi].value);
                snprintf(fitsfileinfo[file_count].kw[kwi].comment, FLEN_COMMENT, "%s", finfo.kw[kwi].comment);
                /*printf("   \"%8s\"  %12s  %s\n",
                       fitsfileinfo[file_count].kw[kwi].keyname,
                       fitsfileinfo[file_count].kw[kwi].value,
                       fitsfileinfo[file_count].kw[kwi].comment
                      );*/
            }
            file_count++;
        }
        if (scanstatus == 2) // error
        {
            scanOK = 0;
        }
        if (scanstatus == -1) // no more files
        {
            scanOK = 0;
        }
    }
    // Free temporary finfo
    free(finfo.kw);





    // Scan data for PDI processing
    int cam1nbframe = 0;
    int cam2nbframe = 0;

    // arrays used to sort files by time
    // cam1 times, unix time
    int cam1nbfile = 0;
    double *cam1time = (double *)malloc(sizeof(double) * file_count);
    long *cam1index = (long *)malloc(sizeof(long) * file_count);

    // cam2 times, unix time
    int cam2nbfile = 0;
    double *cam2time = (double *)malloc(sizeof(double) * file_count);
    long *cam2index = (long *)malloc(sizeof(long) * file_count);



    VAMPIRESFRAME_PDIINFO frame_pdiinfo;
    for(int file_idx=0; file_idx<file_count; file_idx++)
    {
        frame_pdiinfo.camindex = -1;
        frame_pdiinfo.WPangle = -1;
        double mjd = 0.0;

        for(int kwi=0; kwi<fitsfileinfo[file_idx].nbkey; kwi++)
        {
            // Look for keyname DETECTOR
            if (strcmp(fitsfileinfo[file_idx].kw[kwi].keyname, "DETECTOR") == 0) {
                //printf("Found DETECTOR at %d\n", kwi);
                // check if value contains CAM1
                if (strstr(fitsfileinfo[file_idx].kw[kwi].value, "CAM1") != NULL) {
                    frame_pdiinfo.camindex = 1;
                }
                else if (strstr(fitsfileinfo[file_idx].kw[kwi].value, "CAM2") != NULL)
                {
                    frame_pdiinfo.camindex = 2;
                }
            }

            // Look for keyname RET-ANG1
            if (strcmp(fitsfileinfo[file_idx].kw[kwi].keyname, "RET-ANG1") == 0) {
                frame_pdiinfo.WPangle = atof(fitsfileinfo[file_idx].kw[kwi].value);
            }

            // Look for MJD
            if (strcmp(fitsfileinfo[file_idx].kw[kwi].keyname, "MJD") == 0) {
                mjd = atof(fitsfileinfo[file_idx].kw[kwi].value);
            }
        }

        if(frame_pdiinfo.camindex == 1) {
            cam1time[cam1nbfile] = (mjd - 40587.0) * 86400.0;
            cam1index[cam1nbfile] = file_idx;
            cam1nbfile++;
            cam1nbframe += fitsfileinfo[file_idx].naxes[2];
        }

        if(frame_pdiinfo.camindex == 2) {
            cam2time[cam2nbfile] = (mjd - 40587.0) * 86400.0;
            cam2index[cam2nbfile] = file_idx;
            cam2nbfile++;
            cam2nbframe += fitsfileinfo[file_idx].naxes[2];
        }


        printf("[%5d/%5d]  %32s  [%ld %ld %ld] CAM=%d  WPangle %4.1f   mjd = %f\n",
               file_idx, file_count,
               fitsfileinfo[file_idx].fname,
               fitsfileinfo[file_idx].naxes[0],
               fitsfileinfo[file_idx].naxes[1],
               fitsfileinfo[file_idx].naxes[2],
               frame_pdiinfo.camindex,
               frame_pdiinfo.WPangle,
               mjd
              );
    }

    printf("cam1 : %d files  %d frames\n", cam1nbfile, cam1nbframe);
    printf("cam2 : %d files  %d frames\n", cam2nbfile, cam2nbframe);


    quick_sort2l(cam1time, cam1index, cam1nbfile);
    quick_sort2l(cam2time, cam2index, cam2nbfile);

    AlignedPoint* syncseq = (AlignedPoint *)malloc(sizeof(AlignedPoint) * (cam1nbframe+cam2nbframe));

    int nbmatchedpts = synchronize_timestreams2(cam1time, cam1nbfile, cam2time, cam2nbfile, 1.0, syncseq, (cam1nbframe+cam2nbframe));

    for(int i=0; i<nbmatchedpts; i++)
    {
        // print each matched point
        printf("[%4d] %5d %5d    %.6f %.6f   %6f\n", i,
               syncseq[i].index1, syncseq[i].index2,
               cam1time[syncseq[i].index1],
               cam2time[syncseq[i].index2],
               cam1time[syncseq[i].index1]-cam1time[syncseq[i].index2]
            );
    }


    free(syncseq);

    free(cam1time);
    free(cam1index);
    free(cam2time);
    free(cam2index);









    // Free the allocated memory when done.
    printf("Cleaning up allocated memory...\n");

    // Clean up already allocated filenames
    for (int i = 0; i < file_count; i++) {
        free(fitsfileinfo[file_count].kw);
    }
    free(fitsfileinfo);


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
