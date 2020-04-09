/*
 * K4815 Pattern Generator - Scale Map
 *
 * Copyright 2010: Kilpatrick Audio
 * Written by: Andrew Kilpatrick
 * Version: 1.1
 *
 * Scale Address: 0x6000 - 0x611f
 *
 */
//
// SCALES
//
// scale 0 - minor, small
#pragma DATA	0x6000, 60, 62, 63, 65, 67, 69, 70, 72
#pragma DATA	0x6008, 72, 74, 75, 77, 79, 81, 82, 84
#pragma DATA	0x6010, 60, 62, 63, 65, 67, 69, 70, 72
#pragma DATA	0x6018, 72, 74, 75, 77, 79, 81, 82, 84
#pragma DATA	0x6020, 60, 62, 63, 65, 67, 69, 70, 72
#pragma DATA	0x6028, 72, 74, 75, 77, 79, 81, 82, 84
#pragma DATA	0x6030, 60, 62, 63, 65, 67, 69, 70, 72
#pragma DATA	0x6038, 72, 74, 75, 77, 79, 81, 82, 84

// scale 1 - minor, large
#pragma DATA	0x6040, 48, 50, 51, 53, 55, 57, 58, 60
#pragma DATA	0x6048, 60, 62, 63, 65, 67, 69, 70, 72
#pragma DATA	0x6050, 72, 74, 75, 77, 79, 81, 82, 84
#pragma DATA	0x6058, 84, 86, 87, 89, 91, 93, 94, 96
#pragma DATA	0x6060, 48, 50, 51, 53, 55, 57, 58, 60
#pragma DATA	0x6068, 60, 62, 63, 65, 67, 69, 70, 72
#pragma DATA	0x6070, 72, 74, 75, 77, 79, 81, 82, 84
#pragma DATA	0x6078, 84, 86, 87, 89, 91, 93, 94, 96

// scale 2 - major, small
#pragma DATA	0x6080, 60, 62, 64, 65, 67, 69, 71, 72
#pragma DATA	0x6088, 72, 74, 76, 77, 79, 81, 83, 84
#pragma DATA	0x6090, 60, 62, 64, 65, 67, 69, 71, 72
#pragma DATA	0x6098, 72, 74, 76, 77, 79, 81, 83, 84
#pragma DATA	0x60a0, 60, 62, 64, 65, 67, 69, 71, 72
#pragma DATA	0x60a8, 72, 74, 76, 77, 79, 81, 83, 84
#pragma DATA	0x60b0, 60, 62, 64, 65, 67, 69, 71, 72
#pragma DATA	0x60b8, 72, 74, 76, 77, 79, 81, 83, 84

// scale 3 - major, large
#pragma DATA	0x60c0, 48, 50, 52, 53, 55, 57, 59, 60
#pragma DATA	0x60c8, 60, 62, 64, 65, 67, 69, 71, 72
#pragma DATA	0x60d0, 72, 74, 76, 77, 79, 81, 83, 84
#pragma DATA	0x60d8, 84, 86, 88, 89, 91, 93, 95, 96
#pragma DATA	0x60e0, 48, 50, 52, 53, 55, 57, 59, 60
#pragma DATA	0x60e8, 60, 62, 64, 65, 67, 69, 71, 72
#pragma DATA	0x60f0, 72, 74, 76, 77, 79, 81, 83, 84
#pragma DATA	0x60f8, 84, 86, 88, 89, 91, 93, 95, 96

// X/Y - minor, small
#pragma DATA	0x6100, 32, 41, 50, 59, 68, 77, 86, 95
// X/Y - minor, large
#pragma DATA	0x6108, 0, 18, 36, 54, 72, 90, 108, 127
// X/Y - major, small
#pragma DATA	0x6110, 95, 86, 77, 68, 59, 50, 41, 32
// X/Y - major, large
#pragma DATA	0x6118, 127, 108, 90, 72, 54, 36, 18, 0
