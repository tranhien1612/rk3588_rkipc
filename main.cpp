#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include "rk3588.h"

#include <rk_aiq_user_api2_camgroup.h>
#include <rk_aiq_user_api2_imgproc.h>
#include <rk_aiq_user_api2_sysctl.h>

rtsp_demo_handle g_rtsplive1 = NULL;
static rtsp_session_handle g_rtsp_session1;

static volatile bool g_running = true;
void handle_sigint(int sig) {
    (void)sig;
    printf("\nCaught SIGINT (Ctrl + C), exiting...\n");
    g_running = false;
}

rk_aiq_sys_ctx_t * aiq_ctx;

static void *stream_handle(void *arg) {
	(void)arg;
	LOG_INFO("Start Stream Handle Function\n");
	int s32Ret;

	VENC_STREAM_S stFrame;
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));

	while (1) {
		s32Ret = RK_MPI_VENC_GetStream(1, &stFrame, 2500);
		if (s32Ret == RK_SUCCESS) {
			void* pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
			rtsp_tx_video(g_rtsp_session1,(uint8_t *)pData, stFrame.pstPack->u32Len,
							stFrame.pstPack->u64PTS);
			rtsp_do_event(g_rtsplive1);

			RK_U64 nowUs = RK3588_GetNowUs();
			// LOG_INFO("chn:1, enc->seq:%d wd:%d pts=%ld delay=%ldus\n",
			// 		stFrame.u32Seq, stFrame.pstPack->u32Len,
			// 		stFrame.pstPack->u64PTS, nowUs - stFrame.pstPack->u64PTS);
			s32Ret = RK_MPI_VENC_ReleaseStream(1, &stFrame);
			if (s32Ret != RK_SUCCESS) {
				LOG_ERROR("RK_MPI_VENC_ReleaseStream fail, ret=%d\n", s32Ret);
			}
		}

		s32Ret = RK_MPI_VENC_GetStream(2, &stFrame, 2500);
		if (s32Ret == RK_SUCCESS) {
			void* pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
			RK_U64 nowUs = RK3588_GetNowUs();
			// LOG_INFO("chn:2, enc->seq:%d wd:%d pts=%ld delay=%ldus\n",
			// 		stFrame.u32Seq, stFrame.pstPack->u32Len,
			// 		stFrame.pstPack->u64PTS, nowUs - stFrame.pstPack->u64PTS);

			s32Ret = RK_MPI_VENC_ReleaseStream(2, &stFrame);
			if (s32Ret != RK_SUCCESS) {
				LOG_ERROR("RK_MPI_VENC_ReleaseStream fail, ret=%d\n", s32Ret);
			}
		}
		usleep(10 * 1000);
	}

	LOG_INFO("exit\n");

	free(stFrame.pstPack);
	return NULL;
}

static void *pic_handle(void *arg) {
	(void)arg;
	LOG_INFO("Start Pic Handle Function\n");
	int s32Ret;

	VENC_STREAM_S stFrame;
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));

	char file_name[128] = {0};

	while (1) {
		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, 1000);
		if (s32Ret == RK_SUCCESS) {

			void* pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
			RK_U64 nowUs = RK3588_GetNowUs();
			LOG_INFO("chn:0, enc->seq:%d wd:%d pts=%ld delay=%ldus\n",
					stFrame.u32Seq, stFrame.pstPack->u32Len,
					stFrame.pstPack->u64PTS, nowUs - stFrame.pstPack->u64PTS);

			// time_t t = time(NULL);
			// struct tm tm = *localtime(&t);
			// snprintf(file_name, 128, "%d%02d%02d%02d%02d%02d.jpeg", tm.tm_year + 1900,
			//          tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
			// LOG_INFO("file_name is %s\n", file_name);
			// FILE *fp = fopen(file_name, "wb");
			// fwrite(pData, 1, stFrame.pstPack->u32Len, fp);
			// fflush(fp);
			// fclose(fp);

			s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
			if (s32Ret != RK_SUCCESS) {
				LOG_ERROR("RK_MPI_VENC_ReleaseStream fail, ret=%d\n", s32Ret);
			}
		}

		usleep(10 * 1000);
	}

	LOG_INFO("exit\n");

	free(stFrame.pstPack);
	return NULL;
}

static void *keyboard_handle(void *arg) {
    (void)arg;
    LOG_INFO("Keyboard input thread started. Press 'q' to quit.\n");

    // Set terminal to raw mode (no need to press Enter)
    // system("stty -echo -icanon");
	int8_t zoomLevel = 1;
	uint8_t brightnessLevel = 1;
    while (1) {
        int c = getchar();
		if (c == 'a' || c == 'A'){
			zoomLevel += 300;
			rk3588_GrpSetZoom(zoomLevel);
		}
		else if (c == 'b' || c == 'B'){
			rk3588_SetChnRotation(VPSS_CHN1, ROTATION_90);
		}
		else if (c == 'c' || c == 'C'){
			brightnessLevel += 10;
			rk3588_SetBrightness(aiq_ctx, brightnessLevel);
		}
		
        else if (c == 'q' || c == 'Q') {
            printf("\n'q' pressed — exiting...\n");
            g_running = false;
            break;
        }
        usleep(100000); // avoid busy loop
    }

    // Restore terminal settings
    system("stty sane");
    LOG_INFO("Keyboard thread exiting.\n");
    return NULL;
}

void rtsp_init(){
    g_rtsplive1 = create_rtsp_demo(554);
    g_rtsp_session1 = rtsp_new_session(g_rtsplive1, "/live/0");
    rtsp_set_video(g_rtsp_session1, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session1, rtsp_get_reltime(), rtsp_get_ntptime());
}

int rkaiq_init(){
	RK_BOOL multi_sensor = RK_FALSE;	
	const char *iq_dir = "/etc/iqfiles";
    int camId = 0;
	rk_aiq_static_info_t aiq_static_info;
	rk_aiq_uapi2_sysctl_enumStaticMetasByPhyId(camId, &aiq_static_info);
    printf("ID: %d, sensor_name is %s, iqfiles is %s\n", camId, aiq_static_info.sensor_info.sensor_name, iq_dir);

    rk_aiq_uapi2_sysctl_preInit_scene(aiq_static_info.sensor_info.sensor_name, "normal", "day");
    aiq_ctx = rk_aiq_uapi2_sysctl_init(aiq_static_info.sensor_info.sensor_name, iq_dir, NULL, NULL);

    if (rk_aiq_uapi2_sysctl_prepare(aiq_ctx, 0, 0, RK_AIQ_WORKING_MODE_NORMAL)) {
		printf("rkaiq engine prepare failed !\n");
		return -1;
	}
	if (rk_aiq_uapi2_sysctl_start(aiq_ctx)) {
		printf("rk_aiq_uapi2_sysctl_start  failed\n");
		return -1;
	}
	printf("rk_aiq_uapi2_sysctl_start succeed\n");
    return 0;
}

/*
							  |--> VPSSChannel0 --> VENC[0]
Sensor --> VI --> VPSSGroup --|--> VPSSChannel1 --> VENC[1]
							  |--> VPSSChannel2 --> VENC[2]
							  |--> VPSSChannel3 --> VENC[3]

gst-launch-1.0 videotestsrc is-live=true ! videoconvert ! x264enc tune=zerolatency bitrate=500 speed-preset=ultrafast ! rtph264pay config-interval=1 pt=96 ! udpsink host=192.168.1.70 port=5000

*/

int main(){
	signal(SIGINT, handle_sigint);
    rkaiq_init();

    // rtsp init	
	rtsp_init();

    if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		LOG_ERROR("RK_MPI_SYS_Init fail!\n");
		return -1;
	}

    // vi_init
    vi_dev_init();
    vi_chn_init(0, 4224, 3136); //mainpath
	// vpss
	vpss_group_init(4224, 3136);
	// vpss channel
	vpss_channel_init(VPSS_CHN0, 4224, 3136); //vpssChannelId: 0
	vpss_channel_init(VPSS_CHN1, 1920, 1088); //vpssChannelId: 1
	vpss_channel_init(VPSS_CHN2, 1920, 1088); //vpssChannelId: 2
	
	vpss_start();
	// venc
	venc_init(0, 4224, 3136, RK_VIDEO_ID_AVC);//photo //RK_VIDEO_ID_JPEG
	venc_init(1, 1920, 1088, RK_VIDEO_ID_AVC); //stream
	venc_init(2, 1920, 1088, RK_VIDEO_ID_AVC); //video
	
    // bind vi to vpss
	rk3588_bind(RK_ID_VI, RK_ID_VPSS, 0, 0);
	// bind vpss to venc
	rk3588_bind(RK_ID_VPSS, RK_ID_VENC, 0, 0);
	rk3588_bind(RK_ID_VPSS, RK_ID_VENC, 1, 1);
	rk3588_bind(RK_ID_VPSS, RK_ID_VENC, 2, 2);

	pthread_t stream_thread;
	pthread_create(&stream_thread, NULL, stream_handle, NULL);

	pthread_t pic_thread;
	pthread_create(&pic_thread, NULL, pic_handle, NULL);

	pthread_t key_thread;
	pthread_create(&key_thread, NULL, keyboard_handle, NULL);

    while (g_running) {	
		usleep(10000);
	}

	rk3588_unbind(RK_ID_VI, RK_ID_VPSS, 0, 0);
	rk3588_unbind(RK_ID_VPSS, RK_ID_VENC, 0, 0);
	rk3588_unbind(RK_ID_VPSS, RK_ID_VENC, 1, 1);
	rk3588_unbind(RK_ID_VPSS, RK_ID_VENC, 2, 2);

	vpss_deinit();
	venc_deinit(0);
	venc_deinit(1);
	venc_deinit(2);
	vi_dev_deinit();

	RK_MPI_SYS_Exit();

    return 0;
}