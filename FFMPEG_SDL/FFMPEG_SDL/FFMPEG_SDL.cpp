#include "stdafx.h"
#include <stdio.h>
#define __STD_CONSTANT_MACROS

extern "C"
{
	#include <libavutil/opt.h>
	#include "libavcodec/avcodec.h"     //包含解码器信息
	#include "libavformat/avformat.h"	//包含封装信息
	#include "libswscale/swscale.h"     //处理每一帧图像数据
	#include "SDL2/SDL.h"
}

//SDL2.0
#define REFRESH_EVENT  (SDL_USEREVENT + 1)
#define BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit = 0;     //控制线程输出
int flag = 0;           //控制暂停标志位

//线程函数，实现视频的显示
int refresh_video(void *opaque){
	thread_exit = 0;
	while (thread_exit == 0) {
		if (flag == 0){
			SDL_Event event;
			event.type = REFRESH_EVENT;
			SDL_PushEvent(&event);
			SDL_Delay(20);
		}
	}
	thread_exit = 0;
	//Break
	SDL_Event event;
	event.type = BREAK_EVENT;
	SDL_PushEvent(&event);
	return 0;
}



int _tmain(int argc, char* argv[])
{
	AVFormatContext *pFormatCtx;
	char *filepath = "Titanic.ts";
	/*char filepath[20];*/
	//strcpy(filepath, argv[1]);

	//char *filepath=argv[1];
	av_register_all();    //注册所有组件
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();   //开辟内存
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) < 0) //打开输入视频文件
	{
		printf("Can't open the input stream.\n");
		return -1;
	}
	if (avformat_find_stream_info(pFormatCtx, NULL)<0)     //判断文件流，视频流还是音频流
	{
		printf("Can't find the stream information!\n");
		return -1;
	}

	//判断视频流数据
	int i, index_video = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)      //如果是视频流，则记录存储。
		{
			index_video = i;
			break;
		}
	}
	if (index_video == -1)
	{
		printf("Can't find a video stream;\n");
		return -1;
	}

	//查找解码器信息
	AVCodecContext *pCodecCtx = pFormatCtx->streams[index_video]->codec;
	AVCodec *pCodec = avcodec_find_decoder(pCodecCtx->codec_id);     //查找解码器
	if (pCodec == NULL)
	{
		printf("Can't find a decoder!\n");
		return -1;
	}

	//打开编码器
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)   
	{
		printf("Can't open the decoder!\n");
		return -1;
	}

	//存储解码后数据
	AVFrame *pFrame = av_frame_alloc();  //this only allocates the AVFrame itself, not the data buffers
	AVFrame *pFrameYUV = av_frame_alloc();
	uint8_t *out_buffer = (uint8_t *)av_malloc(avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height));  //开辟缓冲区
	avpicture_fill((AVPicture *)pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height);//帧和申请的内存结合
	
	//存储解码前数据
	AVPacket *pkt = (AVPacket *)av_malloc(sizeof(AVPacket));;
	av_init_packet(pkt);
	SwsContext * img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
	
	//SDL2.0
	int screen_w = 500, screen_h = 500;
	if (SDL_Init(SDL_INIT_VIDEO))
	{
		printf("Can't initialize SDL - %s\n", SDL_GetError());
	}
	SDL_Window *screen = SDL_CreateWindow("SDL EVENT TEST", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screen_w, screen_h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE); //创建显示窗口
	if (screen == NULL)
	{
		printf("Can't creat a window:%s\n", SDL_GetError());
		return -1;
	}

	SDL_Renderer *sdlrenderer = SDL_CreateRenderer(screen, -1, 0);//创建渲染器
	SDL_Texture *sdltexture = SDL_CreateTexture(sdlrenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);//创建纹理

	SDL_Thread  *refresh_thread = SDL_CreateThread(refresh_video, NULL, NULL);
	SDL_Rect sdlrect;
	SDL_Event event;

	int frame_cnt = 0;
	int get_frame;
	int flag = 1;
	while (1)
	{
		if (flag == 1)
		{
			SDL_WaitEvent(&event);
			switch (event.type)
			{
			case REFRESH_EVENT: {
				while (1)
				{
					if (av_read_frame(pFormatCtx, pkt) < 0)
					{
						flag = 0;
						thread_exit = 1;
					}
					if(pkt->stream_index == index_video)
						break;


				}
				/*if (av_read_frame(pFormatCtx, pkt) >= 0)
				{*/
					if (pkt->stream_index == index_video)
					{
						if (avcodec_decode_video2(pCodecCtx, pFrame, &get_frame, pkt) < 0)
						{
							printf("Decode Error!\n");
							return -1;
						}
						if (get_frame)
						{
							printf("Decoded frame index: %d\n", frame_cnt);
							sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
								pFrameYUV->data, pFrameYUV->linesize);

							SDL_UpdateTexture(sdltexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]); //设置纹理的数据

							//设置窗口大小
							sdlrect.x = 0;
							sdlrect.y = 0;
							sdlrect.w = screen_w;
							sdlrect.h = screen_h;

							SDL_RenderCopy(sdlrenderer, sdltexture, NULL, &sdlrect); //复制纹理信息到渲染器目标
							SDL_RenderPresent(sdlrenderer);
							frame_cnt++;
						}

					}
					av_free_packet(pkt);
				//}
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_SPACE)
				{
					flag = !flag;      //取反
					printf("key %s down！\n", SDL_GetKeyName(event.key.keysym.sym));
				}
				break;
			case SDL_WINDOWEVENT:
				SDL_GetWindowSize(screen, &screen_w, &screen_h);
				break;
			case SDL_QUIT:
				thread_exit = 1;
				break;
			case BREAK_EVENT:
				SDL_Quit();
				flag = 0;
				break;
			}

			}
		}
		else
			break;
	}
	//free
	sws_freeContext(img_convert_ctx);
	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	avformat_free_context(pFormatCtx);

	return 0;
}

