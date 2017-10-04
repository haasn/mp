/*
 * This file is part of libplacebo.
 *
 * libplacebo is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libplacebo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libplacebo.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBPLACEBO_COLORSPACE_H
#define LIBPLACEBO_COLORSPACE_H

#include <stdbool.h>

// The underlying colorspace representation (e.g. RGB, XYZ or YCbCr)
enum pl_color_space {
    PL_COLOR_SPACE_UNKNOWN = 0,
    // YCbCr-like colorspaces:
    PL_COLOR_SPACE_BT_601,      // ITU-R Rec. BT.601 (SD)
    PL_COLOR_SPACE_BT_709,      // ITU-R Rec. BT.709 (HD)
    PL_COLOR_SPACE_SMPTE_240M,  // SMPTE-240M
    PL_COLOR_SPACE_BT_2020_NC,  // ITU-R Rec. BT.2020 (non-constant luminance)
    PL_COLOR_SPACE_BT_2020_C,   // ITU-R Rec. BT.2020 (constant luminance)
    PL_COLOR_SPACE_YCGCO,       // YCgCo (derived from RGB)
    // Other colorspaces:
    PL_COLOR_SPACE_RGB,         // Red, Green and Blue
    PL_COLOR_SPACE_XYZ,         // CIE 1931 XYZ
    PL_COLOR_SPACE_COUNT
};

bool pl_color_space_is_ycbcr_like(enum pl_color_space space);

// Guesses the best YCbCr-like colorspace based on a image given resolution.
// This only picks conservative values. (In particular, BT.2020 is never
// auto-guessed, even for 4K resolution content)
enum pl_color_space pl_color_space_guess_ycbcr(int width, int height);

// Returns a colorspace-dependent multiplication factor for converting from
// one bit depth to another. For YCbCr-like color spaces, this is equal to
// directly shifting the 8-bit range; i.e. 0-255 becomes 0-1020, not 0-1023.
float pl_color_space_texture_mul(enum pl_color_space space,
                                 int old_bits, int new_bits);

// The numerical range of the representation (where applicable).
enum pl_color_levels {
    PL_COLOR_LEVELS_UNKNOWN = 0,
    PL_COLOR_LEVELS_TV,         // TV range, e.g. 16-235
    PL_COLOR_LEVELS_PC,         // PC range, e.g. 0-255
    PL_COLOR_LEVELS_COUNT,
};

// The colorspace's primaries (gamut)
enum pl_color_primaries {
    PL_COLOR_PRIM_UNKNOWN = 0,
    // Standard gamut:
    PL_COLOR_PRIM_BT_601_525,   // ITU-R Rec. BT.601 (525-line = NTSC, SMPTE-C)
    PL_COLOR_PRIM_BT_601_625,   // ITU-R Rec. BT.601 (625-line = PAL, SECAM)
    PL_COLOR_PRIM_BT_709,       // ITU-R Rec. BT.709 (HD), also sRGB
    PL_COLOR_PRIM_BT_470M,      // ITU-R Rec. BT.470 M
    // Wide gamut:
    PL_COLOR_PRIM_BT_2020,      // ITU-R Rec. BT.2020 (UltraHD)
    PL_COLOR_PRIM_APPLE,        // Apple RGB
    PL_COLOR_PRIM_ADOBE,        // Adobe RGB (1998)
    PL_COLOR_PRIM_PRO_PHOTO,    // ProPhoto RGB (ROMM)
    PL_COLOR_PRIM_CIE_1931,     // CIE 1931 RGB primaries
    PL_COLOR_PRIM_DCI_P3,       // DCI-P3 (Digital Cinema)
    PL_COLOR_PRIM_V_GAMUT,      // Panasonic V-Gamut (VARICAM)
    PL_COLOR_PRIM_S_GAMUT,      // Sony S-Gamut
    PL_COLOR_PRIM_COUNT
};

bool pl_color_primaries_is_wide_gamut(enum pl_color_primaries prim);

// Guesses the best primaries based on a resolution. This always guesses
// conservatively, i.e. it will never return a wide gamut color space even if
// the resolution is 4K.
enum pl_color_primaries pl_color_primaries_guess(int width, int height);

// The colorspace's transfer function (gamma / EOTF)
enum pl_color_transfer {
    PL_COLOR_TRC_UNKNOWN = 0,
    // Standard dynamic range:
    PL_COLOR_TRC_BT_1886,       // ITU-R Rec. BT.1886 (CRT emulation + OOTF)
    PL_COLOR_TRC_SRGB,          // IEC 61966-2-4 sRGB (CRT emulation)
    PL_COLOR_TRC_LINEAR,        // Linear light content
    PL_COLOR_TRC_GAMMA18,       // Pure power gamma 1.8
    PL_COLOR_TRC_GAMMA22,       // Pure power gamma 2.2
    PL_COLOR_TRC_GAMMA28,       // Pure power gamma 2.8
    PL_COLOR_TRC_PRO_PHOTO,     // ProPhoto RGB (ROMM)
    // High dynamic range:
    PL_COLOR_TRC_PQ,            // ITU-R BT.2100 PQ (perceptual quantizer), aka SMPTE ST2048
    PL_COLOR_TRC_HLG,           // ITU-R BT.2100 HLG (hybrid log-gamma), aka ARIB STD-B67
    PL_COLOR_TRC_V_LOG,         // Panasonic V-Log (VARICAM)
    PL_COLOR_TRC_S_LOG1,        // Sony S-Log1
    PL_COLOR_TRC_S_LOG2,        // Sony S-Log2
    PL_COLOR_TRC_COUNT
};

// Returns the nominal peak of a given transfer function, relative to the
// reference white. This refers to the highest encodable signal level.
// Always equal to 1.0 for SDR curves.
float pl_color_transfer_nominal_peak(enum pl_color_transfer trc);

static inline bool pl_color_transfer_is_hdr(enum pl_color_transfer trc)
{
    return pl_color_transfer_nominal_peak(trc) > 1.0;
}

// This defines the standard reference white level (in cd/m^2) that is assumed
// throughout standards such as those from by ITU-R, EBU, etc.
// This is particularly relevant for HDR conversions, as this value is used
// as a reference for conversions between absolute transfer curves (e.g. PQ)
// and relative transfer curves (e.g. SDR, HLG).
#define PL_COLOR_REF_WHITE 100.0

// The semantic interpretation of the decoded image, how is it mastered?
enum pl_color_light {
    PL_COLOR_LIGHT_UNKNOWN = 0,
    PL_COLOR_LIGHT_DISPLAY,     // Display-referred, output as-is
    PL_COLOR_LIGHT_SCENE_HLG,   // Scene-referred, HLG OOTF
    PL_COLOR_LIGHT_SCENE_709_1886, // Scene-referred, OOTF = 709/1886 interaction
    PL_COLOR_LIGHT_SCENE_1_2,   // Scene-referred, OOTF = gamma 1.2
    PL_COLOR_LIGHT_COUNT
};

bool pl_color_light_is_scene_referred(enum pl_color_light light);

// Rendering intent for colorspace transformations. These constants match the
// ICC specification (Table 23)
enum pl_rendering_intent {
    PL_INTENT_PERCEPTUAL = 0,
    PL_INTENT_RELATIVE_COLORIMETRIC = 1,
    PL_INTENT_SATURATION = 2,
    PL_INTENT_ABSOLUTE_COLORIMETRIC = 3
};

// Struct describing a physical color space. This can be used to represent the
// different colorspaces at a high level.
struct pl_color {
    enum pl_color_space space;
    enum pl_color_levels levels;
    enum pl_color_primaries primaries;
    enum pl_color_transfer transfer;
    enum pl_color_light light;

    // The highest value that occurs in the signal, relative to the reference
    // white. (0 = unknown)
    float sig_peak;
};

extern const struct pl_color pl_color_unknown;

// Replaces unknown values in the first struct by those of the second struct.
void pl_color_merge(struct pl_color *orig, const struct pl_color *new);

// Returns whether two colorspaces are exactly identical.
bool pl_color_equal(struct pl_color c1, struct pl_color c2);

// This represents metadata about extra operations to perform during colorspace
// conversion, which correspond to artistic adjustments of the color.
struct pl_color_adjustment {
    // Brightness boost. 0.0 = neutral, 1.0 = solid white, -1.0 = solid black
    float brightness;
    // Contrast boost. 1.0 = neutral, 0.0 = solid black
    float contrast;
    // Saturation gain. 1.0 = neutral, 0.0 = grayscale
    float saturation;
    // Hue shift, corresponding to a rotation around the [U, V] subvector.
    // Only meaningful for YCbCr-like colorspaces. 0.0 = neutral
    float hue;
    // Gamma adjustment. 1.0 = neutral, 0.0 = solid black
    float gamma;
};

// A struct pre-filled with all-neutral values.
extern const struct pl_color_adjustment pl_color_adjustment_neutral;

// Represents the chroma placement with respect to the luma samples. This is
// only relevant for YCbCr-like colorspaces with chroma subsampling.
enum pl_chroma_location {
    PL_CHROMA_UNKNOWN = 0,
    PL_CHROMA_LEFT,             // MPEG2/4, H.264
    PL_CHROMA_CENTER,           // MPEG1, JPEG
    PL_CHROMA_COUNT,
};

// Fills *x and *y with the offset in half-pixels corresponding to a given
// chroma location.
void pl_chroma_location_offset(enum pl_chroma_location loc, int *x, int *y);

// Represents a single CIE xy coordinate (i.e. CIE Yxy with Y = 1.0)
struct pl_cie_xy {
    float x, y;
};

// Recovers (X / Y) from a CIE xy value.
static inline float pl_cie_X(struct pl_cie_xy xy) {
    return xy.x / xy.y;
}

// Recovers (Z / Y) from a CIE xy value.
static inline float pl_cie_Z(struct pl_cie_xy xy) {
    return (1 - xy.x - xy.y) / xy.y;
}

// Represents the raw physical primaries corresponding to a color space.
struct pl_raw_primaries {
    struct pl_cie_xy red, green, blue, white;
};

// Returns the raw primaries for a given color space.
struct pl_raw_primaries pl_raw_primaries_get(enum pl_color_primaries prim);

// Represents a row-major matrix, i.e. the following matrix
//     [ a11 a12 a13 ]
//     [ a21 a22 a23 ]
//     [ a31 a32 a33 ]
// is represented in C like this:
//   { { a11, a12, a13 },
//     { a21, a22, a23 },
//     { a31, a32, a33 } };
struct pl_color_matrix {
    float m[3][3];
};

// Inverts a color matrix. Only use where precision is not that important.
struct pl_color_matrix pl_color_matrix_invert(struct pl_color_matrix m);

// Represents an affine transformation, which is basically a 3x3 color matrix
// together with a column vector to add onto the output. The interpretation of
// `m` is identical to `pl_color_matrix`.
struct pl_color_transform {
    struct pl_color_matrix mat;
    float c[3];
};

// Inverts a color transform. Only use where precision is not that important.
struct pl_color_transform pl_color_transform_invert(struct pl_color_transform t);

// Returns an RGB->XYZ conversion matrix for a given set of primaries.
// Multiplying this into the RGB color transforms it to CIE XYZ, centered
// around the color space's white point.
struct pl_color_matrix pl_get_rgb2xyz_matrix(struct pl_raw_primaries prim);

// Similar to pl_get_rgb2xyz_matrix, but gives the inverse transformation.
struct pl_color_matrix pl_get_xyz2rgb_matrix(struct pl_raw_primaries prim);

// Returns a primary adaptation matrix, which converts from one set of
// primaries to another. This is an RGB->RGB transformation. For rendering
// intents other than PL_INTENT_ABSOLUTE_COLORIMETRIC, the white point is
// adapted using the Bradford matrix.
struct pl_color_matrix pl_get_rgb2rgb_matrix(struct pl_raw_primaries src,
                                             struct pl_raw_primaries dst,
                                             enum pl_rendering_intent intent);

// Returns a YUV->RGB conversion matrix for a given combination of source
// space, adjustment parameters, destination color levels, and texture bit
// depths. This also works for XYZ->RGB and RGB->RGB, which still benefit from
// the applicable artistic adjustments and levels conversions. The resulting
// matrix does not perform any gamut mapping, it merely takes care of the
// conversion to RGB.
//
// Note: For BT.2020 constant-luminance, this outputs chroma information in the
// range [-0.5, 0.5]. Since the CL system conversion is non-linear, further
// processing must be done by the caller. The channel order is CrYCb
struct pl_color_transform pl_get_yuv2rgb_matrix(struct pl_color color,
                                                struct pl_color_adjustment params,
                                                int in_bits, int out_bits,
                                                enum pl_color_levels out_levels);

#endif // LIBPLACEBO_COLORSPACE_H
