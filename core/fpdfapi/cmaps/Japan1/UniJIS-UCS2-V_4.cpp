// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/cmaps/Japan1/cmaps_japan1.h"

namespace fxcmap {

const uint16_t kUniJIS_UCS2_V_4[251 * 2] = {
    0x00B0, 0x204D, 0x2010, 0x1ED5, 0x2015, 0x1ED4, 0x2016, 0x1ED7, 0x2018,
    0x2059, 0x2019, 0x205A, 0x201C, 0x2057, 0x201D, 0x2058, 0x2025, 0x1EDA,
    0x2026, 0x1ED9, 0x2032, 0x2051, 0x2033, 0x205B, 0x2190, 0x02E2, 0x2191,
    0x02E0, 0x2192, 0x02E3, 0x2193, 0x02E1, 0x2195, 0x2FB6, 0x21C4, 0x2077,
    0x21C5, 0x2076, 0x21C6, 0x2078, 0x21E6, 0x1F4C, 0x21E7, 0x1F4E, 0x21E8,
    0x1F4B, 0x21E9, 0x1F4D, 0x2225, 0x1ED7, 0x223C, 0x1ED6, 0x22EF, 0x1ED9,
    0x2500, 0x1D39, 0x2501, 0x1D3A, 0x2502, 0x1D37, 0x2503, 0x1D38, 0x2504,
    0x1D3D, 0x2505, 0x1D3E, 0x2506, 0x1D3B, 0x2507, 0x1D3C, 0x2508, 0x1D41,
    0x2509, 0x1D42, 0x250A, 0x1D3F, 0x250B, 0x1D40, 0x250C, 0x1D47, 0x250D,
    0x1D49, 0x250E, 0x1D48, 0x250F, 0x1D4A, 0x2510, 0x1D4F, 0x2511, 0x1D51,
    0x2512, 0x1D50, 0x2513, 0x1D52, 0x2514, 0x1D43, 0x2515, 0x1D45, 0x2516,
    0x1D44, 0x2517, 0x1D46, 0x2518, 0x1D4B, 0x2519, 0x1D4D, 0x251A, 0x1D4C,
    0x251B, 0x1D4E, 0x251C, 0x1D63, 0x251D, 0x1D67, 0x251E, 0x1D65, 0x251F,
    0x1D64, 0x2520, 0x1D66, 0x2521, 0x1D69, 0x2522, 0x1D68, 0x2523, 0x1D6A,
    0x2524, 0x1D6B, 0x2525, 0x1D6F, 0x2526, 0x1D6D, 0x2527, 0x1D6C, 0x2528,
    0x1D6E, 0x2529, 0x1D71, 0x252A, 0x1D70, 0x252B, 0x1D72, 0x252C, 0x1D5B,
    0x252D, 0x1D5D, 0x252E, 0x1D5E, 0x252F, 0x1D5F, 0x2530, 0x1D5C, 0x2531,
    0x1D60, 0x2532, 0x1D61, 0x2533, 0x1D62, 0x2534, 0x1D53, 0x2535, 0x1D55,
    0x2536, 0x1D56, 0x2537, 0x1D57, 0x2538, 0x1D54, 0x2539, 0x1D58, 0x253A,
    0x1D59, 0x253B, 0x1D5A, 0x253D, 0x1D77, 0x253E, 0x1D78, 0x253F, 0x1D79,
    0x2540, 0x1D75, 0x2541, 0x1D74, 0x2542, 0x1D76, 0x2543, 0x1D7B, 0x2544,
    0x1D7D, 0x2545, 0x1D7A, 0x2546, 0x1D7C, 0x2547, 0x1D81, 0x2548, 0x1D80,
    0x2549, 0x1D7E, 0x254A, 0x1D7F, 0x261C, 0x201D, 0x261D, 0x201B, 0x261E,
    0x201E, 0x261F, 0x201C, 0x2702, 0x2F92, 0x27A1, 0x2011, 0x3001, 0x1ECF,
    0x3002, 0x1ED0, 0x3008, 0x1EE3, 0x3009, 0x1EE4, 0x300A, 0x1EE5, 0x300B,
    0x1EE6, 0x300C, 0x1EE7, 0x300D, 0x1EE8, 0x300E, 0x1EE9, 0x300F, 0x1EEA,
    0x3010, 0x1EEB, 0x3011, 0x1EEC, 0x3013, 0x204E, 0x3014, 0x1EDD, 0x3015,
    0x1EDE, 0x301C, 0x1ED6, 0x301D, 0x1F14, 0x301F, 0x1F15, 0x3041, 0x1EEE,
    0x3043, 0x1EEF, 0x3045, 0x1EF0, 0x3047, 0x1EF1, 0x3049, 0x1EF2, 0x3063,
    0x1EF3, 0x3083, 0x1EF4, 0x3085, 0x1EF5, 0x3087, 0x1EF6, 0x308E, 0x1EF7,
    0x309B, 0x2050, 0x309C, 0x204F, 0x30A1, 0x1EF8, 0x30A3, 0x1EF9, 0x30A5,
    0x1EFA, 0x30A7, 0x1EFB, 0x30A9, 0x1EFC, 0x30C3, 0x1EFD, 0x30E3, 0x1EFE,
    0x30E5, 0x1EFF, 0x30E7, 0x1F00, 0x30EE, 0x1F01, 0x30F5, 0x1F02, 0x30F6,
    0x1F03, 0x30FC, 0x1ED3, 0x3300, 0x209E, 0x3301, 0x2EB6, 0x3302, 0x2EB7,
    0x3303, 0x2092, 0x3304, 0x2EB8, 0x3305, 0x208D, 0x3306, 0x2EB9, 0x3307,
    0x2EBD, 0x3308, 0x2EBB, 0x3309, 0x2EC0, 0x330A, 0x2EBE, 0x330B, 0x2EC2,
    0x330C, 0x2EC4, 0x330D, 0x1F0E, 0x330E, 0x2EC5, 0x330F, 0x2EC6, 0x3310,
    0x2EC7, 0x3311, 0x2EC8, 0x3312, 0x2EC9, 0x3313, 0x2ECA, 0x3314, 0x1F05,
    0x3315, 0x2094, 0x3316, 0x208A, 0x3317, 0x2ECC, 0x3318, 0x2093, 0x3319,
    0x2ECE, 0x331A, 0x2ED0, 0x331B, 0x2ED1, 0x331C, 0x2ED2, 0x331D, 0x2ED3,
    0x331E, 0x20A1, 0x331F, 0x2ED4, 0x3320, 0x2ED5, 0x3321, 0x2ED6, 0x3322,
    0x2089, 0x3323, 0x209C, 0x3324, 0x2ED7, 0x3325, 0x2ED9, 0x3326, 0x1F0F,
    0x3327, 0x1F09, 0x3328, 0x2EDC, 0x3329, 0x2EDD, 0x332A, 0x20A4, 0x332B,
    0x1F11, 0x332D, 0x2EDF, 0x332E, 0x2EE2, 0x332F, 0x2EE3, 0x3330, 0x2EE4,
    0x3331, 0x20A6, 0x3332, 0x2EE5, 0x3333, 0x208E, 0x3334, 0x2EE8, 0x3335,
    0x2EE9, 0x3336, 0x1F0B, 0x3337, 0x2EEE, 0x3338, 0x2EF0, 0x3339, 0x2097,
    0x333A, 0x2EF1, 0x333B, 0x209D, 0x333C, 0x2EEA, 0x333D, 0x2EF2, 0x333E,
    0x2EF4, 0x333F, 0x2EF5, 0x3340, 0x2EF6, 0x3341, 0x2EF3, 0x3342, 0x209B,
    0x3343, 0x2EF7, 0x3344, 0x2EF8, 0x3345, 0x2EF9, 0x3346, 0x2EFA, 0x3347,
    0x20A5, 0x3348, 0x2EFB, 0x3349, 0x1F04, 0x334A, 0x1F12, 0x334B, 0x2EFC,
    0x334C, 0x2EFD, 0x334D, 0x1F07, 0x334E, 0x2091, 0x334F, 0x2EFE, 0x3350,
    0x2EFF, 0x3351, 0x1F0C, 0x3352, 0x2F02, 0x3353, 0x2F06, 0x3354, 0x2F03,
    0x3355, 0x2F07, 0x3356, 0x2F08, 0x3357, 0x2098, 0x337F, 0x2084, 0xFF08,
    0x1EDB, 0xFF09, 0x1EDC, 0xFF0C, 0x204C, 0xFF0E, 0x2052, 0xFF1D, 0x1EED,
    0xFF3B, 0x1EDF, 0xFF3D, 0x1EE0, 0xFF3F, 0x1ED2, 0xFF5B, 0x1EE1, 0xFF5C,
    0x1ED8, 0xFF5D, 0x1EE2, 0xFF5E, 0x1ED6, 0xFFE3, 0x1ED1,
};

}  // namespace fxcmap
