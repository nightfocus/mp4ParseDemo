#if defined( _WIN32 )
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <mp4v2/mp4v2.h>

#if defined _WIN32
#include "libplatform/platform_win32.h"
#endif


unsigned char sps[64], pps[64];
uint8_t aacAdtsHeader[7]; // adts header use len: 7 bytes.
int spslen = 0, ppslen = 0;
uint32_t g264FrameLenSize = 0; // mp4里的AVCC格式的264帧的长度字段，占用的字节数，一般是4.
uint8_t gAudioType = 0;
int gAudioChanNum = -1;
int gAudioRateIndex = -1;

int get264Stream(MP4FileHandle oMp4File, int VTrackId, int totalFrame);
int getAACStream(MP4FileHandle oMp4File, int ATrackId, int totalFrame);
int demuxMp4File(const char *sMp4file);
void fillAdtsHeader(uint8_t* aacAdtsHeader, int packetLen);

int mymp4test2(const char* mp4FileName)
{
    demuxMp4File(mp4FileName);
    return 0;
}


int get264Stream(MP4FileHandle oMp4File, int VTrackId, int totalFrame)
{
    if (!oMp4File) 
        return -1;

    char NAL[4] = { 0x00,0x00,0x00,0x01 };
    
    unsigned int nSize = 1024 * 1024 * 1;
    unsigned char *pData = (unsigned char *)malloc(nSize); // 最大允许1MB的帧

    unsigned char *p = NULL;
    MP4Timestamp pStartTime;
    MP4Duration pDuration;
    MP4Duration pRenderingOffset;
    bool pIsSyncSample = 0;
    unsigned int frameSize = 0;
    bool firstWriteFile = true;

    int nReadIndex = 0;
    FILE *pFile = NULL;
    pFile = fopen("e:\\out1.264", "wb");

    while (nReadIndex < totalFrame)
    {
        nReadIndex++;
        //printf("nReadIndex:%d\n",nReadIndex);
        nSize = 1024 * 1024 * 1;
        MP4ReadSample(oMp4File, VTrackId, nReadIndex, &pData, &nSize, &pStartTime, &pDuration, &pRenderingOffset, &pIsSyncSample);

        // printf("I:%d, nSize:%u.   nReadIndex:%d\n", pIsSyncSample, nSize, nReadIndex);

        // IDR帧，写入sps pps先
        // 这个如果是写文件，则只写一次即可，如果是流传输，那最好每个IDR帧写入.
        if (firstWriteFile && pIsSyncSample)
        {
            fwrite(NAL, sizeof(NAL), 1, pFile);
            fwrite(sps, spslen, 1, pFile);

            fwrite(NAL, sizeof(NAL), 1, pFile);
            fwrite(pps, ppslen, 1, pFile);

            firstWriteFile = false;
        }

        p = pData;
        while (nSize > 0) 
        {
            // 标准的264帧，前面4个字节就是frame的长度（H264 AVCC格式）
            // 需要先写入标准的264 nal 头.
            fwrite(NAL, sizeof(NAL), 1, pFile);

            frameSize = 0x0;
            if (g264FrameLenSize == 4)
            {
                frameSize |= *p << 24;
                frameSize |= *(p + 1) << 16;
                frameSize |= *(p + 2) << 8;
                frameSize |= *(p + 3);
            }
            else if (g264FrameLenSize == 2)
            {
                frameSize |= *p << 8;
                frameSize |= *(p + 1);
            }
            else if (g264FrameLenSize == 1)
            {
                frameSize |= *p;
            }
            else
            {
                printf("Err. The g264FrameLenSize is %d.\n", g264FrameLenSize);
                break;
            }
            
            // 将nSize递减，偏移到下一个帧的位置
            nSize = nSize - frameSize - g264FrameLenSize;
            if(nSize < 0)
                printf("Err. I:%d, framesize=%x, nSize=%d\n", pIsSyncSample, frameSize, nSize);
            p = p + g264FrameLenSize; // 长度字段不输出
            fwrite(p, frameSize, 1, pFile);
            if (nSize > 0) // 表示此次MP4ReadSample()获取的数据，不止1帧，所以继续处理
                p = p + frameSize;
        }

        // 如果传入MP4ReadSample的视频pData是null
        // 它内部就会new 一个内存
        // 如果传入的是已知的内存区域，
        // 则需要保证空间bigger then max frames size.
        // free(p);
        // pData = NULL;
        // p = NULL;
    }
    fflush(pFile);
    fclose(pFile);

    free(pData);

    return 0;
}

int getAACStream(MP4FileHandle oMp4File, int ATrackId, int totalFrame)
{
    if (!oMp4File) 
        return -1;
    
    unsigned char *pData = NULL;
    // unsigned char *p = NULL;
    unsigned int i = 0;
    unsigned int nSize = 0;
    MP4Timestamp pStartTime;
    MP4Duration pDuration;
    MP4Duration pRenderingOffset;

    int nReadIndex = 0;
    FILE *pFile = NULL;
    pFile = fopen("E:\\out2.aac", "wb");

    while (nReadIndex < totalFrame)
    {
        nReadIndex++;

        MP4ReadSample(oMp4File, ATrackId, nReadIndex, &pData, &nSize, &pStartTime, &pDuration, &pRenderingOffset);
        fillAdtsHeader(aacAdtsHeader, nSize);
        fwrite(aacAdtsHeader, sizeof(aacAdtsHeader), 1, pFile);
        fwrite(pData, nSize, 1, pFile);


        //如果传入MP4ReadSample的视频pData是null
        // 它内部就会new 一个内存
        //如果传入的是已知的内存区域，
        //则需要保证空间bigger then max frames size.
        free(pData);
        pData = NULL;
    }

    fflush(pFile);
    fclose(pFile);

    return 0;
}

// 
// Ref: https://www.cnblogs.com/lidabo/p/7261558.html
// https://blog.nowcoder.net/n/e8f9c78aa2e0453aa2800cce162435c5
/*
Structure{
    AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP (QQQQQQQQ QQQQQQQQ)
    Header consists of 7 or 9 bytes (without or with CRC).
}
Letter Length (bits) Description
A     12      syncword 0xFFF, all bits must be 1
B     1       MPEG Version: 0 for MPEG-4, 1 for MPEG-2
C     2       Layer: always 0
D     1       protection absent, Warning, set to 1 if there is no CRC and 0 if there is CRC
E     2       profile, the MPEG-4 Audio Object Type minus 1
F     4       MPEG-4 Sampling Frequency Index (15 is forbidden)
G     1       private stream, set to 0 when encoding, ignore when decoding
H     3       MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE)
I     1       originality, set to 0 when encoding, ignore when decoding
J     1       home, set to 0 when encoding, ignore when decoding
K     1       copyrighted stream, set to 0 when encoding, ignore when decoding
L     1       copyright start, set to 0 when encoding, ignore when decoding
M     13      frame length, this value must include 7 or 9 bytes of header length: FrameLength =

(D == 1 ? 7 : 9) + size(AACFrame)
O     11      Buffer fullness
P     2       Number of AAC frames (RDBs) in ADTS frame minus 1, for maximum compatibility always use 1 AAC frame per ADTS frame
Q     16      CRC if protection absent is 0

Usage in MPEG-TS
ADTS packet must be a content of PES packet. Pack AAC data inside ADTS frame, than pack inside PES packet, then mux by TS packetizer.

Usage in Shoutcast
ADTS frames goes one by one in TCP stream. Look for syncword, parse header and look for next syncword after.
*/
void fillAdtsHeader(uint8_t* ah, int packetLen)
{
    packetLen += 7; // ADTS帧长度包括 ADTS头大小 + AAC帧长

    // fill in ADTS data
    ah[0] = (uint8_t)0xFF;
    ah[1] = (uint8_t)0xF1;
    ah[2] = (uint8_t)(((gAudioType - 1) << 6) + (gAudioRateIndex << 2) + (gAudioChanNum >> 2));
    ah[3] = (uint8_t)(((gAudioChanNum & 3) << 6) + (packetLen >> 11));
    ah[4] = (uint8_t)((packetLen & 0x7FF) >> 3);
    ah[5] = (uint8_t)(((packetLen & 7) << 5) + 0x1F);
    ah[6] = (uint8_t)0xFC;
}

/* demuxMp4File
 * 解析一个mp4文件（假定是h264 + AAC），分别将音视频存储到不同目标文件中
 * h264目标文件是Annex B格式（带00 00 00 01头的那种），参考：
 * https://blog.csdn.net/romantic_energy/article/details/50508332
 * https://blog.csdn.net/lq496387202/article/details/81510622
 * aac目标文件是AAC裸流（填充了adts头），参考：
 * https://www.cnblogs.com/lidabo/p/7261558.html
 * https://www.cnblogs.com/yanwei-wang/p/12758570.html
 * 雷神的blog：https://blog.csdn.net/leixiaohua1020/article/details/39767055
 * https://blog.nowcoder.net/n/e8f9c78aa2e0453aa2800cce162435c5
*/
int demuxMp4File(const char *sMp4file)
{
    MP4FileHandle oMp4File;
    // int i;

    //unsigned int oStreamDuration;
    unsigned int vFrameCount;
    unsigned int aFrameCount;

    oMp4File = MP4Read(sMp4file);
    int videoindex = -1, audioindex = -1;
    uint32_t numSamples;
    //uint32_t timescale;
    //uint64_t duration;        

    if (!oMp4File)
    {
        printf("Err. Read %s is failed.\n", sMp4file);
        return -1;
    }

    MP4TrackId trackId = MP4_INVALID_TRACK_ID;
    uint32_t numTracks = MP4GetNumberOfTracks(oMp4File, NULL, 0);
    printf("numTracks:%d\n", numTracks);

    for (uint32_t i = 0; i < numTracks; i++)
    {
        trackId = MP4FindTrackId(oMp4File, i, NULL, 0);
        const char* trackType = MP4GetTrackType(oMp4File, trackId);
        if (MP4_IS_VIDEO_TRACK_TYPE(trackType))
        {
            //printf("[%s %d] trackId:%d\r\n",__FUNCTION__,__LINE__,trackId);
            videoindex = trackId;

            //duration = MP4GetTrackDuration(oMp4File, trackId );
            numSamples = MP4GetTrackNumberOfSamples(oMp4File, trackId);
            //timescale = MP4GetTrackTimeScale(oMp4File, trackId);          
            //oStreamDuration = duration/(timescale/1000);          
            vFrameCount = numSamples;

            // read sps/pps 
            uint8_t **seqheader;
            uint8_t **pictheader;
            uint32_t *pictheadersize;
            uint32_t *seqheadersize;
            uint32_t ix;

            MP4GetTrackH264SeqPictHeaders(oMp4File, trackId, &seqheader, &seqheadersize, &pictheader, &pictheadersize);

            /* Ref:
              mp4存储H.264流的方式是AVCC格式，
              在这种格式中，每一个NALU包都加上了一个指定其长度(NALU包大小)的前缀(in big endian format大端格式)，
              这种格式的包非常容易解析，但是这种格式去掉了Annex B格式中的字节对齐特性，而且前缀可以是1、2或4字节，
              这让AVCC格式变得更复杂了，指定前缀字节数(1、2或4字节)的值保存在一个头部对象中(流开始的部分)
            */
            MP4GetTrackH264LengthSize(oMp4File, trackId, &g264FrameLenSize);

            for (ix = 0; seqheadersize[ix] != 0; ix++)
            {
                memcpy(sps, seqheader[ix], seqheadersize[ix]);
                spslen = seqheadersize[ix];
                free(seqheader[ix]);
            }
            free(seqheader);
            free(seqheadersize);

            for (ix = 0; pictheadersize[ix] != 0; ix++)
            {
                memcpy(pps, pictheader[ix], pictheadersize[ix]);
                ppslen = pictheadersize[ix];
                free(pictheader[ix]);
            }
            free(pictheader);
            free(pictheadersize);
        }
        else if (MP4_IS_AUDIO_TRACK_TYPE(trackType))
        {
            audioindex = trackId;
            aFrameCount = MP4GetTrackNumberOfSamples(oMp4File, trackId);
            // 获取音频的类型/声道数/采样率
            // gAudioType具体解释见 mp4v2库的：src/mp4info.cpp -> PrintAudioInfo()
            gAudioType = MP4GetTrackAudioMpeg4Type(oMp4File, trackId);
            gAudioChanNum = MP4GetTrackAudioChannels(oMp4File, trackId);
            uint32_t audio_time_scale = MP4GetTrackTimeScale(oMp4File, trackId);

            printf("audio: type %x, channel_num: %d, %u Hz\n", gAudioType, gAudioChanNum, audio_time_scale);

            //const char * audio_name = MP4GetTrackMediaDataName(inFile, id);
            //TRACE("=== 音频编码：%s ===\n", audio_name);
            
            // TRACE("=== 音频每秒刻度数(采样率)：%lu ===\n", audio_time_scale);
            
            //  采样率与下标的关系, Ref: ffmpeg mpeg4audio.c int avpriv_mpeg4audio_sample_rates[16]
            int asri[] = {
                    96000, 88200, 64000, 48000, 44100, 32000,
                    24000, 22050, 16000, 12000, 11025, 8000,  7350
            };
            for (int i = 0; i < sizeof(asri) / sizeof(int); i++)
            {
                if (audio_time_scale == asri[i])
                {
                    gAudioRateIndex = i; // such as: audio_time_scale == 48000 hz, gAudioRateIndex = 3
                    break;
                }
            }
            if (gAudioRateIndex == -1)
            {
                printf("Err. audio rate is error.\n");
                // todo.
            }

        }
    }

    // 解析完了mp4，开始将各自track分离存储
    if (videoindex >= 0)
        get264Stream(oMp4File, videoindex, vFrameCount);
    if (audioindex >= 0)
        getAACStream(oMp4File, audioindex, aFrameCount);

    // 需要mp4close
    MP4Close(oMp4File, 0);
    return 0;
}

