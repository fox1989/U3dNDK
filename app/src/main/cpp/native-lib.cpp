#include <jni.h>
#include <string>
#include "media/NdkMediaCodec.h"
#include <android/log.h>
#include "threadsafe_queue.cpp"
#include <unistd.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaExtractor.h>
#include <unistd.h>
#include <fcntl.h>


threadsafe_queue<u_char*> frame_queue;
int width;
int height;
int frameRate;
int generateIndex;
FILE* fp;
bool isRuning= false;
AMediaCodec* mediaCodec;
int  TIMEOUT_US=12000;
uint8_t* m_info;
AMediaFormat *m_format;
//AMediaMuxer* mediaMuxer;
//ssize_t trackIndex;
int m_infoSize;
extern "C" JNIEXPORT jstring JNICALL
Java_com_fox_u3dndk_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}


long computePresentationTime(long frameIndex){
    return 132 + frameIndex * 1000000 / frameRate;
}


void *startEncode(void* obj)
{
    while (isRuning)
    {
        if(!frame_queue.empty()&&mediaCodec!=NULL)
        {
            u_char* input=*frame_queue.wait_and_pop().get();
            int tempFrameSize=width*height;
            uint8_t* U=(uint8_t*)malloc(tempFrameSize/4);
            uint8_t* V=(uint8_t*)malloc(tempFrameSize/4);

            memcpy(U,input+tempFrameSize,tempFrameSize/4);
            memcpy(V,input+tempFrameSize*5/4,tempFrameSize/4);


            memcpy(input+tempFrameSize,V,tempFrameSize/4);
            memcpy(input+tempFrameSize*5/4,U,tempFrameSize/4);

            free(U);
            free(V);
            ssize_t ibufidx, obufidx;
            AMediaCodecBufferInfo info;
            size_t bufsize;
            int  YUVSize=width*height*3/2;
            /*First queue input image*/
            uint8_t *buf;

            ibufidx = AMediaCodec_dequeueInputBuffer(mediaCodec, TIMEOUT_US);
            if (ibufidx >= 0)
            {
                buf = AMediaCodec_getInputBuffer(mediaCodec, ibufidx, &bufsize);
                if (buf)
                {
                    memcpy(buf, input, YUVSize);
                    auto curTime =computePresentationTime(generateIndex); //timeGetTime();
                    AMediaCodec_queueInputBuffer(mediaCodec, ibufidx, 0, bufsize, curTime, 0);
                    generateIndex++;
                }
                else
                {
                    __android_log_print(ANDROID_LOG_INFO,"Unity:","MediaCodecH264Enc: obtained InputBuffer, but no address.");
                }
            }
            else if (ibufidx == AMEDIA_ERROR_UNKNOWN)
            {
                __android_log_print(ANDROID_LOG_INFO,"Unity:","MediaCodecH264Enc: AMediaCodec_dequeueInputBuffer() had an exception");
            }
            //int pos = 0;
            /*Second, dequeue possibly pending encoded frames*/
            while ((obufidx = AMediaCodec_dequeueOutputBuffer(mediaCodec, &info, TIMEOUT_US)) >= 0)
            {
                auto oBuf = AMediaCodec_getOutputBuffer(mediaCodec, obufidx, &bufsize);
               // __android_log_print(ANDROID_LOG_INFO,"Unity:","info.size %d   bufSize: %d",info.size,bufsize);
                int bufSize=info.size;
                if (oBuf)
                {
                    if(m_info==NULL) {
                        m_infoSize = info.size;


                        m_info = (uint8_t *) malloc(m_infoSize);
                        if (info.flags == 2) {
                            memcpy(m_info, oBuf, m_infoSize);
                        } else
                        {
                            __android_log_print(ANDROID_LOG_INFO,"Unity:","encoded m_info error");
                        }
                    }

                    if (info.flags==1)
                    {
                        uint8_t* keyFrame=(uint8_t*)malloc(bufSize+ m_infoSize);
                        memcpy(keyFrame, m_info, m_infoSize);
                        memcpy(keyFrame + m_infoSize, oBuf, bufSize);
                        fwrite(keyFrame,1,bufSize+m_infoSize,fp);
                        //AMediaMuxer_writeSampleData(mediaMuxer, trackIndex, keyFrame, &info);
                    }
                    else{
                        uint8_t* outdata = (uint8_t*)malloc(bufSize);
                        memcpy(outdata, oBuf, bufSize);
                        fwrite(outdata,1,bufSize,fp);
                        //AMediaMuxer_writeSampleData(mediaMuxer, trackIndex, outdata, &info);
                    }
                    //__android_log_print(ANDROID_LOG_INFO,"Unity:","Out finish");
                }
                AMediaCodec_releaseOutputBuffer(mediaCodec, obufidx, false);
            }
            free(input);
        }
        else
            usleep(50000); //50*1000
    }
    __android_log_print(ANDROID_LOG_INFO,"Unity:","thread over0");
    while (!frame_queue.empty())
    {
        __android_log_print(ANDROID_LOG_INFO,"Unity:","threa over queue pop");
        uint8_t* picture_buf = *frame_queue.wait_and_pop().get();
        free(picture_buf);
    }
    __android_log_print(ANDROID_LOG_INFO,"Unity:","thread over1");


    if(fp!=NULL) {
        fclose(fp);
        fp=NULL;
    }
    if(mediaCodec!=NULL)
    {
        AMediaCodec_stop(mediaCodec);
        AMediaCodec_delete(mediaCodec);
        //free(mediaCodec);
    }
    __android_log_print(ANDROID_LOG_INFO,"Unity:","thread over2");

    if(m_info!=NULL)
    {
        free(m_info);
        m_info=NULL;
    }


//    if(mediaMuxer!=NULL)
//    {
//        AMediaMuxer_stop(mediaMuxer);
//
//        AMediaMuxer_delete(mediaMuxer);
//    }
    __android_log_print(ANDROID_LOG_INFO,"Unity:","thread over3");

    return 0;
}




 void NV21ToNV12(char* nv21,char* nv12){
    if(nv21 == NULL || nv12 == NULL) {
        return;
    }
    int framesize = width*height;
    int i = 0,j = 0;

    memcpy(nv12,nv21,framesize);
    for(i = 0; i < framesize; i++){
        nv12[i] = nv21[i];
    }
    for (j = 0; j < framesize/2; j+=2)
    {
        nv12[framesize + j-1] = nv21[j+framesize];
    }
    for (j = 0; j < framesize/2; j+=2)
    {
        nv12[framesize + j] = nv21[j+framesize-1];
    }
}




extern "C"
{
    int add(int a,int b)
    {
        __android_log_print(ANDROID_LOG_INFO,"Unity:","a+b=%d",(a+b));
        return  a+b;
    }


    void Init2H264(int w,int h,char* filePath)
    {

        __android_log_print(ANDROID_LOG_INFO,"Unity:","star init:%s",filePath);
        __android_log_print(ANDROID_LOG_INFO,"Unity:","star w:%d  h:%d  rate:%d",w,h,30);
        if(isRuning)
        {
            __android_log_print(ANDROID_LOG_INFO,"Unity:","is runing");
            return;
        }

        m_infoSize=0;
        generateIndex=0;
        frameRate=25;
        width=w;
        height=h;
        const char* mime ="video/avc";//"video/avc"
        mediaCodec=AMediaCodec_createEncoderByType(mime);
        if(mediaCodec==NULL)
        {
            __android_log_print(ANDROID_LOG_INFO,"Unity:","mediaCode is null");
        }

        m_format=AMediaFormat_new();
        AMediaFormat_setString(m_format, AMEDIAFORMAT_KEY_MIME, mime);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_WIDTH, width);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_HEIGHT, height);

//        AMEDIAFORMAT_KEY_BITRATE_MODE
//        BITRATE_MODE_CQ  0
//        表示不控制码率，尽最大可能保证图像质量
//                BITRATE_MODE_VBR  1
//        表示 MediaCodec 会根据图像内容的复杂度来动态调整输出码率，图像负责则码率高，图像简单则码率低
//                BITRATE_MODE_CBR  2
//        表示 MediaCodec 会把输出的码率控制为设定的大小
//

        int bitrate =500000000;  //码率
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_FRAME_RATE, frameRate);

        //AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_BITRATE_MODE,1);

        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 3);
        AMediaFormat_setInt32(m_format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 19);
        media_status_t status= AMediaCodec_configure(mediaCodec,m_format,NULL,NULL,AMEDIACODEC_CONFIGURE_FLAG_ENCODE);
        __android_log_print(ANDROID_LOG_INFO,"Unity:","star init  mediaCodec");

        if (status != 0)
        {
            __android_log_print(ANDROID_LOG_INFO,"Unity:","AMediaCodec_configure() failed with error %i for format %u", (int)status, 21);
        }
        else
        {
            if ((status = AMediaCodec_start(mediaCodec)) != AMEDIA_OK)
                __android_log_print(ANDROID_LOG_INFO,"Unity:","AMediaCodec_start: Could not start encoder.");
            else
                __android_log_print(ANDROID_LOG_INFO,"Unity:","AMediaCodec_start: encoder successfully started");

        }


//
//        int mp4_fb=  open(mp4Path,O_WRONLY|O_CREAT|O_TRUNC,0666);
//        mediaMuxer= AMediaMuxer_new(mp4_fb,AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
//        trackIndex = AMediaMuxer_addTrack(mediaMuxer,m_format);
//        AMediaMuxer_start(mediaMuxer);


        //CreatFile

        if(fp!=NULL)
            fclose(fp);

        fp = fopen(filePath, "wb");

        if (fp == NULL)
            __android_log_print(ANDROID_LOG_INFO,"Unity:","fp is null");

        isRuning= true;

        __android_log_print(ANDROID_LOG_INFO,"Unity:","star thread");
        pthread_t thread;
        pthread_create(&thread, NULL, startEncode, NULL);
    }


    void pushOneFrame2H264(u_char* buf,int count)
    {
      //__android_log_print(ANDROID_LOG_INFO,"Unity:","pushOneFrame");
        if(!isRuning)
            return;
        int frameSize=width*height;

        u_char* new_buf = (u_char*)malloc(frameSize * 3 / 2);
        memcpy(new_buf, buf, frameSize * 3 / 2);
        frame_queue.push(new_buf);

        u_char* new_buf2 = (u_char*)malloc(frameSize * 3 / 2);
        memcpy(new_buf2, (buf + frameSize * 3 / 2), frameSize * 3 / 2);
        frame_queue.push(new_buf2);
    }



    void endThread2H264()
    {
        isRuning= false;
    }
}


