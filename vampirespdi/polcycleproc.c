#include <stdio.h>
#include <stdlib.h>
#include <math.h>   // Required for fabs()
#include <float.h>  // Required for DBL_MAX

#include "CommandLineInterface/CLIcore.h"
#include "COREMOD_iofits/COREMOD_iofits.h"  // load_fits
#include "COREMOD_tools/quicksort.h" // sort


#include "read_asciiconf.h"
#include "scanFITSfiles.h"




#define MAXNBFILES 10000

// Maximum number of keywords in single FITS file
// Used for statistically allocated finfo to load one header at a time
#define FITSMAXNCARD 10000


typedef struct {
    int camindex;
    double WPangle;
} VAMPIRESFRAME_PDIINFO;


typedef struct {
    double WPangle;
    double tstamp;     // Unix timestamp
    int    fileindex;  // Which FITS file is this frame from?
    int    frameindex; // Which frame index within FITS file?
} PDIframe;




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





#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Reads an ASCII data file and populates a double array with time values.
 *
 * The function parses a file where each data line contains 7 columns. It extracts
 * an index from column 1 and a time value from column 5, placing the time into
 * the output array at the specified index. Lines starting with '#' are ignored.
 *
 * @param filename The path to the ASCII file to read.
 * @param time_array A pointer to a pre-allocated double array to store the results.
 * @param array_size The total number of elements in time_array (for bounds checking).
 *
 * @return Returns 0 on success, -1 on failure (e.g., file not found).
 */
static int read_time_data(const char *filename, double *time_array, size_t array_size) {
    // Open the file for reading ("r" mode)
    FILE *file_ptr = fopen(filename, "r");
    if (file_ptr == NULL) {
        perror("Error opening file");
        return -1; // Indicate failure
    }

    char line_buffer[256]; // Buffer to hold one line of the file
    int line_number = 0;

    // Read the file line by line until the end
    while (fgets(line_buffer, sizeof(line_buffer), file_ptr) != NULL) {
        line_number++;

        // Skip comment lines (which start with '#') or empty lines
        if (line_buffer[0] == '#' || line_buffer[0] == '\n') {
            continue;
        }

        int frame_index;
        double absolute_time;

        // Use sscanf to parse the line.
        // The '%*...' format specifiers read a value but discard it (assignment suppression).
        // We only care about the 1st (%d) and 5th (%lf) values.
        int items_scanned = sscanf(line_buffer, "%d %*d %*lf %*lf %lf %*d %*d",
                                   &frame_index, &absolute_time);

        // A correctly formatted data line will result in 2 successfully scanned items.
        if (items_scanned == 2) {
            // CRITICAL: Perform a bounds check before writing to the array.
            if (frame_index >= 0 && (size_t)frame_index < array_size) {
                time_array[frame_index] = absolute_time;
            } else {
                fprintf(stderr, "Warning: Index %d on line %d is out of bounds for array of size %zu. Skipping.\n",
                        frame_index, line_number, array_size);
            }
        }
    }

    // Close the file stream
    fclose(file_ptr);

    return 0; // Indicate success
}




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
    long xsize = -1;
    long ysize = -1;
    int cropnb = -1;
    long zsize = -1;
    for (int i = 0; i < pair_count; i++) {
        if (strcmp(config[i].key, "rawdatadir") == 0) {
            rawdatadir = config[i].value;
        }
        if (strcmp(config[i].key, "cropxsize") == 0) {
            xsize = atoi(config[i].value);
        }

        if (strcmp(config[i].key, "cropysize") == 0) {
            ysize = atoi(config[i].value);
        }

        if (strcmp(config[i].key, "cropnb") == 0) {
            cropnb = atoi(config[i].value);
        }
    }

    int * cam1crop_xcenter = (int*)malloc(sizeof(int) * cropnb);
    int * cam1crop_ycenter = (int*)malloc(sizeof(int) * cropnb);

    int * cam2crop_xcenter = (int*)malloc(sizeof(int) * cropnb);
    int * cam2crop_ycenter = (int*)malloc(sizeof(int) * cropnb);


    for (int crop=0; crop<cropnb; crop++)
    {
        char keystring[20];

        sprintf(keystring, "cam1.crop%d.xcenter", crop);
        for (int i = 0; i < pair_count; i++) {
            if (strcmp(config[i].key, keystring) == 0) {
                cam1crop_xcenter[crop] = atoi(config[i].value);
            }
        }
        sprintf(keystring, "cam1.crop%d.ycenter", crop);
        for (int i = 0; i < pair_count; i++) {
            if (strcmp(config[i].key, keystring) == 0) {
                cam1crop_ycenter[crop] = atoi(config[i].value);
            }
        }
    }

    for (int crop=0; crop<cropnb; crop++)
    {
        char keystring[20];

        sprintf(keystring, "cam2.crop%d.xcenter", crop);
        for (int i = 0; i < pair_count; i++) {
            if (strcmp(config[i].key, keystring) == 0) {
                cam2crop_xcenter[crop] = atoi(config[i].value);
            }
        }
        sprintf(keystring, "cam2.crop%d.ycenter", crop);
        for (int i = 0; i < pair_count; i++) {
            if (strcmp(config[i].key, keystring) == 0) {
                cam2crop_ycenter[crop] = atoi(config[i].value);
            }
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

            fitsfileinfo[file_count].destframeidx = (int *)malloc(sizeof(int) * finfo.naxes[2]);
            for(int frame_idx=0; frame_idx<finfo.naxes[2]; frame_idx++)
            {
                fitsfileinfo[file_count].destframeidx[frame_idx] = -1;
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


    // collect frame data
    PDIframe *cam_PDIframe[2]; // Array to hold pointers for cam1 and cam2
    int *nbfile_arr[2] = {&cam1nbfile, &cam2nbfile};
    double *time_arr[2] = {cam1time, cam2time};
    long *index_arr[2] = {cam1index, cam2index};
    int *nbframe_arr[2] = {&cam1nbframe, &cam2nbframe};

    for (int cam_idx = 0; cam_idx < 2; cam_idx++) {
        int current_nbfile = *nbfile_arr[cam_idx];
        int current_nbframe = *nbframe_arr[cam_idx];
        double *current_time = time_arr[cam_idx];
        long *current_index = index_arr[cam_idx];

        printf("\nCollecting timing info for cam%d (%d files, %d frames)\n", cam_idx + 1, current_nbfile, current_nbframe);

        cam_PDIframe[cam_idx] = (PDIframe *)malloc(sizeof(PDIframe) * current_nbframe);
        if (cam_PDIframe[cam_idx] == NULL) {
            fprintf(stderr, "Memory allocation failed for cam%d_PDIframe\n", cam_idx + 1);
            // Free previously allocated memory before returning
            if (cam_idx > 0) free(cam_PDIframe[0]);
            return 2;
        }

        int camframe_counter = 0;
        for (int camfileidx = 0; camfileidx < current_nbfile; camfileidx++) {

            // Get WP angle
            double current_WPangle = -1.0;
            for(int kwi_file=0; kwi_file<fitsfileinfo[current_index[camfileidx]].nbkey; kwi_file++) {
                if (strcmp(fitsfileinfo[current_index[camfileidx]].kw[kwi_file].keyname, "RET-ANG1") == 0) {
                    current_WPangle = atof(fitsfileinfo[current_index[camfileidx]].kw[kwi_file].value);
                    break;
                }
            }


            // get timing data
            printf("File index %ld, name %s\n", current_index[camfileidx], fitsfileinfo[current_index[camfileidx]].fname);

            char *timingfname = malloc(strlen(fitsfileinfo[current_index[camfileidx]].fname) + 8);
            if (timingfname == NULL) {
                // Handle memory allocation failure
                free(cam_PDIframe[cam_idx]);
                if (cam_idx > 0) free(cam_PDIframe[0]);
                return 2;
            }

            strcpy(timingfname, fitsfileinfo[current_index[camfileidx]].fname);
            char *dot_fits_ptr = strstr(timingfname, ".fits");
            if (dot_fits_ptr != NULL) {
                strcpy(dot_fits_ptr, ".txt");
            }

            /*printf("cam%d %2d/%2d %4d/%4d  FILE name %s -> %s\n",
                   cam_idx + 1, camfileidx, current_nbfile,
                   camframe_counter, current_nbframe,
                   fitsfileinfo[current_index[camfileidx]].fname, timingfname);*/

            double* timearray = (double*)malloc(sizeof(double) * fitsfileinfo[current_index[camfileidx]].naxes[2]);
            if (timearray == NULL) {
                // Handle memory allocation failure
                free(cam_PDIframe[cam_idx]);
                if (cam_idx > 0) free(cam_PDIframe[0]);
                return 2;
            }


            read_time_data(timingfname,
                           timearray,
                           fitsfileinfo[current_index[camfileidx]].naxes[2]);
            // print times
            for (int i = 0; i < fitsfileinfo[current_index[camfileidx]].naxes[2]; i++) {
                printf("time %4d = %.6f\n", i, timearray[i]);
            }
            free(timingfname);

            printf("WRITING %ld frames\n", fitsfileinfo[current_index[camfileidx]].naxes[2]);

            for (int frameidx = 0; frameidx < fitsfileinfo[current_index[camfileidx]].naxes[2]; frameidx++) {
                cam_PDIframe[cam_idx][camframe_counter].WPangle = current_WPangle;
                cam_PDIframe[cam_idx][camframe_counter].tstamp = timearray[frameidx]; // current_time[camfileidx];
                cam_PDIframe[cam_idx][camframe_counter].fileindex = current_index[camfileidx];
                cam_PDIframe[cam_idx][camframe_counter].frameindex = frameidx;
                camframe_counter++;
            }
            free(timearray);
        }
    }



    // print entries
    for(int cam_idx=0; cam_idx<2; cam_idx++)
        for(int camframe_counter=0; camframe_counter<*nbframe_arr[cam_idx]; camframe_counter++) {
            printf("cam %d frame %5d  time %.6f  WPangle %4.1f  %s\n",
                   cam_idx + 1, camframe_counter,
                   cam_PDIframe[cam_idx][camframe_counter].tstamp,
                   cam_PDIframe[cam_idx][camframe_counter].WPangle,
                   fitsfileinfo[cam_PDIframe[cam_idx][camframe_counter].fileindex].fname
                  );
        }


    double * cam1frametime = (double *)malloc(sizeof(double) * cam1nbframe);
    long * cam1frameindex = (long *)malloc(sizeof(long) * cam1nbframe);
    for(int camframe_counter=0; camframe_counter<*nbframe_arr[0]; camframe_counter++) {
        cam1frametime[camframe_counter] = cam_PDIframe[0][camframe_counter].tstamp;
        cam1frameindex[camframe_counter] = camframe_counter;
    }

    double * cam2frametime = (double *)malloc(sizeof(double) * cam2nbframe);
    long * cam2frameindex = (long *)malloc(sizeof(long) * cam2nbframe);
    for(int camframe_counter=0; camframe_counter<*nbframe_arr[1]; camframe_counter++) {
        cam2frametime[camframe_counter] = cam_PDIframe[1][camframe_counter].tstamp;
        cam2frameindex[camframe_counter] = camframe_counter;
    }


    quick_sort2l(cam1frametime, cam1frameindex, cam1nbframe);
    quick_sort2l(cam2frametime, cam2frameindex, cam2nbframe);

    for(int frame_idx=0; frame_idx<10; frame_idx++)
    {
        printf("TIME %.6f  %6f\n",
               cam1frametime[frame_idx],
               cam2frametime[frame_idx]);
    }

    AlignedPoint* syncseq = (AlignedPoint *)malloc(sizeof(AlignedPoint) * (cam1nbframe+cam2nbframe));

    int nbmatchedpts =
        synchronize_timestreams2(
            cam1frametime, cam1nbframe,
            cam2frametime, cam2nbframe,
            0.1, syncseq, (cam1nbframe+cam2nbframe));




    cam1nbframe = 0;
    cam2nbframe = 0;
    int previndex1 = -1;
    int previndex2 = -1;
    for(int i=0; i<nbmatchedpts; i++)
    {
        // print unmatched index1
        while(syncseq[i].index1 - previndex1 > 1) {
            previndex1++;
            printf("MISSED %5d -----    %.6f ------ \n",
                   previndex1,
                   cam1frametime[previndex1]
                  );
        }
        // print unmatched index2
        while(syncseq[i].index2 - previndex2 > 1) {
            previndex2++;
            printf("MISSED ----- %5d    ------ %.6f\n",
                   previndex2,
                   cam2frametime[previndex2]
                  );
        }


        int franeidx1 = cam1frameindex[syncseq[i].index1];
        int franeidx2 = cam2frameindex[syncseq[i].index2];
        // print each matched point
        printf("[%4d]  %4.1f %4.1f   %5d %5d    %.6f %.6f   %6f    (%2d %3d) (%2d %3d)     %s %s\n", i,
               cam_PDIframe[0][franeidx1].WPangle,
               cam_PDIframe[1][franeidx2].WPangle,
               syncseq[i].index1, syncseq[i].index2,
               cam1frametime[syncseq[i].index1],
               cam2frametime[syncseq[i].index2],
               cam1frametime[syncseq[i].index1]-cam2frametime[syncseq[i].index2],
               cam_PDIframe[0][franeidx1].fileindex, cam_PDIframe[0][franeidx1].frameindex,
               cam_PDIframe[1][franeidx2].fileindex, cam_PDIframe[1][franeidx2].frameindex,
               fitsfileinfo[cam_PDIframe[0][franeidx1].fileindex].fname,
               fitsfileinfo[cam_PDIframe[1][franeidx2].fileindex].fname
              );
        fitsfileinfo[cam_PDIframe[0][franeidx1].fileindex].selected = 1; // selected for camera 1
        fitsfileinfo[cam_PDIframe[1][franeidx2].fileindex].selected = 2; // selected for camera 2

        // write destination indices
        fitsfileinfo[cam_PDIframe[0][franeidx1].fileindex].destframeidx[cam_PDIframe[0][franeidx1].frameindex] = i;
        fitsfileinfo[cam_PDIframe[1][franeidx2].fileindex].destframeidx[cam_PDIframe[1][franeidx2].frameindex] = i;

        previndex1 = syncseq[i].index1;
        previndex2 = syncseq[i].index2;
    }




    // Read image content
    printf("xsize = %ld  ysize = %ld\n", xsize, ysize);
    printf("cropnb = %d\n", cropnb);

    IMGID imgcam1  = makeIMGID_3D("cam1", xsize*cropnb, ysize, nbmatchedpts);
    imcreateIMGID(&imgcam1);
    list_image_ID();

    for(int file_idx=0; file_idx<file_count; file_idx++)
    {
        // read pixel array
        fitsfile *fptr;     // FITS file pointer
        int status = 0;     // CFITSIO status value MUST be initialized to 0
        int bitpix, naxis;
        long naxes[3];      // Dimensions of the image (NAXIS1, NAXIS2)
        long fpixel = 1;    // First pixel to read (1-based)
        long nelements;     // Total number of pixels to read
        float *buffer;      // Memory buffer to hold the image

        // Open the FITS file for reading
        if (fits_open_file(&fptr, fitsfileinfo[file_idx].fname, READONLY, &status)) {
            fits_report_error(stderr, status);
            return(status);
        }
        else {
            // If we get here, status is still 0, meaning the file opened successfully.
            int total_hdus = 0;
            // Get the total number of HDUs in the file
            printf("Getting total number of HDUs\n");
            if (fits_get_num_hdus(fptr, &total_hdus, &status)) {
                fits_report_error(stderr, status);
                fits_close_file(fptr, &status);
                return(status);
            }
            printf("Total number of HDUs: %d\n", total_hdus);

            // move to last HDU
            printf("Moving to last HDU\n");
            if (fits_movabs_hdu(fptr, total_hdus, NULL, &status)) {
                fits_report_error(stderr, status);
                return(status);
            }

            // get image size, bitpix
            printf("Getting image size and bitpix\n");
            if (fits_get_img_param(fptr, 8, &bitpix, &naxis, naxes, &status)) {
                fits_report_error(stderr, status);
                return(status);
            }



            // Calculate the total number of pixels
            nelements = naxes[0] * naxes[1] * naxes[2];
            printf("Image input size : %ld x %ld x %ld = %ld pixels\n", naxes[0], naxes[1], naxes[2], nelements);


            // Allocate memory for the image buffer
            buffer = (float *) malloc(nelements * sizeof(float));
            if (buffer == NULL) {
                printf("Memory allocation error\n");
                return(1);
            }

            // Read the entire image into the buffer
            // TFLOAT specifies that we want the data converted to float in our buffer.
            if (fits_read_img(fptr, TFLOAT, fpixel, nelements, NULL, buffer, NULL, &status)) {
                fits_report_error(stderr, status);
            } else {
                printf("Image read successfully into buffer.\n");
                // Example: Print the value of the first pixel
                printf("Value of the first pixel (1,1): %f\n", buffer[0]);
            }
            // Close the FITS file
            fits_close_file(fptr, &status);




            if(fitsfileinfo[file_idx].selected == 1)
            {
                // write pixels to cam1
                int nbframe = fitsfileinfo[file_idx].naxes[2];
                for(int frame_idx=0; frame_idx<nbframe; frame_idx++)
                {
                    int destframeidx = fitsfileinfo[file_idx].destframeidx[frame_idx];
                    printf("FILE %s frame %d  -> cam1 frame %d\n", fitsfileinfo[file_idx].fname, frame_idx, destframeidx);

                    for(int crop=0; crop<cropnb; crop++)
                    {
                        long ii0offset = cam1crop_xcenter[crop] - xsize/2;
                        long jj0offset = cam1crop_ycenter[crop] - ysize/2;
                        long ii1offset = crop * xsize;
                        long jj1offset = 0;

                        for(long ii=0; ii<xsize; ii++)
                        {
                            long ii0 = ii + ii0offset;
                            long ii1 = ii + ii1offset;
                            for(long jj=0; jj<ysize; jj++)
                            {
                                long jj0 = jj + jj0offset;
                                long jj1 = jj + jj1offset;
                                imgcam1.im->array.F[xsize*ysize*cropnb*destframeidx + jj1*xsize*cropnb + ii1] = buffer[naxes[0]*naxes[1]*frame_idx + jj0*naxes[0] + ii0];
                            }

                        }
                    }
                }
            }
            if(fitsfileinfo[file_idx].selected == 2)
            {
                // write pixels to cam2
                int nbframe = fitsfileinfo[file_idx].naxes[2];
                for(int frame_idx=0; frame_idx<nbframe; frame_idx++)
                {
                    int destframeidx = fitsfileinfo[file_idx].destframeidx[frame_idx];
                    printf("FILE %s frame %d  -> cam2 frame %d\n", fitsfileinfo[file_idx].fname, frame_idx, destframeidx);
                }

            }
            // Free the memory
            free(buffer);
        }
    }















    for (int cam_idx = 0; cam_idx < 2; cam_idx++) {
        free(cam_PDIframe[cam_idx]);
    }

    free(syncseq);

    free(cam1time);
    free(cam1index);
    free(cam2time);
    free(cam2index);
    free(cam1frametime);
    free(cam2frametime);









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
