#pragma once

extern void gluStuffOrtho( float* matrix, float left, float right, float bottom, float top, float near, float far );
extern int  gluStuffProject( float objx, float objy, float objz, const float modelMatrix[ 16 ], const float projMatrix[ 16 ], const int viewport[ 4 ], float* winx, float* winy, float* winz );
extern int  gluStuffUnProject( float winx, float winy, float winz, const float modelMatrix[ 16 ], const float projMatrix[ 16 ], const int viewport[ 4 ], float* objx, float* objy, float* objz );
