#include <dirent.h> // opendir

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fitsio.h>






int scanFITSfiles(char *directory, char **FITSfilelist)
{
    int file_count = 0;
    if (directory != NULL) {
        printf("Scanning directory: %s\n", directory);

        DIR *d;
        struct dirent *dir;
        d = opendir(directory);
        if (d) {
            // First pass to count files
            while ((dir = readdir(d)) != NULL) {
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
                }
                else {
                    // If we get here, status is still 0, meaning the file opened successfully.
                    printf("✅ '%s' is a valid FITS file.\n", filename);

                    int close_status = 0;
                    if (fits_close_file(fptr, &close_status)) {
                        fits_report_error(stderr, close_status);
                        return close_status;
                    }
                    file_count++;
                }

                /*
                // remove .fz or .gz from end of filename
                char *fname = strdup(dir->d_name); // Duplicate to modify
                if (fname == NULL) {
                    fprintf(stderr, "Error: strdup failed.\n");
                    continue;
                }

                char *dot_fz = strstr(fname, ".fz");
                if (dot_fz != NULL && *(dot_fz + 3) == '\0') { // Ensure it's at the end
                    *dot_fz = '\0';
                } else {
                    char *dot_gz = strstr(fname, ".gz");
                    if (dot_gz != NULL && *(dot_gz + 3) == '\0') { // Ensure it's at the end
                        *dot_gz = '\0';
                    }
                }

                char *dot_fits = strstr(fname, ".fits");
                if (dot_fits != NULL && *(dot_fits + 5) == '\0') { // Ensure it's at the end
                    file_count++;
                }
                    */

            }
            closedir(d);

            printf("Found %d files\n", file_count);

            if (file_count > 0) {
                FITSfilelist = (char **)malloc(sizeof(char *) * (file_count));
                if (FITSfilelist == NULL) {
                    fprintf(stderr, "Error: Failed to allocate memory for FITSfilelist.\n");
                    return 1;
                }

                int current_file_idx = 0;
                d = opendir(directory); // Reopen directory for second pass
                if (d) {
                    while ((dir = readdir(d)) != NULL) {

                        fitsfile *fptr;   // Pointer to the FITS file
                        int status = 0;   // FITSIO status, MUST be initialized to 0

                        if (fits_open_file(&fptr, dir->d_name, READONLY, &status)) {
                            printf("'%s' is not a valid FITS file or cannot be opened.\n", dir->d_name);
                        }
                        else {
                            int close_status = 0;
                            if (fits_close_file(fptr, &close_status)) {
                                fits_report_error(stderr, close_status);
                                return close_status;
                            }

                            size_t path_len = strlen(directory) + 1 + strlen(dir->d_name) + 1;
                            FITSfilelist[current_file_idx] = (char *)malloc(sizeof(char) * path_len);
                            if (FITSfilelist[current_file_idx] == NULL) {
                                fprintf(stderr, "Error: Failed to allocate memory for filename.\n");
                                // Clean up already allocated filenames
                                for (int i = 0; i < current_file_idx; i++) {
                                    free(FITSfilelist[i]);
                                }
                                free(FITSfilelist);
                                return 1;
                            }
                            snprintf(FITSfilelist[current_file_idx], path_len, "%s/%s", directory, dir->d_name);
                            printf("[%5d] %s -> %s\n", current_file_idx, dir->d_name, FITSfilelist[current_file_idx]);


                            current_file_idx++;
                        }

                        /*
                        // remove .fz or .gz from end of filename
                        char *fname = strdup(dir->d_name); // Duplicate to modify
                        if (fname == NULL) {
                            fprintf(stderr, "Error: strdup failed.\n");
                            continue;
                        }

                        char *dot_fz = strstr(fname, ".fz");
                        if (dot_fz != NULL && *(dot_fz + 3) == '\0') { // Ensure it's at the end
                            *dot_fz = '\0';
                        } else {
                            char *dot_gz = strstr(fname, ".gz");
                            if (dot_gz != NULL && *(dot_gz + 3) == '\0') { // Ensure it's at the end
                                *dot_gz = '\0';
                            }
                        }

                        char *dot_fits = strstr(fname, ".fits");
                        if (dot_fits != NULL && *(dot_fits + 5) == '\0') { // Ensure it's at the end
                            // Construct full path
                            size_t path_len = strlen(directory) + 1 + strlen(dir->d_name) + 1;
                            FITSfilelist[current_file_idx] = (char *)malloc(sizeof(char) * path_len);
                            if (FITSfilelist[current_file_idx] == NULL) {
                                fprintf(stderr, "Error: Failed to allocate memory for filename.\n");
                                // Clean up already allocated filenames
                                for (int i = 0; i < current_file_idx; i++) {
                                    free(FITSfilelist[i]);
                                }
                                free(FITSfilelist);
                                return 1;
                            }
                            snprintf(FITSfilelist[current_file_idx], path_len, "%s/%s", directory, dir->d_name);
                            printf("[%5d] %s -> %s\n", current_file_idx, dir->d_name, FITSfilelist[current_file_idx]);
                            current_file_idx++;
                        }
                            */
                    }
                    closedir(d);
                }
            }
        }
    }
    return file_count;
}
