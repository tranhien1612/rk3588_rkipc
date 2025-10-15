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

#include "rk_debug.h"
#include "rk_defines.h"
#include "rk_mpi_adec.h"
#include "rk_mpi_aenc.h"
#include "rk_mpi_ai.h"
#include "rk_mpi_ao.h"
#include "rk_mpi_avs.h"
#include "rk_mpi_cal.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_rgn.h"
#include "rk_mpi_sys.h"
#include "rk_mpi_tde.h"
#include "rk_mpi_vdec.h"
#include "rk_mpi_venc.h"
#include "rk_mpi_vi.h"
#include "rk_mpi_vo.h"
#include "rk_mpi_vpss.h"

#include <rk_aiq_user_api2_acsm.h>
#include <rk_aiq_user_api2_ae.h>
#include <rk_aiq_user_api2_camgroup.h>
#include <rk_aiq_user_api2_imgproc.h>
#include <rk_aiq_user_api2_sysctl.h>

#include "rtsp_demo.h"

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

#define LOG_INFO(format, ...)                                                                           \
	do {                                                                                                \
		fprintf(stderr, COLOR_GREEN "[INFO][%s]: " format COLOR_RESET, __FUNCTION__, ##__VA_ARGS__);    \
	} while (0)

#define LOG_WARN(format, ...)                                                                           \
	do {                                                                                                \
		fprintf(stderr, COLOR_YELLOW "[WARN][%s]: " format COLOR_RESET, __FUNCTION__, ##__VA_ARGS__);   \
	} while (0)

#define LOG_ERROR(format, ...)                                                                          \
	do {                                                                                                \
		fprintf(stderr, COLOR_RED "[ERROR][%s]: " format COLOR_RESET, __FUNCTION__, ##__VA_ARGS__);     \
	} while (0)

#define LOG_DEBUG(format, ...)                                                                          \
	do {                                                                                                \
		fprintf(stderr, COLOR_CYAN "[DEBUG][%s]: " format COLOR_RESET, __FUNCTION__, ##__VA_ARGS__);    \
	} while (0)


int test_stream();

RK_U64 RK3588_GetNowUs();

int vi_dev_init();
int vi_dev_deinit();

int vi_chn_init(int channelId, int width, int height);
int vi_chn_deinit(int channelId);

int venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType);
int venc_deinit(int chnId);

int vpss_group_init(int width, int height);
int vpss_channel_init(int vpssChnId, int width, int height);
int vpss_start();
int vpss_deinit();

int rk3588_bind(MOD_ID_E srcModId, MOD_ID_E dstModId, int srcChnId, int dstChnId);
int rk3588_unbind(MOD_ID_E srcModId, MOD_ID_E dstModId, int srcChnId, int dstChnId);

int rk3588_GrpSetZoom(RK_U32 u32Zoom);
int rk3588_SetChnRotation(VPSS_CHN VpssChn, ROTATION_E enRotation);
int rk3588_SetBrightness(const rk_aiq_sys_ctx_t* sys_ctx, int value);
int rk3588_SetContrast(const rk_aiq_sys_ctx_t* sys_ctx, int value);
