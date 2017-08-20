/**
 * @FILE  mris_make_surfaces.c
 * @brief creates white matter and grey matter surface files.
 *
 * This program positions the tessellation of the cortical surface
 * at the white matter surface, then the gray matter surface
 * and generate surface files for these surfaces as well as a
 * 'curvature' file for the cortical thickness, and a surface file
 * which approximates layer IV of the cortical sheet.
 */
/*
 * Original Author: Bruce Fischl
 * CVS Revision Info:
 *    $Author: fischl $
 *    $Date: 2017/02/16 19:42:36 $
 *    $Revision: 1.172 $
 *
 * Copyright © 2011-2012 The General Hospital Corporation (Boston, MA) "MGH"
 *
 * Terms and conditions for use, reproduction, distribution and contribution
 * are found in the 'FreeSurfer Software License Agreement' contained
 * in the file 'LICENSE' found in the FreeSurfer distribution, and here:
 *
 * https://surfer.nmr.mgh.harvard.edu/fswiki/FreeSurferSoftwareLicense
 *
 * Reporting: freesurfer@nmr.mgh.harvard.edu
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "macros.h"
#include "error.h"
#include "diag.h"
#include "proto.h"
#include "timer.h"
#include "mrisurf.h"
#include "mrisutils.h"
#include "mri.h"
#include "cma.h"
#include "macros.h"
#include "mrimorph.h"
#include "tags.h"
#include "mrinorm.h"
#include "version.h"
#include "label.h"

#define CONTRAST_T1    0
#define CONTRAST_T2    1
#define CONTRAST_FLAIR 2

#define  MAX_HISTO_BINS 1000

static char vcid[] =
  "$Id: mris_make_surfaces.c,v 1.172 2017/02/16 19:42:36 fischl Exp $";

int main(int argc, char *argv[]) ;

#define MIN_NONCORTEX_VERTICES 10
#define BRIGHT_LABEL         130
#define BRIGHT_BORDER_LABEL  100

#define MIN_PEAK_PCT 0.1
static double Ghisto_min_outside_peak_pct = MIN_PEAK_PCT ;
static double Ghisto_min_inside_peak_pct = MIN_PEAK_PCT ;
static int contrast_type = CONTRAST_T2 ;
static int unpinch = 0 ;
static int find_and_mark_pinched_regions(MRI_SURFACE *mris,
    MRI *mri_T2,
    float nstd_below,
    float nstd_above) ;

static float T2_max_inside = 300 ;
static float T2_min_inside = 60 ;
static double T2_min_outside = 90 ;  
static double T2_max_outside = 300 ;
static double max_outward_dist = 1 ; // for normal cases, for pathology might be much larger

// 0 disables use of max_gray in setting outside_hi in white surfacee deformation
// the larger this is, the closer outside_hi will be to max_gray. Set it high if
// the white surface is settling too far into the white matter
static double max_gray_scale = 0.0 ;  
static int above_set = 0 ;
static int below_set = 0 ;

static double l_repulse = 0.0 ;

int MRISremoveSelfIntersections(MRI_SURFACE *mris) ;
static int  near_bright_structure(MRI *mri_aseg, int xi, int yi, int zi, int whalf) ;
static int compute_pial_target_locations(MRI_SURFACE *mris,
					 MRI *mri_T2,
					 LABEL **labels,
					 int nlabels,
					 int contrast_type,
					 MRI *mri_aseg,
					 double T2_min_inside,
					 double T2_max_inside,
					 double T2_min_outside,
					 double T2_max_outside,
                                         double max_outward_dist,
                                         double min_inside_peak_pct,
                                         double min_outside_peak_pct,
					 double wm_weight,
                                         MRI *mri_T1) ;
static int compute_white_target_locations(MRI_SURFACE *mris,
					  MRI *mri_T2,
					  MRI *mri_aseg,
					  float nstd_below,
					  float nstd_above,
					  int contrast_type,
					  float T2_min_inside,
					  float T2_max_inside,
					  int below_set,
					  int above_set,
					  double sigma) ;
static MRI *
compute_white_matter_intensities(MRI_SURFACE *mris,
				 MRI *mri_T2,
				 MRI *mri_T1,
				 MRI *mri_aseg,
				 float nstd_above,
				 float nstd_below,
				 MRI *mri_dst);
static int compute_label_normal(MRI *mri_aseg, int x0, int y0, int z0,
                                int label, int whalf,
                                double *pnx, double *pny,
                                double *pnz, int use_abs);

static int edit_aseg_with_surfaces(MRI_SURFACE *mris, MRI *mri_aseg) ;
#if 0
static double mark_dura(MRI_SURFACE *mris,
                        MRI *mri_ratio,
                        MRI *mri_brain,
                        double sigma) ;
static MRI *compute_T2star_map(MRI **mri_echos, int nvolumes) ;
#endif
static double  compute_brain_thresh(MRI_SURFACE *mris,
                                    MRI *mri_ratio,
                                    float nstd) ;
static int fix_midline(MRI_SURFACE *mris,
                       MRI *mri_aseg,
                       MRI *mri_brain,
                       char *hemi, int which, int fix_mtl) ;
#if 0
static MRI *smooth_contra_hemi(MRI *mri_filled,
                               MRI *mri_src,
                               MRI *mri_dst,
                               float ipsi_label,
                               float contra_label) ;
static MRI *smooth_contra_hemi_with_aseg(MRI *mri_filled,
					 MRI *mri_src,
					 MRI *mri_dst,
					 char *hemi);
#endif
static int  get_option(int argc, char *argv[]) ;
static void usage_exit(void) ;
static void print_usage(void) ;
static void print_help(void) ;
static void print_version(void) ;
static int  mrisFindMiddleOfGray(MRI_SURFACE *mris) ;
static MRI  *MRIsmoothMasking(MRI *mri_src, MRI *mri_mask, MRI *mri_dst,
                              int mask_val, int wsize) ;
MRI *MRIfillVentricle(MRI *mri_inv_lv, MRI *mri_T1, float thresh,
                      int out_label, MRI *mri_dst);

int MRISfindExpansionRegions(MRI_SURFACE *mris) ;
int MRIsmoothBrightWM(MRI *mri_T1, MRI *mri_wm) ;
MRI *MRIfindBrightNonWM(MRI *mri_T1, MRI *mri_wm) ;

static int fix_mtl = 0 ;
static char *read_pinch_fname = NULL ;
static LABEL *highres_label = NULL ;
static char T1_name[STRLEN] = "brain" ;

static int erase_cerebellum = 0 ;
static int erase_brainstem = 0 ;
static float nsigma = 2.0 ;
static float pial_blur_sigma = 0.0 ;
static float dura_thresh = -1 ;
static float nsigma_above = 6.0 ;
static double wm_weight = 3 ;    // threshold for finding gm outliers: thresh = (wt*wm + gm )/(wt+1)
static float nsigma_below = 6.0 ;
static int remove_contra = 1 ;
static char *write_aseg_fname = NULL ;
static char *white_fname = NULL ;
static int use_mode = 1 ;

static char *orig_white = NULL ;
static char *orig_pial = NULL ;

char *Progname ;

static double std_scale = 1.0;

static int label_cortex = 1 ;
static int graymid = 0 ;
static int curvature_avgs = 10 ;
static int create = 1 ;
static int smoothwm = 0 ;
static int white_only = 0 ;
static int overlay = 0 ;
static int inverted_contrast = 0 ;
static char *filled_name = "filled" ;
static char *wm_name = "wm" ;
static int auto_detect_stats = 1 ;
static char *dura_echo_name = NULL ;
static char *T2_name = NULL ;
static char *flair_or_T2_name = NULL ;
static int nechos = 0 ;

static int in_out_in_flag = 0 ;  /* for Arthur (as are most things) */

static int nbhd_size = 20 ;

static INTEGRATION_PARMS  parms ;
#define BASE_DT_SCALE    1.0
static float base_dt_scale = BASE_DT_SCALE ;
static float pial_target_offset = 0 ;
static float white_target_offset = 0 ;

static COLOR_TABLE *ctab  = NULL;
static MRI *mri_cover_seg = NULL ;
static char *aseg_name = "aseg" ;
static char *aparc_name = "aparc" ;  // for midline and cortex label
static MRI *mri_aseg = NULL;
static int add = 0 ;

static double l_tsmooth = 0.0 ;
static double l_surf_repulse = 5.0 ;

static int smooth_pial = 0 ;
static int smooth = 5 ;
static int vavgs = 5 ;
static int nwhite = 100 /* 25 */ /*5*/ ;
static int ngray = 100  /* 30*/ /*45*/ ;

static int nowhite = 0 ;
static int flairwhite = 0 ;
static int nbrs = 2 ;
static int write_vals = 0 ;

static char *orig_name = ORIG_NAME ; // "orig"
static char *suffix = "" ;
static char *output_suffix = "" ;
static char *xform_fname = NULL ;

static char pial_name[STRLEN] = "pial" ;
static char white_matter_name[STRLEN] = WHITE_MATTER_NAME ; // "white"

static int lh_label = LH_LABEL ;
static int rh_label = RH_LABEL ;

static int max_pial_averages = 16 ;
static int min_pial_averages = 2 ;
static int max_white_averages = 4 ;
static int min_white_averages = 0 ;
static float pial_sigma = 2.0f ;
static float white_sigma = 2.0f ;
static float max_thickness = 5.0 ;

static float variablesigma = 3.0;

#define MAX_WHITE             120
#define MAX_BORDER_WHITE      105
#define MIN_BORDER_WHITE       85
#define MIN_GRAY_AT_WHITE_BORDER  70

#define MAX_GRAY               95
#define MIN_GRAY_AT_CSF_BORDER    40
#define MID_GRAY               ((max_gray + min_gray_at_csf_border) / 2)
#define MAX_GRAY_AT_CSF_BORDER    75
#define MIN_CSF                10
#define MAX_CSF                40

static  int   max_border_white_set = 0,
              min_border_white_set = 0,
              min_gray_at_white_border_set = 0,
              max_gray_set = 0,
              max_gray_at_csf_border_set = 0,
              min_gray_at_csf_border_set = 0,
              min_csf_set = 0,
              max_csf_set = 0 ;

static  float   max_border_white = MAX_BORDER_WHITE,
                min_border_white = MIN_BORDER_WHITE,
                min_gray_at_white_border = MIN_GRAY_AT_WHITE_BORDER,
                max_gray = MAX_GRAY,
                max_gray_at_csf_border = MAX_GRAY_AT_CSF_BORDER,
                min_gray_at_csf_border = MIN_GRAY_AT_CSF_BORDER,
                min_csf = MIN_CSF,
                max_csf = MAX_CSF ;
static char sdir[STRLEN] = "" ;

static int MGZ = 1; // for use with MGZ format

static int longitudinal = 0;

static int pial_num = 0 ;
static int pial_nbrs = 0 ;
static int white_num = 0 ;
#define MAX_VERTICES 1000
static float pial_vals[MAX_VERTICES] ;
static int pial_vnos[MAX_VERTICES] ;
static float white_vals[MAX_VERTICES] ;
static int white_vnos[MAX_VERTICES] ;

static int fill_interior = 0 ;
static float check_contrast_direction(MRI_SURFACE *mris,MRI *mri_T1) ;
int
main(int argc, char *argv[])
{
  char          *hemi, *sname, *cp, fname[STRLEN], mdir[STRLEN];
  int           nargs, i, msec, n_averages, j ;
  MRI_SURFACE   *mris ;
  MRI           *mri_wm, *mri_kernel = NULL;
  MRI *mri_smooth = NULL, *mri_mask = NULL;
  MRI *mri_filled, *mri_T1, *mri_labeled, *mri_T1_white = NULL, *mri_T1_pial ;
  float         max_len ;
  float         white_mean, white_std, gray_mean, gray_std ;
  double        current_sigma, thresh = 0, spring_scale = 1;
  struct timeb  then ;
  M3D           *m3d ;
  INTEGRATION_PARMS old_parms ;

  char cmdline[CMD_LINE_LEN] ;

  make_cmd_version_string
  (argc, argv,
   "$Id: mris_make_surfaces.c,v 1.172 2017/02/16 19:42:36 fischl Exp $",
   "$Name:  $", cmdline);

  /* rkt: check for and handle version tag */
  nargs = handle_version_option
          (argc, argv,
           "$Id: mris_make_surfaces.c,v 1.172 2017/02/16 19:42:36 fischl Exp $",
           "$Name:  $");
  if (nargs && argc - nargs == 1)
  {
    exit (0);
  }
  argc -= nargs;

  Gdiag |= DIAG_SHOW ;
  Progname = argv[0] ;
  ErrorInit(NULL, NULL, NULL) ;
  DiagInit(NULL, NULL, NULL) ;

  memset(&parms, 0, sizeof(parms)) ;
  // don't let gradient use exterior information (slows things down)
  parms.fill_interior = 0 ;
  parms.projection = NO_PROJECTION ;
  parms.tol = 1e-4 ;
  parms.dt = 0.5f ;
  parms.base_dt = BASE_DT_SCALE*parms.dt ;
  parms.l_spring = 1.0f ;
  parms.l_curv = 1.0 ;
  parms.l_intensity = 0.2 ;
  parms.l_spring = 0.0f ;
  parms.l_intensity = 0.2 ;
  parms.l_tspring = 1.0f ;
  parms.l_nspring = 0.5f ;

  parms.niterations = 0 ;
  parms.write_iterations = 0 /*WRITE_ITERATIONS */;
  parms.integration_type = INTEGRATE_MOMENTUM ;
  parms.momentum = 0.0 /*0.8*/ ;
  parms.dt_increase = 1.0 /* DT_INCREASE */;
  parms.dt_decrease = 0.50 /* DT_DECREASE*/ ;
  parms.error_ratio = 50.0 /*ERROR_RATIO */;
  /*  parms.integration_type = INTEGRATE_LINE_MINIMIZE ;*/
  parms.l_surf_repulse = 0.0 ;
  parms.l_repulse = 5 ;

  for ( ; argc > 1 && ISOPTION(*argv[1]) ; argc--, argv++)
  {
    nargs = get_option(argc, argv) ;
    argc -= nargs ;
    argv += nargs ;
  }

  if (argc < 3)
  {
    usage_exit() ;
  }

  /* set default parameters for white and gray matter surfaces */
  parms.niterations = nwhite ;
  if (parms.momentum < 0.0)
  {
    parms.momentum = 0.0 /*0.75*/ ;
  }

  TimerStart(&then) ;
  sname = argv[1] ;
  hemi = argv[2] ;
  if (!strlen(sdir))
  {
    cp = getenv("SUBJECTS_DIR") ;
    if (!cp)
      ErrorExit(ERROR_BADPARM,
                "%s: SUBJECTS_DIR not defined in environment.\n", Progname) ;
    strcpy(sdir, cp) ;
  }

  cp = getenv("FREESURFER_HOME") ;
  if (!cp)
    ErrorExit(ERROR_BADPARM,
              "%s: FREESURFER_HOME not defined in environment.\n", Progname) ;
  strcpy(mdir, cp) ;

  // print out version of this program and mrisurf.c
  printf("%s\n",vcid);
  printf("%s\n",MRISurfSrcVersion());
  fflush(stdout);
  sprintf(fname, "%s/%s/surf/mris_make_surfaces.%s.mrisurf.c.version",
          sdir, sname, hemi) ;

  sprintf(fname, "%s/%s/mri/%s", sdir, sname, filled_name) ;
  if (MGZ)
  {
    strcat(fname, ".mgz");
  }
  fprintf(stderr, "reading volume %s...\n", fname) ;
  mri_filled = MRIread(fname) ;
  if (!mri_filled)
    ErrorExit(ERROR_NOFILE, "%s: could not read input volume %s",
              Progname, fname) ;
  ////////////////////////////// we can handle only conformed volumes
//  setMRIforSurface(mri_filled);

  sprintf(fname, "%s/%s/mri/%s", sdir, sname, T1_name) ;
  if (MGZ)
  {
    strcat(fname, ".mgz");
  }
  fprintf(stderr, "reading volume %s...\n", fname) ;
  mri_T1 = mri_T1_pial = MRIread(fname) ;

  if (!mri_T1)
    ErrorExit(ERROR_NOFILE, "%s: could not read input volume %s",
              Progname, fname) ;
  /////////////////////////////////////////
//  setMRIforSurface(mri_T1);


  // make sure filled and wm are in same voxel coords as T1
  if (MRIcompareHeaders(mri_T1, mri_filled) != 0)
  {
    MRI *mri_tmp ;
    printf("resampling filled volume to be in voxel register with T1\n") ;
    mri_tmp = MRIresample(mri_filled, mri_T1, SAMPLE_NEAREST) ;
    MRIfree(&mri_filled) ; mri_filled = mri_tmp ;
  }
  if (white_fname != NULL)
  {
    sprintf(fname, "%s/%s/mri/%s", sdir, sname, white_fname) ;
    if (MGZ)
    {
      strcat(fname, ".mgz");
    }
    fprintf(stderr, "reading volume %s...\n", fname) ;
    mri_T1_white = MRIread(fname) ;
    if (!mri_T1_white)
      ErrorExit(ERROR_NOFILE, "%s: could not read input volume %s",
                Progname, fname) ;
    /////////////////////////////////////////
    if (mri_T1_white->type != MRI_UCHAR)
    {
      MRI *mri_tmp ;

      MRIeraseNegative(mri_T1_white, mri_T1_white) ;
      mri_tmp = MRIchangeType(mri_T1_white, MRI_UCHAR, 0, 255, 1) ;
      MRIfree(&mri_T1_white) ;
      mri_T1_white = mri_tmp ; // swap
    }

//    setMRIforSurface(mri_T1_white);
  }

  if (xform_fname)
  {
    char fname[STRLEN], ventricle_fname[STRLEN] ;
    MRI  *mri_lv, *mri_inv_lv ;

    sprintf(fname, "%s/%s/mri/transforms/%s", sdir, sname,xform_fname) ;
    fprintf(stderr, "reading transform %s...\n", fname) ;
    m3d = MRI3DreadSmall(fname) ;
    if (!m3d)
      ErrorExit(ERROR_NOFILE, "%s: could not open transform file %s\n",
                Progname, fname) ;
    sprintf(ventricle_fname, "%s/average/%s_ventricle.mgz#0@mgh",
            mdir, !stricmp(hemi, "lh") ? "left" : "right") ;
    fprintf(stderr,"reading ventricle representation %s...\n",
            ventricle_fname);
    mri_lv = MRIread(ventricle_fname) ;
    if (!mri_lv)
      ErrorExit(ERROR_NOFILE,"%s: could not read %s",
                Progname,ventricle_fname);
    fprintf(stderr, "applying inverse morph to ventricle...\n") ;
    mri_inv_lv = MRIapplyInverse3DMorph(mri_lv,m3d,NULL);
    MRI3DmorphFree(&m3d) ;
    MRIfree(&mri_lv) ;
    fprintf(stderr, "filling in ventricle...\n") ;
    mri_lv = MRIfillVentricle(mri_inv_lv,mri_T1,100,
                              DEFAULT_DESIRED_WHITE_MATTER_VALUE, NULL);
    MRIfree(&mri_inv_lv) ;
    MRIunion(mri_lv, mri_T1, mri_T1) ;
    MRIfree(&mri_lv) ;
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    {
      sprintf(fname, "%s/%s/mri/T1_filled", sdir, sname) ;
      MRIwrite(mri_T1, fname) ;
    }
  }
  if (aseg_name)
  {
    char fname[STRLEN] ;
    sprintf(fname, "%s/%s/mri/%s", sdir, sname, aseg_name) ;
    if (MGZ)
    {
      strcat(fname, ".mgz");
    }

    fprintf(stderr, "reading volume %s...\n", fname) ;
    mri_aseg = MRIread(fname) ;
    if (mri_aseg == NULL)
      ErrorExit(ERROR_NOFILE, "%s: could not read segmentation volume %s",
                Progname, fname) ;
    if (erase_brainstem)
    {
      MRImaskLabel(mri_T1, mri_T1, mri_aseg, Brain_Stem, 0) ;
      MRImaskLabel(mri_T1, mri_T1, mri_aseg,  Left_VentralDC, 0) ;
      MRImaskLabel(mri_T1, mri_T1, mri_aseg,  Right_VentralDC, 0) ;
      if (mri_T1_white && mri_T1_white != mri_T1)
      {
	MRImaskLabel(mri_T1_white, mri_T1_white, mri_aseg, Brain_Stem, 0) ;
	MRImaskLabel(mri_T1_white, mri_T1_white, mri_aseg,  Left_VentralDC, 0) ;
	MRImaskLabel(mri_T1_white, mri_T1_white, mri_aseg,  Right_VentralDC, 0) ;
      }

      if (mri_T1_pial && mri_T1_pial != mri_T1)
      {
	MRImaskLabel(mri_T1_pial, mri_T1_pial, mri_aseg, Brain_Stem, 0) ;
	MRImaskLabel(mri_T1_pial, mri_T1_pial, mri_aseg,  Left_VentralDC, 0) ;
	MRImaskLabel(mri_T1_pial, mri_T1_pial, mri_aseg,  Right_VentralDC, 0) ;
      }

      MRImaskLabel(mri_filled, mri_filled, mri_aseg, Brain_Stem, 0) ;
      MRImaskLabel(mri_filled, mri_filled, mri_aseg,  Left_VentralDC, 0) ;
      MRImaskLabel(mri_filled, mri_filled, mri_aseg,  Right_VentralDC, 0) ;
    }
    if (erase_cerebellum)
    {
      MRImaskLabel(mri_T1, mri_T1, mri_aseg, Left_Cerebellum_White_Matter, 0) ;
      MRImaskLabel(mri_T1, mri_T1, mri_aseg, Left_Cerebellum_Cortex, 0) ;
      MRImaskLabel(mri_T1, mri_T1, mri_aseg, Right_Cerebellum_White_Matter, 0) ;
      MRImaskLabel(mri_T1, mri_T1, mri_aseg, Right_Cerebellum_Cortex, 0) ;

      if (mri_T1_white && mri_T1_white != mri_T1)
      {
	MRImaskLabel(mri_T1_white, mri_T1_white, mri_aseg, Left_Cerebellum_White_Matter, 0) ;
	MRImaskLabel(mri_T1_white, mri_T1_white, mri_aseg, Left_Cerebellum_Cortex, 0) ;
	MRImaskLabel(mri_T1_white, mri_T1_white, mri_aseg, Right_Cerebellum_White_Matter, 0) ;
	MRImaskLabel(mri_T1_white, mri_T1_white, mri_aseg, Right_Cerebellum_Cortex, 0) ;
      }

      if (mri_T1_pial && mri_T1_pial != mri_T1)
      {
	MRImaskLabel(mri_T1_pial, mri_T1_pial, mri_aseg, Left_Cerebellum_White_Matter, 0) ;
	MRImaskLabel(mri_T1_pial, mri_T1_pial, mri_aseg, Left_Cerebellum_Cortex, 0) ;
	MRImaskLabel(mri_T1_pial, mri_T1_pial, mri_aseg, Right_Cerebellum_White_Matter, 0) ;
	MRImaskLabel(mri_T1_pial, mri_T1_pial, mri_aseg, Right_Cerebellum_Cortex, 0) ;
      }

      MRImaskLabel(mri_filled, mri_filled, mri_aseg, Left_Cerebellum_White_Matter, 0) ;
      MRImaskLabel(mri_filled, mri_filled, mri_aseg, Left_Cerebellum_Cortex, 0) ;
      MRImaskLabel(mri_filled, mri_filled, mri_aseg, Right_Cerebellum_White_Matter, 0) ;
      MRImaskLabel(mri_filled, mri_filled, mri_aseg, Right_Cerebellum_Cortex, 0) ;
    }
  }
  else
  {
    mri_aseg = NULL ;
  }
  sprintf(fname, "%s/%s/mri/%s", sdir, sname, wm_name) ;
  if (MGZ)
  {
    strcat(fname, ".mgz");
  }
  fprintf(stderr, "reading volume %s...\n", fname) ;
  mri_wm = MRIread(fname) ;
  if (!mri_wm)
    ErrorExit(ERROR_NOFILE, "%s: could not read input volume %s",
              Progname, fname) ;
  if (mri_wm->type != MRI_UCHAR)
  {
    MRI *mri_tmp ;
    printf("changing type of input wm volume to UCHAR...\n") ;
    mri_tmp = MRIchangeType(mri_wm, MRI_UCHAR, 0, 255, 1) ;
    MRIfree(&mri_wm) ;
    mri_wm = mri_tmp ;
  }
  if (MRIcompareHeaders(mri_T1, mri_wm) != 0)
  {
    MRI *mri_tmp ;
    printf("resampling wm volume to be in voxel register with T1\n") ;
    mri_tmp = MRIresample(mri_wm, mri_T1, SAMPLE_NEAREST) ;
    MRIfree(&mri_wm) ; mri_wm = mri_tmp ;
  }
  //////////////////////////////////////////
//  setMRIforSurface(mri_wm);

  MRIsmoothBrightWM(mri_T1, mri_wm) ;
  if (fill_interior == 0)
    mri_labeled = MRIfindBrightNonWM(mri_T1, mri_wm) ;
  else
    mri_labeled = MRIclone(mri_T1, NULL) ;
  if (mri_T1_white)
  {
    MRIsmoothBrightWM(mri_T1_white, mri_wm) ;
  }
  if (overlay)
  {
    fprintf(stderr, "overlaying editing into T1 volume...\n") ;
    MRImask(mri_T1, mri_wm, mri_T1,
            WM_EDITED_ON_VAL, DEFAULT_DESIRED_WHITE_MATTER_VALUE);
    MRIsmoothMasking(mri_T1, mri_wm, mri_T1, WM_EDITED_ON_VAL, 15) ;
    sprintf(fname, "%s/%s/mri/T1_overlay", sdir, sname) ;
    MRIwrite(mri_T1, fname) ;
  }


  sprintf(fname, "%s/%s/surf/%s.%s%s", sdir, sname, hemi, orig_name, suffix) ;
  fprintf(stderr, "reading original surface position from %s...\n", fname) ;
  mris = MRISreadOverAlloc(fname, 1.1) ;
  if (!mris)
    ErrorExit(ERROR_NOFILE, "%s: could not read surface file %s",
              Progname, fname) ;
  if (mris->vg.valid && !FZERO(mris->vg.xsize))
    spring_scale = 3/(mris->vg.xsize + mris->vg.ysize + mris->vg.zsize) ;
  else
  {
    spring_scale = 3/(mri_T1->xsize + mri_T1->ysize + mri_T1->zsize) ;
    mris->vg.xsize = mri_T1->xsize ;
    mris->vg.ysize = mri_T1->ysize ;
    mris->vg.zsize = mri_T1->zsize ;
  }
  if (!FEQUAL(spring_scale, 1.0))
    printf("spring scale = %2.2f\n", spring_scale) ;
  MRISaddCommandLine(mris, cmdline) ;
  if (mris->ct == NULL && ctab)  
    mris->ct = ctab ;  // add user-specified color table to structure

  MRISremoveIntersections(mris) ;
  if (pial_nbrs > 2)
  {
    MRISsetNeighborhoodSize(mris, pial_nbrs) ;
  }
  if (auto_detect_stats)
  {
    MRI *mri_tmp ;
    float white_mode, gray_mode ;

    mri_tmp = MRIbinarize
              (mri_wm, NULL, WM_MIN_VAL, MRI_NOT_WHITE, MRI_WHITE) ;
    fprintf(stderr, "computing class statistics...\n");
    MRISsaveVertexPositions(mris, WHITE_VERTICES) ;
    MRIcomputeClassStatistics(mri_T1, mri_tmp, 30, WHITE_MATTER_MEAN,
                              &white_mean, &white_std, &gray_mean,
                              &gray_std) ;
    if (use_mode)
    {
      printf("using class modes intead of means, discounting robust sigmas....\n") ;
//      MRIScomputeClassModes(mris, mri_T1, &white_mode, &gray_mode, NULL, &white_std, &gray_std, NULL);
      MRIScomputeClassModes(mris, mri_T1, &white_mode, &gray_mode, NULL, NULL, NULL, NULL);
      white_mean = white_mode ;
      gray_mean = gray_mode ;
    }

    white_std /= std_scale;
    gray_std /= std_scale;

    if (!min_gray_at_white_border_set)
    {
      min_gray_at_white_border = gray_mean-gray_std ;
    }
    if (!max_border_white_set)
    {
      max_border_white = white_mean+white_std ;
    }
    if (!max_csf_set)
    {
      max_csf = gray_mean - MAX(.5, (variablesigma-1.0))*gray_std ;
    }
    if (!min_border_white_set)
    {
      min_border_white = gray_mean ;
    }

    // apply some sanity checks
#define MAX_SCALE_DOWN .2

    if (min_gray_at_white_border < MAX_SCALE_DOWN*MIN_GRAY_AT_WHITE_BORDER)
      min_gray_at_white_border = nint(MAX_SCALE_DOWN*MIN_GRAY_AT_WHITE_BORDER) ;
    if (max_border_white < MAX_SCALE_DOWN*MAX_BORDER_WHITE)
      max_border_white = nint(MAX_SCALE_DOWN*MAX_BORDER_WHITE) ;
    if (min_border_white < MAX_SCALE_DOWN*MIN_BORDER_WHITE)
      min_border_white = nint(MAX_SCALE_DOWN*MIN_BORDER_WHITE) ;
    if (max_csf < MAX_SCALE_DOWN*MAX_CSF)
      max_csf = MAX_SCALE_DOWN*MAX_CSF ;

    fprintf(stderr, "setting MIN_GRAY_AT_WHITE_BORDER to %2.1f (was %d)\n",
            min_gray_at_white_border, MIN_GRAY_AT_WHITE_BORDER) ;
    fprintf(stderr, "setting MAX_BORDER_WHITE to %2.1f (was %d)\n",
            max_border_white, MAX_BORDER_WHITE) ;
    fprintf(stderr, "setting MIN_BORDER_WHITE to %2.1f (was %d)\n",
            min_border_white, MIN_BORDER_WHITE) ;
    fprintf(stderr, "setting MAX_CSF to %2.1f (was %d)\n",
            max_csf, MAX_CSF) ;

    if (!max_gray_set)
    {
      max_gray = white_mean-white_std ;
    }
    if (!max_gray_at_csf_border_set)
    {
//      max_gray_at_csf_border = gray_mean-0.5*gray_std ;
      max_gray_at_csf_border = gray_mean-1.0*gray_std ;   // changed to push pial surfaces further out BRF 12/10/2015
    }
    if (!min_gray_at_csf_border_set)
    {
      min_gray_at_csf_border = gray_mean - variablesigma*gray_std ;
    }
    if (max_gray < MAX_SCALE_DOWN*MAX_GRAY)
      max_gray = nint(MAX_SCALE_DOWN*MAX_GRAY) ;
    if (max_gray_at_csf_border < MAX_SCALE_DOWN*MAX_GRAY_AT_CSF_BORDER)
      max_gray_at_csf_border = nint(MAX_SCALE_DOWN*MAX_GRAY_AT_CSF_BORDER) ;
    if (min_gray_at_csf_border < MAX_SCALE_DOWN*MIN_GRAY_AT_CSF_BORDER)
      min_gray_at_csf_border = nint(MAX_SCALE_DOWN*MIN_GRAY_AT_CSF_BORDER) ;
    fprintf(stderr, "setting MAX_GRAY to %2.1f (was %d)\n",
            max_gray, MAX_GRAY) ;
    fprintf(stderr, "setting MAX_GRAY_AT_CSF_BORDER to %2.1f (was %d)\n",
            max_gray_at_csf_border, MAX_GRAY_AT_CSF_BORDER) ;
    fprintf(stderr, "setting MIN_GRAY_AT_CSF_BORDER to %2.1f (was %d)\n",
            min_gray_at_csf_border, MIN_GRAY_AT_CSF_BORDER) ;
    MRIfree(&mri_tmp) ;
  }
  if (dura_echo_name == NULL)
  {
    MRIfree(&mri_wm) ;
  }
  inverted_contrast = (check_contrast_direction(mris,mri_T1) < 0) ;
  if (inverted_contrast)
  {
    printf("inverted contrast detected....\n") ;
  }
  if (highres_label)
  {
    LabelRipRestOfSurface(highres_label, mris) ;
  }
  if (smooth && !nowhite && !dura_echo_name)
  {
    printf("smoothing surface for %d iterations...\n", smooth) ;
    MRISaverageVertexPositions(mris, smooth) ;
  }

  if (nbrs > 1)
  {
    MRISsetNeighborhoodSize(mris, nbrs) ;
  }

  sprintf(parms.base_name, "%s%s%s",
          white_matter_name, output_suffix, suffix) ;
  if (orig_white)
  {
    printf("reading initial white vertex positions from %s...\n",
           orig_white) ;
    if (MRISreadVertexPositions(mris, orig_white) != NO_ERROR)
    {
      ErrorExit(Gerror, "reading of orig white failed...");
    }
    MRISremoveIntersections(mris) ;
  }
  MRIScomputeMetricProperties(mris) ;    /* recompute surface normals */
  MRISstoreMetricProperties(mris) ;
  MRISsaveVertexPositions(mris, ORIGINAL_VERTICES) ;
  MRISsaveVertexPositions(mris, WHITE_VERTICES) ;

  if (add)
  {
    fprintf(stderr, "adding vertices to initial tessellation...\n") ;
    for (max_len = 1.5*8 ; max_len > 1 ; max_len /= 2)
      while (MRISdivideLongEdges(mris, max_len) > 0) {}
  }
  MRISsetVals(mris, -1) ;  /* clear white matter intensities */

  if (aparc_name)
  {
    if (MRISreadAnnotation(mris, aparc_name) != NO_ERROR)
      ErrorExit(ERROR_NOFILE, "%s: could not read annotation",
                aparc_name) ;
  }
#if 0
  if (dura_echo_name)
  {
#define MAX_VOLUMES 100
    char fname[STRLEN], fmt[STRLEN] ;
    MRI *mri_ratio, *mri_T2star, *mri_echos[MAX_VOLUMES] ;
    int  e ;

    sprintf(parms.base_name, "%s%s%s", pial_name, output_suffix, suffix) ;
    for (e = 0 ; e < nechos ; e++)
    {
      if (e != 0 && e != nechos-1)
      {
        continue ;  // only read 1st and last echo (they are the only ones used)
      }
      sprintf(fmt, "%s/%s/mri/%s", sdir, sname, dura_echo_name) ;
      sprintf(fname, fmt, e) ;
      mri_echos[e] = MRIread(fname) ;
      if (mri_echos[e] == NULL)
        ErrorExit
        (ERROR_BADPARM,
         "%s: could not read %dth echo for dura localization from %s",
         Progname, e, fname) ;
    }


    if (auto_detect_stats)
    {
      MRI *mri_tmp ;
      float white_mode, gray_mode ;

      mri_tmp = MRIbinarize
                (mri_wm, NULL, WM_MIN_VAL, MRI_NOT_WHITE, MRI_WHITE) ;
      fprintf(stderr, "computing class statistics...\n");
      MRISsaveVertexPositions(mris, WHITE_VERTICES) ;
      MRIcomputeClassStatistics(mri_echos[nechos-1], mri_tmp, 30, WHITE_MATTER_MEAN,
				&white_mean, &white_std, &gray_mean, &gray_std) ;
      if (use_mode)
      {
        printf("using class modes intead of means....\n") ;
	MRIScomputeClassModes(mris, mri_echos[nechos-1], &white_mode, &gray_mode, NULL,
			      &white_std, &gray_std,NULL);
        white_mean = white_mode ;
        gray_mean = gray_mode ;
      }

      white_std /= std_scale;
      gray_std /= std_scale;

      if (!min_gray_at_white_border_set)
      {
        min_gray_at_white_border = gray_mean-gray_std ;
      }
      if (!max_border_white_set)
      {
        max_border_white = white_mean+white_std ;
      }
      if (!max_csf_set)
      {
        max_csf = gray_mean - variablesigma*gray_std ;
      }
      if (!min_border_white_set)
      {
        min_border_white = gray_mean ;
      }
      fprintf(stderr,
              "setting MIN_GRAY_AT_WHITE_BORDER to %2.1f (was %d)\n",
              min_gray_at_white_border, MIN_GRAY_AT_WHITE_BORDER) ;
      fprintf(stderr, "setting MAX_BORDER_WHITE to %2.1f (was %d)\n",
              max_border_white, MAX_BORDER_WHITE) ;
      fprintf(stderr, "setting MIN_BORDER_WHITE to %2.1f (was %d)\n",
              min_border_white, MIN_BORDER_WHITE) ;
      fprintf(stderr, "setting MAX_CSF to %2.1f (was %d)\n",
              max_csf, MAX_CSF) ;

      if (!max_gray_set)
      {
        max_gray = white_mean-white_std ;
      }
      if (!max_gray_at_csf_border_set)
      {
        max_gray_at_csf_border = gray_mean-0.5*gray_std ;
      }
      if (!min_gray_at_csf_border_set)
      {
        min_gray_at_csf_border = gray_mean - variablesigma*gray_std ;
      }
      fprintf(stderr, "setting MAX_GRAY to %2.1f (was %d)\n",
              max_gray, MAX_GRAY) ;
      fprintf(stderr, "setting MAX_GRAY_AT_CSF_BORDER to %2.1f (was %d)\n",
              max_gray_at_csf_border, MAX_GRAY_AT_CSF_BORDER) ;
      fprintf(stderr, "setting MIN_GRAY_AT_CSF_BORDER to %2.1f (was %d)\n",
              min_gray_at_csf_border, MIN_GRAY_AT_CSF_BORDER) ;
      MRIfree(&mri_tmp) ;
    }


    mri_T2star = compute_T2star_map(mri_echos, nechos) ;
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    {
      MRIwrite(mri_T2star, "T2star.mgz") ;
    }
    mri_ratio = MRIdivide(mri_echos[0], mri_echos[nechos-1], NULL) ;
    sprintf(fname, "%s/%s/surf/%s.%s", sdir, sname, hemi, pial_name) ;
    if (MRISreadVertexPositions(mris, fname) != NO_ERROR)
    {
      ErrorExit(Gerror, "reading of pial from %s failed...", fname);
    }
    MRIScomputeMetricProperties(mris) ;

    sprintf(fname, "%s/%s/surf/%s.%s",
            sdir, sname, hemi, white_matter_name) ;
    if (MRISreadOriginalProperties(mris, fname) != NO_ERROR)
      ErrorExit(Gerror,
                "reading of white matter surface from %s failed...",
                fname);

    if (!mri_smooth)
    {
      mri_smooth = MRIcopy(mri_T1, NULL) ;
    }
    current_sigma = pial_sigma ;
    parms.l_surf_repulse = l_surf_repulse ;
    parms.l_repulse = 0 ;
    parms.l_intensity /= 4 ;
    for (n_averages = max_pial_averages, i = 0 ;
         n_averages >= min_pial_averages ;
         n_averages /= 2, current_sigma /= 2, i++)
    {
      parms.sigma = current_sigma ;
      thresh = mark_dura(mris, mri_ratio, mri_T1, current_sigma) ;
      if (mri_aseg)
      {
        fix_midline(mris, mri_aseg, mri_T1, hemi, GRAY_WHITE, 0) ;
      }
      if (flair_or_T2_name == NULL) // otherwise already done
        MRIScomputeBorderValues
        (mris, mri_T1, mri_smooth, max_gray,
         max_gray_at_csf_border, min_gray_at_csf_border,
         min_csf,(max_csf+min_gray_at_csf_border)/2,
         current_sigma, 2*max_thickness, parms.fp,
         GRAY_CSF, mri_ratio, thresh, parms.flags,mri_aseg) ;
      MRISaddToValues(mris, white_target_offset) ;
      {
        int i, vno ;
        for (i = 0; i < pial_num ; i++)
        {
          vno = pial_vnos[i] ;
          mris->vertices[vno].val = pial_vals[i] ;
          mris->vertices[vno].marked = 1 ;
        }
      }
      if (vavgs)
      {
        fprintf
        (stderr,
         "averaging target values for %d iterations...\n",
         vavgs) ;
        MRISaverageMarkedVals(mris, vavgs) ;
        if (Gdiag_no > 0)
        {
          VERTEX *v ;
          v = &mris->vertices[Gdiag_no] ;
          fprintf
          (stderr,
           "v %d, target value = %2.1f, mag = %2.1f, dist=%2.2f\n",
           Gdiag_no, v->val, v->mean, v->d) ;
        }
      }
      mri_kernel = MRIgaussian1d(current_sigma, 100) ;
      fprintf(stderr, "smoothing T1 volume with sigma = %2.3f\n",
              current_sigma) ;
      parms.n_averages = n_averages ;
      parms.l_tsmooth = l_tsmooth ;
      *(&old_parms) = *(&parms) ;
      parms.l_tspring *= spring_scale ; parms.l_nspring *= spring_scale ; parms.l_spring *= spring_scale ;
      parms.l_tspring = MIN(1.0,parms.l_tspring) ;
      parms.l_nspring = MIN(1.0, parms.l_nspring) ;
      parms.l_spring = MIN(1.0, parms.l_spring) ;
      MRISpositionSurface(mris, mri_T1, mri_smooth,&parms);
      old_parms.start_t = parms.start_t ;
      *(&parms) = *(&old_parms) ;
      MRISunrip(mris) ;
    }

    for (e = 0 ; e < nechos ; e++)
    {
      MRIfree(&mri_echos[e]) ;
    }

    MRISwrite(mris, "dura_deformed") ;
    exit(0) ;
  }
#endif

  if (!nowhite)
  {
    fprintf(stderr,
            "repositioning cortical surface to gray/white boundary\n");

    MRImask(mri_T1, mri_labeled, mri_T1, BRIGHT_LABEL, 0) ;
    MRImask(mri_T1, mri_labeled, mri_T1, BRIGHT_BORDER_LABEL, 0) ;
    if (mri_T1_white)
    {
      MRImask
      (mri_T1_white, mri_labeled, mri_T1_white, BRIGHT_LABEL, 0) ;
      MRImask
      (mri_T1_white, mri_labeled, mri_T1_white, BRIGHT_BORDER_LABEL, 0) ;
    }
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    {
      MRIwrite(mri_T1, "white_masked.mgz") ;
    }
  }
  if (mri_T1_white)
  {
    if (mri_T1 != mri_T1_pial)
    {
      MRIfree(&mri_T1);
    }
    mri_T1 = mri_T1_white ; // T1 and T1_white is swapped
  }
  current_sigma = white_sigma ;
  for (n_averages = max_white_averages, i = 0 ;
       n_averages >= min_white_averages ;
       n_averages /= 2, current_sigma /= 2, i++)
  {
    if (nowhite)
    {
      break ;
    }

    parms.sigma = current_sigma ;
    mri_kernel = MRIgaussian1d(current_sigma, 100) ;
    fprintf(stderr, "smoothing T1 volume with sigma = %2.3f\n",
            current_sigma) ;
    if (!mri_smooth)
    {
      mri_smooth = MRIcopy(mri_T1, NULL) ;
    }
#if 0
    MRIconvolveGaussian(mri_T1, mri_smooth, mri_kernel) ;
#endif

    MRIfree(&mri_kernel) ;
    if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
    {
      char fname[STRLEN] ;
      sprintf(fname, "sigma%.0f.mgz", current_sigma) ;
      fprintf(stderr, "writing smoothed volume to %s...\n", fname) ;
      MRIwrite(mri_smooth, fname) ;
    }

    parms.n_averages = n_averages ;
    MRISprintTessellationStats(mris, stderr) ;
    if (mri_aseg)
    {
      fix_midline(mris, mri_aseg, mri_T1, hemi, GRAY_WHITE, 0) ;
    }
    if (mri_cover_seg)
    {
      MRI *mri_tmp, *mri_bin ;

      if (i == 0)
      {
        mri_bin = MRIclone(mri_T1, NULL) ;

        printf("creating distance transform volume from segmentation\n") ;
        if (mris->hemisphere == LEFT_HEMISPHERE)
        {
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_Cerebral_White_Matter) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_Thalamus_Proper) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_Caudate) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_Pallidum) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_Putamen) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_VentralDC) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_Lateral_Ventricle) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_Lesion) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_Accumbens_area) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_WM_hypointensities) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_non_WM_hypointensities) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Left_vessel) ;
        }
        else
        {
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_Cerebral_White_Matter) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_Thalamus_Proper) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_Caudate) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_Pallidum) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_Putamen) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_Lateral_Ventricle) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_Lesion) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_Accumbens_area) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_VentralDC) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_WM_hypointensities) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_non_WM_hypointensities) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Right_vessel) ;
        }
        MRIcopyLabel(mri_cover_seg, mri_bin, Brain_Stem) ;
        MRIcopyLabel(mri_cover_seg, mri_bin, Third_Ventricle) ;
        MRIcopyLabel(mri_cover_seg, mri_bin, WM_hypointensities) ;
        MRIbinarize(mri_bin, mri_bin, 1, 0, 1) ;
        mri_tmp =
          MRIdistanceTransform(mri_bin, NULL, 1, 20, DTRANS_MODE_SIGNED, NULL) ;
        // to be in same range as intensities:
        MRIscalarMul(mri_tmp, mri_tmp, (100.0/mri_bin->xsize)) ;
	if (mri_T1 == mri_T1_pial)
	  mri_T1_pial = mri_tmp ;
        MRIfree(&mri_T1) ;
        mri_T1 = mri_tmp ;
        MRISsetVals(mris, 0) ;   // target is 0 distance transform val
        MRIfree(&mri_bin) ;
      }
    }
    else if (flair_or_T2_name == NULL) // otherwise already done

    {
      double outside_hi = (max_border_white + max_gray_scale*max_gray) / (max_gray_scale+1.0) ;

      if (!FZERO(max_gray_scale))
	printf("setting outside hi = %2.1f (was %2.1f) in white surface deformation\n", outside_hi, max_border_white) ;
      MRIScomputeBorderValues(mris, mri_T1, mri_smooth,
                              MAX_WHITE, max_border_white, min_border_white,
                              min_gray_at_white_border,
                              outside_hi, current_sigma,
                              2*max_thickness, parms.fp, GRAY_WHITE, NULL, 0, parms.flags,mri_aseg) ;
      MRISfindExpansionRegions(mris) ;
    }
    else if (flairwhite)
    {
      MRI  *mri_flair = NULL ;
      char fname[STRLEN] ;
      
      if (orig_pial)
	strcpy(fname, orig_pial) ;
      else
	strcpy(fname, "pial") ;

      printf("reading initial pial vertex positions from %s...\n", fname) ;

      if (MRISreadPialCoordinates(mris, fname) != NO_ERROR)
	ErrorExit(Gerror, "reading orig pial positions failed") ;
      MRISremoveIntersections(mris) ;
      
      strcpy(fname, flair_or_T2_name) ;
      if (MGZ)
	strcat(fname, ".mgz");
      
      printf("repositioning white surface locations using  %s\n", fname) ;

      mri_flair = MRIread(fname) ;
      if (mri_flair == NULL)
	ErrorExit(ERROR_NOFILE, "%s: could not load flair volume %s", Progname, fname) ;

      {
	char fname[STRLEN] ;
	MRI *mri = compute_white_matter_intensities(mris, mri_flair, mri_T1, mri_aseg, 2, 2, NULL) ;
	if (orig_pial)
	{
	  printf("reading initial pial vertex positions from %s...\n", orig_pial) ;
	  
	  if (MRISreadPialCoordinates(mris, orig_pial) != NO_ERROR)
	  {
	    ErrorExit(Gerror, "reading orig pial positions failed") ;
	  }
	}
	mri = compute_white_matter_intensities(mris, mri_flair, mri_T1, mri_aseg, 2, 2, NULL) ;
	sprintf(fname, "%ch.interior.mgz", mris->hemisphere == LEFT_HEMISPHERE ? 'l' : 'r') ;
	MRIwrite(mri, fname) ;
	exit(0); ;
      }
      compute_white_target_locations(mris, mri_flair, mri_aseg,
				    3, 3,
				     CONTRAST_FLAIR, T2_min_inside, T2_max_inside, below_set, above_set, white_sigma) ;
    }
    if (vavgs)
    {
      fprintf
      (stderr,
       "averaging target values for %d iterations...\n",vavgs) ;
      MRISaverageMarkedVals(mris, vavgs) ;
      if (Gdiag_no > 0)
      {
        VERTEX *v ;
        v = &mris->vertices[Gdiag_no] ;
        fprintf
        (stderr,
         "v %d, target value = %2.1f, mag = %2.1f, dist=%2.2f, ripflag=%d\n",
         Gdiag_no, v->val, v->mean, v->d, v->ripflag) ;
      }
    }

    //    MRISunrip(mris) ;   bring midline back into surface

    /*
      there are frequently regions of gray whose intensity is fairly
      flat. We want to make sure the surface settles at the innermost
      edge of this region, so on the first pass, set the target
      intensities artificially high so that the surface will move
      all the way to white matter before moving outwards to seek the
      border (I know it's a hack, but it improves the surface in
      a few areas. The alternative is to explicitly put a gradient-seeking
      term in the cost functional instead of just using one to find
      the target intensities).
    */
#if 0
    if (!i)
    {
      parms.l_nspring = 1.0 ;
      MRISscaleVals(mris, 1.05) ;  /* move inwards on first pass */
    }
    else
    {
      parms.l_nspring = 0.0 ;
    }
#endif

    if (write_vals)
    {
      sprintf(fname, "./%s-white%2.2f.mgz", hemi, current_sigma) ;
      MRISwriteValues(mris, fname);
    }
    *(&old_parms) = *(&parms) ;
    parms.l_tspring *= spring_scale ; parms.l_nspring *= spring_scale ; parms.l_spring *= spring_scale ;
    parms.l_tspring = MIN(1.0,parms.l_tspring) ;
    parms.l_nspring = MIN(1.0, parms.l_nspring) ;
    parms.l_spring = MIN(1.0, parms.l_spring) ;
    MRISpositionSurface(mris, mri_T1, mri_smooth,&parms);
    old_parms.start_t = parms.start_t ;
    *(&parms) = *(&old_parms) ;

    if (add)
    {
      for (max_len = 1.5*8 ; max_len > 1 ; max_len /= 2)
        while (MRISdivideLongEdges(mris, max_len) > 0) {}
    }
    if (!n_averages)
    {
      break ;
    }
  }

  if (!nowhite)
  {
    MRISunrip(mris) ;
  }
  else /* read in previously generated white matter surface */
  {
    if (orig_white)
    {
      sprintf(fname, "%s%s", orig_white, suffix) ;
      printf("reading white vertex positions from %s...\n",
             orig_white) ;
      if (MRISreadVertexPositions(mris, fname) != NO_ERROR)
        ErrorExit(Gerror, "%s: could not read white matter surface.",
                  Progname) ;
      MRISremoveIntersections(mris) ;
    }
    else // read default white (something needs to be
      // read if nowhite was created)
    {
      // if you don't like the default, give an error message here and exit,
      // to force passing the -orig_white white
      sprintf(fname, "%s%s", white_matter_name, suffix) ;
      if (MRISreadVertexPositions(mris, fname) != NO_ERROR)
        ErrorExit(Gerror, "%s: could not read white matter surface.",
                  Progname) ;

    }
    MRIScomputeMetricProperties(mris) ;
  }


  if (mri_aseg) //update aseg using either generated or orig_white
  {
//    fix_midline(mris, mri_aseg, mri_T1, hemi, GRAY_CSF, fix_mtl) ;   //moved to later
    if (write_aseg_fname)
    {
      edit_aseg_with_surfaces(mris, mri_aseg) ;
      printf("writing corrected aseg to %s\n", write_aseg_fname) ;
      MRIwrite(mri_aseg, write_aseg_fname) ;
    }
  }

  // NJS HACK: if filename passed to -white is "NOWRITE", then dont write
  // the white, curv, area, and cortex.label files.  this is in lieu of
  // -nowhite not creating pial surfaces that match those created
  // w/o the -nowhite option.
  if (!nowhite && strcmp(white_matter_name,"NOWRITE"))
  {
    MRISremoveIntersections(mris) ;
    sprintf(fname,
            "%s/%s/surf/%s.%s%s%s",
            sdir, sname,hemi,white_matter_name,
            output_suffix,suffix);
    fprintf(stderr, "writing white matter surface to %s...\n", fname) ;
    MRISaverageVertexPositions(mris, smoothwm) ;
    MRISwrite(mris, fname) ;
    if (mri_aseg && label_cortex)
    {
      LABEL *lcortex, **labels ;
      int   n, max_l, max_n, nlabels ;

      //lcortex = MRIScortexLabel(mris, mri_aseg, MIN_NONCORTEX_VERTICES) ;
      lcortex = MRIScortexLabel(mris, mri_aseg, -1) ;
      if (Gdiag & DIAG_VERBOSE_ON)
      {
        sprintf(fname,
                "%s/%s/label/%s.%s%s%s_orig.label",
                sdir, sname,hemi,"cortex",
                output_suffix,suffix);
        printf("writing cortex label to %s...\n", fname) ;
        LabelWrite(lcortex, fname) ;
      }
      LabelErode(lcortex, mris, 4) ;
      if (Gdiag & DIAG_VERBOSE_ON)
      {
        sprintf(fname,
                "%s/%s/label/%s.%s%s%s_erode.label",
                sdir, sname,hemi,"cortex",
                output_suffix,suffix);
        printf("writing cortex label to %s...\n", fname) ;
        LabelWrite(lcortex, fname) ;
      }
      LabelDilate(lcortex, mris, 4, CURRENT_VERTICES) ;
      if (Gdiag & DIAG_VERBOSE_ON)
      {
        sprintf(fname,
                "%s/%s/label/%s.%s%s%s_dilate.label",
                sdir, sname,hemi,"cortex",
                output_suffix,suffix);
        printf("writing cortex label to %s...\n", fname) ;
        LabelWrite(lcortex, fname) ;
      }
      MRISclearMarks(mris) ;
      LabelMark(lcortex, mris) ;
      MRISsegmentMarked(mris, &labels, &nlabels, 1) ;
      max_n = 0 ;
      max_l = labels[0]->n_points ;
      for (n = 1 ; n < nlabels ; n++)
        if (labels[n]->n_points > max_l)
        {
          max_l = labels[n]->n_points ;
          max_n = n ;
        }
      for (n = 0 ; n < nlabels ; n++)
      {
        if (n != max_n)
        {
          LabelUnmark(labels[n], mris) ;
        }
        LabelFree(&labels[n]) ;
      }
      LabelFree(&lcortex) ;
      lcortex = LabelFromMarkedSurface(mris) ;

      sprintf(fname,
              "%s/%s/label/%s.%s%s%s.label",
              sdir, sname,hemi,"cortex",
              output_suffix,suffix);
      printf("writing cortex label to %s...\n", fname) ;
      LabelWrite(lcortex, fname) ;
      LabelFree(&lcortex) ;
    }

#if 0
    if (smoothwm > 0)
    {
      MRISaverageVertexPositions(mris, smoothwm) ;
      sprintf(fname,
              "%s/%s/surf/%s.%s%s",
              sdir, sname,hemi,SMOOTH_NAME,
              suffix);
      fprintf(stderr,
              "writing smoothed white matter surface to %s...\n",fname);
      MRISwrite(mris, fname) ;
    }
#endif

    if (create)   /* write out curvature and area files */
    {
      MRIScomputeMetricProperties(mris) ;
      MRIScomputeSecondFundamentalForm(mris) ;
      MRISuseMeanCurvature(mris) ;
      MRISaverageCurvatures(mris, curvature_avgs) ;
      sprintf(fname, "%s.curv%s%s",
              mris->hemisphere == LEFT_HEMISPHERE?"lh":"rh", output_suffix,
              suffix);
      fprintf(stderr, "writing smoothed curvature to %s\n", fname) ;
      MRISwriteCurvature(mris, fname) ;
      sprintf(fname, "%s.area%s%s",
              mris->hemisphere == LEFT_HEMISPHERE?"lh":"rh", output_suffix,
              suffix);
#if 1
      fprintf(stderr, "writing smoothed area to %s\n", fname) ;
      MRISwriteArea(mris, fname) ;
#endif
      MRISprintTessellationStats(mris, stderr) ;
    }
  }

  if (white_only)
  {
    msec = TimerStop(&then) ;
    fprintf(stderr,
            "refinement took %2.1f minutes\n", (float)msec/(60*1000.0f));
    MRIfree(&mri_T1);
    exit(0) ;
  }

  MRISsetVal2(mris, 0) ;   // will be marked for vertices near lesions

  MRISunrip(mris) ;
  if (mri_aseg) //update aseg using either generated or orig_white
    fix_midline(mris, mri_aseg, mri_T1, hemi, GRAY_CSF, fix_mtl) ;
  //////////////////////////////////////////////////////////////////
  // pial surface
  //////////////////////////////////////////////////////////////////

  parms.t = parms.start_t = 0 ;
  sprintf(parms.base_name, "%s%s%s", pial_name, output_suffix, suffix) ;
  parms.niterations = ngray ;
  MRISsaveVertexPositions(mris, ORIGINAL_VERTICES) ; /* save white-matter */
  parms.l_surf_repulse = l_surf_repulse ;

  MRISsetVals(mris, -1) ;  /* clear target intensities */

  if (smooth && !nowhite)
  {
    printf("smoothing surface for %d iterations...\n", smooth) ;
    MRISaverageVertexPositions(mris, smooth) ;
  }

  if (fill_interior)
  {
    MRI *mri_interior ;
    if (fill_interior== 255 && mri_T1_pial->type == MRI_UCHAR)
    {
      // see MRIcomputeBorderValues which changes all 255 to 0
      printf("!!!!!!!!!!!!!!! WARNING - 255 is a protected value for uchar volumes. Changing to 254... !!!!!!!!!!!!!!!!!!!!!!\n") ;
      fill_interior = 254 ;
    }
    mri_interior = MRIcopy(mri_T1_pial, NULL) ;
    MRIScopyVolGeomFromMRI(mris, mri_T1_pial) ;
    MRISfillInterior(mris, mri_T1_pial->xsize, mri_interior) ;
    MRImask(mri_T1_pial, mri_interior, mri_T1_pial, 1, fill_interior) ;
    printf("writing filled_interior.mgz\n") ;
    MRIwrite(mri_T1_pial, "filled_interior.mgz") ; 
    MRIfree(&mri_interior) ;
  }

#if 1
  if (orig_pial)
  {
    printf("reading initial pial vertex positions from %s...\n", orig_pial) ;

    if (longitudinal)
    {
      //save final white location into TMP_VERTICES (v->tx, v->ty, v->tz)
      MRISsaveVertexPositions(mris, TMP_VERTICES);
    }

    if (MRISreadVertexPositions(mris, orig_pial) != NO_ERROR)
    {
      ErrorExit(Gerror, "reading orig pial positions failed") ;
    }
    if (smooth_pial)
    {
      printf("smoothing pial surface for %d iterations before deformation\n", smooth_pial) ;
      MRISaverageVertexPositions(mris, smooth_pial) ;
    }
    MRISremoveIntersections(mris) ;
    MRISsaveVertexPositions(mris, PIAL_VERTICES) ;

    if (longitudinal)
    {
      //reset starting point to be between final white and orig pial
      int vno;
      VERTEX *v;
      //reset the starting position to be
      //slightly inside the orig_pial in the longitudinal case
      for (vno = 0; vno < mris->nvertices; vno++)
      {
        v = &mris->vertices[vno];
        if (v->ripflag)
        {
          continue;
        }
        // where tx ty tz is the TMP_VERTICES (final white)
        v->x = 0.75*v->x + 0.25*v->tx;
        v->y = 0.75*v->y + 0.25*v->ty;
        v->z = 0.75*v->z + 0.25*v->tz;
      }
    }
    MRIScomputeMetricProperties(mris) ; //shouldn't this be done whenever
    // orig_pial is used??? Maybe that's why the cross-intersection
    // was caused
  }
  /*    parms.l_convex = 1000 ;*/
  mri_T1 = mri_T1_pial ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_T1, "p.mgz") ;
  }
  if (dura_echo_name)
  {
#define MAX_VOLUMES 100
    char fname[STRLEN], fmt[STRLEN] ;
    MRI *mri_ratio, *mri_echos[MAX_VOLUMES] ;
    int  e ;

    printf("masking dura from pial surface locations\n") ;
    sprintf(parms.base_name, "%s%s%s", pial_name, output_suffix, suffix) ;
    for (e = 0 ; e < nechos ; e++)
    {
      if (e != 0 && e != nechos-1)
      {
        mri_echos[e] = NULL ;
        continue ;
      }
      sprintf(fmt, "%s/%s/mri/%s", sdir, sname, dura_echo_name) ;
      sprintf(fname, fmt, e) ;
      mri_echos[e] = MRIread(fname) ;
      if (mri_echos[e] == NULL)
        ErrorExit
        (ERROR_BADPARM,
         "%s: could not read %dth echo for dura localization from %s",
         Progname, e, fname) ;
      if (mri_echos[e]->type != MRI_FLOAT)
      {
	MRI *mri_tmp = MRIchangeType(mri_echos[e], MRI_FLOAT, 0, 1, 1) ;
	printf("changing echo voxel type to FLOAT\n") ;
	MRIfree(&mri_echos[e]) ;
	mri_echos[e] = mri_tmp ;
      }
      if (MRImatchDimensions(mri_T1, mri_echos[e]) == 0)
      {
	MRI *mri_tmp = MRIresample(mri_echos[e], mri_T1, SAMPLE_TRILINEAR) ;
	printf("echo not conformed - reslicing to match conformed volumes\n") ;
	MRIfree(&mri_echos[e]) ;
	mri_echos[e] = mri_tmp ;
      }
    }

    mri_ratio = MRIdivide(mri_echos[0], mri_echos[nechos-1], NULL) ;
    if (dura_thresh < 0)
      thresh = compute_brain_thresh(mris, mri_ratio, nsigma) ;
    else
      thresh = dura_thresh ;
    mri_mask = mri_ratio ;
    if (Gdiag & DIAG_WRITE)
    {
      char fname[STRLEN] ;
      sprintf(fname, "%s_ratio.mgz", parms.base_name) ;
      printf("writing dura ratio image to %s...\n", fname) ;
      MRIwrite(mri_ratio, fname) ;
    }
    parms.mri_dura = mri_ratio ;
    parms.dura_thresh = thresh ;
    parms.l_dura = 10 ;
    printf("setting dura threshold to %2.3f\n", thresh) ;

    for (e = 0 ; e < nechos ; e++)
      if (mri_echos[e])
      {
        MRIfree(&mri_echos[e]) ;
      }
  }
  else
  {
    mri_mask = NULL ;
  }
#endif

  sprintf(parms.base_name, "%s%s%s", pial_name, output_suffix, suffix) ;
  fprintf(stderr, "repositioning cortical surface to gray/csf boundary.\n") ;
  parms.l_repulse = l_repulse ;
  MRISclearMark2s(mris) ;  // will be used to keep track of which vertices found pial surface in previous cycle
//  for (j = 0 ; j <= 0 ; parms.l_intensity *= 2, j++)  /* only once for now */

  for (j = 0 ; j < 1 ; j++)  /* only once for now */
  {
    current_sigma = pial_sigma ;
    for (n_averages = max_pial_averages, i = 0 ;
         n_averages >= min_pial_averages ;
         n_averages /= 2, current_sigma /= 2, i++)
    {
      if (flair_or_T2_name)
      {
        MRI  *mri_flair = NULL ;
        LABEL             **labels ;
        int               nlabels ;
        char             fname[STRLEN] ;

        strcpy(fname, flair_or_T2_name) ;
        if (MGZ)
        {
          strcat(fname, ".mgz");
        }

        printf("repositioning pial surface locations using  %s\n", fname) ;
        if (mri_flair)  // first time
        {
          MRIfree(&mri_flair) ;
        }

	if (n_averages <= min_pial_averages)
	  parms.l_repulse = 0 ;  // for final round turn off the 'unpinching' term

        mri_flair = MRIread(fname) ;
        if (mri_flair == NULL)
        {
          ErrorExit(ERROR_NOFILE, "%s: could not load flair volume %s", Progname, fname) ;
        }


	if (mri_aseg && erase_cerebellum)
	{
	  MRImaskLabel(mri_flair, mri_flair, mri_aseg, Left_Cerebellum_White_Matter, 0) ;
	  MRImaskLabel(mri_flair, mri_flair, mri_aseg,  Left_Cerebellum_Cortex, 0) ;
	  MRImaskLabel(mri_flair, mri_flair, mri_aseg,  Right_Cerebellum_White_Matter, 0) ;
	  MRImaskLabel(mri_flair, mri_flair, mri_aseg, Right_Cerebellum_Cortex, 0) ;
	}

	if (mri_aseg && erase_brainstem)
	{
	  MRImaskLabel(mri_flair, mri_flair, mri_aseg, Brain_Stem, 0) ;
	  MRImaskLabel(mri_flair, mri_flair, mri_aseg,  Left_VentralDC, 0) ;
	  MRImaskLabel(mri_flair, mri_flair, mri_aseg,  Right_VentralDC, 0) ;
	}

        if (read_pinch_fname)
        {
          char marked_fname[STRLEN] ;

          MRISreadVertexPositions(mris, read_pinch_fname) ;
          sprintf(marked_fname, "%s.marked.mgz", read_pinch_fname) ;
          MRISreadMarked(mris, marked_fname) ;
          MRISsegmentMarked(mris, &labels, &nlabels, 1) ;
        }
        else if (unpinch)
        {
          if (mri_aseg && erase_cerebellum)
          {
            MRImaskLabel(mri_flair, mri_flair, mri_aseg, Left_Cerebellum_White_Matter, 0) ;
            MRImaskLabel(mri_flair, mri_flair, mri_aseg, Left_Cerebellum_Cortex, 0) ;
            MRImaskLabel(mri_flair, mri_flair, mri_aseg, Right_Cerebellum_White_Matter, 0) ;
            MRImaskLabel(mri_flair, mri_flair, mri_aseg, Right_Cerebellum_Cortex, 0) ;
          }
          find_and_mark_pinched_regions(mris, mri_flair,
                                        nsigma_below, nsigma_above) ;
          if (MRIScountMarked(mris, 1) > 0)
          {
            INTEGRATION_PARMS saved_parms ;

            MRISsegmentMarked(mris, &labels, &nlabels, 1) ;
            *(&saved_parms) = *(&parms) ;

            printf("%d vertices found in imminent self-intersecting "
                   "regions, deforming to remove...\n",
                   MRIScountMarked(mris,1));
            MRISinvertMarks(mris) ;
            MRISerodeMarked(mris, 2) ;
            MRISripMarked(mris) ;

            MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
            MRISremoveCompressedRegions(mris, .5) ;
            MRISwrite(mris, "after_uncompress") ;
            MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
            MRISsoapBubbleVertexPositions(mris, 25) ;
            MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
            MRISwrite(mris, "after_soap") ;

#if 0
            parms.l_spring = 0 ;
            parms.l_convex = 0 ;
            parms.l_parea = 1 ;
            parms.l_location = .1 ;
            parms.l_curv = 0 ;
            parms.l_nltspring = 1 ;
            parms.n_averages = max_pial_averages ;
            parms.sigma = pial_sigma ;
            parms.l_intensity = parms.l_tspring = parms.l_nspring = 0 ;
//      parms.flags |= IPFLAG_NO_SELF_INT_TEST ;
            MRISpositionSurface(mris, mri_T1, mri_T1,&parms);
            printf("surface pinch removeal complete\n") ;

            start_t = parms.start_t ;
            *(&parms) = *(&saved_parms) ;
            parms.start_t = start_t ;
#endif
            MRISunrip(mris) ;
            MRISremoveIntersections(mris) ;
          }

          MRISwriteMarked(mris, "distant.mgz") ;
          MRIScomputeMetricProperties(mris) ;
        }
        else
        {
          nlabels = 0 ;
        }

	MRISremoveIntersections(mris) ;
        compute_pial_target_locations(mris, mri_flair,
                                      labels, nlabels,
                                      contrast_type, mri_aseg, T2_min_inside, T2_max_inside, T2_min_outside, T2_max_outside,max_outward_dist,Ghisto_min_inside_peak_pct, Ghisto_min_outside_peak_pct, wm_weight, mri_T1_pial) ;
	parms.l_location /= spring_scale ; 

        if (Gdiag & DIAG_WRITE)
        {
          char fname[STRLEN] ;
	  static int n = 0 ;
          MRISsaveVertexPositions(mris, TMP_VERTICES) ;
          MRISrestoreVertexPositions(mris, TARGET_VERTICES) ;
          sprintf(fname, "%s.flair.target.%3.3d", parms.base_name, n++) ;
          printf("writing surface targets to %s\n", fname) ;
          MRISwrite(mris, fname) ;
          MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
        }
//  parms.l_histo = 1 ;
        parms.l_intensity = 0 ;
	if (i == 0) // only the first time
	{
#if 0
//	  parms.l_nspring *= 0.1 ;
//	  parms.l_tspring *= 0.1 ;
//	  parms.l_curv *= 0.1 ;
	  parms.l_nspring *= 0.25 ;
	  parms.l_tspring *= 0.25 ;
	  parms.l_curv *= 0.25 ;
#endif
	}

//  parms.l_max_spring = .1 ;
//  parms.l_parea = .05 ;
      }

      if (T2_name)
      {
        MRI  *mri_T2 = NULL ;
        int n = 0 ;
        LABEL             **labels ;
        int               nlabels ;

        printf("removing non-brain from pial surface locations"
               " using T2 volume %s\n", T2_name) ;
        if (mri_T2)  // first time
        {
          MRIfree(&mri_T2) ;
        }
        mri_T2 = MRIread(T2_name) ;
        if (mri_T2 == NULL)
        {
          ErrorExit(ERROR_NOFILE,
                    "%s: could not load T2 volume %s", Progname, T2_name) ;
        }


        if (read_pinch_fname)
        {
          char marked_fname[STRLEN] ;

          MRISreadVertexPositions(mris, read_pinch_fname) ;
          sprintf(marked_fname, "%s.marked.mgz", read_pinch_fname) ;
          MRISreadMarked(mris, marked_fname) ;
          MRISsegmentMarked(mris, &labels, &nlabels, 1) ;
        }
        else if (unpinch)
        {
          if (mri_aseg && erase_cerebellum)
          {
            MRImaskLabel(mri_T2, mri_T2, mri_aseg, Left_Cerebellum_White_Matter, 0) ;
            MRImaskLabel(mri_T2, mri_T2, mri_aseg, Left_Cerebellum_Cortex, 0) ;
            MRImaskLabel(mri_T2, mri_T2, mri_aseg, Right_Cerebellum_White_Matter, 0) ;
            MRImaskLabel(mri_T2, mri_T2, mri_aseg, Right_Cerebellum_Cortex, 0) ;
          }
          find_and_mark_pinched_regions(mris, mri_T2,
                                        nsigma_below, nsigma_above) ;
          if (MRIScountMarked(mris, 1) > 0)
          {
            INTEGRATION_PARMS saved_parms ;

            MRISsegmentMarked(mris, &labels, &nlabels, 1) ;
            *(&saved_parms) = *(&parms) ;

            printf("%d vertices found in imminent self-intersecting regions, deforming to remove...\n", MRIScountMarked(mris,1));
            MRISinvertMarks(mris) ;
            MRISerodeMarked(mris, 2) ;
            MRISripMarked(mris) ;

            MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
            MRISremoveCompressedRegions(mris, .5) ;
            MRISwrite(mris, "after_uncompress") ;
            MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
            MRISsoapBubbleVertexPositions(mris, 25) ;
            MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
            MRISwrite(mris, "after_soap") ;

            MRISunrip(mris) ;
            MRISremoveIntersections(mris) ;
          }

          MRISwriteMarked(mris, "distant.mgz") ;
          MRIScomputeMetricProperties(mris) ;
        }
        else
        {
          nlabels = 0 ;
        }
        if (0)
        {
          compute_pial_target_locations(mris, mri_T2,
                                        labels, nlabels,
                                        contrast_type, mri_aseg, T2_min_inside, T2_max_inside, T2_min_outside, T2_max_outside,max_outward_dist,Ghisto_min_inside_peak_pct,Ghisto_min_outside_peak_pct, wm_weight, mri_T1_pial) ;
        }

        if (Gdiag & DIAG_WRITE)
        {
          char fname[STRLEN] ;
          MRISsaveVertexPositions(mris, TMP_VERTICES) ;
          MRISrestoreVertexPositions(mris, TARGET_VERTICES) ;
          sprintf(fname, "%s.T2.target.%3.3d", parms.base_name, n++) ;
          printf("writing surface targets to %s\n", fname) ;
          MRISwrite(mris, fname) ;
          MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
        }
//        parms.l_histo = 1 ;
        parms.l_location = .5 ;
        parms.l_intensity = 0 ;
#if 0
        parms.l_nspring *= 0.1 ;
        parms.l_tspring *= 0.1 ;
        parms.l_curv *= 0.1 ;
#endif
      }
      parms.sigma = current_sigma ;
      mri_kernel = MRIgaussian1d(current_sigma, 100) ;
      fprintf(stderr, "smoothing T1 volume with sigma = %2.3f\n",
              current_sigma) ;
      parms.n_averages = n_averages ;
      parms.l_tsmooth = l_tsmooth ;
      /*
        replace bright stuff such as eye sockets with 255.
        Simply zeroing it out
        would make the border always go through the sockets,
        and ignore subtle
        local minima in intensity at the border of the sockets.
        Will set to 0
        after border values have been computed so that it
        doesn't mess up gradients.
      */
      if (fill_interior == 0)
      {
	MRImask(mri_T1, mri_labeled, mri_T1, BRIGHT_LABEL, 255) ;
	MRImask(mri_T1, mri_labeled, mri_T1, BRIGHT_BORDER_LABEL, MID_GRAY) ;
      }
      if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
      {
        MRIwrite(mri_T1, "pial_masked.mgz") ; 
     }
      if (mri_cover_seg)
      {
        MRI *mri_tmp, *mri_bin ;

        if (i == 0)
        {
          mri_bin = MRIclone(mri_T1, NULL) ;

          printf("creating distance transform volume from segmentation\n") ;
          if (mris->hemisphere == LEFT_HEMISPHERE)
          {
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Cerebral_White_Matter) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Cerebral_Cortex) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Thalamus_Proper) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Caudate) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Pallidum) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Putamen) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_VentralDC) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Lateral_Ventricle) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Lesion) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_Accumbens_area) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_WM_hypointensities) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_non_WM_hypointensities) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Left_vessel) ;
          }
          else
          {
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Cerebral_Cortex) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Cerebral_White_Matter) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Thalamus_Proper) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Caudate) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Pallidum) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Putamen) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Lateral_Ventricle) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Lesion) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_Accumbens_area) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_VentralDC) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_WM_hypointensities) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_non_WM_hypointensities) ;
            MRIcopyLabel(mri_cover_seg, mri_bin, Right_vessel) ;
          }
          MRIcopyLabel(mri_cover_seg, mri_bin, Brain_Stem) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, Third_Ventricle) ;
          MRIcopyLabel(mri_cover_seg, mri_bin, WM_hypointensities) ;
          MRIbinarize(mri_bin, mri_bin, 1, 0, 1) ;
          mri_tmp = MRIdistanceTransform(mri_bin, NULL, 1, 20,
                                         DTRANS_MODE_SIGNED, NULL) ;
          MRIscalarMul(mri_tmp, mri_tmp,
                       (5/mri_tmp->xsize)) ;// same range as intensities
	  if (Gdiag & DIAG_WRITE)
	  {
              sprintf(fname, "%s.cseg.dtrans%s%s.mgz", hemi, output_suffix, suffix) ;
              printf("writing dtrans volume to %s\n", fname) ;
              MRIwrite(mri_tmp, fname) ;
	  }
//          parms.grad_dir = 1 ;
          MRIfree(&mri_T1) ;
          mri_T1 = mri_tmp ;
          MRISsetVals(mris, 0) ;   // target is 0 distance transform val
          MRIfree(&mri_bin) ;
#if 0
	  if (nowhite)
	  {
	    parms.mri_brain = mri_T1 ;
	    MRISexpandSurface(mris, 1, &parms, 0, 1) ;
	  }
#endif
        }
      }
      else if (flair_or_T2_name == NULL) // otherwise already done
      {
        MRIScomputeBorderValues
        (mris, mri_T1, mri_smooth, max_gray,
         max_gray_at_csf_border, min_gray_at_csf_border,
         min_csf,(max_csf+max_gray_at_csf_border)/2,
         current_sigma, 2*max_thickness, parms.fp,
         GRAY_CSF, mri_mask, thresh, parms.flags,mri_aseg) ;
        MRImask(mri_T1, mri_labeled, mri_T1, BRIGHT_LABEL, 0) ;
      }
      if (!FZERO(pial_target_offset))
      {
	MRISaddToValues(mris, pial_target_offset) ;
	printf("adding %2.2f to pial targets\n", pial_target_offset);
      }
      {
        int ii, vno, n, vtotal ;
        VERTEX *v ;
        for (ii = 0; ii < pial_num ; ii++)
        {
          vno = pial_vnos[ii] ;
          v = &mris->vertices[vno] ;
          v->val = pial_vals[ii] ;
          v->marked = 1 ;
	  if (v->val2 > 0)
	    v->val2 = 3*current_sigma ;
	  else
	    v->val2 = current_sigma ;

          vtotal = 0 ;
          switch (pial_nbrs)
          {
          case 1:
            vtotal = v->vnum ;
            break ;
          case 2:
            vtotal = v->v2num ;
            break ;
          case 3:
            vtotal = v->v3num ;
            break ;
          default:
            break ;
          }
          for (n = 0 ; n < vtotal ; n++)
          {
            mris->vertices[v->v[n]].val = pial_vals[ii] ;
            mris->vertices[v->v[n]].marked = 1 ;
            mris->vertices[v->v[n]].val2 = current_sigma ;
          }
        }
      }
      if (pial_blur_sigma>0)
      {
	MRI *mri_tmp ;
	printf("blurring input image with sigma = %2.3f\n", pial_blur_sigma) ;
	mri_kernel = MRIgaussian1d(pial_blur_sigma, 100) ;
	mri_tmp = MRIconvolveGaussian(mri_T1_pial, NULL, mri_kernel) ;
	MRIfree(&mri_kernel) ; MRIfree(&mri_T1_pial) ;
	mri_T1_pial = mri_T1 = mri_tmp ;
	MRIwrite(mri_T1_pial, "pblur.mgz") ;
      }

      if (vavgs)
      {
        fprintf
        (stderr,
         "averaging target values for %d iterations...\n",vavgs) ;
        MRISaverageMarkedVals(mris, vavgs) ;
        if (Gdiag_no >= 0)
        {
          VERTEX *v ;
          v = &mris->vertices[Gdiag_no] ;
          fprintf
          (stderr,
           "v %d, target value = %2.1f, mag = %2.1f, dist=%2.2f, ripflag=%d\n",
           Gdiag_no, v->val, v->mean, v->d, v->ripflag) ;
        }
      }

      if (write_vals)
      {
        sprintf(fname, "./%s-gray%2.2f.mgz", hemi, current_sigma) ;
        MRISwriteValues(mris, fname) ;
      }
      if (!mri_smooth)
      {
        mri_smooth = MRIcopy(mri_T1, NULL) ;
      }
#if 1
      if (j == 0 && i == 0 && orig_pial == NULL)  // first time increase smoothness to prevent pinching when pushing out from white surface
      {
#if 1
	INTEGRATION_PARMS  saved_parms ;
	int k, start_t ;

	printf("perforing initial smooth deformation to move away from white surface\n") ;
	*(&saved_parms) = *(&parms) ;
//	parms.l_intensity /= 5 ;
	parms.dt /= 10 ;   // take small steps to unkink pinched areas
//	parms.l_spring = 1 ;
	parms.niterations = 10 ;
	parms.l_surf_repulse /= 5 ;

	for (k = 0 ; k < 3 ; k++)
	{
	  *(&old_parms) = *(&parms) ;
	  parms.l_tspring *= spring_scale ; parms.l_nspring *= spring_scale ; parms.l_spring *= spring_scale ;
	  parms.l_tspring = MIN(1.0,parms.l_tspring) ;
	  parms.l_nspring = MIN(1.0, parms.l_nspring) ;
	  parms.l_spring = MIN(1.0, parms.l_spring) ;
	  MRISpositionSurface(mris, mri_T1, mri_smooth,&parms);
	  old_parms.start_t = parms.start_t ;
	  *(&parms) = *(&old_parms) ;
	  if (!FZERO(parms.l_intensity))
	    MRIScomputeBorderValues
	      (mris, mri_T1, mri_smooth, max_gray,
	       max_gray_at_csf_border, min_gray_at_csf_border,
	       min_csf,(max_csf+max_gray_at_csf_border)/2,
	       current_sigma, 2*max_thickness, parms.fp,
	       GRAY_CSF, mri_mask, thresh, parms.flags,mri_aseg) ;
	}
	start_t = parms.start_t ;
	*(&parms) = *(&saved_parms) ;
	parms.start_t = start_t ;
	parms.l_surf_repulse = l_surf_repulse ;
#else
	parms.l_surf_repulse /= 5 ;
#endif
      }
#endif
      *(&old_parms) = *(&parms) ;
      parms.l_tspring *= spring_scale ; parms.l_nspring *= spring_scale ; parms.l_spring *= spring_scale ;
      parms.l_tspring = MIN(1.0,parms.l_tspring) ;
      parms.l_nspring = MIN(1.0, parms.l_nspring) ;
      parms.l_spring = MIN(1.0, parms.l_spring) ;
      MRISpositionSurface(mris, mri_T1, mri_smooth,&parms);
      old_parms.start_t = parms.start_t ;
      *(&parms) = *(&old_parms) ;

      if (parms.l_location > 0)
      {
	int vno ;
	VERTEX *v ;
	double dist ;

	for (vno = 0 ; vno < mris->nvertices ; vno++)
	{
	  v = &mris->vertices[vno] ;
	  dist = sqrt(SQR(v->targx - v->x) + SQR(v->targy-v->y) + SQR(v->targz-v->z)) ;
	  v->cropped = nint((float)v->cropped * dist) ;
	}
	if (Gdiag_no > 0)
	  MRISwriteCropped(mris, "cropped.mgz") ;
	DiagBreak() ;
	MRISsetCroppedToZero(mris) ;
      }


      /*    parms.l_nspring = 0 ;*/
      if (!n_averages)
      {
        break ;
      }
    }
  }

  MRISremoveIntersections(mris) ;
  sprintf(fname, "%s/%s/surf/%s.%s%s%s", sdir, sname, hemi, pial_name,
          output_suffix, suffix) ;
  fprintf(stderr, "writing pial surface to %s...\n", fname) ;
  MRISwrite(mris, fname) ;
  if (create)   /* write out curvature and area files */
  {
    MRIScomputeMetricProperties(mris) ;
    MRIScomputeSecondFundamentalForm(mris) ;
    MRISuseMeanCurvature(mris) ;
    MRISaverageCurvatures(mris, curvature_avgs) ;
    sprintf(fname, "%s.curv.pial%s%s",
            mris->hemisphere == LEFT_HEMISPHERE?"lh":"rh", output_suffix,
            suffix);
    fprintf(stderr, "writing smoothed curvature to %s\n", fname) ;
    MRISwriteCurvature(mris, fname) ;
    sprintf(fname, "%s.area.pial%s%s",
            mris->hemisphere == LEFT_HEMISPHERE?"lh":"rh", output_suffix,
            suffix);
#if 1
    fprintf(stderr, "writing smoothed area to %s\n", fname) ;
    MRISwriteArea(mris, fname) ;
#endif
    MRISprintTessellationStats(mris, stderr) ;
  }

  if (in_out_in_flag)
  {
    sprintf(parms.base_name, "%s%s%s",
            white_matter_name, output_suffix, suffix) ;
    MRIScomputeMetricProperties(mris) ;    /* recompute surface normals */
    MRISstoreMetricProperties(mris) ;
    MRISsaveVertexPositions(mris, TMP_VERTICES) ;
    fprintf(stderr,
            "repositioning cortical surface to gray/white boundary\n");

    MRISsetVals(mris, -1) ;  /* clear white matter intensities */

    current_sigma = white_sigma ;
    MRImask(mri_T1, mri_labeled, mri_T1, BRIGHT_LABEL, 0) ;
    for (n_averages = max_white_averages, i = 0 ;
         n_averages >= min_white_averages ;
         n_averages /= 2, current_sigma /= 2, i++)
    {
      if (nowhite)
      {
        break ;
      }

      parms.sigma = current_sigma ;
      mri_kernel = MRIgaussian1d(current_sigma, 100) ;
      fprintf(stderr, "smoothing T1 volume with sigma = %2.3f\n",
              current_sigma) ;
      if (!mri_smooth)
      {
        mri_smooth = MRIclone(mri_T1, NULL) ;
      }
#if 0
      MRIconvolveGaussian(mri_T1, mri_smooth, mri_kernel) ;
#endif
      MRIfree(&mri_kernel) ;
      if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
      {
        char fname[STRLEN] ;
        sprintf(fname, "sigma%.0f.mgz", current_sigma) ;
        fprintf(stderr, "writing smoothed volume to %s...\n", fname) ;
        MRIwrite(mri_smooth, fname) ;
      }

      parms.n_averages = n_averages ;
      MRISprintTessellationStats(mris, stderr) ;
      if (flair_or_T2_name == NULL) // otherwise already done
        MRIScomputeBorderValues
        (mris, mri_T1, mri_smooth, MAX_WHITE,
         max_border_white, min_border_white,
         min_gray_at_white_border, max_border_white /*max_gray*/,
         current_sigma, 2*max_thickness, parms.fp,
         GRAY_WHITE, NULL, 0, parms.flags, mri_aseg) ;
      MRISfindExpansionRegions(mris) ;
      if (vavgs)
      {
        fprintf
        (stderr,
         "averaging target values for %d iterations...\n",vavgs);
        MRISaverageMarkedVals(mris, vavgs) ;
        if (Gdiag_no > 0)
        {
          VERTEX *v ;
          v = &mris->vertices[Gdiag_no] ;
          fprintf
          (stderr,
           "v %d, target value = %2.1f, mag = %2.1f, dist=%2.2f, ripflag=%d\n",
           Gdiag_no, v->val, v->mean, v->d, v->ripflag) ;
        }
      }

      if (write_vals)
      {
        sprintf(fname, "./%s-white%2.2f.mgz", hemi, current_sigma) ;
        MRISwriteValues(mris, fname) ;
      }
      MRISpositionSurface(mris, mri_T1, mri_smooth,&parms);
      if (parms.l_location > 0)
      {
	int vno ;
	VERTEX *v ;
	double dist ;

	for (vno = 0 ; vno < mris->nvertices ; vno++)
	{
	  v = &mris->vertices[vno] ;
	  dist = sqrt(SQR(v->targx - v->x) + SQR(v->targy-v->y) + SQR(v->targz-v->z)) ;
	  v->cropped = nint((float)v->cropped * dist) ;
	}
	if (Gdiag_no > 0)
	  MRISwriteCropped(mris, "cropped.mgz") ;
	DiagBreak() ;
	MRISsetCroppedToZero(mris) ;
      }
      if (!n_averages)
      {
        break ;
      }
    }
    MRISsaveVertexPositions(mris, ORIGINAL_VERTICES) ; /* gray/white surface */
    MRISrestoreVertexPositions(mris, TMP_VERTICES) ;  /* pial surface */
    MRISremoveIntersections(mris) ;
    sprintf(fname, "%s/%s/surf/%s.%s2%s", sdir, sname,hemi,white_matter_name,
            suffix);
    fprintf(stderr, "writing gray/white surface to %s...\n", fname) ;
    MRISwrite(mris, fname) ;
  }
  MRIfree(&mri_T1);

  /*  if (!(parms.flags & IPFLAG_NO_SELF_INT_TEST))*/
  {
    fprintf(stderr, "measuring cortical thickness...\n") ;
    if (longitudinal)
    {
      MRISmeasureCorticalThickness(mris, nbhd_size, 5.0) ;
    }
    else
    {
      MRISmeasureCorticalThickness(mris, nbhd_size, max_thickness) ;
    }

    fprintf(stderr,
            "writing cortical thickness estimate to 'thickness' file.\n") ;
    sprintf(fname, "thickness%s%s", output_suffix, suffix) ;
    MRISwriteCurvature(mris, fname) ;

    /* at this point, the v->curv slots contain the cortical surface. Now
       move the white matter surface out by 1/2 the thickness as an estimate
       of layer IV.
    */
    if (graymid)
    {
      MRISsaveVertexPositions(mris, TMP_VERTICES) ;
      mrisFindMiddleOfGray(mris) ;
      sprintf(fname, "%s/%s/surf/%s.%s%s", sdir, sname, hemi, GRAYMID_NAME,
              suffix) ;
      fprintf(stderr, "writing layer IV surface to %s...\n", fname) ;
      MRISwrite(mris, fname) ;
      MRISrestoreVertexPositions(mris, TMP_VERTICES) ;
    }
  }

  msec = TimerStop(&then) ;
  fprintf(stderr,"positioning took %2.1f minutes\n", (float)msec/(60*1000.0f));

  exit(0) ;
  return(0) ;  /* for ansi */
}
/*----------------------------------------------------------------------
  Parameters:

  Description:
  ----------------------------------------------------------------------*/
static int
get_option(int argc, char *argv[])
{
  int  nargs = 0 ;
  char *option ;

  option = argv[1] + 1 ;            /* past '-' */
  if (!stricmp(option, "-help")||!stricmp(option, "-usage"))
  {
    print_help() ;
  }
  else if (!stricmp(option, "-version"))
  {
    print_version() ;
  }
  else if (!stricmp(option, "nocerebellum") || !stricmp(option, "-erasecerebellum") || !stricmp(option, "erase_cerebellum"))
  {
    erase_cerebellum = 1 ;
    printf("erasing cerebellum in aseg before deformation\n") ;
  }
  else if (!stricmp(option, "nobrainstem") || !stricmp(option, "-erasebrainstem") || !stricmp(option, "erase_brainstem"))
  {
    erase_brainstem = 1 ;
    printf("erasing brainstem in aseg before deformation\n") ;
  }
  else if (!stricmp(option, "nbrs"))
  {
    nbrs = atoi(argv[2]) ;
    fprintf(stderr,  "using neighborhood size = %d\n", nbrs) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "min_peak_pct"))
  {
    Ghisto_min_inside_peak_pct = atof(argv[2]) ;
    Ghisto_min_outside_peak_pct = atof(argv[3]) ;
    fprintf(stderr,  "using min peak pct %2.2f and %2.2f (default=%2.2f) to find bounds of GM range\n",
	    Ghisto_min_inside_peak_pct, Ghisto_min_outside_peak_pct, MIN_PEAK_PCT) ;
    nargs = 2 ;
  }
  else if (!stricmp(option, "repulse"))
  {
    l_repulse = atof(argv[2]) ;
    fprintf(stderr,  "setting l_repulse = %2.2f\n", l_repulse) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "max_gray_scale"))
  {
    max_gray_scale = atof(argv[2]) ;
    fprintf(stderr,  "setting max_gray_scale (mgs) = %2.1f in calculation of outside_hi = (max_border_white + mgs*max_gray) / (mgs+1.0)\n", max_gray_scale) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "T2_min_inside"))
  {
    T2_min_inside = atof(argv[2]) ;
    fprintf(stderr,  "using min interior T2 gray matter threshold %2.1f\n", T2_min_inside) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "T2_min"))
  {
    T2_min_outside = T2_min_inside = atof(argv[2]) ;
    fprintf(stderr,  "using min T2 gray matter (interior and exterior) threshold %2.1f\n", T2_min_inside) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "max_out"))
  {
    max_outward_dist = atof(argv[2]) ;
    fprintf(stderr,  "using max outward dist %2.1f\n", max_outward_dist) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "T2_max_inside"))
  {
    T2_max_inside = atof(argv[2]) ;
    fprintf(stderr,  "using max T2 gray matter (interior and exterior) threshold %2.1f\n", T2_max_inside) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "T2_max_outside"))
  {
    T2_max_outside = atof(argv[2]) ;
    fprintf(stderr,  "using max T2 gray matter (interior and exterior) threshold %2.1f\n", T2_max_inside) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "T2_min_outside"))
  {
    T2_min_outside = atof(argv[2]) ;
    fprintf(stderr,  "using min T2 exterior gray matter threshold %2.1f\n", T2_min_outside) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "T2_max_outside"))
  {
    T2_max_outside = atof(argv[2]) ;
    fprintf(stderr,  "using max T2 exterior gray matter threshold %2.1f\n", T2_max_outside) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "grad_dir"))
  {
    int i = atoi(argv[2]) ;
    parms.grad_dir =  i < 0 ? -1 : i > 0 ? 1 : 0 ;
    fprintf(stderr,  "setting grad dir to %d\n", parms.grad_dir) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "ct"))
  {
    printf("reading color table from %s\n", argv[2]) ;
    ctab = CTABreadASCII(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "soap"))
  {
    parms.smooth_intersections = 1 ;
    printf("using soap bubble smoothing to remove vertex intersections\n") ;
  }
  else if (!stricmp(option, "read_pinch"))
  {
    read_pinch_fname = argv[2] ;
    printf("reading pinch initialization from %s\n", read_pinch_fname) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "pial_offset"))
  {
    pial_target_offset = atof(argv[2]) ;
    fprintf(stderr,  "offseting pial target vals by %2.2f\n",
            pial_target_offset) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "white_offset"))
  {
    white_target_offset = atof(argv[2]) ;
    fprintf(stderr,  "offseting white target vals by %2.0f\n",
            white_target_offset) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "both"))
  {
    remove_contra = 0 ;
    fprintf(stderr,  "not removing contralateral hemi\n") ;
  }
  else if (!stricmp(option, "nsigma") || !stricmp(option, "nsigmas"))
  {
    nsigma = atof(argv[2]) ;
    fprintf(stderr,
            "using dura threshold of %2.2f sigmas from mean (default=2)\n",
            nsigma) ;
    nargs = 1;
  }
  else if (!stricmp(option, "dura_thresh") || !stricmp(option, "dura+thresh"))
  {
    dura_thresh = atof(argv[2]) ;
    fprintf(stderr, "setting dura threshold to %2.2f instead of estimating it\n", dura_thresh) ;
    nargs = 1;
  }
  else if (!stricmp(option, "nsigma_above") ||
           !stricmp(option, "nsigmas_above"))
  {
    above_set = 1 ;
    nsigma_above = atof(argv[2]) ;
    fprintf(stderr,
            "using T2 threshold of %2.2f sigmas above the mean (default=2)\n",
            nsigma_above) ;
    nargs = 1;
  }
  else if (!stricmp(option, "wm_weight"))
  {
    wm_weight = atof(argv[2]) ;
    fprintf(stderr,
            "using wm weight to compute T2 threshold of %2.2f (default=3)\n", wm_weight) ;
    nargs = 1;
  }
  else if (!stricmp(option, "nsigma_below") ||
           !stricmp(option, "nsigmas_below"))
  {
    below_set = 1 ;
    nsigma_below = atof(argv[2]) ;
    fprintf(stderr,
            "using T2 threshold of %2.2f sigmas below the mean (default=2)\n",
            nsigma_below) ;
    nargs = 1;
  }
  else if (!stricmp(option, "dura"))
  {
    dura_echo_name = argv[2] ;
    nechos = atoi(argv[3]) ;
    fprintf(stderr,
            "detecting dura using %d echos from %s\n",
            nechos, dura_echo_name) ;
    nargs = 2 ;
  }
  else if (!stricmp(option, "T2dura") || !stricmp(option, "T2"))
  {
    contrast_type = CONTRAST_T2 ;
    parms.l_location = 0.5 ;
    parms.l_nspring = 1.0 ;
    parms.l_curv = 0.5 ;
    parms.check_tol = 1 ;
    flair_or_T2_name = argv[2] ;
    wm_weight = 3 ;


    T2_max_inside = 300 ;
    T2_min_inside = 110 ;
    T2_min_outside = 130 ;  
    T2_max_outside = 300 ;
    max_pial_averages = 4 ;
    max_outward_dist = 3 ;
    Ghisto_min_inside_peak_pct = 0.01 ;
    Ghisto_min_outside_peak_pct = 0.5 ;
    l_repulse = .025 ;

    fprintf(stderr,
            "refining pial surfaces placement using T2 volume %s\n", 
            flair_or_T2_name) ;
    parms.check_tol = 1 ;
    nargs = 1 ;
    printf("setting default T2 deformation parameters to: \n") ;
    printf("\tT2_max_inside = %2.1f\n", T2_max_inside) ;
    printf("\tT2_min_inside = %2.1f\n", T2_min_inside) ;
    printf("\tT2_min_outside = %2.1f\n", T2_min_outside) ;
    printf("\tT2_max_outside = %2.1f\n", T2_max_outside) ;
    printf("\tmax_pial_averages = %d\n", max_pial_averages) ;
    printf("\tmax_outward_dist = %2.1f\n", max_outward_dist) ;
    printf("\tGhisto_min_inside_peak_pct = %2.2f\n", Ghisto_min_inside_peak_pct) ;
    printf("\tGhisto_min_outside_peak_pct = %2.2f\n", Ghisto_min_outside_peak_pct) ;
    printf("\tparms.l_repulse  = %2.3f\n", l_repulse) ;
    printf("\tparms.l_curv     = %2.1f\n", parms.l_curv) ;
    printf("\tparms.l_nspring  = %2.1f\n", parms.l_nspring) ;
    printf("\tparms.l_location = %2.1f\n", parms.l_location) ;
  }
  else if (!stricmp(option, "flair"))
  {
    contrast_type = CONTRAST_FLAIR ;
    parms.l_location = 0.5 ;
    parms.l_nspring = 1.0 ;
    parms.l_curv = 0.5 ;
    parms.check_tol = 1 ;
    flair_or_T2_name = argv[2] ;
    fprintf(stderr,
            "deforming surfaces based on FLAIR volume %s\n", flair_or_T2_name) ;
    nargs = 1 ;
    T2_max_inside = 300 ;
    T2_min_inside = 110 ;   
    T2_min_outside = 130 ;  
    T2_max_outside = 300 ;
    max_pial_averages = 4 ;
    max_outward_dist = 3 ;
    Ghisto_min_inside_peak_pct = 0.01 ;
    Ghisto_min_outside_peak_pct = 0.5 ;
    l_repulse = .025 ;
    printf("setting default FLAIR deformation parameters to: \n") ;
    printf("\tT2_max_inside = %2.1f\n", T2_max_inside) ;
    printf("\tT2_min_inside = %2.1f\n", T2_min_inside) ;
    printf("\tT2_min_outside = %2.1f\n", T2_min_outside) ;
    printf("\tT2_max_outside = %2.1f\n", T2_max_outside) ;
    printf("\tmax_pial_averages = %d\n", max_pial_averages) ;
    printf("\tmax_outward_dist = %2.1f\n", max_outward_dist) ;
    printf("\tGhisto_min_inside_peak_pct = %2.2f\n", Ghisto_min_inside_peak_pct) ;
    printf("\tGhisto_min_outside_peak_pct = %2.2f\n", Ghisto_min_outside_peak_pct) ;
    printf("\tparms.l_repulse  = %2.3f\n", l_repulse) ;
    printf("\tparms.l_curv     = %2.1f\n", parms.l_curv) ;
    printf("\tparms.l_nspring  = %2.1f\n", parms.l_nspring) ;
    printf("\tparms.l_location = %2.1f\n", parms.l_location) ;
  }
  else if (!stricmp(option, "location"))
  {
    parms.l_location = atof(argv[2]) ;
    printf("setting location term coefficent to l_location = %2.2f\n", parms.l_location) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "cortex"))
  {
    label_cortex = atoi(argv[2]) ;
    printf("%sgenerating cortex label to subject's "
           "label/?h.cortex.label file\n", label_cortex ? "" : "not ") ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "fix_mtl"))
  {
    fix_mtl = 1 ;
    printf("not allowing deformations in hippocampus or amygdala "
           "when estimating pial surface\n") ;
  }
  else if (!stricmp(option, "mode"))
  {
    use_mode = atoi(argv[2]) ;
    printf("%susing class modes instead of means...\n",
           use_mode ? "" : "NOT ") ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "aseg"))
  {
    aseg_name = argv[2] ;
    printf("using aseg volume %s to prevent surfaces crossing the midline\n",
           aseg_name) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "cover_seg"))
  {
    auto_detect_stats = 0 ;
    printf("creating surfaces to cover  segmented volume %s\n", argv[2]) ;
    mri_cover_seg = MRIread(argv[2]) ;
    if (mri_cover_seg == NULL)
    {
      ErrorExit(ERROR_NOFILE,
                "%s: could not read segmentation volume %s", argv[2]) ;
    }
    parms.grad_dir = 1 ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "write_aseg"))
  {
    write_aseg_fname = argv[2] ;
    printf("writing corrected  aseg volume to %s\n", write_aseg_fname) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "noaseg"))
  {
    aseg_name = NULL ;
    printf("not using aseg volume to prevent surfaces "
           "crossing the midline\n");
    nargs = 0 ;
  }
  else if (!stricmp(option, "noaparc"))
  {
    aparc_name = NULL ;
    printf("not using aparc to prevent surfaces "
           "crossing the midline\n");
    nargs = 0 ;
  }
  else if (!stricmp(option, "wval"))
  {
    if (white_num >= MAX_VERTICES)
    {
      ErrorExit(ERROR_NOMEMORY,
                "%s: too many white vertex vals specified", Progname) ;
    }
    white_vnos[white_num] = atoi(argv[2]) ;
    white_vals[white_num] = atof(argv[3]) ;
    printf("constraining white surface val for vno %d to be %2.0f\n",
           white_vnos[white_num], white_vals[white_num]) ;
    white_num++ ;
    nargs = 2 ;
  }
  else if (!stricmp(option, "pval"))
  {
    if (pial_num >= MAX_VERTICES)
    {
      ErrorExit(ERROR_NOMEMORY,
                "%s: too many pial vertex vals specified", Progname) ;
    }
    pial_vnos[pial_num] = atoi(argv[2]) ;
    pial_vals[pial_num] = atof(argv[3]) ;
    printf("constraining pial surface val for vno %d to be %2.0f\n",
           pial_vnos[pial_num], pial_vals[pial_num]) ;
    pial_num++ ;
    nargs = 2 ;
  }
  else if (!stricmp(option, "pnbrs"))
  {
    pial_nbrs = atoi(argv[2]) ;
    printf("setting pvals out to %d nbrs\n", pial_nbrs) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "T1") || !stricmp(option, "gvol"))
  {
    strcpy(T1_name, argv[2]) ;
    printf("using %s as T1 volume...\n", T1_name) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "wvol"))
  {
    white_fname = argv[2] ;
    printf("using %s as volume for white matter deformation...\n",
           white_fname) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "hires") || !stricmp(option, "highres"))
  {
    highres_label = LabelRead(NULL, argv[2]) ;
    if (!highres_label)
      ErrorExit(ERROR_NOFILE,
                "%s: could not read highres label %s", Progname, argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "long"))
  {
    longitudinal = 1;
    printf("Using longitudinal scheme\n");
  }
  else if (!stricmp(option, "SDIR"))
  {
    strcpy(sdir, argv[2]) ;
    printf("using %s as SUBJECTS_DIR...\n", sdir) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "orig_white"))
  {
    orig_white = argv[2] ;
    printf("using %s starting white location...\n", orig_white) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "orig_pial"))
  {
    orig_pial = argv[2] ;
    printf("using %s starting pial locations...\n", orig_pial) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "max_border_white"))
  {
    max_border_white_set = 1 ;
    max_border_white = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "min_border_white"))
  {
    min_border_white_set = 1 ;
    min_border_white = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "wlo"))     // same flag name as mri_segment
  {
    min_border_white_set = 1 ;
    min_border_white = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "scale_std"))
  {
    std_scale = atof(argv[2]);
    printf("scale the estimated WM and GM std by %g \n", std_scale) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "min_gray_at_white_border"))
  {
    min_gray_at_white_border_set = 1 ;
    min_gray_at_white_border = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "max_gray"))
  {
    max_gray_set = 1 ;
    max_gray = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "ghi"))     // same flag name as mri_segment
  {
    max_gray_set = 1 ;
    max_gray = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "max_gray_at_csf_border"))
  {
    max_gray_at_csf_border_set = 1 ;
    max_gray_at_csf_border = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "min_gray_at_csf_border"))
  {
    min_gray_at_csf_border_set = 1 ;
    min_gray_at_csf_border = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "variablesigma"))
  {
    variablesigma = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "min_csf"))
  {
    min_csf_set = 1 ;
    min_csf = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "max_csf"))
  {
    max_csf_set = 1 ;
    max_csf = atof(argv[2]) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "noauto"))
  {
    auto_detect_stats = 0 ;
    fprintf(stderr, "disabling auto-detection of border ranges...\n") ;
  }
  else if (!stricmp(option, "inoutin"))
  {
    in_out_in_flag = 1 ;
    fprintf(stderr, "applying final white matter deformation after pial\n") ;
  }
  else if (!stricmp(option, "graymid"))
  {
    graymid = 1 ;
    fprintf(stderr, "generating graymid surface...\n") ;
  }
  else if (!strcmp(option, "rval"))
  {
    rh_label = atoi(argv[2]) ;
    nargs = 1 ;
   
    fprintf(stderr,"using %d as fill val for right hemisphere.\n", rh_label);
  }
  else if (!stricmp(option, "fill_interior"))
  {
    fill_interior = atoi(argv[2]) ;
    nargs = 1 ;
    printf("filling interior of surface with %d\n", fill_interior) ;
  }
  else if (!strcmp(option, "nbhd_size"))
  {
    nbhd_size = atoi(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr,"using %d size nbhd for thickness calculation.\n",
            nbhd_size);
  }
  else if (!strcmp(option, "lval"))
  {
    lh_label = atoi(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr,"using %d as fill val for left hemisphere.\n", lh_label);
  }
  else if (!stricmp(option, "whiteonly"))
  {
    white_only = 1 ;
    fprintf(stderr,  "only generating white matter surface\n") ;
  }
  else if (!stricmp(option, "overlay"))
  {
    overlay = !overlay ;
    fprintf(stderr,  "%soverlaying T1 volume with edited white matter\n",
            overlay ? "" : "not") ;
  }
  else if (!stricmp(option, "pial"))
  {
    strcpy(pial_name, argv[2]) ;
    fprintf(stderr,  "writing pial surface to file named %s\n", pial_name) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "write_vals"))
  {
    write_vals = 1 ;
    fprintf(stderr,  "writing gray and white surface targets to .mgz files\n") ;
  }
  else if (!stricmp(option, "name"))
  {
    strcpy(parms.base_name, argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "base name = %s\n", parms.base_name) ;
  }
  else if (!stricmp(option, "dt"))
  {
    parms.dt = atof(argv[2]) ;
    parms.base_dt = base_dt_scale*parms.dt ;
    parms.integration_type = INTEGRATE_MOMENTUM ;
    fprintf(stderr,  "using dt = %2.1e\n", parms.dt) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "spring"))
  {
    parms.l_spring = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_spring = %2.3f\n", parms.l_spring) ;
  }
  else if (!stricmp(option, "tsmooth"))
  {
    l_tsmooth = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_tsmooth = %2.3f\n", l_tsmooth) ;
  }
  else if (!stricmp(option, "grad"))
  {
    parms.l_grad = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_grad = %2.3f\n", parms.l_grad) ;
  }
  else if (!stricmp(option, "tspring"))
  {
    parms.l_tspring = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_tspring = %2.3f\n", parms.l_tspring) ;
  }
  else if (!stricmp(option, "unpinch"))
  {
    unpinch = 1 ;
    fprintf(stderr, "removing pinches from surface before deforming\n") ;
  }
  else if (!stricmp(option, "nltspring"))
  {
    parms.l_nltspring = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_nltspring = %2.3f\n", parms.l_nltspring) ;
  }
  else if (!stricmp(option, "nspring"))
  {
    parms.l_nspring = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_nspring = %2.3f\n", parms.l_nspring) ;
  }
  else if (!stricmp(option, "curv"))
  {
    parms.l_curv = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_curv = %2.3f\n", parms.l_curv) ;
  }
  else if (!stricmp(option, "smooth"))
  {
    smooth = atoi(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "smoothing for %d iterations\n", smooth) ;
  }
  else if (!stricmp(option, "smooth_pial"))
  {
    smooth_pial = atoi(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "smoothing pial surface for %d iterations before deforming\n", smooth_pial) ;
  }
  else if (!stricmp(option, "output"))
  {
    output_suffix = argv[2] ;
    nargs = 1 ;
    fprintf(stderr, "appending %s to output names...\n", output_suffix) ;
  }
  else if (!stricmp(option, "vavgs"))
  {
    vavgs = atoi(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "smoothing values for %d iterations\n", vavgs) ;
  }
  else if (!stricmp(option, "white"))
  {
    strcpy(white_matter_name, argv[2]) ;
    nargs = 1 ;
    // NJS HACK: if filename passed to -white is "NOWRITE", then dont write
    // the white, curv, area, and cortex.label files.
    // this is in lieu of -nowhite not creating pial surfaces that
    // match those created w/o the -nowhite option.
    if (!strcmp(white_matter_name,"NOWRITE"))
    {
      fprintf(stderr, "-white NOWRITE indicates that white, curv, area, "
              "and cortex.label files will not be written...\n") ;
    }
    else
    {
      fprintf(stderr, "using %s as white matter name...\n",
              white_matter_name) ;
    }
  }
  else if (!stricmp(option, "intensity"))
  {
    parms.l_intensity = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, "l_intensity = %2.3f\n", parms.l_intensity) ;
  }
  else if (!stricmp(option, "lm"))
  {
    parms.integration_type = INTEGRATE_LINE_MINIMIZE ;
    fprintf(stderr, "integrating with line minimization\n") ;
  }
  else if (!stricmp(option, "nwhite"))
  {
    nwhite = atoi(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr,
            "integrating gray/white surface positioning for %d time steps\n",
            nwhite) ;
  }
  else if (!stricmp(option, "nowhite"))
  {
    nowhite = 1 ;
    fprintf(stderr, "reading previously compute gray/white surface\n") ;
  }
  else if (!stricmp(option, "flair_white") || !stricmp(option, "flairwhite"))
  {
    contrast_type = CONTRAST_FLAIR ;
    flairwhite = 1 ;
    fprintf(stderr, "deforming white matter surfaces to match FLAIR volume\n") ;
  }
  else if (!stricmp(option, "smoothwm"))
  {
    smoothwm = atoi(argv[2]) ;
    fprintf(stderr, "writing smoothed (%d iterations) wm surface\n",
            smoothwm) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "filled"))
  {
    filled_name = (argv[2]) ;
    fprintf(stderr, "using %s as filled name\n", filled_name) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "wm"))
  {
    wm_name = (argv[2]) ;
    fprintf(stderr, "using %s as filled name\n", wm_name) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "ngray"))
  {
    ngray = atoi(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr,
            "integrating pial surface positioning for %d time steps\n",
            ngray) ;
  }
  else if (!stricmp(option, "tol"))
  {
    parms.tol = atof(argv[2]) ;
    nargs = 1 ;
    fprintf(stderr, 
            "setting tol = %2.1f%% change in sse/rms\n", parms.tol) ;
  }
  else if (!stricmp(option, "wsigma"))
  {
    white_sigma = atof(argv[2]) ;
    fprintf(stderr,  "smoothing volume with Gaussian sigma = %2.1f\n",
            white_sigma) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "psigma"))
  {
    pial_sigma = atof(argv[2]) ;
    fprintf(stderr,  "smoothing volume with Gaussian sigma = %2.1f\n",
            pial_sigma) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "pblur"))
  {
    pial_blur_sigma = atof(argv[2]) ;
    fprintf(stderr,  "smoothing pial volume with Gaussian sigma = %2.1f\n",
            pial_blur_sigma) ;
    nargs = 1 ;
  }
  else if (!stricmp(option, "pa"))
  {
    max_pial_averages = atoi(argv[2]) ;
    fprintf(stderr, "using max pial averages = %d\n", max_pial_averages) ;
    nargs = 1 ;
    if (isdigit(*argv[3]))
    {
      min_pial_averages = atoi(argv[3]) ;
      fprintf
      (stderr, "using min pial averages = %d\n", min_pial_averages) ;
      nargs++ ;
    }
  }
  else if (!stricmp(option, "wa"))
  {
    max_white_averages = atoi(argv[2]) ;
    fprintf(stderr, "using max white averages = %d\n", max_white_averages) ;
    nargs = 1 ;
    if (isdigit(*argv[3]))
    {
      min_white_averages = atoi(argv[3]) ;
      fprintf
      (stderr, "using min white averages = %d\n", min_white_averages) ;
      nargs++ ;
    }
  }
  else if (!stricmp(option, "add"))
  {
    add = 1 ;
    fprintf(stderr, "adding vertices to tessellation during deformation.\n");
  }
  else if (!stricmp(option, "first_wm_peak"))
  {
    parms.flags |= IPFLAG_FIND_FIRST_WM_PEAK ;
    printf("settling WM surface at first peak in intensity profile\n") ;
  }
  else if (!stricmp(option, "max") || !stricmp(option, "max_thickness"))
  {
    max_thickness = atof(argv[2]) ;
    nargs = 1 ;
    printf("using max_thickness = %2.1f\n", max_thickness) ;
  }
  else if (!stricmp(option, "mgz"))
  {
    MGZ = 1;
    printf("INFO: assuming MGZ format for volumes.\n");
  }
  else switch (toupper(*option))
    {
    case 'S':
      suffix = argv[2] ;
      fprintf(stderr, "using %s as suffix\n", suffix) ;
      nargs = 1 ;
      break ;
    case '?':
    case 'H':
    case 'U':
      print_usage() ;
      exit(1) ;
      break ;
    case 'T':
      xform_fname = argv[2] ;
      nargs = 1;
      fprintf(stderr, "applying ventricular xform %s\n", xform_fname);
      break ;
    case 'O':
      orig_name = argv[2] ;
      nargs = 1 ;
      fprintf
      (stderr, "reading original vertex positions from %s\n", orig_name);
      break ;
    case 'Q':
      parms.flags |= IPFLAG_NO_SELF_INT_TEST ;
      fprintf(stderr,
              "doing quick (no self-intersection) surface positioning.\n") ;
      break ;
#if 0
    case 'A':
      max_averages = atoi(argv[2]) ;
      fprintf(stderr, "using n_averages = %d\n", max_averages) ;
      nargs = 1 ;
      if (isdigit(*argv[3]))
      {
        min_averages = atoi(argv[3]) ;
        fprintf(stderr, "using min_averages = %d\n", min_averages) ;
        nargs++ ;
      }
      break ;
#endif
    case 'M':
      parms.integration_type = INTEGRATE_MOMENTUM ;
      parms.momentum = atof(argv[2]) ;
      nargs = 1 ;
      fprintf(stderr, "momentum = %2.2f\n", parms.momentum) ;
      break ;
    case 'R':
      l_surf_repulse = atof(argv[2]) ;
      fprintf(stderr, "l_surf_repulse = %2.3f\n", l_surf_repulse) ;
      nargs = 1 ;
      break ;
    case 'B':
      base_dt_scale = atof(argv[2]) ;
      parms.base_dt = base_dt_scale*parms.dt ;
      nargs = 1;
      break ;
    case 'V':
      Gdiag_no = atoi(argv[2]) ;
      nargs = 1 ;
      break ;
    case 'C':
      create = !create ;
      fprintf(stderr,
              "%screating area and curvature files for wm surface...\n",
              create ? "" : "not ") ;
      break ;
    case 'W':
      sscanf(argv[2], "%d", &parms.write_iterations) ;
      nargs = 1 ;
      fprintf(stderr, "write iterations = %d\n", parms.write_iterations) ;
      Gdiag |= DIAG_WRITE ;
      break ;
    case 'N':
      sscanf(argv[2], "%d", &parms.niterations) ;
      nargs = 1 ;
      fprintf(stderr, "niterations = %d\n", parms.niterations) ;
      break ;
    default:
      fprintf(stderr, "unknown option %s\n", argv[1]) ;
      print_help() ;
      exit(1) ;
      break ;
    }

  return(nargs) ;
}

static void
usage_exit(void)
{
  print_usage() ;
  exit(1) ;
}

#include "mris_make_surfaces.help.xml.h"
static void
print_usage(void)
{
  outputHelpXml(mris_make_surfaces_help_xml,mris_make_surfaces_help_xml_len);
}

static void
print_help(void)
{
  print_usage() ;
  exit(1) ;
}

static void
print_version(void)
{
  fprintf(stderr, "%s\n", vcid) ;
  exit(1) ;
}

static int
mrisFindMiddleOfGray(MRI_SURFACE *mris)
{
  int     vno ;
  VERTEX  *v ;
  float   nx, ny, nz, thickness ;

  MRISaverageCurvatures(mris, 3) ;
  MRISsaveVertexPositions(mris, TMP_VERTICES) ;
  MRISrestoreVertexPositions(mris, ORIGINAL_VERTICES) ;
  MRIScomputeMetricProperties(mris);
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
    {
      continue ;
    }
    nx = v->nx ;
    ny = v->ny ;
    nz = v->nz ;
    thickness = 0.5 * v->curv ;
    v->x = v->origx + thickness * nx ;
    v->y = v->origy + thickness * ny ;
    v->z = v->origz + thickness * nz ;
  }
  return(NO_ERROR) ;
}

MRI *
MRIfillVentricle(MRI *mri_inv_lv, MRI *mri_T1, float thresh,
                 int out_label, MRI *mri_dst)
{
  BUFTYPE   *pdst, *pinv_lv, out_val, inv_lv_val ;
  int       width, height, depth, x, y, z,
            ventricle_voxels;

  if (!mri_dst)
  {
    mri_dst = MRIclone(mri_T1, NULL) ;
  }

  width = mri_T1->width ;
  height = mri_T1->height ;
  depth = mri_T1->depth ;
  /* now apply the inverse morph to build an average wm representation
     of the input volume
  */


  ventricle_voxels = 0 ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      pinv_lv = &MRIvox(mri_inv_lv, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        inv_lv_val = *pinv_lv++ ;
        out_val = 0 ;
        if (inv_lv_val >= thresh)
        {
          ventricle_voxels++ ;
          out_val = out_label ;
        }
        *pdst++ = out_val ;
      }
    }
  }

#if 0
  MRIfillRegion(mri_T1, mri_dst, 30, out_label, 2*ventricle_voxels) ;
  MRIdilate(mri_dst, mri_dst) ;
  MRIdilate(mri_dst, mri_dst) ;
#endif
  return(mri_dst) ;
}

static MRI *
MRIsmoothMasking(MRI *mri_src, MRI *mri_mask, MRI *mri_dst, int mask_val,
                 int wsize)
{
  int      width, height, depth, x, y, z, xi, yi, zi, xk, yk, zk, whalf,
           nvox, mean, avg ;
  BUFTYPE  *psrc, *pdst ;

  whalf = (wsize-1) / 2 ;
  width = mri_src->width ;
  height = mri_src->height ;
  depth = mri_src->depth ;
  if (!mri_dst)
  {
    mri_dst = MRIcopy(mri_src, NULL) ;
  }

  for ( z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      psrc = &MRIvox(mri_src, 0, y, z) ;
      pdst = &MRIvox(mri_dst, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        mean = *psrc++ ;
        nvox = 1 ;

        /* this is a hack to prevent smoothing of non-white values */
        if (MRIvox(mri_mask, x, y, z) > WM_MIN_VAL)
        {
          avg = 0 ;  /* only average if a masked
                        value is close to this one */
          for (zk = -whalf ; zk <= whalf ; zk++)
          {
            zi = mri_mask->zi[z+zk] ;
            for (yk = -whalf ; yk <= whalf ; yk++)
            {
              yi = mri_mask->yi[y+yk] ;
              for (xk = -whalf ; xk <= whalf ; xk++)
              {
                xi = mri_mask->xi[x+xk] ;
                if (MRIvox(mri_mask, xi, yi, zi) == mask_val)
                {
                  avg = 1 ;
                  break ;
                }
              }
              if (avg)
              {
                break ;
              }
            }
            if (avg)
            {
              break ;
            }
          }
          if (avg)
          {
            for (zk = -whalf ; zk <= whalf ; zk++)
            {
              zi = mri_mask->zi[z+zk] ;
              for (yk = -whalf ; yk <= whalf ; yk++)
              {
                yi = mri_mask->yi[y+yk] ;
                for (xk = -whalf ; xk <= whalf ; xk++)
                {
                  xi = mri_mask->xi[x+xk] ;
                  if (MRIvox(mri_mask, xi, yi, zi) >=
                      WM_MIN_VAL)
                  {
                    mean += MRIvox(mri_src, xi, yi, zi) ;
                    nvox++ ;
                  }
                }
              }
            }
          }
        }
        *pdst++ = (BUFTYPE)nint((float)mean/(float)nvox) ;
      }
    }
  }
  return(mri_dst) ;
}

int
MRISfindExpansionRegions(MRI_SURFACE *mris)
{
  int    vno, num, n, num_long, total ;
  float  d, dsq, mean, std, dist ;
  VERTEX *v, *vn ;

  d = dsq = 0.0f ;
  for (total = num = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag || v->val <= 0)
    {
      continue ;
    }
    num++ ;
    dist = fabs(v->d) ;
    d += dist ;
    dsq += (dist*dist) ;
  }

  mean = d / num ;
  std = sqrt(dsq/num - mean*mean) ;
  fprintf(stderr, "mean absolute distance = %2.2f +- %2.2f\n", mean, std) ;

  for (num = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->curv = 0 ;
    if (v->ripflag || v->val <= 0)
    {
      continue ;
    }
    if (fabs(v->d) < mean+2*std)
    {
      continue ;
    }
    for (num_long = num = 1, n = 0 ; n < v->vnum ; n++)
    {
      vn = &mris->vertices[v->v[n]] ;
      if (vn->val <= 0 || v->ripflag)
      {
        continue ;
      }
      if (fabs(vn->d) >= mean+2*std)
      {
        num_long++ ;
      }
      num++ ;
    }

    if ((float)num_long / (float)num > 0.25)
    {
      v->curv = fabs(v->d) ;
      total++ ;
#if 0
      fprintf(stderr, "v %d long: (%d of %d)\n", vno, num_long, num) ;
#endif
    }
  }
  if (Gdiag & DIAG_SHOW)
  {
    fprintf(stderr, "%d vertices more than 2 sigmas from mean.\n", total) ;
  }
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRISwriteCurvature(mris, "long") ;
  }
  return(NO_ERROR) ;
}

int
MRIsmoothBrightWM(MRI *mri_T1, MRI *mri_wm)
{
  int     width, height, depth, x, y, z, nthresholded ;
  BUFTYPE *pwm, val, wm ;

  width = mri_T1->width ;
  height = mri_T1->height ;
  depth = mri_T1->depth ;

  nthresholded = 0 ;
  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pwm = &MRIvox(mri_wm, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        val = MRIgetVoxVal(mri_T1, x, y, z, 0) ;
        wm = *pwm++ ;
        if (wm >= WM_MIN_VAL)  /* labeled as white */
        {
          if (val > DEFAULT_DESIRED_WHITE_MATTER_VALUE)
          {
            nthresholded++ ;
            val = DEFAULT_DESIRED_WHITE_MATTER_VALUE ;
          }
        }
        MRIsetVoxVal(mri_T1, x, y, z, 0, val) ;
      }
    }
  }

  fprintf(stderr, "%d bright wm thresholded.\n", nthresholded) ;

  return(NO_ERROR) ;
}
MRI *
MRIfindBrightNonWM(MRI *mri_T1, MRI *mri_wm)
{
  int     width, height, depth, x, y, z, nlabeled, nwhite,
          xk, yk, zk, xi, yi, zi;
  BUFTYPE *pwm, val, wm ;
  MRI     *mri_labeled, *mri_tmp ;

  mri_labeled = MRIclone(mri_T1, NULL) ;
  width = mri_T1->width ;
  height = mri_T1->height ;
  depth = mri_T1->depth ;

  for (z = 0 ; z < depth ; z++)
  {
    for (y = 0 ; y < height ; y++)
    {
      pwm = &MRIvox(mri_wm, 0, y, z) ;
      for (x = 0 ; x < width ; x++)
      {
        val = MRIgetVoxVal(mri_T1, x, y, z, 0) ;
        wm = *pwm++ ;

        if (x == Gx && y == Gy && z == Gz)  /* T1=127 */
        {
          DiagBreak() ;
        }
        /* not white matter and bright (e.g. eye sockets) */
        if ((wm < WM_MIN_VAL) && (val > 125))
        {
          nwhite = 0 ;
          for (xk = -1 ; xk <= 1 ; xk++)
          {
            xi = mri_T1->xi[x+xk] ;
            for (yk = -1 ; yk <= 1 ; yk++)
            {
              yi = mri_T1->yi[y+yk] ;
              for (zk = -1 ; zk <= 1 ; zk++)
              {
                zi = mri_T1->zi[z+zk] ;
                if (MRIvox(mri_wm, xi, yi, zi) >= WM_MIN_VAL)
                {
                  nwhite++ ;
                }
              }
            }
          }
#define MIN_WHITE  ((3*3*3-1)/2)
          if (nwhite < MIN_WHITE)
          {
            MRIvox(mri_labeled, x, y, z) = BRIGHT_LABEL ;
          }
        }
      }
    }
  }

  /* find all connected voxels that are above 115 */
  MRIdilateThreshLabel(mri_labeled, mri_T1, NULL, BRIGHT_LABEL, 10,115);
  MRIclose(mri_labeled, mri_labeled) ;

  /* expand once more to all neighboring voxels that are bright. At
     worst we will erase one voxel of white matter.
  */
  mri_tmp =
    MRIdilateThreshLabel(mri_labeled, mri_T1, NULL, BRIGHT_LABEL,1,100);
  MRIxor(mri_labeled, mri_tmp, mri_tmp, 1, 255) ;
  MRIreplaceValues(mri_tmp, mri_tmp, 1, BRIGHT_BORDER_LABEL) ;
  MRIunion(mri_tmp, mri_labeled, mri_labeled) ;
#if 0
  fprintf(stderr, "selectively smoothing volume....\n") ;
  MRIsoapBubbleLabel(mri_T1, mri_labeled, mri_T1, BRIGHT_LABEL, 200) ;
#endif
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_labeled, "label.mgz") ;
  }
  /*    MRIwrite(mri_tmp, "tmp.mgz") ;*/
  nlabeled = MRIvoxelsInLabel(mri_labeled, BRIGHT_LABEL) ;
  fprintf(stderr, "%d bright non-wm voxels segmented.\n", nlabeled) ;

  /* dilate outwards if exactly 0 */
  MRIdilateInvThreshLabel
  (mri_labeled, mri_T1, mri_labeled, BRIGHT_LABEL, 3, 0) ;

  MRIfree(&mri_tmp) ;
  return(mri_labeled) ;
}
static float
check_contrast_direction(MRI_SURFACE *mris,MRI *mri_T1)
{
  int     vno, n ;
  VERTEX  *v ;
  Real    x, y, z, xw, yw, zw, val, mean_inside, mean_outside ;

  mean_inside = mean_outside = 0.0 ;
  for (n = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag != 0)
    {
      continue ;
    }
    x = v->x+0.5*v->nx ;
    y = v->y+0.5*v->ny ;
    z = v->z+0.5*v->nz ;
    MRIsurfaceRASToVoxel(mri_T1, x, y, z, &xw, &yw, &zw);
    MRIsampleVolume(mri_T1, xw, yw, zw, &val) ;
    mean_outside += val ;

    x = v->x-0.5*v->nx ;
    y = v->y-0.5*v->ny ;
    z = v->z-0.5*v->nz ;
    MRIsurfaceRASToVoxel(mri_T1, x, y, z, &xw, &yw, &zw);
    MRIsampleVolume(mri_T1, xw, yw, zw, &val) ;
    mean_inside += val ;
    n++ ;
  }
  mean_inside /= (float)n ;
  mean_outside /= (float)n ;
  printf("mean inside = %2.1f, mean outside = %2.1f\n",
         mean_inside, mean_outside) ;
  return(mean_inside - mean_outside) ;
}

#if 0
static MRI *
smooth_contra_hemi(MRI *mri_filled,
                   MRI *mri_src,
                   MRI *mri_dst,
                   float ipsi_label,
                   float contra_label)
{
  MRI    *mri_ctrl ;

  mri_dst = MRIcopy(mri_src, mri_dst) ;

  printf("smoothing contralateral hemisphere...\n") ;

  // do soap bubble smoothing within the contra hemi do blur out any boundaries
  mri_ctrl = MRIreplaceValues(mri_filled, NULL, ipsi_label, 0) ;
  MRIdilate(mri_ctrl, mri_ctrl) ;
  mri_dst = MRIsmoothLabel(mri_src, mri_ctrl, mri_dst, 10, contra_label, 1.0) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_ctrl, "ctrl.mgz") ;
    MRIwrite(mri_dst, "contra_smoothed.mgz") ;
  }

  MRIfree(&mri_ctrl) ;
  return(mri_dst) ;
}

static MRI *
smooth_contra_hemi_with_aseg(MRI *mri_aseg,
			     MRI *mri_src,
			     MRI *mri_dst,
			     char *hemi)
{
  MRI    *mri_ctrl ;
  int    is_left_hemi, x, y, z, label, ctrl ;

  is_left_hemi = stricmp(hemi, "lh") == 0 ;

  mri_dst = MRIcopy(mri_src, mri_dst) ;
  mri_ctrl = MRIclone(mri_src, NULL) ;

  printf("smoothing contralateral hemisphere with aseg...\n") ;
  for (x = 0 ; x < mri_aseg->width ; x++)
    for (y = 0 ; y < mri_aseg->height ; y++)
      for (z = 0 ; z < mri_aseg->depth ; z++)
      {
	if (x == Gx && y == Gy && z == Gz)
	  DiagBreak() ;
	label = MRIgetVoxVal(mri_aseg, x, y, z, 0) ;
	ctrl = 0 ;
	if (is_left_hemi)
	{
	  if (IS_LH_CLASS(label))
	    ctrl = 1 ;
	  else
	  {
	    if (MRIlabelsInNbhd(mri_aseg, x, y, z, 2, Left_Cerebral_Cortex)) // adjacent to left cortex
	    {
	      // if it's cortex in the other hemi that is next to cortex in this hemi,smooth it out
	      if (label == Right_Cerebral_Cortex)
		ctrl = 0 ;
	      else
		ctrl = 1 ;  // if adjacent to cortex but not cortex in aseg, don't smooth since aseg isn't perfect
	    }
	  }
	}
	else // right hemisphere
	{
	  if (IS_RH_CLASS(label))
	    ctrl = 1 ;
	  else
	  {
	    if (MRIlabelsInNbhd(mri_aseg, x, y, z, 2, Right_Cerebral_Cortex)) // adjacent to left cortex
	    {
	      // if it's cortex in the other hemi that is next to cortex in this hemi,smooth it out
	      if (label == Left_Cerebral_Cortex)
		ctrl = 0 ;
	      else
		ctrl = 1 ;  // if adjacent to cortex but not cortex in aseg, don't smooth since aseg isn't perfect
	    }
	  }
	}
	MRIsetVoxVal(mri_ctrl, x, y, z, 0, ctrl) ;
      }

  // do soap bubble smoothing within the contra hemi do blur out any boundaries
  mri_dst = MRIsmoothLabel(mri_src, mri_ctrl, mri_dst, 10, 0, 1.0) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_ctrl, "ctrl.mgz") ;
    MRIwrite(mri_dst, "contra_smoothed.mgz") ;
  }

  MRIfree(&mri_ctrl) ;
  return(mri_dst) ;
}
#endif

static int
fix_midline(MRI_SURFACE *mris, MRI *mri_aseg, MRI *mri_brain, char *hemi,
            int which, int fix_mtl)
{
  int vno, label, contra_wm_label, nvox=0, total_vox=0, adjacent=0;
  int wm_label, gm_label, nlabels, n, index, annotation, entorhinal_index ;
  VERTEX   *v ;
  double   xv, yv, zv, val, xs, ys, zs, d, nx, ny, nz ;
  LABEL    **labels ;

  printf("inhibiting deformation at non-cortical midline structures...\n") ;
  if (stricmp(hemi, "lh") == 0)
  {
    contra_wm_label = Right_Cerebral_White_Matter ;
    wm_label = Left_Cerebral_White_Matter ;
    gm_label = Left_Cerebral_Cortex ;
  }
  else
  {
    contra_wm_label = Left_Cerebral_White_Matter ;
    wm_label = Right_Cerebral_White_Matter ;
    gm_label = Right_Cerebral_Cortex ;
  }
  MRISclearMark2s(mris) ;
  MRISclearMarks(mris) ;

#if 0
  if (mris->ct && CTABfindName(mris->ct, "unknown", &index) == NO_ERROR)
  {
    CTABannotationAtIndex(mris->ct, index, &annotation) ;
    for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag)
      {
        continue ;
      }

      if (vno == Gdiag_no )
      {
        DiagBreak() ;
      }
      if (v->annotation == annotation)
      {
        v->marked = 1 ;
      }
    }
    MRISdilateMarked(mris, 3) ;
#if 0
    MRISinvertMarks(mris) ;  // 1 -- means can't be unknown
#endif
    MRIScopyMarkedToMarked2(mris) ;
    MRISclearMarks(mris) ;
  }
#endif
  if (mris->ct)
    CTABfindName(mris->ct, "entorhinal", &entorhinal_index);
  else
    entorhinal_index = -1 ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag || v->marked2 > 0)
    {
      v->marked = 1 ; // it was ripped previously - it should still be excluded
      continue ;
    }

    if (mris->ct)
      CTABfindAnnotation(mris->ct, v->annotation, &index);

    if (vno == Gdiag_no )
    {
      printf("vno %d: annotation %x, index %d, EC %d\n", vno, v->annotation, index, entorhinal_index) ;
      DiagBreak() ;
    }

    if (mris->ct && index == entorhinal_index)  // don't freeze vertices that are in EC and very close to hippocampus
      continue ;
    // search outwards
    for (d = 0 ; d <= 2 ; d += 0.5)
    {
      xs = v->x + d*v->nx ;
      ys = v->y + d*v->ny ;
      zs = v->z + d*v->nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_aseg, xs, ys, zs, &xv, &yv, &zv);
      MRIsampleVolumeType(mri_aseg, xv, yv, zv, &val, SAMPLE_NEAREST) ;
      label = nint(val) ;
      if (vno == Gdiag_no)
      {
	printf("vno %d: dist %2.2f - %s (%d)\n", vno, d, cma_label_to_name(label), label) ;
	DiagBreak() ;
      }
      if (d > 0 && label == gm_label)
	break ;  // cortical gray matter of this hemisphere - doesn't matter what is outside it
      if (label == contra_wm_label ||
          label == Left_Lateral_Ventricle ||
          label == Left_vessel ||
          label == Right_vessel ||
          label == Optic_Chiasm ||
          label == Left_choroid_plexus ||
          label == Right_choroid_plexus ||
          label == Third_Ventricle ||
          label == Right_Lateral_Ventricle ||
          ((label == Left_Accumbens_area ||
            label == Right_Accumbens_area) &&  // only for gray/white
           which == GRAY_WHITE) ||
          ((label == Left_Lesion ||
            label == Right_Lesion ||
            label == WM_hypointensities ||
            label == Left_WM_hypointensities ||
            label == Right_non_WM_hypointensities ||
            label == Left_non_WM_hypointensities ||
            label == Right_WM_hypointensities) &&  // only for gray/white
           which == GRAY_WHITE) ||
          label == Left_Caudate ||
          label == Right_Caudate ||
          label == Left_Pallidum ||
          IS_CC(label) ||
          ((IS_HIPPO(label)  || IS_AMYGDALA(label)) && fix_mtl)  ||
          label == Right_Pallidum ||
          label == Right_Thalamus_Proper ||
          label == Left_Thalamus_Proper ||
          label == Brain_Stem ||
          label == Left_VentralDC ||
          label == Right_VentralDC)
      {
	// these are labels where the intensities aren't useful, so just freeze surface there
	if (label == Left_Lesion || label == Right_Lesion || IS_WMSA(label))
	  v->marked2 = 1 ;
        if (label == Left_Putamen || label == Right_Putamen)
        {
          DiagBreak() ;
        }
        if (vno == Gdiag_no)
        {
          DiagBreak() ;
        }
        MRISvertexToVoxel(mris, v, mri_aseg, &xv, &yv, &zv) ;
        MRIsampleVolume(mri_brain, xv, yv, zv, &val) ;
        v->val = val ;
        v->d = 0 ;
        v->marked = 1 ;
      }
    }

    if (vno == Gdiag_no)
    {
      DiagBreak() ;
    }
    MRISvertexToVoxel(mris, v, mri_aseg, &xv, &yv, &zv) ;
    MRIsampleVolumeType(mri_aseg, xv, yv, zv, &val, SAMPLE_NEAREST) ;
    label = nint(val) ;
    if (IS_PUTAMEN(label))
    {
      compute_label_normal(mri_aseg, xv, yv, zv, label, 3, &nx, &ny, &nz, 1) ;
    }
    else
    {
      nx = ny = nz = 0 ;
    }

    /*
      for gray/white surface, if we are in insula, don't want to let the
      surface diverge into the putamen.
    */
    if (which == GRAY_WHITE)
    {
      // search inwards
      for (d = 0 ; d <= 2 ; d += 0.5)
      {
        xs = v->x - d*v->nx ;
        ys = v->y - d*v->ny ;
        zs = v->z - d*v->nz ;
        MRISsurfaceRASToVoxelCached(mris, mri_aseg, xs, ys, zs, &xv, &yv, &zv);
        MRIsampleVolumeType(mri_aseg, xv, yv, zv, &val, SAMPLE_NEAREST) ;
        label = nint(val) ;
	if (vno == Gdiag_no)
	{
	  printf("vno %d: dist %2.2f - %s (%d)\n", vno, -d, cma_label_to_name(label), label) ;
	  DiagBreak() ;
	}
	// these are labels where the intensities aren't useful, so just freeze surface there
	if (d < 1 && (label == Left_Lesion || label == Right_Lesion || IS_WMSA(label)))
	  v->marked = v->marked2 = 1 ;

        if (IS_PUTAMEN(label) || IS_ACCUMBENS(label))
        {
          compute_label_normal(mri_aseg, xv, yv, zv, label, 3,
                               &nx, &ny, &nz, 1) ;
          if (fabs(nx) > fabs(ny) && fabs(nx) > fabs(nz))
          {
            if (vno == Gdiag_no)
            {
              DiagBreak() ;
            }
	    if (IS_ACCUMBENS(label))
	    {
	      v->d = 0 ;
	      v->marked = 1 ;
	      if (Gdiag & DIAG_SHOW && vno == Gdiag_no)
	      {
		printf("marking vertex %d as adjacent to accumbens on midline\n", vno);
	      }
	    }
	    else
	    {
	      MRISvertexToVoxel(mris, v, mri_aseg, &xv, &yv, &zv) ;
	      MRIsampleVolume(mri_brain, xv, yv, zv, &val) ;
	      v->val = val ;
	      v->d = 0 ;
	      v->marked = 1 ;
	      if (Gdiag & DIAG_SHOW && vno == Gdiag_no)
	      {
		printf("marking vertex %d as adjacent to putamen in insula\n", vno);
	      }
	    }
	    break ;
          }
        }
      }
    }
    // search inwards
    for (d = 0 ; d <= 2 ; d += 0.5)
    {
      xs = v->x - d*v->nx ;
      ys = v->y - d*v->ny ;
      zs = v->z - d*v->nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_aseg, xs, ys, zs, &xv, &yv, &zv);
      MRIsampleVolumeType(mri_aseg, xv, yv, zv, &val, SAMPLE_NEAREST) ;
      label = nint(val) ;
      if (d < 1.1 && (label == wm_label || label == gm_label))
      {
        break ;  // found real white matter next to surface
      }

      if ((which == GRAY_CSF) && (d < 1) && (label == Left_Lesion || label == Right_Lesion || IS_WMSA(label)))
	v->val2 = 1 ;

      if ((label == contra_wm_label ||
           label == Left_vessel ||
           label == Right_vessel ||
           label == Optic_Chiasm ||
           label == Left_choroid_plexus ||
           label == Right_choroid_plexus ||
           label == Left_Lateral_Ventricle ||
           label == Third_Ventricle ||
           label == Right_Lateral_Ventricle ||
           ((label == Left_Accumbens_area ||
             label == Right_Accumbens_area) &&
            which == GRAY_WHITE)||
           label == Left_Caudate ||
           label == Right_Caudate ||
           label == Left_Pallidum ||
           IS_CC(label) ||
           label == Right_Thalamus_Proper ||
           label == Left_Thalamus_Proper ||
           label == Right_Pallidum ||
           label == Brain_Stem ||
           label == Left_VentralDC ||
           label == Right_VentralDC) ||
          // putamen can be adjacent to insula in aseg for pial
          (which == GRAY_WHITE && (d < 1.1) &&
           (label == Left_Putamen || label == Right_Putamen)))

      {
        if (label == Left_Putamen || label == Right_Putamen)
        {
          DiagBreak() ;
        }
        if ((label == Left_Lateral_Ventricle ||
             label == Right_Lateral_Ventricle) &&
            d > 1)  // in calcarine ventricle can be pretty close to wm surface
        {
          break ;
        }
        if (vno == Gdiag_no)
        {
          DiagBreak() ;
        }
        MRISvertexToVoxel(mris, v, mri_aseg, &xv, &yv, &zv) ;
        MRIsampleVolume(mri_brain, xv, yv, zv, &val) ;
        v->val = val ;
        v->d = 0 ;
        v->marked = 1 ;
      }
    }


    /* now check for putamen superior to this point. If there's a lot
       of it there, then we are in basal forebrain and not cortex. */
    if (which == GRAY_WHITE)
    {
      for (adjacent = total_vox = nvox = 0, d = 0 ;
           d <= 10 ; d += 0.5, total_vox++)
      {
        xs = v->x ;
        ys = v->y ;
        zs = v->z + d ;  // sample superiorly
        MRISsurfaceRASToVoxelCached(mris, mri_aseg, xs, ys, zs, &xv, &yv, &zv);
        MRIsampleVolumeType(mri_aseg, xv, yv, zv, &val, SAMPLE_NEAREST) ;
        label = nint(val) ;
        if (label == Left_Putamen || label == Right_Putamen)
        {
          nvox++ ;
          if (d < 1.5)
          {
            adjacent = 1 ;  // right next to putamen
          }
        }
      }
      if (adjacent &&
          (double)nvox/(double)total_vox > 0.5) // more than 50% putamen
      {
        MRISvertexToVoxel(mris, v, mri_aseg, &xv, &yv, &zv) ;
        MRIsampleVolumeType(mri_aseg, xv, yv, zv, &val, SAMPLE_NEAREST) ;
        label = nint(val) ;
        compute_label_normal(mri_aseg, xv, yv, zv, label, 3, &nx, &ny, &nz, 1) ;

#if 0
        if (v->nz < 0 &&
            fabs(v->nz) > fabs(v->nx) &&
            fabs(v->nz) > fabs(v->ny))  // inferior pointing normal
#else
        if (ny > 0 &&
            fabs(ny) > fabs(nx) &&
            fabs(ny) > fabs(nz))
#endif
        {
          if (vno == Gdiag_no)
          {
            DiagBreak() ;
          }
          MRISvertexToVoxel(mris, v, mri_aseg, &xv, &yv, &zv) ;
          MRIsampleVolume(mri_brain, xv, yv, zv, &val) ;
          v->val = val ;
          v->d = 0 ;
          v->marked = 1 ;
        }
      }
    }
#if 0
    if (v->marked == 0)  // check to see if there is any cortical gm in the region in aseg
    {
      int whalf, lh, rh ;
      double scale ;
      whalf = which == GRAY_WHITE ? 5 : 3 ;
      scale = 3/(mri_aseg->xsize+mri_aseg->ysize+mri_aseg->zsize) ;
      whalf = nint(whalf*scale) ;
      MRISvertexToVoxel(mris, v, mri_aseg, &xv, &yv, &zv) ;
      lh = MRIlabelsInNbhd(mri_aseg, xv, yv, zv, whalf, Left_Cerebral_Cortex) ;
      rh = MRIlabelsInNbhd(mri_aseg, xv, yv, zv, whalf, Right_Cerebral_Cortex) ;
      if (vno == Gdiag_no)
	DiagBreak() ;
      if (lh == 0 && rh  == 0)
      {
	MRIsampleVolume(mri_brain, xv, yv, zv, &val) ;
	v->val = val ;
	v->d = 0 ;
	v->marked = 1 ;
	if (vno == Gdiag_no)
	  printf("no cortical GM found in vicinity - freezing vertex %d\n", vno) ;
      }
    }
#endif
  }

  if (Gdiag_no >= 0)
  {
    int index ;

    v = &mris->vertices[Gdiag_no] ;
    if (mris->ct)
      CTABfindAnnotation(mris->ct, v->annotation, &index);
    else
      index = -1 ;
    printf("v %d: ripflag = %d before connected components, annot %d (%d)\n",
           Gdiag_no, v->marked, v->annotation, index) ;
    if (v->marked == 0)
    {
      DiagBreak() ;
    }
    else
    {
      DiagBreak() ;
    }
  }
  MRISdilateMarked(mris, 3) ;
  MRISerodeMarked(mris, 3) ;
  MRISsegmentMarked(mris, &labels, &nlabels, 1) ;
  if (Gdiag_no > 0)
    printf("v %d: ripflag = %d after morphology\n",
           Gdiag_no, mris->vertices[Gdiag_no].marked) ;
  for (n = 0 ; n < nlabels ; n++)
  {
    if (labels[n]->n_points < 5)
    {
      int i ;
      printf("removing %d vertex label from ripped group\n",
             labels[n]->n_points) ;
      for (i = 0 ; i < labels[n]->n_points ; i++)
      {
        mris->vertices[labels[n]->lv[i].vno].marked = 0 ;
      }
    }
    if (mris->ct && CTABfindName(mris->ct, "unknown", &index) == NO_ERROR)
    {
      double pct_unknown;
      int    i ;
      VERTEX *v ;
      CTABannotationAtIndex(mris->ct, index, &annotation) ;

      for (pct_unknown = 0.0, i = 0 ; i < labels[n]->n_points ; i++)
      {
	v = &mris->vertices[labels[n]->lv[i].vno] ;
        if ((v->annotation == annotation || v->annotation == 0) &&
	    (v->marked2 == 0))  // will be 1 if a lesion or WMSA (keep frozen if so)
        {
          pct_unknown = pct_unknown + 1 ;
        }
      }
      pct_unknown /= (double)labels[n]->n_points ;
      if (pct_unknown < .6)
      {
        printf("deleting segment %d with %d points - only %2.2f%% unknown\n",n,
               labels[n]->n_points,100*pct_unknown) ;
        for (i = 0 ; i < labels[n]->n_points ; i++)
        {
	  v = &mris->vertices[labels[n]->lv[i].vno] ;
	  if (v->marked2 == 0)
	  {
	    mris->vertices[labels[n]->lv[i].vno].marked = 0 ;
	    if (labels[n]->lv[i].vno  == Gdiag_no)
	    {
	      printf("removing ripflag from v %d due to non-unknown aparc\n",
		     Gdiag_no) ;
	    }
	  }
        }
      }
    }

    LabelFree(&labels[n]) ;
  }
  free(labels) ;

  if (Gdiag_no > 0)
    printf("v %d: ripflag = %d after connected components\n",
           Gdiag_no, mris->vertices[Gdiag_no].marked) ;
  MRISripMarked(mris) ;
  MRISsetAllMarks(mris, 0) ;
  return(NO_ERROR) ;
}

#if 0
static double
mark_dura(MRI_SURFACE *mris, MRI *mri_ratio, MRI *mri_brain, double sigma)
{
  HISTOGRAM *h, *hsmooth ;
  Real      val, xs, ys, zs, xv, yv, zv, d, mean, std ;
  int       vno, bin, num, found ;
  float     mn, mx ;
  VERTEX    *v ;
  double    thresh ;

  MRIvalRange(mri_ratio, &mn, &mx) ;
  h = HISTOalloc(nint(ceil(mx))) ;
  h->bin_size = 1 ;
  for (bin = 0 ; bin < h->nbins ; bin++)
  {
    h->bins[bin] = bin ;
  }

  mean = std = 0.0 ;
  num = 0 ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
    {
      continue ;
    }
    MRISvertexToVoxel(mris, v, mri_ratio, &xv, &yv, &zv) ;
    MRIsampleVolume(mri_ratio, xv, yv, zv, &val) ;
    for (d = .5 ; d <= .5 ; d += 0.5)
    {
      xs = v->x - d*v->nx ;
      ys = v->y - d*v->ny ;
      zs = v->z - d*v->nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_ratio, xs, ys, zs, &xv, &yv, &zv);
      MRIsampleVolumeType(mri_ratio, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
      if (val < 0)
      {
        continue ;
      }
      bin = nint(val) ;
      h->counts[bin]++ ;
      mean += val ;
      std += val*val ;
      num++ ;
    }
  }

  mean /= num ;
  std = sqrt(std/num - mean*mean) ;
  thresh = mean+2*std ;
  hsmooth = HISTOsmooth(h, NULL, 2.0) ;
  HISTOmakePDF(hsmooth,hsmooth) ;
  HISTOmakePDF(h,h) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    HISTOplot(h, "h.plt") ;
    HISTOplot(hsmooth, "hs.plt") ;
  }
  MRISclearMarks(mris);
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (vno == Gdiag_no)
    {
      DiagBreak() ;
    }
    if (v->ripflag)
    {
      continue ;
    }
    d = .5 ;  // see if there is any dura inside the ribbon
    xs = v->x - d*v->nx ;
    ys = v->y - d*v->ny ;
    zs = v->z - d*v->nz ;
    MRISsurfaceRASToVoxelCached(mris, mri_ratio, xs, ys, zs, &xv, &yv, &zv);
    MRIsampleVolumeType(mri_ratio, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
    if (val > thresh)  // T2* too large for gm or csf
    {
      v->marked = 1 ;
    }
  }
  MRISdilateMarked(mris, 1) ;
  for (num = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (vno == Gdiag_no)
    {
      DiagBreak() ;
    }
    if (v->marked == 0)
    {
      continue ;
    }

    v->val2 = sigma ; // sigma for surface deformation
    found = 0 ;
    for (d = .5 ; d <= 4.0 ; d++)  // should be far enough to
      // get past dura and into brain
    {
      xs = v->x - d*v->nx ;
      ys = v->y - d*v->ny ;
      zs = v->z - d*v->nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_ratio, xs, ys, zs, &xv, &yv, &zv);
      MRIsampleVolumeType(mri_ratio, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
      if (val < thresh)  // find 1st occurrence of
        // bigger T2* - could be gm or csf
      {
        MRIsampleVolumeType
        (mri_brain, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
        v->val = val ;
        v->d = -d ;
        found = 1 ;
        if (vno == Gdiag_no)
        {
          printf("v %d: d = %2.3f, val = %2.1f\n", vno, -d, val) ;
          DiagBreak() ;
        }
        break ;
      }
    }
    if (found == 0)
    {
      v->marked = 0 ;  // don't know what else to do
    }
    else
    {
      num++ ;
    }
  }


  printf("%d vertices detected containing dura\n", num) ;
  if (Gdiag & DIAG_WRITE)
  {
    MRISwriteMarked(mris, "dura") ;
  }
  MRISripUnmarked(mris) ;
  HISTOfree(&h) ;
  HISTOfree(&hsmooth) ;
  return(thresh) ;
}

static MRI *
compute_T2star_map(MRI **mri_echos, int nvolumes)
{
  MATRIX *mD, *mT, *mP, *mTpinv ;
  int    x, y, z, e, width, height, depth ;
  MRI    *mri_T2star ;
  float  T2star, cond ;
  Real   val ;

  mD = MatrixAlloc(nvolumes, 1, MATRIX_REAL) ;
  mT = MatrixAlloc(nvolumes, 2, MATRIX_REAL) ;
  mP = MatrixAlloc(2, 1, MATRIX_REAL) ;  // log(PD) and 1/T2*

  width = mri_echos[0]->width ;
  height = mri_echos[0]->height ;
  depth = mri_echos[0]->depth ;
  mri_T2star = MRIalloc(width, height, depth, MRI_FLOAT) ;
  if (!mri_T2star)
  {
    ErrorExit(ERROR_NOMEMORY, "%s: could not allocate T2* map", Progname) ;
  }
  MRIcopyHeader(mri_echos[0], mri_T2star) ;

  for (e = 0 ; e < nvolumes ; e++)
  {
    *MATRIX_RELT(mT, e+1, 1) = 1 ;
    *MATRIX_RELT(mT, e+1, 2) = -mri_echos[e]->te ;
  }
  mTpinv = MatrixPseudoInverse(mT, NULL) ;
  if (!mTpinv)
    ErrorReturn
    (NULL,
     (ERROR_BADPARM,
      "%s: could not invert matrix for T2* estimation", Progname)) ;

  cond = MatrixConditionNumber(mT) ;
  for (x = 0 ; x < width ; x++)
  {
    for (y = 0 ; y < height ; y++)
    {
      for (z = 0 ; z < depth ; z++)
      {
        if (x == Gx && y == Gy && z == Gz)
        {
          DiagBreak() ;
        }
        for (e = 0 ; e < nvolumes ; e++)
        {
          val = MRIgetVoxVal(mri_echos[e], x, y, z, 0) ;
          *MATRIX_RELT(mD, e+1,1) = val; // log(val) ;
        }
        MatrixMultiply(mTpinv, mD, mP) ;
        val = *MATRIX_RELT(mP, 2, 1);
        if (val > 0)
        {
          T2star = 1 / val ;
        }
        else
        {
          T2star = 0 ;
        }
        T2star = val ;  // actually 1/T2star for now

        if (T2star > 10000 || T2star < -1000)
        {
          DiagBreak() ;
        }
        if (!finite(T2star))
        {
          T2star = 0 ;
        }
        MRIsetVoxVal(mri_T2star, x, y, z, 0, T2star) ;
      }
    }
  }

  MatrixFree(&mT) ;
  MatrixFree(&mP) ;
  MatrixFree(&mTpinv) ;
  MatrixFree(&mD) ;
  return(mri_T2star) ;
}
#endif

static double
compute_brain_thresh(MRI_SURFACE *mris, MRI *mri_ratio, float nstd)
{
  Real      val, xs, ys, zs, xv, yv, zv, d, mean, std ;
  int       vno, num ;
  VERTEX    *v ;
  double    thresh ;
  FILE      *logfp = NULL ;
  MRI       *mri_filled ;

  mri_filled = MRIclone(mri_ratio, NULL) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    logfp = fopen("gm.plt", "w") ;
  }
  mean = std = 0.0 ;
  num = 0 ;
  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->ripflag)
      continue ;

    MRISvertexToVoxel(mris, v, mri_ratio, &xv, &yv, &zv) ;
    MRIsampleVolume(mri_ratio, xv, yv, zv, &val) ;
    for (d = .5 ; d <= 1.0 ; d += 0.5)
    {
      xs = v->x + d*v->nx ;
      ys = v->y + d*v->ny ;
      zs = v->z + d*v->nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_ratio, xs, ys, zs, &xv, &yv, &zv);
      if (MRIgetVoxVal(mri_filled, nint(xv), nint(yv), nint(zv), 0) > 0)
	continue ;  // already visited this voxel
      MRIsetVoxVal(mri_filled, nint(xv), nint(yv), nint(zv), 0, 1) ;
      val = MRIgetVoxVal(mri_ratio, nint(xv), nint(yv), nint(zv), 0) ;
      if (val < 0)
        continue ;

      if (logfp)
        fprintf(logfp, "%f\n", val) ;

      mean += val ;
      std += val*val ;
      num++ ;
    }
  }

  if (logfp)
  {
    fclose(logfp) ;
  }
  mean /= num ;
  std = sqrt(std/num - mean*mean) ;
  thresh = mean+nstd*std ;
  MRIfree(&mri_filled) ;
  return(thresh) ;
}

#define SAMPLE_DIST .1
#define PERCENTILE   0.9

#define PIAL_SURFACE_SMOOTHING_MM 0.5

static int
compute_pial_target_locations(MRI_SURFACE *mris,
                              MRI *mri_T2,
                              LABEL **labels,
                              int nlabels,
                              int contrast_type, MRI *mri_aseg, double T2_min_inside, double T2_max_inside, 
			      double T2_min_outside, double T2_max_outside, double max_outward_dist,
			      double min_inside_peak_pct,
			      double min_outside_peak_pct,
			      double wm_weight,
                              MRI *mri_T1)
{
  Real      val, xs, ys, zs, xv, yv, zv, d, xvf, yvf, zvf, xp, yp, zp ;
  int       vno, num_in, num_out, found_bad_intensity, found ;
  int       outside_of_white, n, outside_of_pial, near_cerebellum ;
  VERTEX    *v ;
  double    min_gray_inside, min_gray_outside, max_gray_outside, max_gray_inside, thickness, nx, ny, nz, sample_dist;
  double    last_white, dist_to_white, dist_to_pial, last_dist_to_pial  ;
//  double last_pial ; 
  MRI       *mri_filled, *mri_filled_pial, *mri_tmp, *mri_dist_lh, *mri_dist_rh, *mri_dist_white, *mri_dist_pial ;

  sample_dist = MIN(SAMPLE_DIST, mri_T2->xsize/2) ;
  if (mri_aseg)
  {
    int x, y, z, label ;
    double T1, T2 ;
    mri_aseg = MRIcopy(mri_aseg, NULL) ;  // so it can be modified
    if (contrast_type == CONTRAST_T2)
    {
      for (x = 0 ; x < mri_aseg->width ; x++)
	for (y = 0 ; y < mri_aseg->height ; y++)
	  for (z = 0 ; z < mri_aseg->depth ; z++)
	  {
	    label = MRIgetVoxVal(mri_aseg, x, y, z, 0) ;
	    T1 = MRIgetVoxVal(mri_T1, x, y, z, 0) ;
	    T2 = MRIgetVoxVal(mri_T2, x, y, z, 0) ;
	    if (IS_CORTEX(label) && 1.1*T1 > T2)
	      MRIsetVoxVal(mri_aseg, x, y, z, 0, 0) ;
	  }
    }
    mri_dist_lh = MRIcloneDifferentType(mri_aseg, MRI_FLOAT) ;
    mri_dist_rh = MRIcloneDifferentType(mri_aseg, MRI_FLOAT) ;
    MRIdistanceTransform(mri_aseg, mri_dist_lh, Left_Cerebral_Cortex, 100, DTRANS_MODE_SIGNED, NULL) ;
    MRIdistanceTransform(mri_aseg, mri_dist_rh, Right_Cerebral_Cortex, 100, DTRANS_MODE_SIGNED, NULL) ;
  }

  MRISsaveVertexPositions(mris, TMP2_VERTICES) ;
  MRISrestoreVertexPositions(mris, WHITE_VERTICES) ;
  mri_tmp = MRISfillInterior(mris, mri_T2->xsize/2, NULL) ;
  mri_filled = MRIextractRegionAndPad(mri_tmp, NULL, NULL, nint(30/mri_T2->xsize)) ; MRIfree(&mri_tmp) ;
  mri_dist_white = MRIcloneDifferentType(mri_filled, MRI_FLOAT) ;
  MRIdistanceTransform(mri_filled, mri_dist_white, 1, 100, DTRANS_MODE_SIGNED, NULL) ;
  MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;
  MRIfree(&mri_filled) ;

  MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;
  MRISaverageVertexPositions(mris, 2) ;
  MRIScomputeMetricProperties(mris) ;
  mri_tmp = MRISfillInterior(mris, mri_T2->xsize/2, NULL) ;
  mri_filled_pial = MRIextractRegionAndPad(mri_tmp, NULL, NULL, nint(30/mri_T2->xsize)) ; MRIfree(&mri_tmp) ;
  mri_dist_pial = MRIcloneDifferentType(mri_filled_pial, MRI_FLOAT) ;
  MRIdistanceTransform(mri_filled_pial, mri_dist_pial, 1, 100, DTRANS_MODE_SIGNED, NULL) ;
  MRIfree(&mri_filled_pial) ;
  MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;

  min_gray_outside = T2_min_outside ;
  max_gray_outside = T2_max_outside ;
  min_gray_inside = T2_min_inside ;
  max_gray_inside = T2_max_inside ;

  printf("locating cortical regions not in interior range [%2.1f --> %2.1f], and not in exterior range [%2.1f --> %2.1f]\n",
         min_gray_inside, max_gray_inside, min_gray_outside, max_gray_outside) ;

  for (n = 0 ; n < nlabels ; n++)
  {
    LabelMarkSurface(labels[n], mris) ;
  }

  for (num_in = num_out = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    HISTOGRAM *h1, *h2, *hcdf_rev, *hs, *hcdf, *hwm, *hwm2, *hwms ;
    MRI_REGION region ;
    int whalf, wsize = 15, bin1, bin2;
    double mean, sigma, mean_wm, sigma_wm ;

    v = &mris->vertices[vno] ;
    v->targx = v->x ; v->targy = v->y ; v->targz = v->z ;
    if (v->ripflag)
      continue ;

    // compute a histogram of local GM values and use it to detect unlikely values in the interior
    // or likely values in the exterior
    MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
    whalf = (wsize-1)/2 ;
    region.x = nint(xv)-whalf ; region.y = nint(yv)-whalf ; region.z = nint(zv)-whalf ;
    region.dx = wsize ;  region.dy = wsize ; region.dz = wsize ; 
    h1 = MRIhistogramLabelRegion(mri_T2, mri_aseg, &region, Left_Cerebral_Cortex, 0) ;
    h2 = MRIhistogramLabelRegion(mri_T2, mri_aseg, &region, Right_Cerebral_Cortex, 0) ;
    HISTOadd(h1, h2, h1) ;
    hs = HISTOsmooth(h1, NULL, 4) ;
    hcdf_rev = HISTOmakeReverseCDF(h1, NULL) ;
    hcdf = HISTOmakeCDF(h1, NULL) ;
    bin1 = HISTOfindBinWithCount(hcdf, 0.01) ;
    bin2 = HISTOfindBinWithCount(hcdf_rev, 0.01) ;
    HISTOrobustGaussianFit(hs, .2, &mean, &sigma) ;
    min_gray_outside = T2_min_outside ;

    hwm = MRIhistogramLabelRegion(mri_T2, mri_aseg, &region, Left_Cerebral_White_Matter, 0) ;
    hwm2 = MRIhistogramLabelRegion(mri_T2, mri_aseg, &region, Right_Cerebral_White_Matter, 0) ;
    HISTOadd(hwm, hwm2, hwm) ;
    hwms = HISTOsmooth(hwm, NULL, 4) ;
    HISTOrobustGaussianFit(hwms, .2, &mean_wm, &sigma_wm) ;

    // one of the primary uses of the T2 deformation is to find the thin line of dark (flair)/bright (T2)
    // voxels that mark the boundary of the cortex and the cerebellum. These get partial volumed and setting
    // a global threshold causes the surfaces to settle too far in over much of the brain.
    // instead in regions that are close to cerebellum, make the thresholds less conservative
 #define CEREBELLUM_OFFSET 20
    MRISvertexToVoxel(mris, v, mri_aseg, &xv, &yv, &zv) ;
    near_cerebellum = (MRIcountValInNbhd(mri_aseg, 7, xv, yv,  zv, Left_Cerebellum_Cortex) > 0);
    near_cerebellum = near_cerebellum || (MRIcountValInNbhd(mri_aseg, 7, xv, yv,  zv, Right_Cerebellum_Cortex) > 0) ;
    if (contrast_type == CONTRAST_FLAIR)
    {
      int bin, peak ; 
      double thresh ;

      peak = HISTOfindHighestPeakInRegion(hs, 0, hs->nbins-1) ;        // most likely GM value

      /* need to worry about dark intensities in the interior of the ribbon in FLAIR, so find the first dark value (leftwards
	 from the peak) that is unlikely to be GM, and don't allow it to be in the interior of the ribbon.   */
      thresh = hs->counts[peak] * min_inside_peak_pct ;
      for (bin = peak -1 ; bin >= 0 ; bin--)
	if (hs->counts[bin] < thresh)
	  break ;
      if (bin >= 0)
      {
	min_gray_inside = hs->bins[bin] ;
	min_gray_inside = MAX(min_gray_inside, T2_min_inside+near_cerebellum*CEREBELLUM_OFFSET) ;
	if (vno == Gdiag_no)
	  printf("resetting min gray inside to be %2.3f (peak was at %2.1f)\n", min_gray_inside, hs->bins[peak]) ;
      }
      else
	min_gray_inside = T2_min_inside+near_cerebellum*CEREBELLUM_OFFSET ;

      /* Now do the same thing for exterior values, using a different threshold. That is, look for values
	 outside the current ribbon that are likely to be GM. This threshold is typically larger than the inside
         one since we trust the T1 more than the T2 - only deform if stuff outside really looks like GM */
      thresh = hs->counts[peak] * min_outside_peak_pct ;
      for (bin = peak -1 ; bin >= 0 ; bin--)
	if (hs->counts[bin] < thresh)
	  break ;
      if (bin >= 0)
      {
	min_gray_outside = hs->bins[bin] ;
	min_gray_outside = MAX(min_gray_outside, T2_min_outside+near_cerebellum*CEREBELLUM_OFFSET) ;
	if (vno == Gdiag_no)
	  printf("resetting min gray outside to be %2.3f (peak was at %2.1f)\n", min_gray_outside, hs->bins[peak]) ;
      }
      else
	min_gray_outside = T2_min_outside+near_cerebellum*CEREBELLUM_OFFSET ;

#if 0       // don't do this for T2
      thresh = hs->counts[peak] * min_inside_peak_pct ;
      for (bin = peak + 1 ; bin < hs->nbins ; bin++)
	if (hs->counts[bin] < thresh)
	  break ;
      if (bin < hs->nbins)
      {
	max_gray_inside = hs->bins[bin] ;
	max_gray_inside = MIN(max_gray_inside, T2_max_inside) ;
	if (vno == Gdiag_no)
	  printf("resetting max gray inside to be %2.3f (peak was at %2.1f)\n", max_gray_inside, hs->bins[peak]) ;
      }

      thresh = hs->counts[peak] * min_outside_peak_pct ;
      for (bin = peak + 1 ; bin < hs->nbins ; bin++)
	if (hs->counts[bin] < thresh)
	  break ;
      if (bin < hs->nbins)
      {
	max_gray_outside = hs->bins[bin] ;
	max_gray_outside = MIN(max_gray_outside, T2_max_outside) ;
	if (vno == Gdiag_no)
	  printf("resetting max gray outside to be %2.3f (peak was at %2.1f)\n", max_gray_outside, hs->bins[peak]) ;
      }
#endif
    }
    else if (contrast_type == CONTRAST_T2)
    {
      int bin, peak ; 
      double thresh ;

      peak = HISTOfindHighestPeakInRegion(hs, 0, hs->nbins-1) ;      
      thresh = hs->counts[peak] * min_inside_peak_pct ;
      for (bin = peak + 1 ; bin < hs->nbins ; bin++)
	if (hs->counts[bin] < thresh)
	  break ;
      if (bin < hs->nbins)
      {
	max_gray_inside = hs->bins[bin] ;
	max_gray_inside = MIN(max_gray_inside, T2_max_inside) ;
	if (vno == Gdiag_no)
	  printf("resetting max gray inside to be %2.3f (peak was at %2.1f)\n", max_gray_inside, hs->bins[peak]) ;
      }
      thresh *= 10 ;  // for T2 there shouldn't be any dark stuff - it is dura
      for (bin = peak - 1 ; bin >= 0 ; bin--)
	if (hs->counts[bin] < thresh)
	  break ;
      if (bin < hs->nbins)
      {
	min_gray_inside = hs->bins[bin] ;
	min_gray_inside = MAX(min_gray_inside, T2_min_inside) ;
	min_gray_inside = MAX(min_gray_inside, (wm_weight*mean_wm+mean)/(wm_weight+1)); 
	if (vno == Gdiag_no)
	  printf("resetting min gray inside to be %2.3f (peak was at %2.1f)\n", min_gray_inside, hs->bins[peak]) ;
      }

      thresh = hs->counts[peak] * min_outside_peak_pct ;
      for (bin = peak + 1 ; bin < hs->nbins ; bin++)
	if (hs->counts[bin] < thresh)
	  break ;
      if (bin < hs->nbins)
      {
	max_gray_outside = hs->bins[bin] ;
	max_gray_outside = MIN(max_gray_outside, T2_max_outside) ;
	if (vno == Gdiag_no)
	  printf("resetting max gray outside to be %2.3f (peak was at %2.1f)\n", max_gray_outside, hs->bins[peak]) ;
      }
      for (bin = peak - 1 ; bin >= 0 ; bin--)
	if (hs->counts[bin] < thresh)
	  break ;
      if (bin < hs->nbins)
      {
	min_gray_outside = hs->bins[bin] ;
	min_gray_outside = MIN(min_gray_outside, T2_min_outside) ;
	min_gray_outside = MAX(min_gray_outside, (wm_weight*mean_wm+mean)/(wm_weight+1));
	if (vno == Gdiag_no)
	  printf("resetting min gray outside to be %2.3f (peak was at %2.1f)\n", min_gray_outside, hs->bins[peak]) ;
      }
    }

//    T2_min_inside = T2_min_outside = mean-nsigma_below*sigma ; T2_max_inside = T2_max_outside = mean + nsigma_above*sigma ;
    if (vno == Gdiag_no)
    {

      printf("T2 range %2.1f --> %2.1f, %2.1f +- %2.1f\n",  hcdf->bins[bin1], hcdf_rev->bins[bin2], mean, sigma) ;
      printf("gm interior range %2.1f --> %2.1f\n",  min_gray_inside, max_gray_inside) ;
      printf("gm exterior range %2.1f --> %2.1f\n",  min_gray_outside, max_gray_outside) ;
      HISTOplot(hwm, "hw.plt") ;
      HISTOplot(h1, "h.plt") ;
      HISTOplot(hs, "hs.plt") ;
      HISTOplot(hcdf_rev, "hr.plt") ;
      HISTOplot(hcdf, "hc.plt") ;
      DiagBreak() ;   // XXX
    }

    HISTOfree(&h1) ; HISTOfree(&h2) ; HISTOfree(&hcdf) ; HISTOfree(&hcdf_rev) ; HISTOfree(&hs) ;
    HISTOfree(&hwm) ; HISTOfree(&hwm2) ; HISTOfree(&hwms) ;

    nx = v->x - v->whitex ; ny = v->y - v->whitey ; nz = v->z - v->whitez ;
    thickness = sqrt(SQR(nx)+SQR(ny)+SQR(nz)) ;
    if (FZERO(thickness))
      continue ;

    MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
    nx /= thickness ; ny /= thickness ; nz /= thickness ;
    found_bad_intensity = 0 ;

    // check for illegal intensities in the interior of the ribbon
    d = MIN(0.5, thickness/2) ; 
    xs = v->whitex + d*nx ; ys = v->whitey + d*ny ; zs = v->whitez + d*nz ;
    outside_of_white = 0 ;
    for ( ; d <= thickness ; d += sample_dist)
    {
      xs = v->whitex + d*nx ; ys = v->whitey + d*ny ; zs = v->whitez + d*nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
      MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
      if (val <= 0)
        continue ;

      MRISsurfaceRASToVoxelCached(mris, mri_dist_white, xs, ys, zs, &xvf, &yvf, &zvf);
      MRIsampleVolume(mri_dist_white, xvf, yvf, zvf, &dist_to_white) ;
      if (dist_to_white <= 0)
        break ;

      if (dist_to_white <= 1)
	continue ;  // too close to wm, avoid partial voluming

      if (val < min_gray_inside && (val > nint(min_gray_outside*.75)) && !outside_of_white)
	continue ;   // all dark intensities - white boundary probably in wrong place
      if (val >= min_gray_inside) 
	outside_of_white = 1 ;

      if (mri_aseg != NULL)
      {
	int label, xv, yv, zv ;
	Real xvf, yvf, zvf ;
	
	MRISsurfaceRASToVoxelCached(mris, mri_aseg, xs, ys, zs, &xvf, &yvf, &zvf);
	xv = nint(xvf) ; yv = nint(yvf) ; zv = nint(zvf) ;
	label = MRIgetVoxVal(mri_aseg, xv, yv, zv, 0) ;
	if (vno == Gdiag_no)
	  printf("v %d: label distance %2.2f = %s @ (%d %d %d), val = %2.1f\n",
		 vno, d, cma_label_to_name(label),xv, yv, zv,val) ;
	if ((mris->hemisphere == LEFT_HEMISPHERE && IS_RH_CLASS(label)) ||
	    (mris->hemisphere == RIGHT_HEMISPHERE && IS_LH_CLASS(label)))
	{
	  if (vno == Gdiag_no)
	      printf("v %d: terminating search at distance %2.2f due to presence of contra tissue (%s)\n",
		     vno, d, cma_label_to_name(label)) ;
	  found_bad_intensity = 1 ;
	  break ;
	}
      }
      if (val < min_gray_inside || val > max_gray_inside)
      {
	if (vno == Gdiag_no)
	  printf("illegal intensity %2.1f found at d=%2.2f, vox=(%2.1f, %2.1f, %2.1f)\n", val, d,xv,yv,zv) ;
        found_bad_intensity = 1 ;
        break ;
      }
    }
    if (vno == Gdiag_no)
      DiagBreak() ;  // XXX
    if (found_bad_intensity)
    {
      num_in++ ;
      // target surface so interior is good value and exterior is bad gm value
      v->targx = xs - (sample_dist/2*nx) ; v->targy = ys - (sample_dist/2*ny) ; v->targz = zs - (sample_dist/2*nz) ;
      MRISsurfaceRASToVoxelCached(mris, mri_T2,
                                  v->targx, v->targy, v->targz,
                                  &xv, &yv, &zv);
      MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
      v->val = val ; v->marked = 1 ; v->marked2++ ;  // marked2 will keep track of how many times pial surface was found
      v->val2 = pial_sigma ; v->d = (d+sample_dist/2)-thickness ;
      if (vno == Gdiag_no)
      {
        printf("vno %d: resetting target location to be d=%2.2f, "
               "(%2.1f %2.1f %2.1f), val @ (%2.1f, %2.1f, %2.1f) = %2.0f\n",
               vno, d-thickness,
               v->targx, v->targy, v->targz,
               xv, yv, zv, val) ;
        DiagBreak() ;
      }
    }
    else  // no invalid intensities found in the interior - not deforming inwards. Now check for valid ones in the exterior.
    {
      outside_of_white = outside_of_pial = 0 ;
      found = 1 ;
      last_white = 0 ;
      MRISsurfaceRASToVoxelCached(mris, mri_dist_pial, v->x, v->y, v->z, &xvf, &yvf, &zvf);
      MRIsampleVolume(mri_dist_pial, xvf, yvf, zvf, &last_dist_to_pial) ;
      // follow the gradient of the distance transform of the pial surface outwards
      // as the surface normal direction becomes meaningless a few mm out from surface
      xp = xvf ; yp = yvf ; zp = zvf ;
      for (d = 0 ; d <= max_outward_dist ; d += sample_dist)
      {
	// [xyz]s are in surface coords, shared by all vols. [xyz]v* are in specific volume voxel coords
	xs = v->x+d*v->nx ; ys = v->y+d*v->ny ;  zs = v->z+d*v->nz ; 
	MRISsurfaceRASToVoxelCached(mris, mri_dist_pial,  xs, ys, zs, &xp, &yp, &zp) ;

        MRISsurfaceRASToVoxelCached(mris, mri_dist_white, xs, ys, zs, &xvf, &yvf, &zvf);
	MRIsampleVolume(mri_dist_white, xvf, yvf, zvf, &dist_to_white) ;
	
        if (MRIindexNotInVolume(mri_dist_white, nint(xvf), nint(yvf), nint(zvf)) || dist_to_white > 0)
          outside_of_white = 1 ;
        else if (!outside_of_white)  // haven't gotten out of the wm yet - ignore intensities
        {
          last_white = d ;
          continue ;
        }
        MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
        MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;

	
	// check to see if we are outside of the pial surface
        MRISsurfaceRASToVoxelCached(mris, mri_dist_pial, xs, ys, zs, &xvf, &yvf, &zvf);
	MRIsampleVolume(mri_dist_pial, xvf, yvf, zvf, &dist_to_pial) ;


	if (dist_to_pial-last_dist_to_pial < -sample_dist)   // oblique to pial surface - tried to step outwards but distance decreased
	{
	  DiagBreak() ;
	  if (vno == Gdiag_no)
	    printf("v %d: terminating search at distance %2.1f (%2.1f, %2.1f, %2.1f) due to pial dist decrease\n",
		   vno, d, xs, ys, zs) ;
	  if (val > min_gray_outside && val < max_gray_outside) // still in GM 
	  {
	    found = 0 ; // hmmm, I guess don't trust FLAIR over T1 here. Assume that we didn't find it
	    d = 0 ;
	  }
	  break ;
	}

        if (dist_to_pial > mri_dist_pial->xsize) // outside of pial surface
          outside_of_pial = 1 ;
	else if (outside_of_pial && (dist_to_pial < mri_dist_pial->xsize || dist_to_pial < last_dist_to_pial))  // was outside of pial and reentered it
	{
	  if (vno == Gdiag_no)
	    printf("v %d: terminating searrch at distance %2.1f (%2.1f, %2.1f, %2.1f) due to pial reentry\n",
		   vno, d, xs, ys, zs) ;
	  d = 0 ;
	  found = 0 ;
	  break ;
	}
	last_dist_to_pial = dist_to_pial ;

        if (outside_of_white && (dist_to_white <= 0))  // interior of wm surface, probably normals are messed up
        {
          if (d-last_white > .5)  // really out of white and not just grazing a corner of the surface
          {
	    if (vno == Gdiag_no)
	      printf("v %d: terminating search at distance %2.1f (%2.1f, %2.1f, %2.1f) due to wm reentry\n",
		     vno, d, xs, ys, zs) ;
            d = 0 ;
	    found = 0 ;
            break ;
          }
          else
            last_white = d ;  // didn't really leave wm
        }
#if 0
	if ((d < 2*sample_dist) && ((val <  min_gray_outside) || (val > max_gray_outside)))
	{
	  d = 0 ;
	  found = 0 ;
	  break ;  // stuff right outside current pial surface is not definitely gray matter
	}
#endif

        if (val < 0)
          continue ;

	if (mri_aseg != NULL)
	{
	  int label, xv, yv, zv ;
	  Real xvf, yvf, zvf ;
	  
	  MRISsurfaceRASToVoxelCached(mris, mri_aseg, xs, ys, zs, &xvf, &yvf, &zvf);
	  xv = nint(xvf) ; yv = nint(yvf) ; zv = nint(zvf) ;
	  label = MRIgetVoxVal(mri_aseg, xv, yv, zv, 0) ;
	  if (vno == Gdiag_no)
	    printf("v %d: label distance %2.2f = %s @ (%d %d %d), dist to white, pial: %2.1f, %2.1f, val = %2.1f\n",
		   vno, d, cma_label_to_name(label),xv, yv, zv, dist_to_white, dist_to_pial, val) ;
	  if ((mris->hemisphere == LEFT_HEMISPHERE && IS_RH_CLASS(label)) ||
	      (mris->hemisphere == RIGHT_HEMISPHERE && IS_LH_CLASS(label)))
	  {
	    if (vno == Gdiag_no)
	      printf("v %d: terminating search at distance %2.2f due to presence of contra tissue (%s)\n",
		     vno, d, cma_label_to_name(label)) ;
	    break ;
	  }
	  if (IS_UNKNOWN(label))
	  {
	    double dleft, dright, hemi_dist, ohemi_dist ;

	    MRIsampleVolume(mri_dist_lh, xvf, yvf, zvf, &dleft) ;
	    MRIsampleVolume(mri_dist_rh, xvf, yvf, zvf, &dright) ;
	    if (mris->hemisphere == LEFT_HEMISPHERE)
	    {
	      hemi_dist = dleft ;
	      ohemi_dist = dright ;
	    }
	    else
	    {
	      ohemi_dist = dleft ;
	      hemi_dist = dright ;
	    }
	    if (ohemi_dist <= (hemi_dist+SAMPLE_DIST)) // keep them from crossing each other
	    {
	      if (vno == Gdiag_no)
		printf("v %d: terminating search at distance %2.2f due to presence of contra hemi %2.1fmm dist <= hemi dist %2.1fmm\n",
		       vno, d, ohemi_dist, hemi_dist) ;
	      break ;
	    }
	  }
	  if (IS_CEREBELLAR_GM(label))
	  {
	    found = 0 ;
	    if (vno == Gdiag_no)
	      printf("v %d: terminating search at distance %2.2f due to presence of cerebellar gm (%s))\n",
		     vno, d, cma_label_to_name(label)) ;
	    break ;
	  }
	}

        if ((val < min_gray_inside && dist_to_white>1.2) ||  (val > max_gray_inside))  
          break ;
      }
      if (vno == Gdiag_no)
	DiagBreak() ;  // XXX
#if 0
      if (val > min_gray_inside && val < min_gray_outside)
	found = 0 ;  // trust T1 and don't displace surface for stuff that is marginal outside
#endif
      if (d <= max_outward_dist && d > 4)
	DiagBreak() ;
      if (!found || d > max_outward_dist)  // couldn't find pial surface
      {
	int xv, yv, zv, label ;

	v->marked = 0 ;
	v->d = d = 0 ;

#define NUDGE_DIST 0.5

	// check just outside current  pial surface, and nudge outwards if it looks a lot like gm
	found = 1 ;
#if 0
	xs = v->x + 0.1*v->nx ; ys = v->y + 0.1*v->ny ;  zs = v->z + 0.1*v->nz ; 
	MRISsurfaceRASToVoxelCached(mris, mri_dist_pial, xs, ys, zs, &xvf, &yvf, &zvf);
	MRIsampleVolume(mri_dist_pial, xvf, yvf, zvf, &dist_to_pial) ;
	if (dist_to_pial < 0)  // not really in the outwards direction
	  found = 0 ;
#endif

	xs = v->x + NUDGE_DIST*v->nx ; ys = v->y + NUDGE_DIST*v->ny ;  zs = v->z + NUDGE_DIST*v->nz ; 
	MRISsurfaceRASToVoxelCached(mris, mri_aseg, xs, ys, zs, &xvf, &yvf, &zvf);
	xv = nint(xvf) ; yv = nint(yvf) ; zv = nint(zvf) ;
	label = MRIgetVoxVal(mri_aseg, xv, yv, zv, 0) ;
	if (IS_CEREBELLAR_GM(label) ||
	    ((mris->hemisphere == LEFT_HEMISPHERE && IS_RH_CLASS(label)) ||
	     (mris->hemisphere == RIGHT_HEMISPHERE && IS_LH_CLASS(label))))
	  found = 0 ;

	MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xvf, &yvf, &zvf);
	MRIsampleVolume(mri_T2, xvf, yvf, zvf, &val) ;
	if (val < min_gray_outside || val > max_gray_outside)
	  found = 0 ;

	if (found)
	  d = NUDGE_DIST+sample_dist ;  // will be decremented later as here it is the dist to the outside, not the border

	if (vno == Gdiag_no)
	{
	  if (found)
	    printf("v %d: could nudging pial surface 0.5mm outwards despite not detecting exterior\n", vno) ;
	  else
	    printf("v %d: could not find pial surface\n", vno) ;
	}
      }
      else
	v->marked = found ;
      if (found)
      {
        d -= sample_dist ;
	xs = v->x+d*v->nx ; ys = v->y+d*v->ny ;  zs = v->z+d*v->nz ; 
        num_out++ ;
	v->targx = xs ; v->targy = ys ; v->targz = zs ; v->d = d ;
	v->d = d ;
	v->marked2++ ;
      }
      else   // couldn't find pial surface outside so leave it as iis
      {
	d = v->d = 0 ;
//	v->targx = v->x+d*v->nx ; v->targy = v->y+d*v->ny ; v->targz = v->z+d*v->nz ;
	if (v->marked2 > 0)  // found surface previously - let it be where it is
	{
	  v->targx = v->x ; v->targy = v->y ; v->targz = v->z ;
	}
	else
	{
	  v->targx = v->pialx ; v->targy = v->pialy ; v->targz = v->pialz ;
	}
      }

      if (Gdiag)  // debugging
      {
	MRISsurfaceRASToVoxelCached(mris, mri_dist_pial,
				    v->targx, v->targy, v->targz,
				    &xvf, &yvf, &zvf);
	MRIsampleVolume(mri_dist_pial, xvf, yvf, zvf, &dist_to_pial) ;
	if (dist_to_pial < -1.5)
	  DiagBreak() ;
	MRISsurfaceRASToVoxelCached(mris, mri_dist_white,
				    v->targx, v->targy, v->targz,
				    &xvf, &yvf, &zvf);
	MRIsampleVolume(mri_dist_white, xvf, yvf, zvf, &dist_to_white) ;
	if (dist_to_white < .5)
	  DiagBreak() ;

	if (d > 10)
	  DiagBreak() ;
      }
      MRISsurfaceRASToVoxelCached(mris, mri_T2, v->targx, v->targy, v->targz, &xv, &yv, &zv);
      MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
      v->val = val ;
      v->val2 = pial_sigma ;
      if (vno == Gdiag_no)
        printf("vno %d: target location found %2.1f mm outwards "
               "(%2.1f, %2.1f, %2.1f) --> vox (%2.1f %2.1f %2.1f)\n",
               vno,d,v->targx,v->targy,v->targz, xv, yv, zv) ;
    }
  }
  if (Gdiag & DIAG_WRITE)
  {
    char fname[STRLEN] ;
    sprintf(fname, "%ch.dist.mgz", mris->hemisphere == LEFT_HEMISPHERE ? 'l' : 'r') ;
    printf("writing distances to %s\n", fname) ;
    MRISwriteD(mris, fname) ;
    MRISsaveVertexPositions(mris, TMP2_VERTICES) ;
    MRISrestoreVertexPositions(mris, TARGET_VERTICES) ;
    sprintf(fname, "%ch.targets", mris->hemisphere == LEFT_HEMISPHERE ? 'l' : 'r') ;
    printf("writing target locations to %s\n", fname) ;
    MRISwrite(mris, fname) ;
    MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;
  }
  MRIfree(&mri_dist_white) ; MRIfree(&mri_dist_pial) ;  

  if (mri_aseg)
  {
    MRIfree(&mri_dist_lh) ;
    MRIfree(&mri_dist_rh) ;
    MRIfree(&mri_aseg) ;
  }
  printf("%d surface locations found to contain inconsistent "
         "values (%d in, %d out)\n",
         num_in+num_out, num_in, num_out) ;
  return(NO_ERROR) ;
}

static int
compute_white_target_locations(MRI_SURFACE *mris,
			       MRI *mri_T2,
			       MRI *mri_aseg,
			       float nstd_below,
			       float nstd_above,
			       int contrast_type, float T2_min_inside, float T2_max_inside, 
			       int below_set, int above_set, double wsigma)
{
  Real      val, xs, ys, zs, xv, yv, zv, d, mean, std, xvf, yvf, zvf ;
  int       vno, num_in, num_out, label ;
  int       done, bin, niter, histo_nbins, in_white ;
  VERTEX    *v ;
  double    min_white, max_white, thickness, nx, ny, nz, mn, mx, sig, sample_dist;
  HISTOGRAM *h, *hcdf, *hcdf_rev ;
  MRI       *mri_filled, *mri_tmp ;
  float     min_val, max_val ;

  sample_dist = MIN(SAMPLE_DIST, mri_T2->xsize/2) ;
  MRIvalRange(mri_T2, &min_val, &max_val) ;
  if (max_val > MAX_HISTO_BINS)
    max_val = MAX_HISTO_BINS ;
  histo_nbins = max_val+1 ;
  h = HISTOalloc(histo_nbins) ; mx = max_val ;
  HISTOinit(h, histo_nbins, 0, max_val) ;

  MRISsaveVertexPositions(mris, TMP2_VERTICES) ;
  MRISrestoreVertexPositions(mris, WHITE_VERTICES) ;
  mri_filled = MRIclone(mri_T2, NULL) ;
  mri_tmp = MRISfillInterior(mris, mri_T2->xsize/2, NULL) ;
  mri_filled = MRIextractRegionAndPad(mri_tmp, NULL, NULL, 30) ; MRIfree(&mri_tmp) ;
  MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;
  MRIScomputeMetricProperties(mris) ;

  mean = std = 0.0 ;
  num_in = 0 ;
  niter = 0 ;
  do
  {
    for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag)
        continue ;

      if (vno == Gdiag_no)
        DiagBreak() ;

      nx = v->pialx - v->whitex ; ny = v->pialy - v->whitey ; nz = v->pialz - v->whitez ;
      thickness = sqrt(SQR(nx)+SQR(ny)+SQR(nz)) ;
      if (FZERO(thickness))  // pial and white in same place - no cortex here
        continue ;

      MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
      MRIsampleVolume(mri_T2, xv, yv, zv, &val) ;
      for (d = 1 ; d <= 3 ; d += sample_dist)
      {
        xs = v->x - d*v->nx ; ys = v->y - d*v->ny ; zs = v->z - d*v->nz ;
        MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
        MRISsurfaceRASToVoxelCached(mris, mri_filled, xs, ys, zs, &xvf, &yvf, &zvf);
        MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
        if (val <= 0 || (MRIgetVoxVal(mri_filled, nint(xvf), nint(yvf), nint(zvf), 0) == 0))
          continue ;  // no longer in wm or out of brainmask (not sure how that could happen)

	label = MRIgetVoxVal(mri_aseg, nint(xv), nint(yv), nint(zv), 0) ;
	if (!IS_WM(label))
	  continue ;  // not in  white matter according to aseg

        mean += val ;
        std += val*val ;
        num_in++ ;
        HISTOaddSample(h, val, 0, mx) ;
      }
    }
    hcdf_rev = HISTOmakeReverseCDF(h, NULL) ;
    hcdf = HISTOmakeCDF(h, NULL) ;
    bin = HISTOfindBinWithCount(hcdf, PERCENTILE);
    if (bin < ceil(PERCENTILE*histo_nbins)/4)  // data range is too compressed for histogram to represent
    {
      done = (niter > 10) ;
      if (niter++ > 0)
        mx /= 2 ;
      else
        mx = 2*bin/(.9) ;  // first time - take an educated guess

      HISTOinit(h, histo_nbins, 0, mx) ;
      printf("compressed histogram detected, changing bin size to %f\n",
             h->bin_size) ;
    }
    else
      done = 1 ;
  }
  while (!done) ;
  mean /= num_in ;
  std = sqrt(std/num_in - mean*mean) ;
  max_white = mean+nstd_above*std ;
  min_white = mean-nstd_below*std ;


  HISTOrobustGaussianFit(h, .9, &mn, &sig) ;
  if (Gdiag & DIAG_WRITE)
    HISTOplot(h, "h.plt") ;

  if (sig < 5)   // something failed in robust fit - use normal fits
  {
    printf("invalid robust sig %2.3f detected, using gaussian std %2.3f instead\n", sig, std) ;
    sig = std ;
  }
  max_white = mn+nstd_above*sig ;
  min_white = mn-nstd_below*sig ;
  if (!above_set || !below_set)
  {
    int bin ;
    bin = HISTOfindBinWithCount(hcdf, 0.01) ;
//    bin = HISTOfindBinWithCount(hcdf, 0.005) ;  // changed to work better with CIMBI data
    if (!below_set)
    {
      printf("min bin %d (%2.1f) found with count %2.3f\n", bin, hcdf->bins[bin], hcdf->counts[bin]) ;
      if (hcdf->bins[bin] > min_white)
      {
	printf("resetting min white %2.1f based on CDF to %2.1f\n", min_white, hcdf->bins[bin]) ;
	min_white = hcdf->bins[bin];
      }
    }

    if (!above_set)
    {
      bin = HISTOfindBinWithCount(hcdf_rev, 0.01) ;
      printf("max bin %d (%2.1f) found with count %2.3f\n", bin, hcdf_rev->bins[bin], hcdf_rev->counts[bin]) ;
      if (hcdf_rev->bins[bin] < max_gray)
      {
	printf("resetting max gray %2.1f based on CDF to %2.1f\n", max_gray, hcdf_rev->bins[bin]) ;
	max_gray = hcdf_rev->bins[bin];
      }
    }
  }
  if (T2_min_inside >= 0)
  {
    min_white = T2_min_inside ;
    printf("using user specified min white threshold %2.1f\n", min_white) ;
  }
  if (T2_max_inside >= 0)
  {
    max_white = T2_max_inside ;
    printf("using user specified max white threshold %2.1f\n", max_white) ;
  }
  printf("locating white matter regions not in the range [%2.2f %2.2f], "
         "wm=%2.2f+-%2.2f, and vertices in regions > %2.1f\n",
         min_white, max_white, mn, sig, mn-.5*sig) ;

  for (num_in = num_out = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->targx = v->x ; v->targy = v->y ; v->targz = v->z ;
    if (v->ripflag)
      continue ;

    if (vno == Gdiag_no)
      DiagBreak() ;

    nx = v->pialx - v->whitex ; ny = v->pialy - v->whitey ; nz = v->pialz - v->whitez ;
    thickness = sqrt(SQR(nx)+SQR(ny)+SQR(nz)) ;
    if (FZERO(thickness))
      continue ;

    MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
    MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;

    if (val > max_white)   // value at vertex is too bright
    {
      in_white = 0 ;
      for (d = sample_dist ; d <= 2 ; d += sample_dist)
      {
	xs = v->x - d*nx ; ys = v->y - d*ny ; zs = v->z - d*nz ;
	MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
	MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
	if (val <= 0)
	  continue ;

	if (val < max_white)
	  break ;   // found a good intensity
	
	MRISsurfaceRASToVoxelCached(mris, mri_filled, xs, ys, zs, &xvf, &yvf, &zvf);
	if (MRIgetVoxVal(mri_filled, nint(xvf), nint(yvf), nint(zvf), 0) == 0) 
	{
	  if (in_white)  // was in white previously but have left it
	    break ;   // not in white matter anymore
	}
	else
	  in_white = 1 ;  // in the interior of the white surface
	
	if (val < min_white || val >= max_white)
	{
	  if (vno == Gdiag_no)
	    printf("illegal intensity %2.1f found at d=%2.2f, vox=(%2.1f, %2.1f, %2.1f)\n", val, d,xv,yv,zv) ;
	}
      }
      num_in++ ;
      v->targx = v->x - d*nx ; v->targy = v->y - d*ny ; v->targz = v->z - d*nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_T2,
                                  v->targx, v->targy, v->targz,
                                  &xv, &yv, &zv);
      MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
      v->val = val ;
      v->val2 = wsigma ;
      if (vno == Gdiag_no)
      {
        printf("vno %d: resetting target location to be d=%2.2f, "
               "(%2.1f %2.1f %2.1f), val @ (%2.1f, %2.1f, %2.1f) = %2.0f\n",
               vno, d,
               v->targx, v->targy, v->targz,
               xv, yv, zv, val) ;
        DiagBreak() ;
      }
    }
  }
  MRIfree(&mri_filled) ; HISTOfree(&h) ; HISTOfree(&hcdf) ; HISTOfree(&hcdf_rev) ; 

  printf("%d surface locations found to contain inconsistent values (%d in, %d out)\n",
         num_in+num_out, num_in, num_out) ;
  return(NO_ERROR) ;
}

#define T1I  0
#define T2I  1
#define WMI  0
#define GMI  1

static MRI *
compute_white_matter_intensities(MRI_SURFACE *mris,
				 MRI *mri_T2,
				 MRI *mri_T1,
				 MRI *mri_aseg,
				 float nstd_above,
				 float nstd_below,
				 MRI *mri_dst)
{
  Real      val, xs, ys, zs, xv, yv, zv, d, mean, std, xvf, yvf, zvf, sum, dist,means[2][2],
    stds[2][2], counts[2];
  int       vno, num, label, xk, yk, zk, xi, yi, zi, whalf ;
  int       done, bin, niter, histo_nbins, in_white, count, filled_wm, filled_pial ;
  VERTEX    *v ;
  double    min_white, max_white, thickness, nx, ny, nz, mn, mx, sig, mn1, sig1,sample_dist;
  HISTOGRAM *h, *hcdf, *hcdf_rev ;
  HISTOGRAM *h1 ;
  MRI       *mri_filled, *mri_tmp, *mri_dist, *mri_filled_pial ;
  float     min_val, max_val ;

  sample_dist = MIN(SAMPLE_DIST, mri_T2->xsize/2) ;
  if (mri_dst == NULL)
    mri_dst = MRIallocSequence(mris->nvertices, 1, 1, MRI_FLOAT, 4) ;

  MRIvalRange(mri_T1, &min_val, &max_val) ;
  if (max_val > MAX_HISTO_BINS)
    max_val = MAX_HISTO_BINS ;
  histo_nbins = max_val+1 ;
  h1 = HISTOalloc(histo_nbins) ; mx = max_val ;
  HISTOinit(h1, histo_nbins, 0, max_val) ;

  MRIvalRange(mri_T2, &min_val, &max_val) ;
  if (max_val > MAX_HISTO_BINS)
    max_val = MAX_HISTO_BINS ;
  histo_nbins = max_val+1 ;
  h = HISTOalloc(histo_nbins) ; mx = max_val ;
  HISTOinit(h, histo_nbins, 0, max_val) ;

  MRISreadCurvatureFile(mris, "sulc") ;

  MRISsaveVertexPositions(mris, TMP2_VERTICES) ;
  MRISrestoreVertexPositions(mris, PIAL_VERTICES) ;
  mri_tmp = MRISfillInterior(mris, mri_T2->xsize/2, NULL) ;
  mri_filled_pial = MRIextractRegionAndPad(mri_tmp, NULL, NULL, 30) ; MRIfree(&mri_tmp) ;
  MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;

  MRIScomputeMetricProperties(mris) ;
  MRISsaveVertexPositions(mris, TMP2_VERTICES) ;
  MRISrestoreVertexPositions(mris, WHITE_VERTICES) ;
  mri_tmp = MRISfillInterior(mris, mri_T2->xsize/2, NULL) ;
  mri_filled = MRIextractRegionAndPad(mri_tmp, NULL, NULL, 30) ; MRIfree(&mri_tmp) ;
  MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;
  MRIScomputeMetricProperties(mris) ;

  mri_dist = MRIScomputeDistanceToSurface(mris, NULL, mri_filled->xsize);
  mean = std = 0.0 ;
  num = 0 ;
  niter = 0 ;
  do
  {
    for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag)
        continue ;

      if (vno == Gdiag_no)
        DiagBreak() ;

      nx = v->pialx - v->whitex ; ny = v->pialy - v->whitey ; nz = v->pialz - v->whitez ;
      thickness = sqrt(SQR(nx)+SQR(ny)+SQR(nz)) ;
      if (FZERO(thickness))  // pial and white in same place - no cortex here
        continue ;

      MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
      MRIsampleVolume(mri_T2, xv, yv, zv, &val) ;
      for (d = 1 ; d <= 3 ; d += sample_dist)
      {
        xs = v->x - d*v->nx ; ys = v->y - d*v->ny ; zs = v->z - d*v->nz ;
        MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
        MRISsurfaceRASToVoxelCached(mris, mri_filled, xs, ys, zs, &xvf, &yvf, &zvf);
        MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
        if (val <= 0 || (MRIgetVoxVal(mri_filled, nint(xvf), nint(yvf), nint(zvf), 0) == 0))
          continue ;  // no longer in wm or out of brainmask (not sure how that could happen)

	label = MRIgetVoxVal(mri_aseg, nint(xv), nint(yv), nint(zv), 0) ;
	if (!IS_WM(label))
	  continue ;  // not in  white matter according to aseg

        mean += val ;
        std += val*val ;
        num++ ;
        HISTOaddSample(h, val, 0, mx) ;
        MRIsampleVolumeType(mri_T1, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
        HISTOaddSample(h1, val, 0, mx) ;
      }
    }
    hcdf_rev = HISTOmakeReverseCDF(h, NULL) ;
    hcdf = HISTOmakeCDF(h, NULL) ;
    bin = HISTOfindBinWithCount(hcdf, PERCENTILE);
    if (bin < ceil(PERCENTILE*histo_nbins)/4)  // data range is too compressed for histogram to represent
    {
      done = (niter > 10) ;
      if (niter++ > 0)
        mx /= 2 ;
      else
        mx = 2*bin/(.9) ;  // first time - take an educated guess

      HISTOinit(h, histo_nbins, 0, mx) ;
      printf("compressed histogram detected, changing bin size to %f\n",
             h->bin_size) ;
    }
    else
      done = 1 ;
  }
  while (!done) ;
  mean /= num ;
  std = sqrt(std/num - mean*mean) ;
  max_white = mean+nstd_above*std ; min_white = mean-nstd_below*std ;

  HISTOrobustGaussianFit(h1, .9, &mn1, &sig1) ;
  HISTOrobustGaussianFit(h, .9, &mn, &sig) ;
  if (Gdiag & DIAG_WRITE)
    HISTOplot(h, "h.plt") ;

  if (sig < 5)   // something failed in robust fit - use normal fits
  {
    printf("invalid robust sig %2.3f detected, using gaussian std %2.3f instead\n", sig, std) ;
    sig = std ;
  }
  max_white = mn+nstd_above*sig ;
  min_white = mn-nstd_below*sig ;
  if (!above_set || !below_set)
  {
    int bin ;
    bin = HISTOfindBinWithCount(hcdf, 0.01) ;
    if (!below_set)
    {
      printf("min bin %d (%2.1f) found with count %2.3f\n", bin, hcdf->bins[bin], hcdf->counts[bin]) ;
      if (hcdf->bins[bin] > min_white)
      {
	printf("resetting min white %2.1f based on CDF to %2.1f\n", min_white, hcdf->bins[bin]) ;
	min_white = hcdf->bins[bin];
      }
    }

    if (!above_set)
    {
      bin = HISTOfindBinWithCount(hcdf_rev, 0.01) ;
      printf("max bin %d (%2.1f) found with count %2.3f\n", bin, hcdf_rev->bins[bin], hcdf_rev->counts[bin]) ;
      if (hcdf_rev->bins[bin] < max_gray)
      {
	printf("resetting max gray %2.1f based on CDF to %2.1f\n", max_gray, hcdf_rev->bins[bin]) ;
	max_gray = hcdf_rev->bins[bin];
      }
    }
  }
  if (T2_min_inside >= 0)
  {
    min_white = T2_min_inside ;
    printf("using user specified min white threshold %2.1f\n", min_white) ;
  }
  if (T2_max_inside >= 0)
  {
    max_white = T2_max_inside ;
    printf("using user specified max white threshold %2.1f\n", max_white) ;
  }
  printf("locating white matter regions not in the range [%2.2f %2.2f], "
         "wm=%2.2f+-%2.2f, and vertices in regions > %2.1f\n",
         min_white, max_white, mn, sig, mn-.5*sig) ;

  for (vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->targx = v->x ; v->targy = v->y ; v->targz = v->z ;
    if (v->ripflag)
      continue ;

    if (vno == Gdiag_no)
      DiagBreak() ;

    nx = v->pialx - v->whitex ; ny = v->pialy - v->whitey ; nz = v->pialz - v->whitez ;
    thickness = sqrt(SQR(nx)+SQR(ny)+SQR(nz)) ;
    if (FZERO(thickness))
      continue ;

    MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
    MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;

    in_white = 0 ;
    for (num = 0, sum = 0.0, d = sample_dist ; d <= 2 ; d += sample_dist)
    {
      xs = v->x - d*v->nx ; ys = v->y - d*v->ny ; zs = v->z - d*v->nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
      MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
      if (val <= 0)
	continue ;
      
      if (val < max_white)
	break ;   // found a good intensity

      MRISsurfaceRASToVoxelCached(mris, mri_filled, xs, ys, zs, &xvf, &yvf, &zvf);
      if (MRIgetVoxVal(mri_filled, nint(xvf), nint(yvf), nint(zvf), 0) == 0) 
      {
	if (in_white)  // was in white previously but have left it
	  break ;   // not in white matter anymore
      }
      else
	in_white = 1 ;  // in the interior of the white surface
      
      sum += val ; num++ ;
    }

    MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
    whalf = 3 ;

    memset(means, 0, sizeof(means)) ;
    memset(stds, 0, sizeof(stds)) ;
    memset(counts, 0, sizeof(counts)) ;

    for (sum = 0, count = num = 0, xk = -whalf ; xk <= whalf ; xk++)
    {
      xi = mri_T2->xi[nint(xv+xk)] ;
      for (yk = -whalf ; yk <= whalf ; yk++)
      {
	yi = mri_T2->yi[nint(yv+yk)] ;
	for (zk = -whalf ; zk <= whalf ; zk++)
	{
	  zi = mri_T2->zi[nint(zv+zk)] ;
	  MRIvoxelToVoxel(mri_T2, mri_filled, xi, yi, zi, &xvf, &yvf, &zvf) ;
	  filled_wm = MRIgetVoxVal(mri_filled, nint(xvf), nint(yvf), nint(zvf), 0) ;
	  MRIvoxelToVoxel(mri_T2, mri_filled_pial, xi, yi, zi, &xvf, &yvf, &zvf) ;
	  filled_pial = MRIgetVoxVal(mri_filled_pial, nint(xvf), nint(yvf), nint(zvf), 0) ;

	  val = MRIgetVoxVal(mri_T2, xi, yi, zi, 0) ;
	  if (filled_wm)    // add it to white tally
	  {
	    counts[WMI]++ ;
	    means[WMI][T2I] += val ;
	    stds[WMI][T2I] += (val*val) ;

	    val = MRIgetVoxVal(mri_T1, xi, yi, zi, 0) ;
	    means[WMI][T1I] += val ;
	    stds[WMI][T1I] += (val*val) ;
	  }
	  else if (filled_pial)  // add it to gm tally
	  {
	    counts[GMI]++ ;
	    means[GMI][T2I] += val ;
	    stds[GMI][T2I] += (val*val) ;

	    val = MRIgetVoxVal(mri_T1, xi, yi, zi, 0) ;
	    means[GMI][T1I] += val ;
	    stds[GMI][T1I] += (val*val) ;
	  }

	  if (filled_wm > 0) // interior of wm
	  {
	    val = MRIgetVoxVal(mri_T2, xi, yi, zi, 0) ;
	    num++ ;
	    sum += val ;
	    if (val > max_white)
	    {
	      label = MRIgetVoxVal(mri_aseg, xi, yi, zi, 0) ;
	      if (IS_WM(label))
	      {
		if (near_bright_structure(mri_aseg, xi, yi, zi, 4) == 0)
		{
		  if (vno == Gdiag_no)
		    DiagBreak() ;
		  MRIvoxelToVoxel(mri_T2, mri_dist, xi, yi, zi, &xvf, &yvf, &zvf) ;
		  dist = MRIgetVoxVal(mri_dist, nint(xvf), nint(yvf), nint(zvf), 0) ;
		  if (dist < -2)
		    count++ ;
		}
	      }
	    }
	  }
	}
      }
    }
    if (vno == Gdiag_no)
    {
      printf("vno %d: sum = %2.1f\n", vno, sum) ;
      DiagBreak() ;
    }
    if (num>0 && v->curv > .5)
      MRIsetVoxVal(mri_dst, vno, 0, 0, 0, count) ;

    if (counts[WMI] > 0)
    {
      means[WMI][T2I] /= counts[WMI] ;
      means[WMI][T1I] /= counts[WMI] ;
      MRIsetVoxVal(mri_dst, vno, 0, 0, 0, means[WMI][T1I]) ;
      MRIsetVoxVal(mri_dst, vno, 0, 0, 1, means[WMI][T2I]) ;
    }
    if (counts[GMI] > 0)
    {
      means[WMI][T2I] /= counts[GMI] ;
      means[WMI][T1I] /= counts[GMI] ;
      MRIsetVoxVal(mri_dst, vno, 0, 0, 2, means[GMI][T1I]) ;
      MRIsetVoxVal(mri_dst, vno, 0, 0, 3, means[GMI][T2I]) ;
    }
  }
  MRIfree(&mri_filled) ; HISTOfree(&h) ; HISTOfree(&hcdf) ; HISTOfree(&hcdf_rev) ; MRIfree(&mri_dist) ;

  return(mri_dst) ;
}

#define HISTO_NBINS 256

static int
find_and_mark_pinched_regions(MRI_SURFACE *mris,
                              MRI *mri_T2,
                              float nstd_below,
                              float nstd_above)
{
  Real      val, xs, ys, zs, xv, yv, zv, d, mean, std ;
  int       vno, num_in, num_out, found_bad_intensity, done, bin, niter ;
  VERTEX    *v ;
  double    min_gray, max_gray, thickness, nx, ny, nz, mn, mx, sig, sample_dist ;
  HISTOGRAM *h, *hcdf ;
  MRI       *mri_filled ;

  sample_dist = MIN(SAMPLE_DIST, mri_T2->xsize/2) ;
  h = HISTOalloc(HISTO_NBINS) ;
  mx = HISTO_NBINS-1 ;
  HISTOinit(h, HISTO_NBINS, 0, mx) ;

  mean = std = 0.0 ;
  num_in = 0 ;
  niter = 0 ;
  do
  {
    for (vno = 0 ; vno < mris->nvertices ; vno++)
    {
      v = &mris->vertices[vno] ;
      if (v->ripflag)
      {
        continue ;
      }
      if (vno == Gdiag_no)
      {
        DiagBreak() ;
      }
      nx = v->x - v->whitex ;
      ny = v->y - v->whitey ;
      nz = v->z - v->whitez ;
      thickness = sqrt(SQR(nx)+SQR(ny)+SQR(nz)) ;
      if (FZERO(thickness))  // pial and white in same place - no cortex here
      {
        continue ;
      }
      MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
      MRIsampleVolume(mri_T2, xv, yv, zv, &val) ;
      for (d = thickness/2 ; d <= thickness ; d += sample_dist)
      {
        xs = v->whitex + d*v->nx ;
        ys = v->whitey + d*v->ny ;
        zs = v->whitez + d*v->nz ;
        MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
        MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
        if (val <= 0)
        {
          continue ;
        }

        mean += val ;
        std += val*val ;
        num_in++ ;
        HISTOaddSample(h, val, 0, mx) ;
      }
    }
    hcdf = HISTOmakeCDF(h, NULL) ;
    bin = HISTOfindBinWithCount(hcdf, PERCENTILE);
    if (bin < ceil(PERCENTILE*HISTO_NBINS)/4)  // data range is too compressed for histogram to represent
    {
      done = (niter > 10) ;
      if (niter++ > 0)
      {
        mx /= 2 ;
      }
      else
      {
        mx = 2*bin/(.9) ;  // first time - take an educated guess
      }
      HISTOinit(h, HISTO_NBINS, 0, mx) ;
      printf("compressed histogram detected, changing bin size to %f\n",
             h->bin_size) ;
    }
    else
    {
      done = 1 ;
    }
  }
  while (!done) ;
  mean /= num_in ;
  std = sqrt(std/num_in - mean*mean) ;
  max_gray = mean+nstd_above*std ;
  min_gray = mean-nstd_below*std ;

  HISTOrobustGaussianFit(h, .9, &mn, &sig) ;
  if (Gdiag & DIAG_WRITE)
    HISTOplot(h, "h.plt") ;
  if (T2_max_inside < 0)
    max_gray = mn+nstd_above*sig ;
  else
    max_gray = T2_max_inside ;
  if (T2_min_inside < 0)
    min_gray = mn-nstd_below*sig ;
  else
    min_gray = T2_min_inside ;
  printf("locating cortical regions not in the range [%2.2f %2.2f], "
         "gm=%2.2f+-%2.2f, and vertices in regions > %2.1f\n",
         min_gray, max_gray, mn, sig, mn-.5*sig) ;

  MRISsaveVertexPositions(mris, TMP2_VERTICES) ;
  MRISrestoreVertexPositions(mris, WHITE_VERTICES) ;
  mri_filled = MRIclone(mri_T2, NULL) ;
  MRISfillInterior(mris, mri_T2->xsize, mri_filled) ;
  MRISrestoreVertexPositions(mris, TMP2_VERTICES) ;

  for (num_in = num_out = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    v->targx = v->x ;
    v->targy = v->y ;
    v->targz = v->z ;
    if (v->ripflag)
    {
      continue ;
    }
    if (vno == Gdiag_no)
    {
      DiagBreak() ;
    }
    nx = v->x - v->whitex ;
    ny = v->y - v->whitey ;
    nz = v->z - v->whitez ;
    thickness = sqrt(SQR(nx)+SQR(ny)+SQR(nz)) ;
    if (FZERO(thickness))
    {
      continue ;
    }
    MRISvertexToVoxel(mris, v, mri_T2, &xv, &yv, &zv) ;
    nx /= thickness ;
    ny /= thickness ;
    nz /= thickness ;
    found_bad_intensity = 0 ;
    for (d = thickness/2 ; d <= thickness ; d += sample_dist)
    {
      if (MRIgetVoxVal(mri_filled, nint(xv), nint(yv), nint(zv), 0) >0) // interior of white surface
      {
        continue ;
      }
      xs = v->whitex + d*nx ;
      ys = v->whitey + d*ny ;
      zs = v->whitez + d*nz ;
      MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
      MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
      if (val < 0)
      {
        continue ;
      }
      if (val < min_gray || val > max_gray)
      {
        found_bad_intensity = 1 ;
        break ;
      }
    }
    if (found_bad_intensity)
    {
      num_in++ ;
      // target surface so interior is good value and exterior is bad gm value
      v->targx = xs - (sample_dist/2*nx) ;
      v->targy = ys - (sample_dist/2*ny) ;
      v->targz = zs - (sample_dist/2*nz) ;
      if (vno == Gdiag_no)
      {
        printf("vno %d: resetting target location to be (%2.1f %2.1f %2.1f), "
               "val = %2.0f\n",
               vno, v->targx, v->targy, v->targz, val) ;
        DiagBreak() ;
      }
    }
    else  // no invalid intensities found in the interior, check for valid ones in the exterior?
    {
#if 1
#define MAX_OUTWARD_DIST2 10
      for (d = 0 ; d <= MAX_OUTWARD_DIST2 ; d += sample_dist)
      {
        xs = v->x + d*v->nx ;
        ys = v->y + d*v->ny ;
        zs = v->z + d*v->nz ;
        MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
        if (MRIgetVoxVal(mri_filled, nint(xv), nint(yv), nint(zv), 0) >0) // interior of white surface
        {
          continue ;
        }
        MRIsampleVolumeType(mri_T2, xv, yv, zv, &val, SAMPLE_TRILINEAR) ;
        if (val < 0)
        {
          continue ;
        }
        if (val < mn-sig ||  val > mn+sig)  // only look for a very narrow range of intensities
        {
          break ;
        }
      }
      if (d > MAX_OUTWARD_DIST2)  // couldn't find pial surface
      {
        d = 0 ;
      }

      if (d > 0)
      {
        d -= sample_dist ;
        num_out++ ;
      }
      v->targx = v->x+d*v->nx ;
      v->targy = v->y+d*v->ny ;
      v->targz = v->z+d*v->nz ;
#else
      v->targx = v->x ;
      v->targy = v->y ;
      v->targz = v->z ;
#endif
      {
        int xk, yk, zk, whalf, found ;
        float xi, yi, zi ;

        MRISsurfaceRASToVoxelCached(mris, mri_T2, xs, ys, zs, &xv, &yv, &zv);
        for (whalf = 1 ; whalf <= 6 ; whalf++)
        {
          found = 0 ;
          for (xk = -whalf ; xk <= whalf ; xk++)
          {
            xi = (xv + xk) ;
            for (yk = -whalf ; yk <= whalf ; yk++)
            {
              yi = (yv + yk) ;
              for (zk = -whalf ; zk <= whalf ; zk++)
              {
                zi = (zv + zk) ;
                if (MRIgetVoxVal(mri_filled, xi, yi, zi, 0) > 0)
                {
                  continue ;  // don't sample interior to white surface
                }
                MRIsampleVolumeType(mri_T2,
                                    xi, yi, zi,
                                    &val, SAMPLE_TRILINEAR) ;
                if (val < 0)
                {
                  continue ;
                }
                if (val < mn-.5*sig)  // found something that could be csf in FLAIR
                {
                  found = 1 ;
                  break ;
                }
              }
              if (found)
              {
                break ;
              }
            }
            if (found)
            {
              break ;
            }
          }
          if (found)
          {
            break ;
          }
        }
        v->marked = whalf ;
      }
    }
  }
#define OUT_DIST 3
  // find regions that don't have any reasonable CSF-like intensity nearby
  MRISmarkedToCurv(mris) ;
  MRISaverageCurvatures(mris, 10) ;
  MRISthresholdCurvature(mris, OUT_DIST, 1) ;
  MRIScurvToMarked(mris) ;
  MRISdilateMarked(mris, 7) ;      // must smooth a nbhd around each pinch
  MRIScopyMarkedToMarked2(mris) ;  // save mark = bad vertex
  MRISwriteMarked(mris, "bad") ;
  MRISinvertMarks(mris) ;
  if (vno >= 0)
  {
    printf("before soap bubble smoothing:\n") ;
    MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
  }
//#define K_bp   0.1
#define K_bp   0.5
#if 0
#define Lambda .63
#define Mu     -.67236
#else
#define Lambda .3
#define Mu     (1.0)/((K_bp)-1.0/Lambda)
#endif
  MRISripMarked(mris) ;
  MRISweightedSoapBubbleVertexPositions(mris, 500) ;
  MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
#if 0
  {
    double l, m ;
    char       fname[STRLEN] ;
    MRISsaveVertexPositions(mris, INFLATED_VERTICES) ;
    for (l = .05 ; l < .9 ; l+=.05)
    {
      m = (1.0)/((K_bp)-1.0/l) ;
      MRIStaubinSmooth(mris, 1000, l, m, TAUBIN_UNIFORM_WEIGHTS) ;
      sprintf(fname, "taubin%.2f", l) ;
      printf("writing %s\n", fname) ;
      MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
      MRISwrite(mris, fname) ;
      MRISrestoreVertexPositions(mris, INFLATED_VERTICES) ;
    }
    DiagBreak() ;
  }
#endif
  MRISunrip(mris) ;
//  MRISweightedSoapBubbleVertexPositions(mris, 500) ;
  if (vno >= 0)
  {
    printf("after soap bubble smoothing:\n") ;
    MRISprintVertexStats(mris, Gdiag_no, Gstdout, CURRENT_VERTICES) ;
    MRISprintVertexStats(mris, Gdiag_no, Gstdout, ORIGINAL_VERTICES) ;
    MRISprintVertexStats(mris, Gdiag_no, Gstdout, WHITE_VERTICES) ;
  }
  MRIScomputeMetricProperties(mris) ;
  MRIScopyMarked2ToMarked(mris) ;  // restore mark = bad vertex
  for (num_in = num_out = vno = 0 ; vno < mris->nvertices ; vno++)
  {
    v = &mris->vertices[vno] ;
    if (v->marked == 0)
    {
      continue ;
    }
    v->targx = v->x+v->nx*OUT_DIST ;
    v->targy = v->y+v->ny*OUT_DIST ;
    v->targz = v->z+v->nz*OUT_DIST ;
    v->val = mn-sig ;   // a mildly CSF intensity in a flair image to seek
    if (vno == Gdiag_no)
    {
      Real xv, yv, zv, xv0, yv0, zv0 ;
      MRISsurfaceRASToVoxelCached(mris, mri_T2,
                                  v->x, v->y, v->z,
                                  &xv0, &yv0, &zv0);
      MRISsurfaceRASToVoxelCached(mris, mri_T2,
                                  v->targx, v->targy, v->targz,
                                  &xv, &yv, &zv);
      printf("vno %d: target = (%2.1f, %2.1f, %2.1f) --> "
             "vox (%2.1f, %2.1f, %2.1f), current=(%2.1f, %2.1f, %2.1f)\n",
             vno, v->targx, v->targy, v->targz, xv, yv, zv, xv0, yv0, zv0) ;
      DiagBreak() ;
    }
  }
  printf("%d surface locations found to contain inconsistent "
         "values (%d in, %d out)\n",
         num_in+num_out, num_in, num_out) ;
  MRIfree(&mri_filled) ;
  return(NO_ERROR) ;
}


#include "mrisegment.h"

static int labels_to_correct[] = { Left_Hippocampus,
                                   Right_Hippocampus,
                                   Left_Amygdala,
                                   Right_Amygdala
                                 } ;
#define NLABELS (sizeof(labels_to_correct)/sizeof(labels_to_correct[0]))

static int
edit_aseg_with_surfaces(MRI_SURFACE *mris, MRI *mri_aseg)
{
  MRI              *mri_filled, *mri_hires_aseg ;
  MRI_SEGMENTATION *mseg1, *mseg2 ;
  MRI_SEGMENT      *mseg ;
  int              label, *counts, x, y, z, max_seg_no, sno,vno, alabel, l ;
  MATRIX           *m_vox2vox ;
  VECTOR           *v1, *v2 ;

  printf("correcting aseg with surfaces...\n");
  mri_filled = MRISfillInterior(mris, mri_aseg->xsize/4, NULL) ;
  mri_filled->c_r += mri_aseg->c_r ;
  mri_filled->c_a += mri_aseg->c_a ;
  mri_filled->c_s += mri_aseg->c_s ;
  mri_hires_aseg = MRIresample(mri_aseg, mri_filled, SAMPLE_NEAREST);
  /*  MRIdilate(mri_filled, mri_filled) ; */// fill small breaks
  MRIcopyLabel(mri_filled, mri_hires_aseg, 1) ;
  if (Gdiag & DIAG_WRITE && DIAG_VERBOSE_ON)
  {
    MRIwrite(mri_hires_aseg, "ha.mgz") ;
    MRIwrite(mri_filled, "hs.mgz") ;
  }

  m_vox2vox = MRIgetVoxelToVoxelXform(mri_hires_aseg, mri_aseg) ;
  v1 = VectorAlloc(4, MATRIX_REAL) ;
  v2 = VectorAlloc(4, MATRIX_REAL) ;
  VECTOR_ELT(v1, 4) = VECTOR_ELT(v2, 4) = 1.0 ;
  counts = MRIhistogramLabels(mri_aseg,  NULL, MAX_CMA_LABEL+1) ;

  for (l = 0 ; l < NLABELS ; l++)
  {
    label = labels_to_correct[l] ;
    if (counts[label] == 0)
    {
      continue ;
    }
    mseg1 = MRIsegment(mri_aseg, label, label) ;
    if (mseg1->nsegments != 1) // wasn't topologically correct
    {
      MRIsegmentFree(&mseg1) ;
      continue ;
    }
    mseg2 = MRIsegment(mri_hires_aseg, label, label) ;
    if (mseg2->nsegments == 1)  // topology already correct
    {
      MRIsegmentFree(&mseg2) ;
      continue ;
    }

    // turn off the other pieces of the label
    max_seg_no = MRIfindMaxSegmentNumber(mseg2) ;
    for (sno = 0 ; sno < mseg2->nsegments ; sno++)
    {
      if (sno == max_seg_no)
      {
        continue ;
      }
      mseg = &mseg2->segments[sno] ;
      printf("label %s: removing %d voxels in segment %d\n",
             cma_label_to_name(label), mseg->nvoxels, sno) ;
      for (vno = 0 ; vno < mseg->nvoxels ; vno++)
      {
        V3_X(v1) = mseg->voxels[vno].x ;
        V3_Y(v1) = mseg->voxels[vno].y ;
        V3_Z(v1) = mseg->voxels[vno].z ;
        MatrixMultiply(m_vox2vox, v1, v2) ; // to lowres coords
        x = nint(V3_X(v2)) ;
        y = nint(V3_Y(v2)) ;
        z = nint(V3_Z(v2)) ;
        alabel = (int)MRIgetVoxVal(mri_aseg, x, y, z, 0) ;
        if (alabel == label)
        {
          MRIsetVoxVal(mri_aseg, x, y, z, 0, Left_undetermined) ;
        }
      }
    }

    MRIsegmentFree(&mseg1) ;
    MRIsegmentFree(&mseg2) ;
  }
  free(counts) ;
  MRIfree(&mri_hires_aseg) ;
  MRIfree(&mri_filled) ;

  return(NO_ERROR) ;
}

static int
compute_label_normal(MRI *mri_aseg, int x0, int y0, int z0,
                     int label, int whalf, double *pnx, double *pny,
                     double *pnz, int use_abs)
{
  int xi, yi, zi, xk, yk, zk, nvox = 0, val, dx, dy, dz, xn, yn, zn ;
  double  nx, ny, nz, mag ;

  nx = ny = nz = 0.0 ;
  for (xk = -whalf ; xk <= whalf ; xk++)
  {
    xi = mri_aseg->xi[x0+xk] ;
    for (yk = -whalf ; yk <= whalf ; yk++)
    {
      yi = mri_aseg->yi[y0+yk] ;
      for (zk = -whalf ; zk <= whalf ; zk++)
      {
        zi = mri_aseg->zi[z0+zk] ;
        val = (int)MRIgetVoxVal(mri_aseg, xi, yi, zi, 0) ;
        if (val != label)
        {
          continue ;
        }
        for (dx = -1 ; dx <= 1 ; dx++)
        {
          for (dy = -1 ; dy <= 1 ; dy++)
          {
            for (dz = -1 ; dz <= 1 ; dz++)
            {
              if (fabs(dx) + fabs(dy) + fabs(dz) != 1)
              {
                continue ;  // only 8-connected nbrs
              }
              xn = mri_aseg->xi[xi+dx] ;
              yn = mri_aseg->yi[yi+dy] ;
              zn = mri_aseg->zi[zi+dz] ;
              val = (int)MRIgetVoxVal(mri_aseg, xn, yn, zn, 0) ;
              if (val != label)  // "surface" of label - interface between label and non-label
              {
                nvox++ ;
                if (use_abs)
                {
                  nx += fabs(dx) ;
                  ny += fabs(dy) ;
                  nz += fabs(dz) ;
                }
                else
                {
                  nx += dx ;
                  ny += dy ;
                  nz += dz ;
                }
              }
            }
          }
        }
      }
    }
  }
  if (nvox > 0)
  {
    nx /= nvox ;
    ny /= nvox ;
    nz /= nvox ;
  }
  mag = sqrt(nx*nx + ny*ny + nz*nz) ;
  if (mag > 0)
  {
    nx /= mag ;
    ny /= mag ;
    nz /= mag ;
  }
  *pnx = nx ;
  *pny = ny ;
  *pnz = nz ;
  return(NO_ERROR) ;
}
static int 
near_bright_structure(MRI *mri_aseg, int xi, int yi, int zi, int whalf)
{
  int label_histo[MAX_CMA_LABEL+1] ;

  memset(label_histo, 0, sizeof(label_histo)) ;

  return(
    (label_histo[Left_Putamen] > 0) ||
    (label_histo[Right_Putamen] > 0) ||
    (label_histo[Left_Pallidum] > 0) ||
    (label_histo[Right_Pallidum] > 0) ||
    (label_histo[Left_Hippocampus] > 0) ||
    (label_histo[Right_Hippocampus] > 0) ||
    (label_histo[Left_Amygdala] > 0) ||
    (label_histo[Right_Amygdala] > 0)
    );
}

int
MRISremoveSelfIntersections(MRI_SURFACE *mris)
{
  MRIS_HASH_TABLE  *mht ;
  int              n, vno, fno ;
  VERTEX           *v ;

  MRISremoveIntersections(mris);
  return(NO_ERROR) ;
  mht = MHTfillTable(mris, NULL) ;
  
  MRISsetMarks(mris, 1) ;  // fixed vertices
  for (fno = 0 ; fno < mris->nfaces ; fno++)
  {
    if (MHTdoesFaceIntersect(mht, mris, fno))
    {
      for (n = 0 ; n < VERTICES_PER_FACE ; n++)
      {
	vno = mris->faces[fno].v[n] ;
	v = &mris->vertices[vno] ;
	v->marked = 0 ;
	if (vno == Gdiag_no)
	  DiagBreak() ;
      }
    }
  }

  MRISdilateMarked(mris, 1) ;
  MRISwriteMarked(mris, "marked.mgz") ;
  MRISsoapBubbleVertexPositions(mris, 500) ;
  MRISwrite(mris, "after_soap") ;

  MHTfree(&mht) ;
  return(NO_ERROR) ; ;
}
