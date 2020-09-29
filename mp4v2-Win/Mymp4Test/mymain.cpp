#include <stdio.h>

int mymp4test(const char* mp4FileName);
int mymp4test2(const char* mp4FileName);

static const char *gMp4FileName = "E:\\BW.mp4";
// static const char *gMp4FileName = "E:\\y8.mp4";

int main()
{
    mymp4test2(gMp4FileName);
    // mymp4test(gMp4FileName);

    return 0;
}
