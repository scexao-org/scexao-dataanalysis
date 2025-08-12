#include <dirent.h> // opendir

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fitsio.h"
#include "scanFITSfiles.h"



// scan one file at a time
// returns 1 if file scanned and FITS file
// returns 0 if file scanned by not FITS file
// returns -1 if no more file to scan
// retruns 2 if erroring
int scan_nextFITSfiles(
    char *directory,
    FITSfileinfo *finfo
)
{
    static int init = 0; // toggles to 1 when starting scan

    static int file_count = 0;
    static DIR *d;
    static struct dirent *dir;

    if (init == 0)
    {
        init = 1;
        d = opendir(directory);
    }

    if ((dir = readdir(d)) != NULL) {
        fitsfile *fptr;   // Pointer to the FITS file
        int status = 0;   // FITSIO status, MUST be initialized to 0

        // assemble full filename from directory and file name
        size_t path_len = strlen(directory) + 1 + strlen(dir->d_name) + 1;
        char *filename = (char *)malloc(sizeof(char) * path_len);
        snprintf(filename, path_len, "%s/%s", directory, dir->d_name);

        //printf("CHECKING file %s\n", filename);

        // Attempt to open the file in read-only mode
        // The fits_open_file function will try to read the primary header.
        // If it fails, it will set the status variable to a non-zero value.
        if (fits_open_file(&fptr, filename, READONLY, &status)) {
            //printf("'%s' is not a valid FITS file or cannot be opened.\n", filename);
            //fits_report_error(stderr, status); // Print the detailed error message
            free(filename);
            return 0; // not a FITS file
        }
        else {
            // If we get here, status is still 0, meaning the file opened successfully.
            int total_hdus = 0;
            // Get the total number of HDUs in the file
            if (fits_get_num_hdus(fptr, &total_hdus, &status)) {
                fits_report_error(stderr, status);
                fits_close_file(fptr, &status);
                free(filename);
                return 2; // Exit loop on error
            }

            // move to last HDU
            if (fits_movabs_hdu(fptr, total_hdus, NULL, &status)) {
                fits_report_error(stderr, status);
                free(filename);
                return 2; // Exit loop on error
            }
            // get image size, bitpix
            if (fits_get_img_param(fptr, 8, &finfo->bitpix, &finfo->naxis, finfo->naxes, &status)) {
                fits_report_error(stderr, status);
                free(filename);
                return 2; // Exit loop on error
            }





            finfo->nbkey = 0;

            for(int hdu=1; hdu<=total_hdus; hdu++)
            {
                // HDU numbers are 1-based
                if (fits_movabs_hdu(fptr, hdu, NULL, &status)) {
                    fits_report_error(stderr, status);
                    free(filename);
                    return 2; // Exit loop on error
                }

                // Get the number of header keywords within this hdu
                int hdunkeys = 0;
                if (fits_get_hdrspace(fptr, &hdunkeys, NULL, &status)) {
                    fits_report_error(stderr, status);
                    free(filename);
                    return 2; // Exit loop on error
                }

                // Loop through each header card
                {
                    char card[FLEN_CARD];

                    for (int i = 1; i <= hdunkeys; i++) {
                        // Read the 80-character card
                        if (fits_read_record(fptr, i, card, &status)) {
                            fits_report_error(stderr, status);
                            break;
                        }

                        // Parse the card into its components
                        int klen = 0;
                        finfo->kw[finfo->nbkey].hdu = hdu;
                        fits_get_keyname(card, finfo->kw[finfo->nbkey].keyname, &klen, &status);
                        fits_parse_value(card, finfo->kw[finfo->nbkey].value, finfo->kw[finfo->nbkey].comment, &status);
                        finfo->nbkey++;
                    }
                }
            }



            int close_status = 0;
            if (fits_close_file(fptr, &close_status)) {
                fits_report_error(stderr, close_status);
                return 2;
            }
            snprintf(finfo->fname, FITSFNAMESTRLEN, "%s", filename);

            printf("✅ '%s' nkey=%d\n", filename, finfo->nbkey);
        }
        free(filename);
        return 1;
    }
    else {
        closedir(d);
        init = 0;
        return -1;
    }
}



