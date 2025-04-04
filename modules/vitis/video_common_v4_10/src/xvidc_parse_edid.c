/* vim: set et fde fdm=syntax ft=c.doxygen ts=4 sts=4 sw=4 : */
/*
 * Copyright © 2011 Saleem Abdulrasool <compnerd@compnerd.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*****************************************************************************/
/**
 *
 * @file xvidc_parse_edid.c
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date        Changes
 * ----- --- ----------   -----------------------------------------------
 * 1.00  mmo 24-01-2017   Included into video_common
 * 1.01  eb  13-04-2018   Fixed XV_VidC_parse_edid API and
 *                            xvidc_edid_extension_handler struct to enhance
 *                            system stability
 * 1.10  eb  03-08-2018   Updated XV_VidC_parse_edid
 * </pre>
 *
 ******************************************************************************/
#include "string.h"
#include "stdlib.h"
#include "stddef.h"

#include "xil_types.h"
#include "xstatus.h"
#include "xil_exception.h"

#include "xvidc_edid_ext.h"

#if XVIDC_EDID_VERBOSITY > 1
static const struct xvidc_cea861_timing {
    const u16 hactive;
    const u16 vactive;
    const enum {
        INTERLACED,
        PROGRESSIVE,
    } mode;
    const u16 htotal;
    const u16 hblank;
    const u16 vtotal;
    const double   vblank;
    const double   hfreq;
    const double   vfreq;
    const double   pixclk;
} xvidc_cea861_timings[] = {
    [  1]  = {  640, 480, PROGRESSIVE,  800,  160,  525, 45.0,  31.469,  59.940,  25.175 },
    [  2]  = {  720, 480, PROGRESSIVE,  858,  138,  525, 45.0,  31.469,  59.940,  27.000 },
    [  3]  = {  720, 480, PROGRESSIVE,  858,  138,  525, 45.0,  31.469,  59.940,  27.000 },
    [  4]  = { 1280, 720, PROGRESSIVE, 1650,  370,  750, 30.0,  45.000,  60.000,  74.250 },
    [  5]  = { 1920,1080,  INTERLACED, 2200,  280, 1125, 22.5,  33.750,  60.000,  72.250 },
    [  6]  = { 1440, 480,  INTERLACED, 1716,  276,  525, 22.5,  15.734,  59.940,  27.000 },
    [  7]  = { 1440, 480,  INTERLACED, 1716,  276,  525, 22.5,  15.734,  59.940,  27.000 },
    [  8]  = { 1440, 240, PROGRESSIVE, 1716,  276,  262, 22.0,  15.734,  60.054,  27.000 },
    [  9]  = { 1440, 240, PROGRESSIVE, 1716,  276,  262, 22.0,  15.734,  59.826,  27.000 },
    [ 10] = { 2880,  480,  INTERLACED, 3432,  552,  525, 22.5,  15.734,  59.940,  54.000 },
    [ 11] = { 2880,  480,  INTERLACED, 3432,  552,  525, 22.5,  15.734,  59.940,  54.000 },
    [ 12] = { 2880,  240, PROGRESSIVE, 3432,  552,  262, 22.0,  15.734,  60.054,  54.000 },
    [ 13] = { 2880,  240, PROGRESSIVE, 3432,  552,  262, 22.0,  15.734,  59.826,  54.000 },
    [ 14] = { 1440,  480, PROGRESSIVE, 1716,  276,  525, 45.0,  31.469,  59.940,  54.000 },
    [ 15] = { 1440,  480, PROGRESSIVE, 1716,  276,  525, 45.0,  31.469,  59.940,  54.000 },
    [ 16] = { 1920, 1080, PROGRESSIVE, 2200,  280, 1125, 45.0,  67.500,  60.000, 148.500 },
    [ 17] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0,  31.250,  50.000,  27.000 },
    [ 18] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0,  31.250,  50.000,  27.000 },
    [ 19] = { 1280,  720, PROGRESSIVE, 1980,  700,  750, 30.0,  37.500,  50.000,  74.250 },
    [ 20] = { 1920, 1080,  INTERLACED, 2640,  720, 1125, 22.5,  28.125,  50.000,  74.250 },
    [ 21] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  15.625,  50.000,  27.000 },
    [ 22] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  15.625,  50.000,  27.000 },
    [ 23] = { 1440,  288, PROGRESSIVE, 1728,  288,  312, 24.0,  15.625,  50.080,  27.000 },
    [ 24] = { 1440,  288, PROGRESSIVE, 1728,  288,  313, 25.0,  15.625,  49.920,  27.000 },
    [ 25] = { 2880,  576,  INTERLACED, 3456,  576,  625, 24.5,  15.625,  50.000,  54.000 },
    [ 26] = { 2880,  576,  INTERLACED, 3456,  576,  625, 24.5,  15.625,  50.000,  54.000 },
    [ 27] = { 2880,  288, PROGRESSIVE, 3456,  576,  312, 24.0,  15.625,  50.080,  54.000 },
    [ 28] = { 2880,  288, PROGRESSIVE, 3456,  576,  313, 25.0,  15.625,  49.920,  54.000 },
    [ 29] = { 1440,  576, PROGRESSIVE, 1728,  288,  625, 49.0,  31.250,  50.000,  54.000 },
    [ 30] = { 1440,  576, PROGRESSIVE, 1728,  288,  625, 49.0,  31.250,  50.000,  54.000 },
    [ 31] = { 1920, 1080, PROGRESSIVE, 2640,  720, 1125, 45.0,  56.250,  50.000, 148.500 },
    [ 32] = { 1920, 1080, PROGRESSIVE, 2750,  830, 1125, 45.0,  27.000,  24.000,  74.250 },
    [ 33] = { 1920, 1080, PROGRESSIVE, 2640,  720, 1125, 45.0,  28.125,  25.000,  74.250 },
    [ 34] = { 1920, 1080, PROGRESSIVE, 2200,  280, 1125, 45.0,  33.750,  30.000,  74.250 },
    [ 35] = { 2880,  480, PROGRESSIVE, 3432,  552,  525, 45.0,  31.469,  59.940, 108.500 },
    [ 36] = { 2880,  480, PROGRESSIVE, 3432,  552,  525, 45.0,  31.469,  59.940, 108.500 },
    [ 37] = { 2880,  576, PROGRESSIVE, 3456,  576,  625, 49.0,  31.250,  50.000, 108.000 },
    [ 38] = { 2880,  576, PROGRESSIVE, 3456,  576,  625, 49.0,  31.250,  50.000, 108.000 },
    [ 39] = { 1920, 1080,  INTERLACED, 2304,  384, 1250, 85.0,  31.250,  50.000,  72.000 },
    [ 40] = { 1920, 1080,  INTERLACED, 2640,  720, 1125, 22.5,  56.250, 100.000, 148.500 },
    [ 41] = { 1280,  720, PROGRESSIVE, 1980,  700,  750, 30.0,  75.000, 100.000, 148.500 },
    [ 42] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0,  62.500, 100.000,  54.000 },
    [ 43] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0,  62.500, 100.000,  54.000 },
    [ 44] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  31.250, 100.000,  54.000 },
    [ 45] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  31.250, 100.000,  54.000 },
    [ 46] = { 1920, 1080,  INTERLACED, 2200,  280, 1125, 22.5,  67.500, 120.000, 148.500 },
    [ 47] = { 1280,  720, PROGRESSIVE, 1650,  370,  750, 30.0,  90.000, 120.000, 148.500 },
    [ 48] = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0,  62.937, 119.880,  54.000 },
    [ 49] = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0,  62.937, 119.880,  54.000 },
    [ 50] = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  31.469, 119.880,  54.000 },
    [ 51] = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  31.469, 119.880,  54.000 },
    [ 52] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0, 125.000, 200.000, 108.000 },
    [ 53] = {  720,  576, PROGRESSIVE,  864,  144,  625, 49.0, 125.000, 200.000, 108.000 },
    [ 54] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  62.500, 200.000, 108.000 },
    [ 55] = { 1440,  576,  INTERLACED, 1728,  288,  625, 24.5,  62.500, 200.000, 108.000 },
    [ 56] = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0, 125.874, 239.760, 108.000 },
    [ 57] = {  720,  480, PROGRESSIVE,  858,  138,  525, 45.0, 125.874, 239.760, 108.000 },
    [ 58] = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  62.937, 239.760, 108.000 },
    [ 59] = { 1440,  480,  INTERLACED, 1716,  276,  525, 22.5,  62.937, 239.760, 108.000 },
    [60 ] = {1280,  720 , PROGRESSIVE, 3300, 2020, 750 , 30  , 18     , 24.0003, 59.4    },
    [61 ] = {1280,  720 , PROGRESSIVE, 3960, 2680, 750 , 30  , 18.75  , 25     , 74.25   },
    [62 ] = {1280,  720 , PROGRESSIVE, 3300, 2020, 750 , 30  , 22.5   , 30.0003, 74.25   },
    [63 ] = {1920,  1080, PROGRESSIVE, 2200, 280 , 1125, 45  , 135    , 120.003, 297     },
    [64 ] = {1920,  1080, PROGRESSIVE, 2640, 720 , 1125, 45  , 112.5  , 100    , 297     },
    [65 ] = {1280,  720 , PROGRESSIVE, 3300, 2020, 750 , 30  , 18     , 24.0003, 59.4    },
    [66 ] = {1280,  720 , PROGRESSIVE, 3960, 2680, 750 , 30  , 18.75  , 25     , 74.25   },
    [67 ] = {1280,  720 , PROGRESSIVE, 3300, 2020, 750 , 30  , 22.5   , 30.0003, 74.25   },
    [68 ] = {1280,  720 , PROGRESSIVE, 1980, 700 , 750 , 30  , 37.5   , 50     , 74.25   },
    [69 ] = {1280,  720 , PROGRESSIVE, 1650, 370 , 750 , 30  , 45     , 60.0003, 74.25   },
    [70 ] = {1280,  720 , PROGRESSIVE, 1980, 700 , 750 , 30  , 75     , 100    , 148.5   },
    [71 ] = {1280,  720 , PROGRESSIVE, 1650, 370 , 750 , 30  , 90     , 120.003, 148.5   },
    [72 ] = {1920,  1080, PROGRESSIVE, 2750, 830 , 1125, 45  , 27     , 24.0003, 74.25   },
    [73 ] = {1920,  1080, PROGRESSIVE, 2640, 720 , 1125, 45  , 28.125 , 25     , 74.25   },
    [74 ] = {1920,  1080, PROGRESSIVE, 2200, 280 , 1125, 45  , 33.75  , 30.0003, 74.25   },
    [75 ] = {1920,  1080, PROGRESSIVE, 2640, 720 , 1125, 45  , 56.25  , 50     , 148.5   },
    [76 ] = {1920,  1080, PROGRESSIVE, 2200, 280 , 1125, 45  , 67.5   , 60.0003, 148.5   },
    [77 ] = {1920,  1080, PROGRESSIVE, 2640, 720 , 1125, 45  , 112.5  , 100    , 297     },
    [78 ] = {1920,  1080, PROGRESSIVE, 2200, 280 , 1125, 45  , 135    , 120.003, 297     },
    [79 ] = {1680,  720 , PROGRESSIVE, 3300, 1620, 750 , 30  , 18     , 24.0003, 59.4    },
    [80 ] = {1680,  720 , PROGRESSIVE, 3168, 1488, 750 , 30  , 18.75  , 25     , 59.4    },
    [81 ] = {1680,  720 , PROGRESSIVE, 2640, 960 , 750 , 30  , 22.5   , 30.0003, 59.4    },
    [82 ] = {1680,  720 , PROGRESSIVE, 2200, 520 , 750 , 30  , 37.5   , 50     , 82.5    },
    [83 ] = {1680,  720 , PROGRESSIVE, 2200, 520 , 750 , 30  , 45     , 60.0003, 99      },
    [84 ] = {1680,  720 , PROGRESSIVE, 2000, 320 , 825 , 105 , 82.5   , 100    , 165     },
    [85 ] = {1680,  720 , PROGRESSIVE, 2000, 320 , 825 , 105 , 99     , 120.003, 198     },
    [86 ] = {2560,  1080, PROGRESSIVE, 3750, 1190, 1100, 20  , 26.4   , 24.0003, 99      },
    [87 ] = {2560,  1080, PROGRESSIVE, 3200, 640 , 1125, 45  , 28.125 , 25     , 90      },
    [88 ] = {2560,  1080, PROGRESSIVE, 3520, 960 , 1125, 45  , 33.75  , 30.0003, 118.8   },
    [89 ] = {2560,  1080, PROGRESSIVE, 3300, 740 , 1125, 45  , 56.25  , 50     , 185.625 },
    [90 ] = {2560,  1080, PROGRESSIVE, 3000, 440 , 1100, 20  , 66     , 60.0003, 198     },
    [91 ] = {2560,  1080, PROGRESSIVE, 2970, 410 , 1250, 170 , 125    , 100    , 371.25  },
    [92 ] = {2560,  1080, PROGRESSIVE, 3300, 740 , 1250, 170 , 150    , 120.003, 495     },
    [93 ] = {3840,  2160, PROGRESSIVE, 5500, 1660, 2250, 90  , 54     , 24.0003, 297     },
    [94 ] = {3840,  2160, PROGRESSIVE, 5280, 1440, 2250, 90  , 56.25  , 25     , 297     },
    [95 ] = {3840,  2160, PROGRESSIVE, 4400, 560 , 2250, 90  , 67.5   , 30.0003, 297     },
    [96 ] = {3840,  2160, PROGRESSIVE, 5280, 1440, 2250, 90  , 112.5  , 50     , 594     },
    [97 ] = {3840,  2160, PROGRESSIVE, 4400, 560 , 2250, 90  , 135    , 60.0003, 594     },
    [98 ] = {4096,  2160, PROGRESSIVE, 5500, 1404, 2250, 90  , 54     , 24.0003, 297     },
    [99 ] = {4096,  2160, PROGRESSIVE, 5280, 1184, 2250, 90  , 56.25  , 25     , 297     },
    [100] = {4096,  2160, PROGRESSIVE, 4400, 304 , 2250, 90  , 67.5   , 30.0003, 297     },
    [101] = {4096,  2160, PROGRESSIVE, 5280, 1184, 2250, 90  , 112.5  , 50     , 594     },
    [102] = {4096,  2160, PROGRESSIVE, 4400, 304 , 2250, 90  , 135    , 60.0003, 594     },
    [103] = {3840,  2160, PROGRESSIVE, 5500, 1660, 2250, 90  , 54     , 24.0003, 297     },
    [104] = {3840,  2160, PROGRESSIVE, 5280, 1440, 2250, 90  , 56.25  , 25     , 297     },
    [105] = {3840,  2160, PROGRESSIVE, 4400, 560 , 2250, 90  , 67.5   , 30.0003, 297     },
    [106] = {3840,  2160, PROGRESSIVE, 5280, 1440, 2250, 90  , 112.5  , 50     , 594     },
    [107] = {3840,  2160, PROGRESSIVE, 4400, 560 , 2250, 90  , 135    , 60.0003, 594     },
    [108] = {1280 , 720 , PROGRESSIVE, 2500 , 1220, 750 , 30 , 36   ,   48.0003, 90      },
    [109] = {1280 , 720 , PROGRESSIVE, 2500 , 1220, 750 , 30 , 36   ,   48.0003, 90      },
    [110] = {1680 , 720 , PROGRESSIVE, 2750 , 1070, 750 , 30 , 36   ,   48.0003, 99      },
    [111] = {1920 , 1080, PROGRESSIVE, 2750 , 830 , 1125, 45 , 54   ,   48.0003, 148.5   },
    [112] = {1920 , 1080, PROGRESSIVE, 2750 , 830 , 1125, 45 , 54   ,   48.0003, 148.5   },
    [113] = {2560 , 1080, PROGRESSIVE, 3750 , 1190, 1100, 20 , 52.8 ,   48.0003, 198     },
    [114] = {3840 , 2160, PROGRESSIVE, 5500 , 1660, 2250, 90 , 108  ,   48.0003, 594     },
    [115] = {4096 , 2160, PROGRESSIVE, 5500 , 1404, 2250, 90 , 108  ,   48.0003, 594     },
    [116] = {3840 , 2160, PROGRESSIVE, 5500 , 1660, 2250, 90 , 108  ,   48.0003, 594     },
    [117] = {3840 , 2160, PROGRESSIVE, 5280 , 1440, 2250, 90 , 225  ,   100    , 1188    },
    [118] = {3840 , 2160, PROGRESSIVE, 4400 , 560 , 2250, 90 , 270  ,   120.003, 1188    },
    [119] = {3840 , 2160, PROGRESSIVE, 5280 , 1440, 2250, 90 , 225  ,   100    , 1188    },
    [120] = {3840 , 2160, PROGRESSIVE, 4400 , 560 , 2250, 90 , 270  ,   120.003, 1188    },
    [121] = {5120 , 2160, PROGRESSIVE, 7500 , 2380, 2200, 40 , 52.8 ,   24.0003, 396     },
    [122] = {5120 , 2160, PROGRESSIVE, 7200 , 2080, 2200, 40 , 55   ,   25     , 396     },
    [123] = {5120 , 2160, PROGRESSIVE, 6000 , 880 , 2200, 40 , 66   ,   30.0003, 396     },
    [124] = {5120 , 2160, PROGRESSIVE, 6250 , 1130, 2475, 315, 118.8,   48.0003, 742.5   },
    [125] = {5120 , 2160, PROGRESSIVE, 6600 , 1480, 2250, 90 , 112.5,   50     , 742.5   },
    [126] = {5120 , 2160, PROGRESSIVE, 5500 , 380 , 2250, 90 , 135  ,   60.0003, 742.5   },
    [127] = {5120 , 2160, PROGRESSIVE, 6600 , 1480, 2250, 90 , 225  ,   100    , 1485    },
    [193] = {5120 , 2160, PROGRESSIVE, 5500 , 380 , 2250, 90 , 270  ,   120.003, 1485    },
    [194] = {7680 , 4320, PROGRESSIVE, 11000, 3320, 4500, 180, 108  ,   24.0003, 1188    },
    [195] = {7680 , 4320, PROGRESSIVE, 10800, 3120, 4400, 80 , 110  ,   25     , 1188    },
    [196] = {7680 , 4320, PROGRESSIVE, 9000 , 1320, 4400, 80 , 132  ,   30.0003, 1188    },
    [197] = {7680 , 4320, PROGRESSIVE, 11000, 3320, 4500, 180, 216  ,   48.0003, 2376    },
    [198] = {7680 , 4320, PROGRESSIVE, 10800, 3120, 4400, 80 , 220  ,   50     , 2376    },
    [199] = {7680 , 4320, PROGRESSIVE, 9000 , 1320, 4400, 80 , 264  ,   60.0003, 2376    },
    [200] = {7680 , 4320, PROGRESSIVE, 10560, 2880, 4500, 180, 450  ,   100    , 4752    },
    [201] = {7680 , 4320, PROGRESSIVE, 8800 , 1120, 4500, 180, 540  ,   120.003, 4752    },
    [202] = {7680 , 4320, PROGRESSIVE, 11000, 3320, 4500, 180, 108  ,   24.0003, 1188    },
    [203] = {7680 , 4320, PROGRESSIVE, 10800, 3120, 4400, 80 , 110  ,   25     , 1188    },
    [204] = {7680 , 4320, PROGRESSIVE, 9000 , 1320, 4400, 80 , 132  ,   30.0003, 1188    },
    [205] = {7680 , 4320, PROGRESSIVE, 11000, 3320, 4500, 180, 216  ,   48.0003, 2376    },
    [206] = {7680 , 4320, PROGRESSIVE, 10800, 3120, 4400, 80 , 220  ,   50     , 2376    },
    [207] = {7680 , 4320, PROGRESSIVE, 9000 , 1320, 4400, 80 , 264  ,   60.0003, 2376    },
    [208] = {7680 , 4320, PROGRESSIVE, 10560, 2880, 4500, 180, 450  ,   100    , 4752    },
    [209] = {7680 , 4320, PROGRESSIVE, 8800 , 1120, 4500, 180, 540  ,   120.003, 4752    },
    [210] = {10240, 4320, PROGRESSIVE, 12500, 2260, 4950, 630, 118.8,   24.0003, 1485    },
    [211] = {10240, 4320, PROGRESSIVE, 13500, 3260, 4400, 80 , 110  ,   25     , 1485    },
    [212] = {10240, 4320, PROGRESSIVE, 11000, 760 , 4500, 180, 135  ,   30.0003, 1485    },
    [213] = {10240, 4320, PROGRESSIVE, 12500, 2260, 4950, 630, 237.6,   48.0003, 2970    },
    [214] = {10240, 4320, PROGRESSIVE, 13500, 3260, 4400, 80 , 220  ,   50     , 2970    },
    [215] = {10240, 4320, PROGRESSIVE, 11000, 760 , 4500, 180, 270  ,   60.0003, 2970    },
    [216] = {10240, 4320, PROGRESSIVE, 13200, 2960, 4500, 180, 450  ,   100    , 5940    },
    [217] = {10240, 4320, PROGRESSIVE, 11000, 760 , 4500, 180, 540  ,   120.003, 5940    },
    [218] = {4096 , 2160, PROGRESSIVE, 5280 , 1184, 2250, 90 , 225  ,   100    , 1188    },
    [219] = {4096 , 2160, PROGRESSIVE, 4400 , 304 , 2250, 90 , 270  ,   120.003, 1188    },
};
#endif

#if XVIDC_EDID_VERBOSITY > 1
#define CM_2_MM(cm)                             ((cm) * 10)
#define CM_2_IN(cm)                             ((cm) * 0.3937)
#endif
#if XVIDC_EDID_VERBOSITY > 0
#define HZ_2_MHZ(hz)                            ((hz) / 1000000)
#endif
#if XVIDC_EDID_VERBOSITY > 1
static void
xvidc_disp_cea861_audio_data(
                  const struct xvidc_cea861_audio_data_block * const adb,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn);

static void
xvidc_disp_cea861_speaker_allocation_data(
          const struct xvidc_cea861_speaker_allocation_data_block * const sadb,
             XV_VidC_EdidCntrlParam *EdidCtrlParam,
             XV_VidC_Verbose VerboseEn);

static void
xvidc_disp_cea861_video_data(
                  const struct xvidc_cea861_video_data_block * const vdb,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn);
#endif

static void
xvidc_disp_cea861_extended_data(
                  const struct xvidc_cea861_extended_data_block * const edb,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn);

static void
xvidc_disp_cea861_vendor_data(
                  const struct xvidc_cea861_vendor_specific_data_block * vsdb,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn);

static void
xvidc_disp_cea861(const struct xvidc_edid_extension * const ext,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn);

static void
xvidc_disp_edid1(const struct edid * const edid,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn);

/*****************************************************************************/
/**
*
* This function parse EDID on General Data & VESA Data
*
* @param    data is a pointer to the EDID array.
* @param    EdidCtrlParam is a pointer the EDID Control parameter
* @param    VerboseEn is a pointer to the XV_HdmiTxSs core instance.
*
* @return None
*
* @note   API Define below here are CEA861 routines
*
******************************************************************************/
static void
xvidc_disp_edid1(const struct edid * const edid,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn)
{
    const struct xvidc_edid_monitor_range_limits *monitor_range_limits = NULL;
    xvidc_edid_monitor_descriptor_string monitor_serial_number = {0};
    xvidc_edid_monitor_descriptor_string monitor_model_name = {0};
    bool has_ascii_string = false;
    char manufacturer[4] = {0};
#if XVIDC_EDID_VERBOSITY > 1
    XV_VidC_DoubleRep min_doubleval;
    XV_VidC_DoubleRep max_doubleval;
#endif

    u8 i;
#if XVIDC_EDID_VERBOSITY > 0
    XV_VidC_TimingParam timing_params;
#endif

#if XVIDC_EDID_VERBOSITY > 1
    struct xvidc_edid_color_characteristics_data characteristics;
    const u8 vlen = edid->maximum_vertical_image_size;
    const u8 hlen = edid->maximum_horizontal_image_size;


    static const char * const display_type[] = {
        [XVIDC_EDID_DISPLAY_TYPE_MONOCHROME] = "Monochrome or greyscale",
        [XVIDC_EDID_DISPLAY_TYPE_RGB]        = "sRGB colour",
        [XVIDC_EDID_DISPLAY_TYPE_NON_RGB]    = "Non-sRGB colour",
        [XVIDC_EDID_DISPLAY_TYPE_UNDEFINED]  = "Undefined",
    };
#endif
    xvidc_edid_manufacturer(edid, manufacturer);

    for (i = 0; i < ARRAY_SIZE(edid->detailed_timings); i++) {
        const struct xvidc_edid_monitor_descriptor * const mon =
            &edid->detailed_timings[i].monitor;

        if (!xvidc_edid_detailed_timing_is_monitor_descriptor(edid, i))
            continue;

        switch (mon->tag) {
        case XVIDC_EDID_MONTIOR_DESCRIPTOR_MANUFACTURER_DEFINED:
            /* This is arbitrary data, just silently ignore it. */
            break;
        case XVIDC_EDID_MONITOR_DESCRIPTOR_ASCII_STRING:
            has_ascii_string = true;
            break;
        case XVIDC_EDID_MONITOR_DESCRIPTOR_MONITOR_NAME:
            strncpy(monitor_model_name, (char *) mon->data,
                    sizeof(monitor_model_name) - 1);
            break;
        case XVIDC_EDID_MONITOR_DESCRIPTOR_MONITOR_RANGE_LIMITS:
            monitor_range_limits =
                         (struct xvidc_edid_monitor_range_limits *) &mon->data;
            break;
        case XVIDC_EDID_MONITOR_DESCRIPTOR_MONITOR_SERIAL_NUMBER:
            strncpy(monitor_serial_number, (char *) mon->data,
                    sizeof(monitor_serial_number) - 1);
            break;
        default:
            if (VerboseEn) {
                xil_printf("unknown monitor descriptor type 0x%02x\n",
                        mon->tag);
            }
            break;
        }
    }

#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("Sink Information\r\n");

        xil_printf("  Model name............... %s\r\n",
               *monitor_model_name ? monitor_model_name : "n/a");
#if XVIDC_EDID_VERBOSITY > 1
        xil_printf("  Manufacturer............. %s\r\n",
               manufacturer);

        xil_printf("  Product code............. %u\r\n",
               (u16) edid->product_u16);

        if (edid->serial_number_u32)
            xil_printf("  Module serial number..... %u\r\n",
                   (u32) edid->serial_number_u32);
#endif
#if defined(DISPLAY_UNKNOWN)
            xil_printf("  Plug and Play ID......... %s\r\n", NULL);
#endif
#if XVIDC_EDID_VERBOSITY > 1
        xil_printf("  Serial number............ %s\r\n",
               *monitor_serial_number ? monitor_serial_number : "n/a");

        xil_printf("  Manufacture date......... %u",
                                                edid->manufacture_year + 1990);
        if (edid->manufacture_week <= 52)
            xil_printf(", ISO week %u", edid->manufacture_week);
        xil_printf("\r\n");
#endif
        xil_printf("  EDID revision............ %u.%u\r\n",
               edid->version, edid->revision);
#if XVIDC_EDID_VERBOSITY > 1
        xil_printf("  Input signal type........ %s\r\n",
          edid->video_input_definition.digital.digital ? "Digital" : "Analog");

        if (edid->video_input_definition.digital.digital) {
            xil_printf("  VESA DFP 1.x supported... %s\r\n",
                   edid->video_input_definition.digital.dfp_1x ? "Yes" : "No");
        } else {
            /* Missing Piece: To print analog flags */
        }
#endif
#if defined(DISPLAY_UNKNOWN)
        xil_printf("  Color bit depth.......... %s\r\n", NULL);
#endif
#if XVIDC_EDID_VERBOSITY > 1
        xil_printf("  Display type............. %s\r\n",
               display_type[edid->feature_support.display_type]);

        xil_printf("  Screen size.............. %u mm x %u mm\r\n",
               CM_2_MM(hlen), CM_2_MM(vlen));

        xil_printf("  Power management......... %s%s%s%s\r\n",
               edid->feature_support.active_off ? "Active off, " : "",
               edid->feature_support.suspend ? "Suspend, " : "",
               edid->feature_support.standby ? "Standby, " : "",

               (edid->feature_support.active_off ||
                edid->feature_support.suspend    ||
                edid->feature_support.standby) ? "\b\b  " : "n/a");
#endif
        xil_printf("  Extension blocks......... %u\r\n",
               edid->extensions);

#if defined(DISPLAY_UNKNOWN)
        xil_printf("  DDC/CI................... %s\r\n", NULL);
#endif

        xil_printf("\r\n");
	}
#endif
        if (has_ascii_string) {
			if (VerboseEn) {
#if XVIDC_EDID_VERBOSITY > 1
				xil_printf("General purpose ASCII string\r\n");
#endif
			}

            for (i = 0; i < ARRAY_SIZE(edid->detailed_timings); i++) {
                if (!xvidc_edid_detailed_timing_is_monitor_descriptor(edid, i))
                    continue;
            }

			if (VerboseEn) {
#if XVIDC_EDID_VERBOSITY > 1
				xil_printf("\r\n");
#endif
			}
        }
#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("Color characteristics\r\n");

        xil_printf("  Default color space...... %ssRGB\r\n",
               edid->feature_support.standard_default_color_space ? "":"Non-");
#if XVIDC_EDID_VERBOSITY > 1
        min_doubleval =
                Double2Int(xvidc_edid_gamma(edid));
        xil_printf("  Display gamma............ %d.%03d\r\n",
        		min_doubleval.Integer, min_doubleval.Decimal);

        characteristics = xvidc_edid_color_characteristics(edid);

        min_doubleval =
			Double2Int(xvidc_edid_decode_fixed_point(characteristics.red.x));
        max_doubleval =
			Double2Int(xvidc_edid_decode_fixed_point(characteristics.red.y));

        xil_printf("  Red chromaticity......... Rx %d.%03d - Ry %d.%03d\r\n",
        	min_doubleval.Integer, min_doubleval.Decimal,
			max_doubleval.Integer, max_doubleval.Decimal);

        min_doubleval =
        	Double2Int(xvidc_edid_decode_fixed_point(characteristics.green.x));
        max_doubleval =
        	Double2Int(xvidc_edid_decode_fixed_point(characteristics.green.y));

        xil_printf("  Green chromaticity....... Gx %d.%03d - Gy %d.%03d\r\n",
        	min_doubleval.Integer, min_doubleval.Decimal,
			max_doubleval.Integer, max_doubleval.Decimal);

        min_doubleval =
        	Double2Int(xvidc_edid_decode_fixed_point(characteristics.blue.x));
        max_doubleval =
        	Double2Int(xvidc_edid_decode_fixed_point(characteristics.blue.y));

        xil_printf("  Blue chromaticity........ Bx %d.%03d - By %d.%03d\r\n",
        	min_doubleval.Integer, min_doubleval.Decimal,
			max_doubleval.Integer, max_doubleval.Decimal);

        min_doubleval =
        	Double2Int(xvidc_edid_decode_fixed_point(characteristics.white.x));
        max_doubleval =
        	Double2Int(xvidc_edid_decode_fixed_point(characteristics.white.y));

        xil_printf("  White point (default).... Wx %d.%03d - Wy %d.%03d\r\n",
        	min_doubleval.Integer, min_doubleval.Decimal,
			max_doubleval.Integer, max_doubleval.Decimal);
#endif
#if defined(DISPLAY_UNKNOWN)
        xil_printf("  Additional descriptors... %s\r\n", NULL);
#endif
        xil_printf("\r\n");

        xil_printf("VESA Timing characteristics\r\n");
    }
#endif
    if (monitor_range_limits) {
#if XVIDC_EDID_VERBOSITY > 0
        if (VerboseEn) {
            xil_printf("  Horizontal scan range.... %u - %u kHz\r\n",
                   monitor_range_limits->minimum_horizontal_rate,
                   monitor_range_limits->maximum_horizontal_rate);

            xil_printf("  Vertical scan range...... %u - %u Hz\r\n",
                   monitor_range_limits->minimum_vertical_rate,
                   monitor_range_limits->maximum_vertical_rate);

            xil_printf("  Video bandwidth.......... %u MHz\r\n",
                   monitor_range_limits->maximum_supported_pixel_clock * 10);
        }
#endif
        EdidCtrlParam->MaxFrameRateSupp =
                                   monitor_range_limits->maximum_vertical_rate;
        EdidCtrlParam->MaxTmdsMhz =
                    (monitor_range_limits->maximum_supported_pixel_clock * 10);
    }

#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
#if defined(DISPLAY_UNKNOWN)
        xil_printf("  CVT standard............. %s\r\n", NULL);
#endif
#if XVIDC_EDID_VERBOSITY > 1
        xil_printf("  GTF standard............. %sSupported\r\n",
               edid->feature_support.default_gtf ? "" : "Not ");
#endif
#if defined(DISPLAY_UNKNOWN)
        xil_printf("  Additional descriptors... %s\r\n", NULL);
#endif
#if XVIDC_EDID_VERBOSITY > 0
        xil_printf("  Preferred timing......... %s\r\n",
               edid->feature_support.preferred_timing_mode ? "Yes" : "No");
#endif
        for (i = 0; i < ARRAY_SIZE(edid->detailed_timings); i++) {
            if (xvidc_edid_detailed_timing_is_monitor_descriptor(edid, i))
                continue;

            timing_params = XV_VidC_timing(&edid->detailed_timings[i].timing);
            EdidCtrlParam->PreferedTiming[i] =
            		XV_VidC_timing(&edid->detailed_timings[i].timing);
#if XVIDC_EDID_VERBOSITY > 0
			if (edid->feature_support.preferred_timing_mode) {
				xil_printf("  Native/preferred timing.. %ux%u%c at %uHz"
											" (%u:%u)\r\n",
											timing_params.hres,
											timing_params.vres,
											timing_params.vidfrmt ? 'i' : 'p',
											timing_params.vfreq,
											timing_params.aspect_ratio.width,
											timing_params.aspect_ratio.height);
				xil_printf("    Modeline............... \"%ux%u\" %u %u %u %u"
								" %u %u %u %u %u %chsync %cvsync\r\n",
							   timing_params.hres,
							   timing_params.vres,
							   HZ_2_MHZ (timing_params.pixclk),
							   (timing_params.hres),
							   (timing_params.hres + timing_params.hfp),
							   (timing_params.hres + timing_params.hfp +
													timing_params.hsync_width),
							   (timing_params.htotal),
							   (timing_params.vres),
							   (timing_params.vres + timing_params.vfp),
							   (timing_params.vres + timing_params.vfp +
													timing_params.vsync_width),
							   (timing_params.vtotal),
							   timing_params.hsync_polarity ? '+' : '-',
							   timing_params.vsync_polarity ? '+' : '-');
			} else {
				xil_printf("  Native/preferred timing.. n/a\r\n");
			}
#endif
        }
#if XVIDC_EDID_VERBOSITY > 0
        xil_printf("\r\n");
#endif
#if XVIDC_EDID_VERBOSITY > 1
        xil_printf("Established Timings supported\r\n");
        if (edid->established_timings.timing_720x400_70)
            xil_printf("   720 x  400p @ 70Hz - IBM VGA\r\n");
        if (edid->established_timings.timing_720x400_88)
            xil_printf("   720 x  400p @ 88Hz - IBM XGA2\r\n");
        if (edid->established_timings.timing_640x480_60)
            xil_printf("   640 x  480p @ 60Hz - IBM VGA\r\n");
        if (edid->established_timings.timing_640x480_67)
            xil_printf("   640 x  480p @ 67Hz - Apple Mac II\r\n");
        if (edid->established_timings.timing_640x480_72)
            xil_printf("   640 x  480p @ 72Hz - VESA\r\n");
        if (edid->established_timings.timing_640x480_75)
            xil_printf("   640 x  480p @ 75Hz - VESA\r\n");
        if (edid->established_timings.timing_800x600_56)
            xil_printf("   800 x  600p @ 56Hz - VESA\r\n");
        if (edid->established_timings.timing_800x600_60)
            xil_printf("   800 x  600p @ 60Hz - VESA\r\n");

        if (edid->established_timings.timing_800x600_72)
            xil_printf("   800 x  600p @ 72Hz - VESA\r\n");
        if (edid->established_timings.timing_800x600_75)
            xil_printf("   800 x  600p @ 75Hz - VESA\r\n");
        if (edid->established_timings.timing_832x624_75)
            xil_printf("   832 x  624p @ 75Hz - Apple Mac II\r\n");
        if (edid->established_timings.timing_1024x768_87)
            xil_printf("  1024 x  768i @ 87Hz - VESA\r\n");
        if (edid->established_timings.timing_1024x768_60)
            xil_printf("  1024 x  768p @ 60Hz - VESA\r\n");
        if (edid->established_timings.timing_1024x768_70)
            xil_printf("  1024 x  768p @ 70Hz - VESA\r\n");
        if (edid->established_timings.timing_1024x768_75)
            xil_printf("  1024 x  768p @ 75Hz - VESA\r\n");
        if (edid->established_timings.timing_1280x1024_75)
            xil_printf("  1280 x 1024p @ 75Hz - VESA\r\n");
#endif
    }
#endif

#if XVIDC_EDID_VERBOSITY > 1
    if (VerboseEn) {
    	xil_printf("Standard Timings supported\r\n");
		for (i = 0; i < ARRAY_SIZE(edid->standard_timing_id); i++) {
			const struct xvidc_edid_standard_timing_descriptor * const desc =
				&edid->standard_timing_id[i];

			if (!memcmp(desc, XVIDC_EDID_STANDARD_TIMING_DESCRIPTOR_INVALID,
																sizeof(*desc)))
			{
				continue;
			} else {
				if (((desc->horizontal_active_pixels + 31)* 8) >= 1000) {
				xil_printf("  %u x",(desc->horizontal_active_pixels + 31)* 8);
				} else {
				xil_printf("   %u x",(desc->horizontal_active_pixels + 31)* 8);
				}
				switch (desc->image_aspect_ratio) {
					case 0: //Aspect Ratio = 16:10
						xil_printf(" %up ",
					(((desc->horizontal_active_pixels + 31)* 8) * 10) / 16);
						break;
					case 1: //Aspect Ratio = 4:3
						xil_printf(" %up ",
					(((desc->horizontal_active_pixels + 31)* 8) * 3) / 4);
						break;
					case 2: //Aspect Ratio = 5:4
						xil_printf(" %up ",
					(((desc->horizontal_active_pixels + 31)* 8) * 4) / 5);
						break;
					case 3: //Aspect Ratio = 16:9
						xil_printf(" %up ",
					(((desc->horizontal_active_pixels + 31)* 8) * 9) / 16);
						break;
					default: //Aspect Ratio = 16:10
						xil_printf(" %up ",
					(((desc->horizontal_active_pixels + 31)* 8) * 10) / 16);
						break;
				}
				xil_printf("@ %uHz\r\n",(desc->refresh_rate + 60));
			}
		}
    }
#endif
#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("\r\n");
    }
#endif
}



/*****************************************************************************/
/**
*
* This function parse EDID on CEA 861 Audio Data
*
* @param    data is a pointer to the EDID array.
* @param    EdidCtrlParam is a pointer the EDID Control parameter
* @param    VerboseEn is a pointer to the XV_HdmiTxSs core instance.
*
* @return None
*
* @note   API Define below here are CEA861 routines
*
******************************************************************************/
#if XVIDC_EDID_VERBOSITY > 1
static void
xvidc_disp_cea861_audio_data(
                  const struct xvidc_cea861_audio_data_block * const adb,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn) {
    /* For Future Usage */
    EdidCtrlParam = EdidCtrlParam;

    const u8 descriptors = adb->header.length / sizeof(*adb->sad);
#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("CE audio data (formats supported)\r\n");
    }
#endif
    for (u8 i = 0; i < descriptors; i++) {
        const struct xvidc_cea861_short_audio_descriptor * const sad =
            (struct xvidc_cea861_short_audio_descriptor *) &adb->sad[i];

        switch (sad->audio_format) {
            case XVIDC_CEA861_AUDIO_FORMAT_LPCM:
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  LPCM        %u-channel, %s%s%s\b%s",
                           sad->channels + 1,
                           sad->flags.lpcm.bitrate_16_bit ? "16/" : "",
                           sad->flags.lpcm.bitrate_20_bit ? "20/" : "",
                           sad->flags.lpcm.bitrate_24_bit ? "24/" : "",

                           ((sad->flags.lpcm.bitrate_16_bit +
                             sad->flags.lpcm.bitrate_20_bit +
                             sad->flags.lpcm.bitrate_24_bit) > 1) ?
                                                      " bit depths" : "-bit");
                }
#endif
                break;
            case XVIDC_CEA861_AUDIO_FORMAT_AC_3:
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  AC-3        %u-channel, %4ukHz max. bit rate",
                           sad->channels + 1,
                           (sad->flags.maximum_bit_rate << 3));
                }
#endif
                break;
            case XVIDC_CEA861_AUDIO_FORMAT_DTS:
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  DTS         %u-channel, %4ukHz max. bit rate",
                           sad->channels + 1,
                           (sad->flags.maximum_bit_rate << 3));
                }
#endif
                break;

            case XVIDC_CEA861_AUDIO_FORMAT_E_AC_3:
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  DD+(E-AC3)  %u-channel,"
                           " %u-Format Dependent Value",
                           sad->channels + 1,
                           (sad->flags.format_dependent));
                }
#endif
                break;

            case XVIDC_CEA861_AUDIO_FORMAT_DTS_HD:
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  DTS-HD      %u-channel,"
                           " %u-Format Dependent Value",
                           sad->channels + 1,
                           (sad->flags.format_dependent));
                }
#endif
                break;

            case XVIDC_CEA861_AUDIO_FORMAT_MLP:
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  MAL (MLP)   %u-channel,"
                           " %u-Format Dependent Value",
                           sad->channels + 1,
                           (sad->flags.format_dependent));
                }
#endif
                break;
            default:
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("Unknown audio format 0x%02x\r\n",
                            sad->audio_format);
                }
#endif
                continue;
        }
#if XVIDC_EDID_VERBOSITY > 0
        if (VerboseEn) {
            xil_printf(" at %s%s%s%s%s%s%s\b kHz\r\n",
                   sad->sample_rate_32_kHz ? "32/" : "",
                   sad->sample_rate_44_1_kHz ? "44.1/" : "",
                   sad->sample_rate_48_kHz ? "48/" : "",
                   sad->sample_rate_88_2_kHz ? "88.2/" : "",
                   sad->sample_rate_96_kHz ? "96/" : "",
                   sad->sample_rate_176_4_kHz ? "176.4/" : "",
                   sad->sample_rate_192_kHz ? "192/" : "");
        }
#endif
    }
#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("\r\n");
    }
#endif
}
#endif

/*****************************************************************************/
/**
*
* This function parse EDID on CEA 861 Extended Data
*
* @param    data is a pointer to the EDID array.
* @param    EdidCtrlParam is a pointer the EDID Control parameter
* @param    VerboseEn is a pointer to the XV_HdmiTxSs core instance.
*
* @return None
*
* @note   None.
*
******************************************************************************/
static void
xvidc_disp_cea861_extended_data(
                  const struct xvidc_cea861_extended_data_block * const edb,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn) {

	/* During Verbosity 0, VerboseEn won't be used */
	/* To avoid compilation warnings */
	VerboseEn = VerboseEn;

#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("CEA Extended Tags\r\n");
    }
#endif
    switch(edb->xvidc_cea861_extended_tag_codes) {
#if XVIDC_EDID_VERBOSITY > 1
        case XVIDC_CEA861_EXT_TAG_TYPE_VIDEO_CAPABILITY:
            if (VerboseEn) {
                xil_printf("  Video capability data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_VENDOR_SPECIFIC:
            if (VerboseEn) {
                xil_printf("  Vendor-specific video data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_VESA_DISPLAY_DEVICE:
            if (VerboseEn) {
                xil_printf("  VESA video display device data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_VESA_VIDEO_TIMING_BLOCK_EXT:
            if (VerboseEn) {
                xil_printf("VESA video timing block extension\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_RESERVED_FOR_HDMI_VIDEO_DATA_BLOCK:
            if (VerboseEn) {
                xil_printf("Reserved for HDMI video data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_COLORIMETRY:
            if (VerboseEn) {
                xil_printf("  Colorimetry data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_HDR_STATIC_METADATA:
            if (VerboseEn) {
                xil_printf("HDR static metadata data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_HDR_DYNAMIC_METADATA:
            if (VerboseEn) {
                xil_printf("  HDR dynamic metadata data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_VIDEO_FRMT_PREFERENCE:
            if (VerboseEn) {
                xil_printf("  Video format preference data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_CEA_MISC_AUDIO_FIELDS:
            if (VerboseEn) {
                xil_printf("Reserved for CEA miscellaneous audio fields\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_VENDOR_SPECIFC_AUDIO:
            if (VerboseEn) {
                xil_printf("  Vendor-specific audio data block\r\n\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_HDMI_AUDIO:
            if (VerboseEn) {
                xil_printf("  HDMI audio data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_ROOM_CONFIGURATION:
            if (VerboseEn) {
                xil_printf("  Room configuration data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_SPEAKER_LOCATION:
            if (VerboseEn) {
                xil_printf("  Speaker location data block\r\n");
            }
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_INFOFRAME:
            if (VerboseEn) {
                xil_printf("  Video capability data block\r\n");
            }
        break;
#endif
        case XVIDC_CEA861_EXT_TAG_TYPE_YCBCR420_VIDEO:
#if XVIDC_EDID_VERBOSITY > 1
            if (VerboseEn) {
                xil_printf("  YCbCr 4:2:0 video data block\r\n");
                xil_printf("    YCbCr 4:2:0.............. Supported\r\n");
            }
#endif
            EdidCtrlParam->IsYCbCr420Supp = XVIDC_SUPPORTED;

#if XVIDC_EDID_VERBOSITY > 1
            if (VerboseEn) {
                xil_printf("  CE video identifiers (VICs) - "
                                              " timing/formats supported\r\n");
            }
            /* For VIC 1 to VIC 64, where, the first 7 bit is the VIC number,
             * and the most significant bit is to define the Native Video Format
             * or not.
             * svd/short video descriptor = [native, 7 bits of VIC]
             *       --> Covers for SVD [1-64] and SVD [129-192]
             * VIC 65 to VIC 127, all the 8 bits are the VIC Number
             * VIC 193 to VIC 253, all the 8 bits are the VIC Number
             * svd/short video descriptor = [8 bits of VIC]
             *       --> SVD= 0, 128, 254 and 255 are reserved.
             */
            for (u8 i = 0; i < edb->header.length - 1; i++) {
               u8 vic;
               u8 native=0;
               if ((edb->data[i] & 0x7F) == 0) {
                   continue;
               } else if  (((edb->data[i]) >= 1) && ((edb->data[i]) <= 64)){
                   vic    = (edb->data[i]) & 0x7F;
               } else if  (((edb->data[i]) >= 65) && ((edb->data[i]) <= 127)){
                   vic    = (edb->data[i]);
               } else if  (((edb->data[i]) >= 129) && ((edb->data[i]) <= 192)){
                   vic    = (edb->data[i]) & 0x7F;
                   native = 1;
               } else if  (((edb->data[i]) >= 193) && ((edb->data[i]) <= 253)){
                   vic    = (edb->data[i]);
               } else {
                   continue;
               }

               const struct xvidc_cea861_timing * const timing =
                   &xvidc_cea861_timings[vic];

               if (VerboseEn) {
                   xil_printf("   %s CEA Mode %02u: %4u x %4u%c @ %dHz\r\n",
                   native ? "*" : " ",
                   vic,
                   timing->hactive, timing->vactive,
                   (timing->mode == INTERLACED) ? 'i' : 'p',
                   (u32)(timing->vfreq));
               }
            }
            if (VerboseEn) {
                xil_printf("\r\n");
            }
#endif
        break;

        case XVIDC_CEA861_EXT_TAG_TYPE_YCBCR420_CAPABILITY_MAP:
#if XVIDC_EDID_VERBOSITY > 1
            if (VerboseEn) {
                xil_printf("  YCbCr 4:2:0 capability map data block\r\n");
                xil_printf("    YCbCr 4:2:0.............. Supported\r\n");
            }
#endif
            EdidCtrlParam->IsYCbCr420Supp = XVIDC_SUPPORTED;
#if XVIDC_EDID_VERBOSITY > 1
            for (u8 i = 0; i < edb->header.length - 1; i++) {
                u8 v = edb->data[i];

                for (u8 j = 0; j < 8; j++) {
                    if (v & (1 << j)) {
                        if (VerboseEn) {
                            const struct xvidc_cea861_timing * const timing =
                 &xvidc_cea861_timings[EdidCtrlParam->SuppCeaVIC[(i * 8) + j]];
                            xil_printf("      CEA Mode %02u: %4u x %4u%c"
                                              "@ %dHz\r\n",
                                      EdidCtrlParam->SuppCeaVIC[(i * 8) + j],
                                      timing->hactive, timing->vactive,
                                      (timing->mode == INTERLACED) ? 'i' : 'p',
                                      (u32)(timing->vfreq));
                        }
                    }
                }
            }
#endif
#if XVIDC_EDID_VERBOSITY > 0
            if (VerboseEn) {
                xil_printf("\r\n");
            }
#endif
        break;

        default :
#if XVIDC_EDID_VERBOSITY > 0
            if (VerboseEn) {
#if XVIDC_EDID_VERBOSITY > 1
                xil_printf("  Not Supported: Ext Tag: %03x\r\n",
                                         edb->xvidc_cea861_extended_tag_codes);
#endif
                xil_printf("\r\n");
            }
#endif
        break;
    }
}
#if XVIDC_EDID_VERBOSITY > 1
static void
xvidc_disp_cea861_video_data(
                  const struct xvidc_cea861_video_data_block * const vdb,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn) {
    /* For Future Usage */
    EdidCtrlParam = EdidCtrlParam;

    /* Variable */
    u8 Vic;
    u8 Native;

    if (VerboseEn) {
       xil_printf("CE video identifiers (VICs) - timing/formats"
                                                             " supported\r\n");
    }

    for (u8 i = 0; i < vdb->header.length; i++) {
        /* For VIC 1 to VIC 64, where, the first 7 bit is the VIC number,
         * and the most significant bit is to define the Native Video Format
         * or not.
         * svd/short video descriptor = [native, 7 bits of VIC]
         *       --> Covers for SVD [1-64] and SVD [129-192]
         * VIC 65 to VIC 127, all the 8 bits are the VIC Number
         * VIC 193 to VIC 253, all the 8 bits are the VIC Number
         * svd/short video descriptor = [8 bits of VIC]
         *       --> SVD= 0, 128, 254 and 255 are reserved.
         */
        if (vdb->svd[i].video_identification_code >= 1 &&
                        vdb->svd[i].video_identification_code <= 127) {
                Native = FALSE;
                Vic    = vdb->svd[i].video_identification_code;
        } else if (vdb->svd[i].video_identification_code >= 129 &&
                        vdb->svd[i].video_identification_code <= 192) {
                Native = TRUE;
                Vic    = vdb->svd[i].video_identification_code & 0x7F;
        } else if (vdb->svd[i].video_identification_code >= 193 &&
                        vdb->svd[i].video_identification_code <= 253) {
                Native = FALSE;
                Vic    = vdb->svd[i].video_identification_code;
        } else {
                /* Reserved or Not Valid */
                Native = FALSE;
                Vic    = vdb->svd[i].video_identification_code;
        }

        const struct xvidc_cea861_timing * const timing =
            &xvidc_cea861_timings[Vic];

        EdidCtrlParam->SuppCeaVIC[i] = Vic;
        if (VerboseEn) {
            xil_printf(" %s CEA Mode %02u: %4u x %4u%c @ %dHz\r\n",
                   Native ? "*" : " ",
                   Vic,
                   timing->hactive, timing->vactive,
                   (timing->mode == INTERLACED) ? 'i' : 'p',
                   (u32)(timing->vfreq));
        }
    }
    if (VerboseEn) {
        xil_printf("\r\n");
    }
}
#endif

/*****************************************************************************/
/**
*
* This function parse EDID on CEA 861 Vendor Specific Data
*
* @param    data is a pointer to the EDID array.
* @param    EdidCtrlParam is a pointer the EDID Control parameter
* @param    VerboseEn is a pointer to the XV_HdmiTxSs core instance.
*
* @return None
*
* @note   None.
*
******************************************************************************/
static void
xvidc_disp_cea861_vendor_data(
                  const struct xvidc_cea861_vendor_specific_data_block * vsdb,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn) {

	/* During Verbosity 0, VerboseEn won't be used */
	/* To avoid compilation warnings */
	VerboseEn = VerboseEn;

    const u8 oui[] = { vsdb->ieee_registration[2],
                            vsdb->ieee_registration[1],
                            vsdb->ieee_registration[0] };
#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("CEA vendor specific data (VSDB)\r\n");
        xil_printf("  IEEE registration number. 0x");
        for (u8 i = 0; i < ARRAY_SIZE(oui); i++)
            xil_printf("%02X", oui[i]);
        xil_printf("\r\n");
    }
#endif
    if (!memcmp(oui, HDMI_OUI, sizeof(oui))) {
       /* HDMI Sink should have the HDMI Vendor Specific Block */
       EdidCtrlParam->IsHdmi = XVIDC_ISHDMI;
       const struct xvidc_cea861_hdmi_vendor_specific_data_block * const hdmi =
            (struct xvidc_cea861_hdmi_vendor_specific_data_block *) vsdb;
#if XVIDC_EDID_VERBOSITY > 0
        if (VerboseEn) {
            xil_printf("  CEC physical address..... %u.%u.%u.%u\r\n",
                   hdmi->port_configuration_a,
                   hdmi->port_configuration_b,
                   hdmi->port_configuration_c,
                   hdmi->port_configuration_d);
        }
#endif

        if (hdmi->header.length >= HDMI_VSDB_EXTENSION_FLAGS_OFFSET) {
#if XVIDC_EDID_VERBOSITY > 0
            if (VerboseEn) {
#if XVIDC_EDID_VERBOSITY > 1
                xil_printf("  Supports AI (ACP, ISRC).. %s\r\n",
                       hdmi->audio_info_frame ? "Yes" : "No");
#endif
                xil_printf("  Supports 48bpp........... %s\r\n",
                       hdmi->colour_depth_48_bit ? "Yes" : "No");
                xil_printf("  Supports 36bpp........... %s\r\n",
                       hdmi->colour_depth_36_bit ? "Yes" : "No");
                xil_printf("  Supports 30bpp........... %s\r\n",
                       hdmi->colour_depth_30_bit ? "Yes" : "No");
                xil_printf("  Supp. YUV444 Deep Color.. %s\r\n",
                       hdmi->yuv_444_supported ? "Yes" : "No");
#if XVIDC_EDID_VERBOSITY > 1
                xil_printf("  Supports dual-link DVI... %s\r\n",
                       hdmi->dvi_dual_link ? "Yes" : "No");
#endif
            }
#endif
            EdidCtrlParam->Is30bppSupp = hdmi->colour_depth_30_bit;
            EdidCtrlParam->Is36bppSupp = hdmi->colour_depth_36_bit;
            EdidCtrlParam->Is48bppSupp = hdmi->colour_depth_48_bit;
            EdidCtrlParam->IsYCbCr444DeepColSupp = hdmi->yuv_444_supported;
        }

        if (hdmi->header.length >= HDMI_VSDB_MAX_TMDS_OFFSET) {
                if (hdmi->max_tmds_clock) {
#if XVIDC_EDID_VERBOSITY > 0
                    if (VerboseEn) {
                        xil_printf("  Maximum TMDS clock....... %uMHz\r\n",
                               hdmi->max_tmds_clock * 5);
                    }
#endif
                    EdidCtrlParam->MaxTmdsMhz = (hdmi->max_tmds_clock * 5);
                } else {
#if XVIDC_EDID_VERBOSITY > 0
                    if (VerboseEn) {
                        xil_printf("  Maximum TMDS clock....... n/a\r\n");
                    }
#endif
                }
        }

        if (hdmi->header.length >= HDMI_VSDB_LATENCY_FIELDS_OFFSET) {
            if (hdmi->latency_fields) {
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  Video latency %s........ %ums\r\n",
                           hdmi->interlaced_latency_fields ? "(p)" : "...",
                           (hdmi->video_latency - 1) << 1);
                    xil_printf("  Audio latency %s........ %ums\r\n",
                           hdmi->interlaced_latency_fields ? "(p)" : "...",
                           (hdmi->audio_latency - 1) << 1);
                }
#endif
            }

            if (hdmi->interlaced_latency_fields) {
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  Video latency (i)........ %ums\r\n",
                           hdmi->interlaced_video_latency);
                    xil_printf("  Audio latency (i)........ %ums\r\n",
                           hdmi->interlaced_audio_latency);
                }
#endif
            }
        }
    }   else if (!memcmp(oui, HDMI_OUI_HF, sizeof(oui))) {
    const struct xvidc_cea861_hdmi_hf_vendor_specific_data_block * const hdmi =
            (struct xvidc_cea861_hdmi_hf_vendor_specific_data_block *) vsdb;

#if XVIDC_EDID_VERBOSITY > 0
            if (VerboseEn) {
                xil_printf("  Version.................. %d\r\n",hdmi->version);
            }
#endif

            if (hdmi->max_tmds_char_rate) {
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
                    xil_printf("  Maximum TMDS clock....... %uMHz\r\n",
                           hdmi->max_tmds_char_rate * 5);
                }
#endif
                    EdidCtrlParam->MaxTmdsMhz = (hdmi->max_tmds_char_rate * 5);
                } else {
#if XVIDC_EDID_VERBOSITY > 0
                    if (VerboseEn) {
                        xil_printf("  Max. Supp. TMDS clock (<=340MHz)\r\n");
                    }
#endif
                }

            switch (hdmi->max_frl_rate) {
            case XVIDC_MAXFRLRATE_NOT_SUPPORTED:
                EdidCtrlParam->MaxFrlLanesSupp = 0;
                EdidCtrlParam->MaxFrlLineRateSupp = 0;
                break;

            case XVIDC_MAXFRLRATE_3X3GBITSPS:
                EdidCtrlParam->MaxFrlLanesSupp = 3;
                EdidCtrlParam->MaxFrlLineRateSupp = 3;
                break;

            case XVIDC_MAXFRLRATE_3X6GBITSPS:
                EdidCtrlParam->MaxFrlLanesSupp = 3;
                EdidCtrlParam->MaxFrlLineRateSupp = 6;
                break;

            case XVIDC_MAXFRLRATE_4X6GBITSPS:
                EdidCtrlParam->MaxFrlLanesSupp = 4;
                EdidCtrlParam->MaxFrlLineRateSupp = 6;
                break;

            case XVIDC_MAXFRLRATE_4X8GBITSPS:
                EdidCtrlParam->MaxFrlLanesSupp = 4;
                EdidCtrlParam->MaxFrlLineRateSupp = 8;
                break;

            case XVIDC_MAXFRLRATE_4X10GBITSPS:
                EdidCtrlParam->MaxFrlLanesSupp = 4;
                EdidCtrlParam->MaxFrlLineRateSupp = 10;
                break;

            case XVIDC_MAXFRLRATE_4X12GBITSPS:
                EdidCtrlParam->MaxFrlLanesSupp = 4;
                EdidCtrlParam->MaxFrlLineRateSupp = 12;
                break;

            default:
                EdidCtrlParam->MaxFrlLanesSupp = 0;
                EdidCtrlParam->MaxFrlLineRateSupp = 0;
                break;
            }

            EdidCtrlParam->MaxFrlRateSupp = hdmi->max_frl_rate;

            if (hdmi->header.length >= HDMI_VSDB_EXTENSION_FLAGS_OFFSET) {
#if XVIDC_EDID_VERBOSITY > 0
                if (VerboseEn) {
#if XVIDC_EDID_VERBOSITY > 1
                    xil_printf("  RRC Capable Support...... %s\r\n",
                            hdmi->rr_capable ? "Yes" : "No");
                    xil_printf("  SCDC Present............. %s\r\n",
                            hdmi->scdc_present ? "Yes" : "No");
                    xil_printf("  HDMI1.4 Scramble Support. %s\r\n",
                            hdmi->lte_340mcsc_scramble ? "Yes" : "No");
#endif
                    xil_printf("  YUV 420 Deep.C. Support..\r\n");
                    xil_printf("    Supports 48bpp......... %s\r\n",
                            hdmi->dc_48bit_yuv420 ? "Yes" : "No");
                    xil_printf("    Supports 36bpp......... %s\r\n",
                            hdmi->dc_36bit_yuv420 ? "Yes" : "No");
                    xil_printf("    Supports 30bpp......... %s\r\n",
                            hdmi->dc_30bit_yuv420 ? "Yes" : "No");
                    xil_printf("    Max FRL Rate Support... %u\r\n",
                            EdidCtrlParam->MaxFrlRateSupp);
                    xil_printf("    FRL Lanes Support...... %u\r\n",
                            EdidCtrlParam->MaxFrlLanesSupp);
                    xil_printf("    Max FRL Line Rate Support. %u\r\n",
                            EdidCtrlParam->MaxFrlLineRateSupp);
                }
#endif
                EdidCtrlParam->IsYCbCr420dc30bppSupp = hdmi->dc_30bit_yuv420;
                EdidCtrlParam->IsYCbCr420dc36bppSupp = hdmi->dc_36bit_yuv420;
                EdidCtrlParam->IsYCbCr420dc48bppSupp = hdmi->dc_48bit_yuv420;
                EdidCtrlParam->IsSCDCReadRequestReady = hdmi->rr_capable;
                EdidCtrlParam->IsSCDCPresent = hdmi->scdc_present;
            }
    }
#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("\r\n");
    }
#endif
}

#if XVIDC_EDID_VERBOSITY > 1
/*****************************************************************************/
/**
*
* This function parse EDID on CEA 861 Speaker Allocation
*
* @param    data is a pointer to the EDID array.
* @param    EdidCtrlParam is a pointer the EDID Control parameter
* @param    VerboseEn is a pointer to the XV_HdmiTxSs core instance.
*
* @return None
*
* @note   None.
*
******************************************************************************/
static void
xvidc_disp_cea861_speaker_allocation_data(
          const struct xvidc_cea861_speaker_allocation_data_block * const sadb,
             XV_VidC_EdidCntrlParam *EdidCtrlParam,
             XV_VidC_Verbose VerboseEn) {
    /* For Future Usage */
    EdidCtrlParam = EdidCtrlParam;

    const struct xvidc_cea861_speaker_allocation * const sa = &sadb->payload;
    const u8 * const channel_configuration = (u8 *) sa;

    if (VerboseEn) {
        xil_printf("CEA speaker allocation data\r\n");
        xil_printf("  Channel configuration.... %u.%u\r\n",
               (__builtin_popcountll(channel_configuration[0] & 0xe9) << 1) +
               (__builtin_popcountll(channel_configuration[0] & 0x14) << 0) +
               (__builtin_popcountll(channel_configuration[1] & 0x01) << 1) +
               (__builtin_popcountll(channel_configuration[1] & 0x06) << 0),
               (channel_configuration[0] & 0x02));
        xil_printf("  Front left/right......... %s\r\n",
               sa->front_left_right ? "Yes" : "No");
        xil_printf("  Front LFE................ %s\r\n",
               sa->front_lfe ? "Yes" : "No");
        xil_printf("  Front center............. %s\r\n",
               sa->front_center ? "Yes" : "No");
        xil_printf("  Rear left/right.......... %s\r\n",
               sa->rear_left_right ? "Yes" : "No");
        xil_printf("  Rear center.............. %s\r\n",
               sa->rear_center ? "Yes" : "No");
        xil_printf("  Front left/right center.. %s\r\n",
               sa->front_left_right_center ? "Yes" : "No");
        xil_printf("  Rear left/right center... %s\r\n",
               sa->rear_left_right_center ? "Yes" : "No");
        xil_printf("  Front left/right wide.... %s\r\n",
               sa->front_left_right_wide ? "Yes" : "No");
        xil_printf("  Front left/right high.... %s\r\n",
               sa->front_left_right_high ? "Yes" : "No");
        xil_printf("  Top center............... %s\r\n",
               sa->top_center ? "Yes" : "No");
        xil_printf("  Front center high........ %s\r\n",
               sa->front_center_high ? "Yes" : "No");

        xil_printf("\r\n");
    }

}
#endif

/*****************************************************************************/
/**
*
* This function Parse and Display the CEA-861
*
* @param    data is a pointer to the EDID array.
* @param    EdidCtrlParam is a pointer the EDID Control parameter
* @param    VerboseEn is a pointer to the XV_HdmiTxSs core instance.
*
* @return None
*
* @note   None.
*
******************************************************************************/
static void
xvidc_disp_cea861(const struct xvidc_edid_extension * const ext,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn) {

    const struct xvidc_cea861_timing_block * const ctb =
        (struct xvidc_cea861_timing_block *) ext;
    const u8 offset = offsetof(struct xvidc_cea861_timing_block, data);
    u8 index = 0;

#if XVIDC_EDID_VERBOSITY > 1
    const struct xvidc_edid_detailed_timing_descriptor *dtd = NULL;
    u8 i;

    XV_VidC_TimingParam timing_params;
#endif

#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {

        xil_printf("CEA-861 Information\r\n");
        xil_printf("  Revision number.......... %u\r\n",
               ctb->revision);
    }
#endif

    if (ctb->revision >= 2) {
#if XVIDC_EDID_VERBOSITY > 0
        if (VerboseEn) {
#if XVIDC_EDID_VERBOSITY > 1
            xil_printf("  IT underscan............. %supported\r\n",
                   ctb->underscan_supported ? "S" : "Not s");
#endif
            xil_printf("  Basic audio.............. %supported\r\n",
                   ctb->basic_audio_supported ? "S" : "Not s");
            xil_printf("  YCbCr 4:4:4.............. %supported\r\n",
                   ctb->yuv_444_supported ? "S" : "Not s");
            xil_printf("  YCbCr 4:2:2.............. %supported\r\n",
                   ctb->yuv_422_supported ? "S" : "Not s");
#if XVIDC_EDID_VERBOSITY > 1
            xil_printf("  Native formats........... %u\r\n",
                   ctb->native_dtds);
#endif
        }
#endif
        EdidCtrlParam->IsYCbCr444Supp = ctb->yuv_444_supported;
        EdidCtrlParam->IsYCbCr422Supp = ctb->yuv_422_supported;
    }

#if XVIDC_EDID_VERBOSITY > 1
    dtd = (struct xvidc_edid_detailed_timing_descriptor *)
                                            ((u8 *) ctb + ctb->dtd_offset);
    for (i = 0; dtd->pixel_clock; i++, dtd++) {

        timing_params = XV_VidC_timing(dtd);
        if (VerboseEn) {
            xil_printf("  Detailed timing #%u....... %ux%u%c at %uHz "
                                                                "(%u:%u)\r\n",
                                        i + 1,
                                        timing_params.hres,
                                        timing_params.vres,
                                        timing_params.vidfrmt ? 'i' : 'p',
                                        timing_params.vfreq,
                                        timing_params.aspect_ratio.width,
                                        timing_params.aspect_ratio.height);

            xil_printf(
            "    Modeline............... \"%ux%u\" %u %u %u %u %u %u %u %u %u"
                                                        " %chsync %cvsync\r\n",
                                   timing_params.hres,
                                   timing_params.vres,
                                   HZ_2_MHZ (timing_params.pixclk),
                                   (timing_params.hres),
                                   (timing_params.hres + timing_params.hfp),
                                   (timing_params.hres + timing_params.hfp +
                                                    timing_params.hsync_width),
                                   (timing_params.htotal),
                                   (timing_params.vres),
                                   (timing_params.vres + timing_params.vfp),
                                   (timing_params.vres + timing_params.vfp +
                                                    timing_params.vsync_width),
                                   (timing_params.vtotal),
                                   timing_params.hsync_polarity ? '+' : '-',
                                   timing_params.vsync_polarity ? '+' : '-');
        }
    }
#endif
#if XVIDC_EDID_VERBOSITY > 0
        if (VerboseEn) {
            xil_printf("\r\n");
        }
#endif

    if (ctb->revision >= 3) {
        do {
            const struct xvidc_cea861_data_block_header * const header =
                (struct xvidc_cea861_data_block_header *) &ctb->data[index];

            switch (header->tag) {

            case XVIDC_CEA861_DATA_BLOCK_TYPE_AUDIO:
                {
#if XVIDC_EDID_VERBOSITY > 1
                    const struct xvidc_cea861_audio_data_block * const db =
                        (struct xvidc_cea861_audio_data_block *) header;

                    xvidc_disp_cea861_audio_data(db,EdidCtrlParam,VerboseEn);
#endif
                }
                break;

            case XVIDC_CEA861_DATA_BLOCK_TYPE_VIDEO:
                {
#if XVIDC_EDID_VERBOSITY > 1
                    const struct xvidc_cea861_video_data_block * const db =
                        (struct xvidc_cea861_video_data_block *) header;

                    xvidc_disp_cea861_video_data(db,EdidCtrlParam,VerboseEn);
#endif
                }
                break;

            case XVIDC_CEA861_DATA_BLOCK_TYPE_VENDOR_SPECIFIC:
                {
                    const struct
                    xvidc_cea861_vendor_specific_data_block * const db =
                     (struct xvidc_cea861_vendor_specific_data_block *) header;

                    xvidc_disp_cea861_vendor_data(db,EdidCtrlParam,VerboseEn);
                }
                break;

            case XVIDC_CEA861_DATA_BLOCK_TYPE_SPEAKER_ALLOCATION:
                {
#if XVIDC_EDID_VERBOSITY > 1
                    const struct
                    xvidc_cea861_speaker_allocation_data_block * const db =
                  (struct xvidc_cea861_speaker_allocation_data_block *) header;

                    xvidc_disp_cea861_speaker_allocation_data(db,
                                                      EdidCtrlParam,VerboseEn);
#endif
                }
                break;

            case XVIDC_CEA861_DATA_BLOCK_TYPE_EXTENDED:
                {
                    const struct xvidc_cea861_extended_data_block * const db =
                        (struct xvidc_cea861_extended_data_block *) header;

                   xvidc_disp_cea861_extended_data(db,EdidCtrlParam,VerboseEn);
                }
                break;

            default:
#if XVIDC_EDID_VERBOSITY > 1
                if (VerboseEn) {
                    xil_printf("Unknown CEA-861 data block type 0x%02x\r\n",
                            header->tag);
                }
#endif
                break;
            }

            index = index + header->length + sizeof(*header);
        } while (index < ctb->dtd_offset - offset);
    }
#if XVIDC_EDID_VERBOSITY > 0
    if (VerboseEn) {
        xil_printf("\r\n");
    }
#endif
}


/*****************************************************************************/
/**
*
* This structure parse parse EDID routines
*
* @note   API XV_VidC_parse_edid's checking needs to be updated to allow
*         extra parsers.
*
******************************************************************************/
static const struct xvidc_edid_extension_handler {
    void (* const inf_disp)(const struct xvidc_edid_extension * const,
           XV_VidC_EdidCntrlParam *EdidCtrlParam, XV_VidC_Verbose VerboseEn);
} xvidc_edid_extension_handlers[] = {
	[XVIDC_EDID_EXTENSION_TIMING]         = { NULL },
	[XVIDC_EDID_EXTENSION_CEA]            = { xvidc_disp_cea861 },
	[XVIDC_EDID_EXTENSION_VTB]            = { NULL },
	[XVIDC_EDID_EXTENSION_XVIDC_EDID_2_0] = { NULL },
	[XVIDC_EDID_EXTENSION_DI]             = { NULL },
	[XVIDC_EDID_EXTENSION_LS]             = { NULL },
	[XVIDC_EDID_EXTENSION_MI]             = { NULL },
	[XVIDC_EDID_EXTENSION_DTCDB_1]        = { NULL },
	[XVIDC_EDID_EXTENSION_DTCDB_2]        = { NULL },
	[XVIDC_EDID_EXTENSION_DTCDB_3]        = { NULL },
	[XVIDC_EDID_EXTENSION_BLOCK_MAP]      = { NULL },
	[XVIDC_EDID_EXTENSION_DDDB]           = { NULL },
};


/*****************************************************************************/
/**
*
* This function parse and print the EDID of the Sink
*
* @param    data is a pointer to the EDID array.
* @param    EdidCtrlParam is a pointer the EDID Control parameter
* @param    VerboseEn is a pointer to the XV_HdmiTxSs core instance.
*
* @return None
*
* @note   None.
*
******************************************************************************/
void
XV_VidC_parse_edid(const u8 * const data,
                  XV_VidC_EdidCntrlParam *EdidCtrlParam,
                  XV_VidC_Verbose VerboseEn) {
    const struct edid * const edid = (struct edid *) data;
    const struct xvidc_edid_extension * const extensions =
        (struct xvidc_edid_extension *) (data + sizeof(*edid));

    XV_VidC_EdidCtrlParamInit(EdidCtrlParam);

    xvidc_disp_edid1(edid,EdidCtrlParam,VerboseEn);

    if(edid->extensions > 1) return;

    for (u8 i = 0; i < edid->extensions; i++) {
        const struct xvidc_edid_extension * const extension = &extensions[i];
        const struct xvidc_edid_extension_handler * const handler =
            &xvidc_edid_extension_handlers[extension->tag];

        if (!handler->inf_disp) {
#if XVIDC_EDID_VERBOSITY > 0
            if (VerboseEn) {
                xil_printf("WARNING: block %u contains unknown extension "
                                         " (%#04x)\r\n", i, extensions[i].tag);
            }
#endif
            continue;
        } else {
		(*handler->inf_disp)(extension,EdidCtrlParam,VerboseEn);
        }
    }
}
