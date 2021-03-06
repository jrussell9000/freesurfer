
subj=$(SUBJECTS_DIR)/$(RECONALL_MAKE_SUBJECT)

all: $(subj) autorecon1 autorecon2 autorecon3

#---------------------- A U T O R E C O N  1 --------------------------
RAW=$(subj)/mri/rawavg.mgz
ORIG=$(subj)/mri/orig.mgz
TAL=$(subj)/mri/transforms/talairach.auto.xfm
CP=$(wildcard $(subj)/tmp/control.dat)
T1=$(subj)/mri/T1.mgz
BRAINMASK=$(subj)/mri/brainmask.mgz
NU=$(subj)/mri/nu.mgz
AUTORECON1= $(ORIG) $(TAL) $(T1) $(BRAINMASK) $(NU)

autorecon1: $(AUTORECON1)

$(ORIG):
	recon-all -s $(subj) -motioncor

$(TAL): $(ORIG)
	recon-all -s $(subj) -talairach -tal-check

$(NU): $(ORIG) $(TAL)
	recon-all -s $(subj) -nuintensitycor

$(T1): $(TAL) $(CP) $(NU)
	recon-all -s $(subj) -normalization

$(BRAINMASK): $(T1) $(NU)
	recon-all -s $(subj) -skullstrip


#------------------- A U T O R E C O N   2   V O L	----------------------
TAL_LTA=$(subj)/mri/transforms/talairach.lta
NORM=$(subj)/mri/norm.mgz
TAL_M3Z=$(subj)/mri/transforms/talairach.m3z
#NU_NONECK=$(subj)/mri/nu_noneck.mgz
#TAL_SKULL_LTA=$(subj)/mri/transforms/talairach_with_skull_2.lta
ASEG_PRESURF=$(subj)/mri/aseg.presurf.mgz
# ASEG_TOUCH can be used as a dependency target in some targets instead of
# aseg.mgz because aseg.mgz can change in autorecon3 as a result of edits
# via the surfaces.
# the ASEG_TOUCH file is created after the aseg is first created.
ASEG_TOUCH=$(subj)/touch/asegmerge.touch
SUBCORTICAL=$(TAL_LTA) $(NORM) $(TAL_M3Z) $(ASEG_PRESURF)
BRAIN=$(subj)/mri/brain.mgz
BRAINFINALSURFS=$(subj)/mri/brain.finalsurfs.mgz
WM=$(subj)/mri/wm.mgz
SEED_PONS=$(wildcard $(subj)/scripts/seed-pons.crs.man.dat)
SEED_CC=$(wildcard $(subj)/scripts/seed-cc.crs.man.dat)
SEED_LH=$(wildcard $(subj)/scripts/seed-lh.crs.man.dat)
SEED_RH=$(wildcard $(subj)/scripts/seed-rh.crs.man.dat)
FILLED=$(subj)/mri/filled.mgz
AUTORECON2_VOL=$(SUBCORTICAL) $(BRAIN) $(BRAINFINALSURFS) $(WM) $(FILLED)

autorecon2-vol: $(AUTORECON2_VOL)

autorecon2-volonly: autorecon2-vol

$(TAL_LTA): $(BRAINMASK) $(NU)
	recon-all -s $(subj) -gcareg

$(NORM): $(TAL_LTA) $(CP) $(NU)
	recon-all -s $(subj) -canorm

$(TAL_M3Z): $(NORM) $(BRAINMASK) $(TAL_LTA)
	recon-all -s $(subj) -careg 

$(ASEG_PRESURF): $(NORM) $(TAL_M3Z)
	recon-all -s $(subj) -calabel

$(BRAIN): $(BRAINMASK) $(NORM) $(ASEG_PRESURF) $(CP)
	recon-all -s $(subj) -normalization2

$(BRAINFINALSURFS): $(BRAIN) $(BRAINMASK)
	recon-all -s $(subj) -maskbfs

$(WM): $(BRAIN) $(ASEG_PRESURF) $(NORM)
	recon-all -s $(subj) -segmentation

$(FILLED): $(WM) $(ASEG_PRESURF) $(TAL_LTA) \
	$(SEED_PONS) $(SEED_CC) $(SEED_LH) $(SEED_RH)
	recon-all -s $(subj) -fill

$(CP):

$(SEED_PONS):

$(SEED_CC):

$(SEED_LH):

$(SEED_RH):


#$(TAL_SKULL_LTA): $(NU_NONECK) $(TAL_LTA)
#	recon-all -s $(subj) -skull-lta

#$(NU_NONECK): $(TAL_M3Z) $(NU)
#	recon-all -s $(subj) -rmneck


#------------------- A U T O R E C O N   2   S U R F -----------------------
LH=$(subj)/surf/lh
RH=$(subj)/surf/rh
ORIG_NOFIX_LH=$(LH).orig.nofix
ORIG_NOFIX_RH=$(RH).orig.nofix
SMOOTHWM_NOFIX_LH=$(LH).smoothwm.nofix
SMOOTHWM_NOFIX_RH=$(RH).smoothwm.nofix
INFLATED_NOFIX_LH=$(LH).inflated.nofix
INFLATED_NOFIX_RH=$(RH).inflated.nofix
QSPHERE_NOFIX_LH=$(LH).qsphere.nofix
QSPHERE_NOFIX_RH=$(RH).qsphere.nofix
ORIG_LH=$(LH).orig
ORIG_RH=$(RH).orig
INFLATED_LH=$(LH).inflated
INFLATED_RH=$(RH).inflated
WHITE_LH_PREAPARC=$(LH).white.preaparc
WHITE_RH_PREAPARC=$(RH).white.preaparc
CURV_LH=$(LH).curv
CURV_RH=$(RH).curv
AREA_LH=$(LH).area
AREA_RH=$(RH).area
AREA_MID_LH=$(LH).area.mid
AREA_MID_RH=$(RH).area.mid
VOLUME_LH=$(LH).volume
VOLUME_RH=$(RH).volume
SMOOTHWM_LH=$(LH).smoothwm
SMOOTHWM_RH=$(RH).smoothwm
SULC_LH=$(LH).sulc
SULC_RH=$(RH).sulc
CURV_HK_LH=$(LH).inflated.H
CURV_HK_RH=$(RH).inflated.K
CURVSTATS_LH=$(subj)/stats/lh.curv.stats
CURVSTATS_RH=$(subj)/stats/rh.curv.stats

AUTORECON2_SURF=$(ORIG_NOFIX_LH) $(ORIG_NOFIX_RH) \
	$(SMOOTHWM_NOFIX_LH) $(SMOOTHWM_NOFIX_RH) \
	$(INFLATED_NOFIX_LH) $(INFLATED_NOFIX_RH) \
	$(QSPHERE_NOFIX_LH) $(QSPHERE_NOFIX_RH) \
	$(ORIG_LH) $(ORIG_RH) \
	$(WHITE_LH_PREAPARC) $(WHITE_RH_PREAPARC) \
	$(AREA_MID_LH) $(AREA_MID_RH) \
	$(SMOOTHWM_LH) $(SMOOTHWM_RH) \
	$(INFLATED_LH) $(INFLATED_RH) \
	$(CURV_HK_LH) $(CURV_HK_RH) \
	$(CURVSTATS_LH) $(CURVSTATS_RH)

autorecon2-surf: $(AUTORECON2_SURF)

autorecon2-perhemi: autorecon2-surf

AUTORECON2=$(AUTORECON2_VOL) $(AUTORECON2_SURF)

autorecon2: $(AUTORECON2)

$(ORIG_NOFIX_LH): $(FILLED) $(NORM)
	recon-all -s $(subj) -hemi lh -tessellate

$(ORIG_NOFIX_RH): $(FILLED) $(NORM)
	recon-all -s $(subj) -hemi rh -tessellate

$(SMOOTHWM_NOFIX_LH): $(ORIG_NOFIX_LH)
	recon-all -s $(subj) -hemi lh -smooth1

$(SMOOTHWM_NOFIX_RH): $(ORIG_NOFIX_RH)
	recon-all -s $(subj) -hemi rh -smooth1

$(INFLATED_NOFIX_LH): $(SMOOTHWM_NOFIX_LH)
	recon-all -s $(subj) -hemi lh -inflate1

$(INFLATED_NOFIX_RH): $(SMOOTHWM_NOFIX_RH)
	recon-all -s $(subj) -hemi rh -inflate1

$(QSPHERE_NOFIX_LH): $(INFLATED_NOFIX_LH)
	recon-all -s $(subj) -hemi lh -qsphere

$(QSPHERE_NOFIX_RH): $(INFLATED_NOFIX_RH)
	recon-all -s $(subj) -hemi rh -qsphere

$(ORIG_LH): $(ORIG_NOFIX_LH) $(INFLATED_NOFIX_LH) $(QSPHERE_NOFIX_LH)
	recon-all -s $(subj) -hemi lh -fix

$(ORIG_RH): $(ORIG_NOFIX_RH) $(INFLATED_NOFIX_RH) $(QSPHERE_NOFIX_RH)
	recon-all -s $(subj) -hemi rh -fix

$(WHITE_LH_PREAPARC): $(BRAINFINALSURFS) $(FILLED) $(ORIG_LH)
	recon-all -s $(subj) -hemi lh -white

$(WHITE_RH_PREAPARC): $(BRAINFINALSURFS) $(FILLED) $(ORIG_RH)
	recon-all -s $(subj) -hemi rh -white

$(SMOOTHWM_LH): $(WHITE_LH_PREAPARC)
	recon-all -s $(subj) -hemi lh -smooth2

$(SMOOTHWM_RH): $(WHITE_RH_PREAPARC)
	recon-all -s $(subj) -hemi rh -smooth2

$(INFLATED_LH): $(SMOOTHWM_LH) $(WHITE_LH_PREAPARC)
	recon-all -s $(subj) -hemi lh -inflate2

$(INFLATED_RH): $(SMOOTHWM_RH) $(WHITE_RH_PREAPARC)
	recon-all -s $(subj) -hemi rh -inflate2

$(CURV_HK_LH): $(INFLATED_LH) $(WHITE_LH_PREAPARC)
	recon-all -s $(subj) -hemi lh -curvHK

$(CURV_HK_RH): $(INFLATED_RH) $(WHITE_RH_PREAPARC)
	recon-all -s $(subj) -hemi rh -curvHK

$(CURVSTATS_LH): $(SMOOTHWM_LH) $(BRAINFINALSURFS) $(FILLED) $(ORIG_LH)
	recon-all -s $(subj) -hemi lh -curvstats

$(CURVSTATS_RH): $(SMOOTHWM_RH) $(BRAINFINALSURFS) $(FILLED) $(ORIG_RH)
	recon-all -s $(subj) -hemi rh -curvstats


#---------------------- A U T O R E C O N	 3 --------------------------
SPHERE_LH=$(LH).sphere
SPHERE_RH=$(RH).sphere
SPHERE_REG_LH=$(LH).sphere.reg
SPHERE_REG_RH=$(RH).sphere.reg
CONTRA_REG_LH=$(LH).rh.sphere.reg
CONTRA_REG_RH=$(RH).lh.sphere.reg
JACOBIAN_WHITE_LH=$(LH).jacobian_white
JACOBIAN_WHITE_RH=$(RH).jacobian_white
AVG_CURV_LH=$(LH).avg_curv
AVG_CURV_RH=$(RH).avg_curv
APARC_ANNOT_LH=$(subj)/label/lh.aparc.annot
APARC_ANNOT_RH=$(subj)/label/rh.aparc.annot
WHITE_LH=$(LH).white
WHITE_RH=$(RH).white
PIAL_LH=$(LH).pial
PIAL_RH=$(RH).pial
THICKNESS_LH=$(LH).thickness
THICKNESS_RH=$(RH).thickness
APARC_STATS_LH=$(subj)/stats/lh.aparc.stats
APARC_STATS_RH=$(subj)/stats/rh.aparc.stats
APARC_A2009S_ANNOT_LH=$(subj)/label/lh.aparc.a2009s.annot
APARC_A2009S_ANNOT_RH=$(subj)/label/rh.aparc.a2009s.annot
APARC_A2009S_STATS_LH=$(subj)/stats/lh.aparc.a2009s.stats
APARC_A2009S_STATS_RH=$(subj)/stats/rh.aparc.a2009s.stats
APARC_DKT40_ANNOT_LH=$(subj)/label/lh.aparc.DKTatlas40.annot
APARC_DKT40_ANNOT_RH=$(subj)/label/rh.aparc.DKTatlas40.annot
APARC_DKT40_STATS_LH=$(subj)/stats/lh.aparc.DKTatlas40.stats
APARC_DKT40_STATS_RH=$(subj)/stats/rh.aparc.DKTatlas40.stats
PCTSURFCON_LH=$(subj)/surf/lh.w-g.pct.mgh
PCTSURFCON_RH=$(subj)/surf/rh.w-g.pct.mgh
RIBBON=$(subj)/mri/ribbon.mgz
ASEG_HYPOS=$(subj)/mri/aseg.presurf.hypos.mgz
ASEG=$(subj)/mri/aseg.mgz
ASEG_STATS=$(subj)/stats/aseg.stats
APARC_ASEG=$(subj)/mri/aparc+aseg.mgz
WMPARC=$(subj)/stats/wmparc.stats
BALABELS_LH=$(subj)/label/lh.BA1_exvivo.label
BALABELS_RH=$(subj)/label/rh.BA1_exvivo.label

AUTORECON3=$(SPHERE_LH) $(SPHERE_RH) \
	$(SPHERE_REG_LH) $(SPHERE_REG_RH) \
	$(JACOBIAN_WHITE_LH) $(JACOBIAN_WHITE_RH) \
	$(AVG_CURV_LH) $(AVG_CURV_RH) \
	$(APARC_ANNOT_LH) $(APARC_ANNOT_RH) \
	$(PIAL_LH) $(PIAL_RH) \
	$(APARC_STATS_LH) $(APARC_STATS_RH) \
	$(APARC_A2009S_ANNOT_LH) $(APARC_A2009S_ANNOT_RH) \
	$(APARC_A2009S_STATS_LH) $(APARC_A2009S_STATS_RH) \
	$(APARC_DKT40_ANNOT_LH) $(APARC_DKT40_ANNOT_RH) \
	$(APARC_DKT40_STATS_LH) $(APARC_DKT40_STATS_RH) \
	$(PCTSURFCON_LH) $(PCTSURFCON_RH) \
	$(RIBBON) \
	$(ASEG_HYPOS) $(ASEG_STATS) \
	$(ASEG) $(APARC_ASEG) $(WMPARC) \
	$(BALABELS_LH) $(BALABELS_RH)

autorecon3: $(AUTORECON3)

$(SPHERE_LH): $(INFLATED_LH) $(SMOOTHWM_LH)
	recon-all -s $(subj) -hemi lh -sphere

$(SPHERE_RH): $(INFLATED_RH) $(SMOOTHWM_RH)
	recon-all -s $(subj) -hemi rh -sphere

$(SPHERE_REG_LH): $(SPHERE_LH)
	recon-all -s $(subj) -hemi lh -surfreg

$(SPHERE_REG_RH): $(SPHERE_RH)
	recon-all -s $(subj) -hemi rh -surfreg

$(JACOBIAN_WHITE_LH): $(WHITE_LH_PREAPARC) $(SPHERE_REG_LH)
	recon-all -s $(subj) -hemi lh -jacobian_white

$(JACOBIAN_WHITE_RH): $(WHITE_RH_PREAPARC) $(SPHERE_REG_RH)
	recon-all -s $(subj) -hemi rh -jacobian_white

$(AVG_CURV_LH): $(SPHERE_REG_LH)
	recon-all -s $(subj) -hemi lh -avgcurv

$(AVG_CURV_RH): $(SPHERE_REG_RH)
	recon-all -s $(subj) -hemi rh -avgcurv

$(APARC_ANNOT_LH): $(SPHERE_REG_LH) $(ASEG_PRESURF)
	recon-all -s $(subj) -hemi lh -cortparc

$(APARC_ANNOT_RH): $(SPHERE_REG_RH) $(ASEG_PRESURF)
	recon-all -s $(subj) -hemi rh -cortparc

# This now creates both pial and white surfaces
$(PIAL_LH): $(ASEG_PRESURF) $(BRAINFINALSURFS) $(WM) $(FILLED) \
	$(ORIG_LH) $(WHITE_LH_PREAPARC) $(APARC_ANNOT_LH)
	recon-all -s $(subj) -hemi lh -pial

$(PIAL_RH): $(ASEG_PRESURF) $(BRAINFINALSURFS) $(WM) $(FILLED) \
	$(ORIG_RH) $(WHITE_RH_PREAPARC) $(APARC_ANNOT_RH)
	recon-all -s $(subj) -hemi rh -pial

#$(THICKNESS_LH): $(ASEG_PRESURF) $(BRAINFINALSURFS) $(WM) $(FILLED) \
#	$(ORIG_LH) $(WHITE_LH) $(APARC_ANNOT_LH)
#	recon-all -s $(subj) -hemi lh -pial

#$(THICKNESS_RH): $(ASEG_PRESURF) $(BRAINFINALSURFS) $(WM) $(FILLED) \
#	$(ORIG_RH) $(WHITE_RH) $(APARC_ANNOT_RH)
#	recon-all -s $(subj) -hemi rh -pial

$(AREA_MID_LH): $(BRAINFINALSURFS) $(FILLED) $(PIAL_LH)
	recon-all -s $(subj) -hemi lh -surfvolume

$(AREA_MID_RH): $(BRAINFINALSURFS) $(FILLED) $(PIAL_RH)
	recon-all -s $(subj) -hemi rh -surfvolume

$(VOLUME_LH): $(AREA_MID_LH) $(THICKNESS_LH)
	recon-all -s $(subj) -hemi lh -surfvolume

$(VOLUME_RH): $(AREA_MID_RH) $(THICKNESS_LH)
	recon-all -s $(subj) -hemi rh -surfvolume

$(APARC_STATS_LH): $(APARC_ANNOT_LH) $(PIAL_LH) $(RIBBON)
	recon-all -s $(subj) -hemi lh -parcstats

$(APARC_STATS_RH): $(APARC_ANNOT_RH) $(PIAL_RH) $(RIBBON)
	recon-all -s $(subj) -hemi rh -parcstats

$(APARC_A2009S_ANNOT_LH): $(SPHERE_REG_LH) $(PIAL_LH) $(ASEG_PRESURF)
	recon-all -s $(subj) -hemi lh -cortparc2

$(APARC_A2009S_ANNOT_RH): $(SPHERE_REG_RH) $(PIAL_RH) $(ASEG_PRESURF)
	recon-all -s $(subj) -hemi rh -cortparc2

$(APARC_A2009S_STATS_LH): $(APARC_A2009S_ANNOT_LH) $(PIAL_LH) $(RIBBON)
	recon-all -s $(subj) -hemi lh -parcstats2

$(APARC_A2009S_STATS_RH): $(APARC_A2009S_ANNOT_RH) $(PIAL_RH) $(RIBBON)
	recon-all -s $(subj) -hemi rh -parcstats2

$(APARC_DKT40_ANNOT_LH): $(SPHERE_REG_LH) $(PIAL_LH) $(ASEG_PRESURF)
	recon-all -s $(subj) -hemi lh -cortparc3

$(APARC_DKT40_ANNOT_RH): $(SPHERE_REG_RH) $(PIAL_RH) $(ASEG_PRESURF)
	recon-all -s $(subj) -hemi rh -cortparc3

$(APARC_DKT40_STATS_LH): $(APARC_DKT40_ANNOT_LH) $(PIAL_LH) $(RIBBON)
	recon-all -s $(subj) -hemi lh -parcstats3

$(APARC_DKT40_STATS_RH): $(APARC_DKT40_ANNOT_RH) $(PIAL_RH) $(RIBBON)
	recon-all -s $(subj) -hemi rh -parcstats3

$(PCTSURFCON_LH): $(ORIG) $(PIAL_LH)
	recon-all -s $(subj) -hemi lh -pctsurfcon

$(PCTSURFCON_RH): $(ORIG) $(PIAL_RH)
	recon-all -s $(subj) -hemi rh -pctsurfcon

$(RIBBON): $(ORIG) $(PIAL_LH) $(PIAL_RH)
	recon-all -s $(subj) -cortribbon

$(ASEG_HYPOS): $(ASEG_PRESURF) $(PIAL_LH) $(PIAL_RH)
	recon-all -s $(subj) -hyporelabel

$(APARC_ASEG): $(ASEG_HYPOS) $(RIBBON) $(APARC_ANNOT_LH) $(APARC_ANNOT_RH)
	recon-all -s $(subj) -aparc2aseg

$(ASEG): $(APARC_ASEG)
	recon-all -s $(subj) -apas2aseg

$(ASEG_STATS): $(ASEG)
	recon-all -s $(subj) -segstats

$(WMPARC): $(ASEG) $(RIBBON)
	recon-all -s $(subj) -wmparc

$(BALABELS_LH): $(SPHERE_REG_LH)
	recon-all -s $(subj) -hemi lh -balabels

$(BALABELS_RH): $(SPHERE_REG_RH)
	recon-all -s $(subj) -hemi rh -balabels

contrasurfreg: $(CONTRA_REG_LH) $(CONTRA_REG_RH)

$(CONTRA_REG_LH): $(SPHERE_LH)
	recon-all -s $(subj) -hemi lh -contrasurfreg

$(CONTRA_REG_RH): $(SPHERE_RH)
	recon-all -s $(subj) -hemi rh -contrasurfreg

#------------------------- Q C A C H E ------------------------------
TARGET=fsaverage

THICKNESS_FWHM0_LH=$(LH).thickness.fwhm0.$(TARGET).mgh
THICKNESS_FWHM0_RH=$(RH).thickness.fwhm0.$(TARGET).mgh
THICKNESS_FWHM5_LH=$(LH).thickness.fwhm5.$(TARGET).mgh
THICKNESS_FWHM5_RH=$(RH).thickness.fwhm5.$(TARGET).mgh
THICKNESS_FWHM10_LH=$(LH).thickness.fwhm10.$(TARGET).mgh
THICKNESS_FWHM10_RH=$(RH).thickness.fwhm10.$(TARGET).mgh
THICKNESS_FWHM15_LH=$(LH).thickness.fwhm15.$(TARGET).mgh
THICKNESS_FWHM15_RH=$(RH).thickness.fwhm15.$(TARGET).mgh
THICKNESS_FWHM20_LH=$(LH).thickness.fwhm20.$(TARGET).mgh
THICKNESS_FWHM20_RH=$(RH).thickness.fwhm20.$(TARGET).mgh
THICKNESS_FWHM25_LH=$(LH).thickness.fwhm25.$(TARGET).mgh
THICKNESS_FWHM25_RH=$(RH).thickness.fwhm25.$(TARGET).mgh

CURV_FWHM0_LH=$(LH).curv.fwhm0.$(TARGET).mgh
CURV_FWHM0_RH=$(RH).curv.fwhm0.$(TARGET).mgh
CURV_FWHM5_LH=$(LH).curv.fwhm5.$(TARGET).mgh
CURV_FWHM5_RH=$(RH).curv.fwhm5.$(TARGET).mgh
CURV_FWHM10_LH=$(LH).curv.fwhm10.$(TARGET).mgh
CURV_FWHM10_RH=$(RH).curv.fwhm10.$(TARGET).mgh
CURV_FWHM15_LH=$(LH).curv.fwhm15.$(TARGET).mgh
CURV_FWHM15_RH=$(RH).curv.fwhm15.$(TARGET).mgh
CURV_FWHM20_LH=$(LH).curv.fwhm20.$(TARGET).mgh
CURV_FWHM20_RH=$(RH).curv.fwhm20.$(TARGET).mgh
CURV_FWHM25_LH=$(LH).curv.fwhm25.$(TARGET).mgh
CURV_FWHM25_RH=$(RH).curv.fwhm25.$(TARGET).mgh

SULC_FWHM0_LH=$(LH).sulc.fwhm0.$(TARGET).mgh
SULC_FWHM0_RH=$(RH).sulc.fwhm0.$(TARGET).mgh
SULC_FWHM5_LH=$(LH).sulc.fwhm5.$(TARGET).mgh
SULC_FWHM5_RH=$(RH).sulc.fwhm5.$(TARGET).mgh
SULC_FWHM10_LH=$(LH).sulc.fwhm10.$(TARGET).mgh
SULC_FWHM10_RH=$(RH).sulc.fwhm10.$(TARGET).mgh
SULC_FWHM15_LH=$(LH).sulc.fwhm15.$(TARGET).mgh
SULC_FWHM15_RH=$(RH).sulc.fwhm15.$(TARGET).mgh
SULC_FWHM20_LH=$(LH).sulc.fwhm20.$(TARGET).mgh
SULC_FWHM20_RH=$(RH).sulc.fwhm20.$(TARGET).mgh
SULC_FWHM25_LH=$(LH).sulc.fwhm25.$(TARGET).mgh
SULC_FWHM25_RH=$(RH).sulc.fwhm25.$(TARGET).mgh

AREA_FWHM0_LH=$(LH).area.fwhm0.$(TARGET).mgh
AREA_FWHM0_RH=$(RH).area.fwhm0.$(TARGET).mgh
AREA_FWHM5_LH=$(LH).area.fwhm5.$(TARGET).mgh
AREA_FWHM5_RH=$(RH).area.fwhm5.$(TARGET).mgh
AREA_FWHM10_LH=$(LH).area.fwhm10.$(TARGET).mgh
AREA_FWHM10_RH=$(RH).area.fwhm10.$(TARGET).mgh
AREA_FWHM15_LH=$(LH).area.fwhm15.$(TARGET).mgh
AREA_FWHM15_RH=$(RH).area.fwhm15.$(TARGET).mgh
AREA_FWHM20_LH=$(LH).area.fwhm20.$(TARGET).mgh
AREA_FWHM20_RH=$(RH).area.fwhm20.$(TARGET).mgh
AREA_FWHM25_LH=$(LH).area.fwhm25.$(TARGET).mgh
AREA_FWHM25_RH=$(RH).area.fwhm25.$(TARGET).mgh

VOLUME_FWHM0_LH=$(LH).volume.fwhm0.$(TARGET).mgh
VOLUME_FWHM0_RH=$(RH).volume.fwhm0.$(TARGET).mgh
VOLUME_FWHM5_LH=$(LH).volume.fwhm5.$(TARGET).mgh
VOLUME_FWHM5_RH=$(RH).volume.fwhm5.$(TARGET).mgh
VOLUME_FWHM10_LH=$(LH).volume.fwhm10.$(TARGET).mgh
VOLUME_FWHM10_RH=$(RH).volume.fwhm10.$(TARGET).mgh
VOLUME_FWHM15_LH=$(LH).volume.fwhm15.$(TARGET).mgh
VOLUME_FWHM15_RH=$(RH).volume.fwhm15.$(TARGET).mgh
VOLUME_FWHM20_LH=$(LH).volume.fwhm20.$(TARGET).mgh
VOLUME_FWHM20_RH=$(RH).volume.fwhm20.$(TARGET).mgh
VOLUME_FWHM25_LH=$(LH).volume.fwhm25.$(TARGET).mgh
VOLUME_FWHM25_RH=$(RH).volume.fwhm25.$(TARGET).mgh

JACOBIAN_WHITE_FWHM0_LH=$(LH).jacobian_white.fwhm0.$(TARGET).mgh
JACOBIAN_WHITE_FWHM0_RH=$(RH).jacobian_white.fwhm0.$(TARGET).mgh
JACOBIAN_WHITE_FWHM5_LH=$(LH).jacobian_white.fwhm5.$(TARGET).mgh
JACOBIAN_WHITE_FWHM5_RH=$(RH).jacobian_white.fwhm5.$(TARGET).mgh
JACOBIAN_WHITE_FWHM10_LH=$(LH).jacobian_white.fwhm10.$(TARGET).mgh
JACOBIAN_WHITE_FWHM10_RH=$(RH).jacobian_white.fwhm10.$(TARGET).mgh
JACOBIAN_WHITE_FWHM15_LH=$(LH).jacobian_white.fwhm15.$(TARGET).mgh
JACOBIAN_WHITE_FWHM15_RH=$(RH).jacobian_white.fwhm15.$(TARGET).mgh
JACOBIAN_WHITE_FWHM20_LH=$(LH).jacobian_white.fwhm20.$(TARGET).mgh
JACOBIAN_WHITE_FWHM20_RH=$(RH).jacobian_white.fwhm20.$(TARGET).mgh
JACOBIAN_WHITE_FWHM25_LH=$(LH).jacobian_white.fwhm25.$(TARGET).mgh
JACOBIAN_WHITE_FWHM25_RH=$(RH).jacobian_white.fwhm25.$(TARGET).mgh

QCACHE= \
	$(THICKNESS_FWHM0_LH) $(THICKNESS_FWHM0_RH) \
	$(THICKNESS_FWHM5_LH) $(THICKNESS_FWHM5_RH) \
	$(THICKNESS_FWHM10_LH) $(THICKNESS_FWHM10_RH) \
	$(THICKNESS_FWHM15_LH) $(THICKNESS_FWHM15_RH) \
	$(THICKNESS_FWHM20_LH) $(THICKNESS_FWHM20_RH) \
	$(THICKNESS_FWHM25_LH) $(THICKNESS_FWHM25_RH) \
	$(CURV_FWHM0_LH) $(CURV_FWHM0_RH) \
	$(CURV_FWHM5_LH) $(CURV_FWHM5_RH) \
	$(CURV_FWHM10_LH) $(CURV_FWHM10_RH) \
	$(CURV_FWHM15_LH) $(CURV_FWHM15_RH) \
	$(CURV_FWHM20_LH) $(CURV_FWHM20_RH) \
	$(CURV_FWHM25_LH) $(CURV_FWHM25_RH) \
	$(SULC_FWHM0_LH) $(SULC_FWHM0_RH) \
	$(SULC_FWHM5_LH) $(SULC_FWHM5_RH) \
	$(SULC_FWHM10_LH) $(SULC_FWHM10_RH) \
	$(SULC_FWHM15_LH) $(SULC_FWHM15_RH) \
	$(SULC_FWHM20_LH) $(SULC_FWHM20_RH) \
	$(SULC_FWHM25_LH) $(SULC_FWHM25_RH) \
	$(AREA_FWHM0_LH) $(AREA_FWHM0_RH) \
	$(AREA_FWHM5_LH) $(AREA_FWHM5_RH) \
	$(AREA_FWHM10_LH) $(AREA_FWHM10_RH) \
	$(AREA_FWHM15_LH) $(AREA_FWHM15_RH) \
	$(AREA_FWHM20_LH) $(AREA_FWHM20_RH) \
	$(AREA_FWHM25_LH) $(AREA_FWHM25_RH) \
	$(VOLUME_FWHM0_LH) $(VOLUME_FWHM0_RH) \
	$(VOLUME_FWHM5_LH) $(VOLUME_FWHM5_RH) \
	$(VOLUME_FWHM10_LH) $(VOLUME_FWHM10_RH) \
	$(VOLUME_FWHM15_LH) $(VOLUME_FWHM15_RH) \
	$(VOLUME_FWHM20_LH) $(VOLUME_FWHM20_RH) \
	$(VOLUME_FWHM25_LH) $(VOLUME_FWHM25_RH) \
	$(JACOBIAN_WHITE_FWHM0_LH) $(JACOBIAN_WHITE_FWHM0_RH) \
	$(JACOBIAN_WHITE_FWHM5_LH) $(JACOBIAN_WHITE_FWHM5_RH) \
	$(JACOBIAN_WHITE_FWHM10_LH) $(JACOBIAN_WHITE_FWHM10_RH) \
	$(JACOBIAN_WHITE_FWHM15_LH) $(JACOBIAN_WHITE_FWHM15_RH) \
	$(JACOBIAN_WHITE_FWHM20_LH) $(JACOBIAN_WHITE_FWHM20_RH) \
	$(JACOBIAN_WHITE_FWHM25_LH) $(JACOBIAN_WHITE_FWHM25_RH)

qcache: $(QCACHE)

T=-target $(TARGET)

$(THICKNESS_FWHM0_LH): $(THICKNESS_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure thickness -fwhm 0 $(T)

$(THICKNESS_FWHM0_RH): $(THICKNESS_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure thickness -fwhm 0 $(T)

$(THICKNESS_FWHM5_LH): $(THICKNESS_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure thickness -fwhm 5 $(T)

$(THICKNESS_FWHM5_RH): $(THICKNESS_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure thickness -fwhm 5 $(T)

$(THICKNESS_FWHM10_LH): $(THICKNESS_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure thickness -fwhm 10 $(T)

$(THICKNESS_FWHM10_RH): $(THICKNESS_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure thickness -fwhm 10 $(T)

$(THICKNESS_FWHM15_LH): $(THICKNESS_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure thickness -fwhm 15 $(T)

$(THICKNESS_FWHM15_RH): $(THICKNESS_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure thickness -fwhm 15 $(T)

$(THICKNESS_FWHM20_LH): $(THICKNESS_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure thickness -fwhm 20 $(T)

$(THICKNESS_FWHM20_RH): $(THICKNESS_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure thickness -fwhm 20 $(T)

$(THICKNESS_FWHM25_LH): $(THICKNESS_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure thickness -fwhm 25 $(T)

$(THICKNESS_FWHM25_RH): $(THICKNESS_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure thickness -fwhm 25 $(T)


$(CURV_FWHM0_LH): $(CURV_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure curv -fwhm 0 $(T)

$(CURV_FWHM0_RH): $(CURV_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure curv -fwhm 0 $(T)

$(CURV_FWHM5_LH): $(CURV_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure curv -fwhm 5 $(T)

$(CURV_FWHM5_RH): $(CURV_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure curv -fwhm 5 $(T)

$(CURV_FWHM10_LH): $(CURV_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure curv -fwhm 10 $(T)

$(CURV_FWHM10_RH): $(CURV_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure curv -fwhm 10 $(T)

$(CURV_FWHM15_LH): $(CURV_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure curv -fwhm 15 $(T)

$(CURV_FWHM15_RH): $(CURV_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure curv -fwhm 15 $(T)

$(CURV_FWHM20_LH): $(CURV_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure curv -fwhm 20 $(T)

$(CURV_FWHM20_RH): $(CURV_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure curv -fwhm 20 $(T)

$(CURV_FWHM25_LH): $(CURV_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure curv -fwhm 25 $(T)

$(CURV_FWHM25_RH): $(CURV_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure curv -fwhm 25 $(T)


$(SULC_FWHM0_LH): $(SULC_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure sulc -fwhm 0 $(T)

$(SULC_FWHM0_RH): $(SULC_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure sulc -fwhm 0 $(T)

$(SULC_FWHM5_LH): $(SULC_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure sulc -fwhm 5 $(T)

$(SULC_FWHM5_RH): $(SULC_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure sulc -fwhm 5 $(T)

$(SULC_FWHM10_LH): $(SULC_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure sulc -fwhm 10 $(T)

$(SULC_FWHM10_RH): $(SULC_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure sulc -fwhm 10 $(T)

$(SULC_FWHM15_LH): $(SULC_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure sulc -fwhm 15 $(T)

$(SULC_FWHM15_RH): $(SULC_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure sulc -fwhm 15 $(T)

$(SULC_FWHM20_LH): $(SULC_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure sulc -fwhm 20 $(T)

$(SULC_FWHM20_RH): $(SULC_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure sulc -fwhm 20 $(T)

$(SULC_FWHM25_LH): $(SULC_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure sulc -fwhm 25 $(T)

$(SULC_FWHM25_RH): $(SULC_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure sulc -fwhm 25 $(T)


$(AREA_FWHM0_LH): $(AREA_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure area -fwhm 0 $(T)

$(AREA_FWHM0_RH): $(AREA_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure area -fwhm 0 $(T)

$(AREA_FWHM5_LH): $(AREA_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure area -fwhm 5 $(T)

$(AREA_FWHM5_RH): $(AREA_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure area -fwhm 5 $(T)

$(AREA_FWHM10_LH): $(AREA_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure area -fwhm 10 $(T)

$(AREA_FWHM10_RH): $(AREA_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure area -fwhm 10 $(T)

$(AREA_FWHM15_LH): $(AREA_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure area -fwhm 15 $(T)

$(AREA_FWHM15_RH): $(AREA_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure area -fwhm 15 $(T)

$(AREA_FWHM20_LH): $(AREA_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure area -fwhm 20 $(T)

$(AREA_FWHM20_RH): $(AREA_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure area -fwhm 20 $(T)

$(AREA_FWHM25_LH): $(AREA_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure area -fwhm 25 $(T)

$(AREA_FWHM25_RH): $(AREA_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure area -fwhm 25 $(T)


$(VOLUME_FWHM0_LH): $(VOLUME_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure volume -fwhm 0 $(T)

$(VOLUME_FWHM0_RH): $(VOLUME_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure volume -fwhm 0 $(T)

$(VOLUME_FWHM5_LH): $(VOLUME_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure volume -fwhm 5 $(T)

$(VOLUME_FWHM5_RH): $(VOLUME_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure volume -fwhm 5 $(T)

$(VOLUME_FWHM10_LH): $(VOLUME_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure volume -fwhm 10 $(T)

$(VOLUME_FWHM10_RH): $(VOLUME_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure volume -fwhm 10 $(T)

$(VOLUME_FWHM15_LH): $(VOLUME_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure volume -fwhm 15 $(T)

$(VOLUME_FWHM15_RH): $(VOLUME_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure volume -fwhm 15 $(T)

$(VOLUME_FWHM20_LH): $(VOLUME_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure volume -fwhm 20 $(T)

$(VOLUME_FWHM20_RH): $(VOLUME_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure volume -fwhm 20 $(T)

$(VOLUME_FWHM25_LH): $(VOLUME_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure volume -fwhm 25 $(T)

$(VOLUME_FWHM25_RH): $(VOLUME_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure volume -fwhm 25 $(T)


$(JACOBIAN_WHITE_FWHM0_LH): $(JACOBIAN_WHITE_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure jacobian_white -fwhm 0 $(T)

$(JACOBIAN_WHITE_FWHM0_RH): $(JACOBIAN_WHITE_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure jacobian_white -fwhm 0 $(T)

$(JACOBIAN_WHITE_FWHM5_LH): $(JACOBIAN_WHITE_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure jacobian_white -fwhm 5 $(T)

$(JACOBIAN_WHITE_FWHM5_RH): $(JACOBIAN_WHITE_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure jacobian_white -fwhm 5 $(T)

$(JACOBIAN_WHITE_FWHM10_LH): $(JACOBIAN_WHITE_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure jacobian_white -fwhm 10 $(T)

$(JACOBIAN_WHITE_FWHM10_RH): $(JACOBIAN_WHITE_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure jacobian_white -fwhm 10 $(T)

$(JACOBIAN_WHITE_FWHM15_LH): $(JACOBIAN_WHITE_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure jacobian_white -fwhm 15 $(T)

$(JACOBIAN_WHITE_FWHM15_RH): $(JACOBIAN_WHITE_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure jacobian_white -fwhm 15 $(T)

$(JACOBIAN_WHITE_FWHM20_LH): $(JACOBIAN_WHITE_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure jacobian_white -fwhm 20 $(T)

$(JACOBIAN_WHITE_FWHM20_RH): $(JACOBIAN_WHITE_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure jacobian_white -fwhm 20 $(T)

$(JACOBIAN_WHITE_FWHM25_LH): $(JACOBIAN_WHITE_LH)
	recon-all -s $(subj) -hemi lh -qcache -measure jacobian_white -fwhm 25 $(T)

$(JACOBIAN_WHITE_FWHM25_RH): $(JACOBIAN_WHITE_RH)
	recon-all -s $(subj) -hemi rh -qcache -measure jacobian_white -fwhm 25 $(T)


#-------------------------------------------------------------------------
clean:
	rm -f $(AUTORECON1) $(AUTORECON2) $(AUTORECON3) $(QCACHE)

