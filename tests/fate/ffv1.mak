# This Makefile checks for $(CONFIG_...) variables being set, so we can
# include/exclude tests accordingly:
ifdef CONFIG_AVCONV
FLAGS_FFV1_V3 = -strict experimental
else
FLAGS_FFV1_V3 =
endif

DEC_SRC = $(TARGET_PATH)/tests/data/fate

fate-ffv1-enc-%: CODEC = $(word 2, $(subst -, ,$(@)))
fate-ffv1-enc-%: FMT = avi
fate-ffv1-enc-%: SRC = tests/data/vsynth1.yuv
# Limit the duration of test videos to 4 frames at 25fps:
fate-ffv1-enc-%: DUR = 0:00:00.160
fate-ffv1-enc-%: CMD = enc_dec "rawvideo -s 352x288 -pix_fmt yuv420p $(RAWDECOPTS)" $(SRC) $(FMT) "-t $(DUR) -c $(CODEC) $(ENCOPTS)" rawvideo "-s 352x288 -pix_fmt yuv420p -vsync 0 $(DECOPTS)" -keep "$(DECINOPTS)"


FATE_FFV1_LEVEL1 =     v1-defaults \
                       v1-gray \
                       v1-rgb32 \
                       v1-yuv410p \
                       v1-yuv411p \
                       v1-yuv420p \
                       v1-yuv422p \
                       v1-yuv444p \
                       v1-bgra \
                       v1-tff \
                       v1-bff

# Target-specific tests:
ifdef CONFIG_FFMPEG
FATE_FFV1_LEVEL1 +=    v1-bgr0 \
                       v1-yuv440p \
                       v1-yuva420p \
                       v1-yuva422p \
                       v1-yuva444p
endif

FATE_FFV1_LEVEL3 =     v3-defaults \
                       v3-rgb32 \
                       v3-yuv410p \
                       v3-yuv420p \
                       v3-yuv422p \
                       v3-yuv444p \
                       v3-yuv420p9 \
                       v3-yuv422p9 \
                       v3-yuv444p9 \
                       v3-yuv420p10 \
                       v3-yuv422p10 \
                       v3-yuv444p10 \
                       v3-yuv420p16 \
                       v3-yuv422p16 \
                       v3-yuv444p16 \
                       v3-yuv422p_crc \
                       v3-yuv422p9_crc \
                       v3-yuv422p10_crc \
                       v3-yuv422p16_crc \
                       v3-yuv422p_pass1 \
                       v3-yuv422p_pass2 \
                       v3-tff \
                       v3-bff

# Target-specific tests:
ifdef CONFIG_FFMPEG
FATE_FFV1_LEVEL3 +=    v3-gray \
                       v3-gray16 \
                       v3-bgr0 \
                       v3-gbrp9 \
                       v3-gbrp10 \
                       v3-gbrp12 \
                       v3-gbrp14 \
                       v3-yuva420p9 \
                       v3-yuva422p9 \
                       v3-yuva444p9 \
                       v3-yuva420p10 \
                       v3-yuva422p10 \
                       v3-yuva444p10 \
                       v3-yuva420p16 \
                       v3-yuva422p16 \
                       v3-yuva444p16
endif


FATE_FFV1 = $(FATE_FFV1_LEVEL1) \
            $(FATE_FFV1_LEVEL3)


# ------------ FFV1 - version 1
###################################################
#  Encoding:
###################################################
#  YUV (8bit)
#  - This also iterates through all coder/context combinations.
fate-ffv1-enc-v1-defaults:       ENCOPTS = -level 1
fate-ffv1-enc-v1-yuv410p:        ENCOPTS = -level 1 -g 1 -coder 0 -context 0 -pix_fmt yuv410p
fate-ffv1-enc-v1-yuv411p:        ENCOPTS = -level 1 -g 1 -coder 0 -context 0 -pix_fmt yuv411p
fate-ffv1-enc-v1-yuv420p:        ENCOPTS = -level 1 -g 1 -coder 0 -context 1 -pix_fmt yuv420p
fate-ffv1-enc-v1-yuv422p:        ENCOPTS = -level 1 -g 1 -coder 1 -context 0 -pix_fmt yuv422p
fate-ffv1-enc-v1-yuv444p:        ENCOPTS = -level 1 -g 1 -coder 1 -context 1 -pix_fmt yuv444p
fate-ffv1-enc-v1-yuv440p:        ENCOPTS = -level 1 -g 1 -coder 1 -context 1 -pix_fmt yuv440p
#  Gray (8bit)
fate-ffv1-enc-v1-gray:           ENCOPTS = -level 1 -g 1 -coder 0 -context 0 -pix_fmt gray
#  RGB (8bit)
fate-ffv1-enc-v1-rgb32:          ENCOPTS = -level 1 -g 1 -coder 0 -context 0 -pix_fmt rgb32
fate-ffv1-enc-v1-bgr0:           ENCOPTS = -level 1 -g 1 -coder 0 -context 0 -pix_fmt bgr0
#  Alpha channel / transparency:
fate-ffv1-enc-v1-bgra:           ENCOPTS = -level 1 -g 1 -coder 0 -context 0 -pix_fmt bgra
fate-ffv1-enc-v1-yuva420p:       ENCOPTS = -level 1 -g 1 -coder 0 -context 1 -pix_fmt yuva420p
fate-ffv1-enc-v1-yuva422p:       ENCOPTS = -level 1 -g 1 -coder 1 -context 0 -pix_fmt yuva422p
fate-ffv1-enc-v1-yuva444p:       ENCOPTS = -level 1 -g 1 -coder 1 -context 1 -pix_fmt yuva444p
# Interlaced:
fate-ffv1-enc-v1-tff:            ENCOPTS = -s pal -level 1 -top 1 -pix_fmt yuv422p
fate-ffv1-enc-v1-bff:            ENCOPTS = -s pal -level 1 -top 0 -pix_fmt yuv422p

###################################################
#  Decoding:
###################################################
#  YUV (8bit)
fate-ffv1-dec-v1-defaults:       $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-defaults.avi) fate-ffv1-enc-v1-defaults
fate-ffv1-dec-v1-yuv410p:        $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuv410p.avi) fate-ffv1-enc-v1-yuv410p
fate-ffv1-dec-v1-yuv411p:        $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuv411p.avi) fate-ffv1-enc-v1-yuv411p
fate-ffv1-dec-v1-yuv420p:        $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuv420p.avi) fate-ffv1-enc-v1-yuv420p
fate-ffv1-dec-v1-yuv422p:        $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuv422p.avi) fate-ffv1-enc-v1-yuv422p
fate-ffv1-dec-v1-yuv444p:        $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuv444p.avi) fate-ffv1-enc-v1-yuv444p
fate-ffv1-dec-v1-yuv440p:        $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuv440p.avi) fate-ffv1-enc-v1-yuv440p
#  Gray (8bit)
fate-ffv1-dec-v1-gray:           $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-gray.avi) fate-ffv1-enc-v1-gray
#  RGB (8bit)
fate-ffv1-dec-v1-rgb32:          $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-rgb32.avi) fate-ffv1-enc-v1-rgb32
fate-ffv1-dec-v1-bgr0:           $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-bgr0.avi) fate-ffv1-enc-v1-bgr0
#  Alpha channel / transparency:
fate-ffv1-dec-v1-bgra:           $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-bgra.avi) fate-ffv1-enc-v1-bgra
fate-ffv1-dec-v1-yuva420p:       $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuva420p.avi) fate-ffv1-enc-v1-yuva420p
fate-ffv1-dec-v1-yuva422p:       $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuva422p.avi) fate-ffv1-enc-v1-yuva422p
fate-ffv1-dec-v1-yuva444p:       $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-yuva444p.avi) fate-ffv1-enc-v1-yuva444p
# Interlaced:
fate-ffv1-dec-v1-tff:            $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-tff.avi) fate-ffv1-enc-v1-tff
fate-ffv1-dec-v1-bff:            $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v1-bff.avi) fate-ffv1-enc-v1-bff


# ------------ FFV1 - version 3
###################################################
#  Encoding:
###################################################
#  - This also iterates through slice variations (4, 12, 24, 30).
#
fate-ffv1-enc-v3-defaults:       ENCOPTS = -level 3 $(FLAGS_FFV1_V3)
#  YUV (8bit)
#  - This also iterates through all coder/context combinations.
fate-ffv1-enc-v3-yuv410p:        ENCOPTS = -level 3 -g 1 -coder 0 -context 0 -slices 4 -slicecrc 0 -pix_fmt yuv410p $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv420p:        ENCOPTS = -level 3 -g 1 -coder 0 -context 1 -slices 12 -slicecrc 0 -pix_fmt yuv420p $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv422p:        ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 24 -slicecrc 0 -pix_fmt yuv422p $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv444p:        ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 30 -slicecrc 0 -pix_fmt yuv444p $(FLAGS_FFV1_V3)
#  YUV (9bit)
fate-ffv1-enc-v3-yuv420p9:       ENCOPTS = -level 3 -g 1 -coder -1 -context 1 -slices 24 -slicecrc 0 -pix_fmt yuv420p9 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv422p9:       ENCOPTS = -level 3 -g 1 -coder 2 -context 0 -slices 30 -slicecrc 0 -pix_fmt yuv422p9 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv444p9:       ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 4  -slicecrc 0 -pix_fmt yuv444p9 $(FLAGS_FFV1_V3)
#  YUV (10bit)
fate-ffv1-enc-v3-yuv420p10:      ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 30 -slicecrc 0 -pix_fmt yuv420p10 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv422p10:      ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 4  -slicecrc 0 -pix_fmt yuv422p10 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv444p10:      ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 12 -slicecrc 0 -pix_fmt yuv444p10 $(FLAGS_FFV1_V3)
#  YUV (16bit)
fate-ffv1-enc-v3-yuv420p16:      ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 4  -slicecrc 0 -pix_fmt yuv420p16 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv422p16:      ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 12 -slicecrc 0 -pix_fmt yuv422p16 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv444p16:      ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 24 -slicecrc 0 -pix_fmt yuv444p16 $(FLAGS_FFV1_V3)
#  Gray
fate-ffv1-enc-v3-gray:           ENCOPTS = -level 3 -g 1 -coder 0 -context 0 -slices 24 -slicecrc 0 -pix_fmt gray $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-gray16:         ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 24 -slicecrc 0 -pix_fmt gray16 $(FLAGS_FFV1_V3)
#  RGB
fate-ffv1-enc-v3-rgb32:          ENCOPTS = -level 3 -g 1 -coder 0 -context 0 -slices 24 -slicecrc 0 -pix_fmt rgb32 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-bgr0:           ENCOPTS = -level 3 -g 1 -coder 0 -context 0 -slices 24 -slicecrc 0 -pix_fmt bgr0 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-gbrp9:          ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 24 -slicecrc 0 -pix_fmt gbrp9 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-gbrp10:         ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 24 -slicecrc 0 -pix_fmt gbrp10 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-gbrp12:         ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 24 -slicecrc 0 -pix_fmt gbrp12 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-gbrp14:         ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 24 -slicecrc 0 -pix_fmt gbrp14 $(FLAGS_FFV1_V3)

# Interlaced:
fate-ffv1-enc-v3-tff:            ENCOPTS = -s pal -level 3 -top 1 -pix_fmt yuv422p $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-bff:            ENCOPTS = -s pal -level 3 -top 0 -pix_fmt yuv422p $(FLAGS_FFV1_V3)

# Slice CRC: On
fate-ffv1-enc-v3-yuv422p_crc:    ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 30 -slicecrc 1 -pix_fmt yuv422p $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv422p9_crc:   ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 30 -slicecrc 1 -pix_fmt yuv422p9 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv422p10_crc:  ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 30 -slicecrc 1 -pix_fmt yuv422p10 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuv422p16_crc:  ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 30 -slicecrc 1 -pix_fmt yuv422p16 $(FLAGS_FFV1_V3)

# Multipass:
fate-ffv1-enc-v3-yuv422p_pass1:  ENCOPTS = -level 3 -an -pix_fmt yuv422p -pass 1 -passlogfile $(DEC_SRC)/ffv1-multipass $(FLAGS_FFV1_V3)

fate-ffv1-enc-v3-yuv422p_pass2:  ${ENCOPTS = -level 3 -pix_fmt yuv422p -pass 2 -passlogfile $(DEC_SRC)/ffv1-multipass $(FLAGS_FFV1_V3)} fate-ffv1-enc-v3-yuv422p_pass1

# Alpha channel / transparency:
#  YUV-A (9bit)
fate-ffv1-enc-v3-yuva420p9:      ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 24 -slicecrc 0 -pix_fmt yuva420p9 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuva422p9:      ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 30 -slicecrc 0 -pix_fmt yuva422p9 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuva444p9:      ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 4  -slicecrc 0 -pix_fmt yuva444p9 $(FLAGS_FFV1_V3)
#  YUV-A (10bit)
fate-ffv1-enc-v3-yuva420p10:     ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 24 -slicecrc 0 -pix_fmt yuva420p10 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuva422p10:     ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 30 -slicecrc 0 -pix_fmt yuva422p10 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuva444p10:     ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 4  -slicecrc 0 -pix_fmt yuva444p10 $(FLAGS_FFV1_V3)
#  YUV-A (16bit)
fate-ffv1-enc-v3-yuva420p16:     ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 24 -slicecrc 0 -pix_fmt yuva420p16 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuva422p16:     ENCOPTS = -level 3 -g 1 -coder 1 -context 0 -slices 30 -slicecrc 0 -pix_fmt yuva422p16 $(FLAGS_FFV1_V3)
fate-ffv1-enc-v3-yuva444p16:     ENCOPTS = -level 3 -g 1 -coder 1 -context 1 -slices 4  -slicecrc 0 -pix_fmt yuva444p16 $(FLAGS_FFV1_V3)


###################################################
#  Decoding:
###################################################
#
fate-ffv1-dec-v3-defaults:       ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-defaults.avi} fate-ffv1-enc-v3-defaults
#  YUV (8bit)
fate-ffv1-dec-v3-yuv410p:        ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv410p.avi} fate-ffv1-enc-v3-yuv410p
fate-ffv1-dec-v3-yuv420p:        ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv420p.avi} fate-ffv1-enc-v3-yuv420p
fate-ffv1-dec-v3-yuv422p:        ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p.avi} fate-ffv1-enc-v3-yuv422p
fate-ffv1-dec-v3-yuv444p:        ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv444p.avi} fate-ffv1-enc-v3-yuv444p
#  YUV (9bit)
fate-ffv1-dec-v3-yuv420p9:       ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv420p9.avi} fate-ffv1-enc-v3-yuv420p9
fate-ffv1-dec-v3-yuv422p9:       ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p9.avi} fate-ffv1-enc-v3-yuv422p9
fate-ffv1-dec-v3-yuv444p9:       ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv444p9.avi} fate-ffv1-enc-v3-yuv444p9
#  YUV (10bit)
fate-ffv1-dec-v3-yuv420p10:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv420p10.avi} fate-ffv1-enc-v3-yuv420p10
fate-ffv1-dec-v3-yuv422p10:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p10.avi} fate-ffv1-enc-v3-yuv422p10
fate-ffv1-dec-v3-yuv444p10:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv444p10.avi} fate-ffv1-enc-v3-yuv444p10
#  YUV (16bit)
fate-ffv1-dec-v3-yuv420p16:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv420p16.avi} fate-ffv1-enc-v3-yuv420p16
fate-ffv1-dec-v3-yuv422p16:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p16.avi} fate-ffv1-enc-v3-yuv422p16
fate-ffv1-dec-v3-yuv444p16:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv444p16.avi} fate-ffv1-enc-v3-yuv444p16
#  Gray
fate-ffv1-dec-v3-gray:           ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-gray.avi} fate-ffv1-enc-v3-gray
fate-ffv1-dec-v3-gray16:         ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-gray16.avi} fate-ffv1-enc-v3-gray16
#  RGB
fate-ffv1-dec-v3-rgb32:          ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-rgb32.avi} fate-ffv1-enc-v3-rgb32
fate-ffv1-dec-v3-bgr0:           ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-bgr0.avi} fate-ffv1-enc-v3-bgr0
fate-ffv1-dec-v3-gbrp9:          ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-gbrp9.avi} fate-ffv1-enc-v3-gbrp9
fate-ffv1-dec-v3-gbrp10:         ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-gbrp10.avi} fate-ffv1-enc-v3-gbrp10
fate-ffv1-dec-v3-gbrp12:         ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-gbrp12.avi} fate-ffv1-enc-v3-gbrp12
fate-ffv1-dec-v3-gbrp14:         ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-gbrp14.avi} fate-ffv1-enc-v3-gbrp14

# Interlaced:
fate-ffv1-dec-v3-tff:            ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-tff.avi} fate-ffv1-enc-v3-tff
fate-ffv1-dec-v3-bff:            ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-bff.avi} fate-ffv1-enc-v3-bff

# Slice CRC: On
fate-ffv1-dec-v3-yuv422p_crc:    ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p_crc.avi} fate-ffv1-enc-v3-yuv422p_crc
fate-ffv1-dec-v3-yuv422p9_crc:   ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p9_crc.avi} fate-ffv1-enc-v3-yuv422p9_crc
fate-ffv1-dec-v3-yuv422p10_crc:  ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p10_crc.avi} fate-ffv1-enc-v3-yuv422p10_crc
fate-ffv1-dec-v3-yuv422p16_crc:  ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p16_crc.avi} fate-ffv1-enc-v3-yuv422p16_crc

# Multipass:
fate-ffv1-dec-v3-yuv422p_pass1:  ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p_pass1.avi} fate-ffv1-enc-v3-yuv422p_pass1
fate-ffv1-dec-v3-yuv422p_pass2:  $(CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuv422p_pass2.avi) fate-ffv1-enc-v3-yuv422p_pass2

# Alpha channel / transparency:
#  YUV (9bit)
fate-ffv1-dec-v3-yuva420p9:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva420p9.avi} fate-ffv1-enc-v3-yuva420p9
fate-ffv1-dec-v3-yuva422p9:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva422p9.avi} fate-ffv1-enc-v3-yuva422p9
fate-ffv1-dec-v3-yuva444p9:      ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva444p9.avi} fate-ffv1-enc-v3-yuva444p9
#  YUV (10bit)
fate-ffv1-dec-v3-yuva420p10:     ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva420p10.avi} fate-ffv1-enc-v3-yuva420p10
fate-ffv1-dec-v3-yuva422p10:     ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva422p10.avi} fate-ffv1-enc-v3-yuva422p10
fate-ffv1-dec-v3-yuva444p10:     ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva444p10.avi} fate-ffv1-enc-v3-yuva444p10
#  YUV (16bit)
fate-ffv1-dec-v3-yuva420p16:     ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva420p16.avi} fate-ffv1-enc-v3-yuva420p16
fate-ffv1-dec-v3-yuva422p16:     ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva422p16.avi} fate-ffv1-enc-v3-yuva422p16
fate-ffv1-dec-v3-yuva444p16:     ${CMD = framecrc -i $(DEC_SRC)/ffv1-enc-v3-yuva444p16.avi} fate-ffv1-enc-v3-yuva444p16

###################################################
# Testing error resilience:
###################################################
#fate-ffv1-fuzzed1:               CMD = framecrc -i $(TARGET_SAMPLES)/ffv1/ffv1.3-yuv422p-fuzzed.avi
#fate-ffv1-fuzzed2:               CMD = framecrc -i $(TARGET_SAMPLES)/ffv1/ffv1.3-yuv422p_crc-fuzzed.avi

###################################################
# Testing invalid arguments:
###################################################
#fate-ffv1-invalid1:              ENCOPTS = -coder 1 -context -1
#fate-ffv1-invalid2:              ENCOPTS = -level 3 -slices 3
#fate-ffv1-invalid3:              ENCOPTS = -pix_fmt gbrp16
#fate-ffv1-invalid4:              ENCOPTS = -level 2
#fate-ffv1-invalid5:              ENCOPTS = -level 3 -coder 0 -context 0 -slices 24 -slicecrc 0 -pix_fmt gbrp9



###################################################
FATE_FFV1 := $(FATE_FFV1:%=fate-ffv1-enc-%) \
             $(FATE_FFV1:%=fate-ffv1-dec-%) \

#             fate-ffv1-invalid1 \
#             fate-ffv1-invalid2 \
#             fate-ffv1-invalid3 \
#             fate-ffv1-invalid4 \
#             fate-ffv1-invalid5 \
#             fate-ffv1-fuzzed1 \
#             fate-ffv1-fuzzed2

FATE_FFV1_LEVEL1 := $(FATE_FFV1_LEVEL1:%=fate-ffv1-enc-%) \
                    $(FATE_FFV1_LEVEL1:%=fate-ffv1-dec-%)
FATE_FFV1_LEVEL3 := $(FATE_FFV1_LEVEL3:%=fate-ffv1-enc-%) \
                    $(FATE_FFV1_LEVEL3:%=fate-ffv1-dec-%)

FATE_FFV1-$(call ENCDEC, FFV1, AVI) += $(FATE_FFV1)
FATE_FFV1_LEVEL1-$(call ENCDEC, FFV1, AVI) += $(FATE_FFV1_LEVEL1)
FATE_FFV1_LEVEL3-$(call ENCDEC, FFV1, AVI) += $(FATE_FFV1_LEVEL3)

FATE_SAMPLES_AVCONV += $(FATE_FFV1-yes)
fate-ffv1: $(FATE_FFV1-yes)
fate-ffv1.1: $(FATE_FFV1_LEVEL1-yes)
fate-ffv1.3: $(FATE_FFV1_LEVEL3-yes)

# Requires generating vsynth1.yuv as input source:
$(FATE_FFV1-yes): tests/data/vsynth1.yuv

