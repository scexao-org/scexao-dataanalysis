#ifndef _VAMPIRES_PDI__SCANFITSFILES_H
#define _VAMPIRES_PDI__SCANFITSFILES_H

#include <fitsio.h> // FITSIO

#define FITSFNAMESTRLEN 1000

// FITS keyword entry
typedef struct {
    int hdu;
    char keyname[FLEN_KEYWORD];
    char value[FLEN_VALUE];
    char comment[FLEN_COMMENT];
} FITSkeyword;



// Structure holding basic info about FITS files
// File name on disk and header info
typedef struct {
    char fname[FITSFNAMESTRLEN];
    int bitpix;
    int naxis;
    long naxes[8]; // max 8 dim
    int  nbkey;
    FITSkeyword *kw;
} FITSfileinfo;




int scan_nextFITSfiles(
    char *directory,
    FITSfileinfo *finfo
);



#endif
