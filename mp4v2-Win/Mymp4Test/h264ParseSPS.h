#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

typedef  unsigned int UINT;
typedef  unsigned char BYTE;
typedef  unsigned long DWORD;

// Ref: https://blog.csdn.net/caoshangpa/article/details/53083410
bool h264_decode_sps(BYTE * buf, unsigned int nLen, int &width, int &height, int &fps);

