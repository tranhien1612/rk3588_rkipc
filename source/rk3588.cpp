#include "rk3588.h"

int test_stream(){
	// rtsp
	rtsp_demo_handle g_rtsplive = create_rtsp_demo(554);
    static rtsp_session_handle g_rtsp_session = rtsp_new_session(g_rtsplive, "/live/0");
    rtsp_set_video(g_rtsp_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
    rtsp_sync_video_ts(g_rtsp_session, rtsp_get_reltime(), rtsp_get_ntptime());

	// MPI
	if (RK_MPI_SYS_Init() != RK_SUCCESS) {
		LOG_ERROR("rk mpi sys init fail!\n");
		return -1;
	}

	RK_S32 s32Ret;
	MPP_CHN_S stSrcChn1, stvencChn;
	vi_dev_init();
    vi_chn_init(1, 1920, 1080);
	venc_init(0, 1920, 1080, RK_VIDEO_ID_AVC);

	// bind vi to venc
	rk3588_bind(RK_ID_VI, RK_ID_VENC, 1, 0);

	// stSrcChn1.enModId = RK_ID_VI;
	// stSrcChn1.s32DevId = 0;
	// stSrcChn1.s32ChnId = 1;
		
	// stvencChn.enModId = RK_ID_VENC;
	// stvencChn.s32DevId = 0;
	// stvencChn.s32ChnId = 0;
	// s32Ret = RK_MPI_SYS_Bind(&stSrcChn1, &stvencChn);
	// if (s32Ret != RK_SUCCESS) {
	// 	LOG_ERROR("Bind VI to VENC fail, ret=%d\n", s32Ret);
	// 	return -1;
	// }
	// LOG_INFO("Bind VI to VENC success\n");

	VENC_STREAM_S stFrame;
	stFrame.pstPack = (VENC_PACK_S *)malloc(sizeof(VENC_PACK_S));
	while (1) {	
		s32Ret = RK_MPI_VENC_GetStream(0, &stFrame, 2500);
		if (s32Ret == RK_SUCCESS) {
			void* pData = RK_MPI_MB_Handle2VirAddr(stFrame.pstPack->pMbBlk);
			rtsp_tx_video(g_rtsp_session,(uint8_t *)pData, stFrame.pstPack->u32Len,
							stFrame.pstPack->u64PTS);
			rtsp_do_event(g_rtsplive);

			s32Ret = RK_MPI_VENC_ReleaseStream(0, &stFrame);
		}
		
		usleep(10000);
	}
	free(stFrame.pstPack);
	return 0;
}

RK_U64 RK3588_GetNowUs() {
	struct timespec time = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &time);
	return (RK_U64)time.tv_sec * 1000000 + (RK_U64)time.tv_nsec / 1000; /* microseconds */
}

int vi_dev_init(){
	LOG_INFO("Start init VI Dev\n");
    int ret = 0;
	int devId = 0;
	int pipeId = devId;

    VI_DEV_ATTR_S stDevAttr;
	VI_DEV_BIND_PIPE_S stBindPipe;
	memset(&stDevAttr, 0, sizeof(stDevAttr));
	memset(&stBindPipe, 0, sizeof(stBindPipe));

    // 0. get dev config status
	ret = RK_MPI_VI_GetDevAttr(devId, &stDevAttr);
	if (ret == RK_ERR_VI_NOT_CONFIG) {
		// 0-1.config dev
		ret = RK_MPI_VI_SetDevAttr(devId, &stDevAttr);
		if (ret != RK_SUCCESS) {
			LOG_ERROR("RK_MPI_VI_SetDevAttr, ret=%d\n", ret);
			return -1;
		}
	} else {
		LOG_INFO("RK_MPI_VI_SetDevAttr already\n");
	}

    // 1.get dev enable status
	ret = RK_MPI_VI_GetDevIsEnable(devId);
	if (ret != RK_SUCCESS) {
		// 1-2.enable dev
		ret = RK_MPI_VI_EnableDev(devId);
		if (ret != RK_SUCCESS) {
			LOG_ERROR("RK_MPI_VI_EnableDev, ret=%d\n", ret);
			return -1;
		}
		// 1-3.bind dev/pipe
		stBindPipe.u32Num = pipeId;
		stBindPipe.PipeId[0] = pipeId;
		ret = RK_MPI_VI_SetDevBindPipe(devId, &stBindPipe);
		if (ret != RK_SUCCESS) {
			LOG_ERROR("RK_MPI_VI_SetDevBindPipe, ret=%d\n", ret);
			return -1;
		}
	} else {
		LOG_INFO("RK_MPI_VI_EnableDev already\n");
	}

	return 0;
}

int vi_dev_deinit(){
	int devId = 0;
	int ret = RK_MPI_VI_DisableDev(devId);
	if (ret)
		LOG_ERROR("VI DEV Deinit fail, ret=%d\n", ret);
	else
		LOG_INFO("VI DEV Deinit success\n");
	return ret;
}

int vi_chn_init(int channelId, int width, int height) {
    LOG_INFO("Start init VI Channel[%d]\n", channelId);
	int ret;
	// VI init
	VI_CHN_ATTR_S vi_chn_attr;
	memset(&vi_chn_attr, 0, sizeof(vi_chn_attr));

	vi_chn_attr.stIspOpt.u32BufCount = 6; //3;
	vi_chn_attr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF; // VI_V4L2_MEMORY_TYPE_MMAP;
	vi_chn_attr.stIspOpt.enCaptureType = VI_V4L2_CAPTURE_TYPE_VIDEO_CAPTURE;
	vi_chn_attr.stSize.u32Width = width;
	vi_chn_attr.stSize.u32Height = height;
	vi_chn_attr.enPixelFormat = RK_FMT_YUV420SP;
	vi_chn_attr.enCompressMode = COMPRESS_MODE_NONE; // COMPRESS_AFBC_16x16;
	vi_chn_attr.u32Depth = 2;//1; 
	
	ret = RK_MPI_VI_SetChnAttr(0, channelId, &vi_chn_attr);
	ret |= RK_MPI_VI_EnableChn(0, channelId);
	if (ret) {
		LOG_ERROR("init VI Channel[%d] fail, ret=%d\n", channelId, ret);
		return ret;
	}

	return ret;
}

int vi_chn_deinit(int channelId) {
	int ret = RK_MPI_VI_DisableChn(0, channelId); //viId, viChannelId
	if (ret)
		LOG_ERROR("VI Channel[%d] Deinit fail, ret=%d\n", channelId, ret);
	else
		LOG_INFO("VI Channel[%d] Deinit success\n", channelId);
	return ret;
}

// VPSS
int vpss_group_init(int width, int height) {
	LOG_INFO("Start init VPSS Group\n");
	int s32GrpId = 0;
	VPSS_GRP_ATTR_S stGrpVpssAttr;
	memset(&stGrpVpssAttr, 0, sizeof(stGrpVpssAttr));
	stGrpVpssAttr.u32MaxW = 8192; //4096 //8192
	stGrpVpssAttr.u32MaxH = 8192;
	stGrpVpssAttr.enPixelFormat = RK_FMT_YUV420SP;
	stGrpVpssAttr.stFrameRate.s32SrcFrameRate = -1;
	stGrpVpssAttr.stFrameRate.s32DstFrameRate = -1;
	stGrpVpssAttr.enCompressMode = COMPRESS_MODE_NONE; //COMPRESS_AFBC_16x16 //COMPRESS_MODE_NONE

	int ret = RK_MPI_VPSS_CreateGrp(s32GrpId, &stGrpVpssAttr);
	if (ret != RK_SUCCESS) {
		LOG_ERROR("RK_MPI_VPSS_CreateGrp[%d] fail, ret=%d\n", s32GrpId, ret);
		return ret;
	}

	return ret;
}

int vpss_channel_init(int vpssChnId, int width, int height) {
	LOG_INFO("Start init VPSS Channel[%d]\n", vpssChnId);
	int s32GrpId = 0;
	VPSS_CHN_ATTR_S stVpssChnAttr;
	memset(&stVpssChnAttr, 0, sizeof(stVpssChnAttr));
	stVpssChnAttr.enChnMode = VPSS_CHN_MODE_USER;
	stVpssChnAttr.enDynamicRange = DYNAMIC_RANGE_SDR8;
	stVpssChnAttr.enPixelFormat = RK_FMT_YUV420SP;//RK_FMT_RGB888;
	stVpssChnAttr.stFrameRate.s32SrcFrameRate = -1;
	stVpssChnAttr.stFrameRate.s32DstFrameRate = -1;
	stVpssChnAttr.u32Width = width;
	stVpssChnAttr.u32Height = height;
	stVpssChnAttr.enCompressMode = COMPRESS_MODE_NONE; //COMPRESS_AFBC_16x16 //COMPRESS_MODE_NONE

	int ret = RK_MPI_VPSS_SetChnAttr(s32GrpId, vpssChnId, &stVpssChnAttr);
	if (ret != RK_SUCCESS) {
		LOG_ERROR("RK_MPI_VPSS_SetChnAttr[%d] fail, ret=%d\n", vpssChnId, ret);
		return ret;
	}
	ret = RK_MPI_VPSS_EnableChn(s32GrpId, vpssChnId);
	if (ret != RK_SUCCESS) {
		LOG_ERROR("RK_MPI_VPSS_EnableChn[%d] fail, ret=%d\n", vpssChnId, ret);
		return ret;
	}

	return ret;
}

int vpss_start(){
	int s32GrpId = 0;
	int ret = RK_MPI_VPSS_StartGrp(s32GrpId);
	if (ret != RK_SUCCESS) {
		LOG_ERROR("RK_MPI_VPSS_StartGrp[%d] fail, ret=%d\n", s32GrpId, ret);
		return ret;
	}

	ret = RK_MPI_VPSS_SetVProcDev(s32GrpId, (VIDEO_PROC_DEV_TYPE_E)VIDEO_PROC_DEV_RGA);
    if (ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VPSS_SetVProcDev[%d] fail, ret=%d!", s32GrpId, ret);
        return ret;
    }
	return ret;
}

int vpss_deinit(){
	int s32GrpId = 0;
	int ret = RK_MPI_VPSS_StopGrp(s32GrpId);

	ret |= RK_MPI_VPSS_DisableChn(s32GrpId, VPSS_CHN0);
	if (ret)
		LOG_ERROR("VPSS Channle[%d] Deinit fail, ret=%d\n", VPSS_CHN0, ret);
	else
		LOG_INFO("VPSS Channle[%d] Deinit success\n", VPSS_CHN0);

	ret |= RK_MPI_VPSS_DisableChn(s32GrpId, VPSS_CHN1);
	if (ret)
		LOG_ERROR("VPSS Channle[%d] Deinit fail, ret=%d\n", VPSS_CHN1, ret);
	else
		LOG_INFO("VPSS Channle[%d] Deinit success\n", VPSS_CHN1);

	ret |= RK_MPI_VPSS_DisableChn(s32GrpId, VPSS_CHN2);
	if (ret)
		LOG_ERROR("VPSS Channle[%d] Deinit fail, ret=%d\n", VPSS_CHN2, ret);
	else
		LOG_INFO("VPSS Channle[%d] Deinit success\n", VPSS_CHN2);

	// ret |= RK_MPI_VPSS_DisableChn(s32GrpId, VPSS_CHN3);
	ret |= RK_MPI_VPSS_DestroyGrp(s32GrpId);
	if (ret)
		LOG_ERROR("VPSS Group[%d] Deinit fail, ret=%d\n", s32GrpId, ret);
	else
		LOG_INFO("VPSS Group[%d] Deinit success\n", s32GrpId);
	return ret;
}

int venc_init(int chnId, int width, int height, RK_CODEC_ID_E enType) {
	LOG_INFO("Start init VENC Channel[%d]\n", chnId);
	VENC_CHN_ATTR_S stAttr;
	memset(&stAttr, 0, sizeof(VENC_CHN_ATTR_S));

	// RTSP H264	
	stAttr.stVencAttr.enType = enType;
	stAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
	// stAttr.stVencAttr.u32Profile = H264E_PROFILE_MAIN;
	stAttr.stVencAttr.u32PicWidth = width;
	stAttr.stVencAttr.u32PicHeight = height;
	stAttr.stVencAttr.u32VirWidth = width;
	stAttr.stVencAttr.u32VirHeight = height;
	stAttr.stVencAttr.u32StreamBufCnt = 2; //5
	stAttr.stVencAttr.u32BufSize = width * height * 3 / 2;
	stAttr.stVencAttr.enMirror = MIRROR_NONE;
		
	stAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
	stAttr.stRcAttr.stH264Cbr.u32BitRate = 3 * 1024;
	stAttr.stRcAttr.stH264Cbr.u32Gop = 1;
	int ret = RK_MPI_VENC_CreateChn(chnId, &stAttr);
	if (ret) {
		LOG_ERROR("RK_MPI_VENC_CreateChn[%d] faild, ret=%d\n", chnId, ret);
		return -1;
	}

	if(enType == RK_VIDEO_ID_AVC){ //Video
		VENC_RC_PARAM_S venc_rc_param;
		RK_MPI_VENC_GetRcParam(chnId, &venc_rc_param);
		venc_rc_param.stParamH264.u32MinQp = 20;
		RK_MPI_VENC_SetRcParam(chnId, &venc_rc_param);

		VENC_RECV_PIC_PARAM_S stRecvParam;
		memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
		stRecvParam.s32RecvPicNum = -1;
		RK_MPI_VENC_StartRecvFrame(chnId, &stRecvParam);
	}else if(enType == RK_VIDEO_ID_JPEG){ //Image
		VENC_JPEG_PARAM_S stJpegParam;
		memset(&stJpegParam, 0, sizeof(stJpegParam));
		stJpegParam.u32Qfactor = 95;
		RK_MPI_VENC_SetJpegParam(chnId, &stJpegParam);
		VENC_RECV_PIC_PARAM_S stRecvParam;
		memset(&stRecvParam, 0, sizeof(VENC_RECV_PIC_PARAM_S));
		stRecvParam.s32RecvPicNum = 1;
		RK_MPI_VENC_StartRecvFrame(chnId, &stRecvParam);
		RK_MPI_VENC_StopRecvFrame(chnId);
	}

	return 0;
}

int venc_deinit(int chnId){
	int ret = 0;
	ret = RK_MPI_VENC_StopRecvFrame(chnId);
	ret |= RK_MPI_VENC_DestroyChn(chnId);
	if (ret)
		LOG_ERROR("VENC Channel[%d] Deinit fail, ret=%d\n", chnId, ret);
	else
		LOG_INFO("VENC Channel[%d] Deinit success\n", chnId);

	return ret;
}

int rk3588_bind(MOD_ID_E srcModId, MOD_ID_E dstModId, int srcChnId, int dstChnId){
	//RK_ID_VI(8), RK_ID_VENC(4), RK_ID_VPSS(6)
	int srcDevId = 0;
	int dstDevId = 0;

	MPP_CHN_S stSrc, stDst;
	stSrc.enModId = srcModId;
	stSrc.s32DevId = srcDevId;
	stSrc.s32ChnId = srcChnId;
	stDst.enModId = dstModId;
	stDst.s32DevId = dstDevId;
	stDst.s32ChnId = dstChnId;
	RK_S32 ret = RK_MPI_SYS_Bind(&stSrc, &stDst);
	if (ret != RK_SUCCESS) {
		LOG_ERROR("Bind ModId:%d (devId:%d, chnId:%d) →→→ ModId:%d (devId:%d, chnId:%d) fail, ret=%d\n", 
			srcModId, srcDevId, srcChnId, dstModId, dstDevId, dstChnId, ret);
		return -1;
	}
	LOG_INFO("Bind ModId:%d (devId:%d, chnId:%d) →→→ ModId:%d (devId:%d, chnId:%d) successful\n", 
		srcModId, srcDevId, srcChnId, dstModId, dstDevId, dstChnId);
	return ret;
}

int rk3588_unbind(MOD_ID_E srcModId, MOD_ID_E dstModId, int srcChnId, int dstChnId){
	//RK_ID_VI(8), RK_ID_VENC(4), RK_ID_VPSS(6)
	int srcDevId = 0;
	int dstDevId = 0;

	MPP_CHN_S stSrc, stDst;
	stSrc.enModId = srcModId;
	stSrc.s32DevId = srcDevId;
	stSrc.s32ChnId = srcChnId;
	stDst.enModId = dstModId;
	stDst.s32DevId = dstDevId;
	stDst.s32ChnId = dstChnId;
	RK_S32 ret = RK_MPI_SYS_UnBind(&stSrc, &stDst);
	if (ret != RK_SUCCESS) {
		LOG_ERROR("UnBind ModId:%d (devId:%d, chnId:%d) →→→ ModId:%d (devId:%d, chnId:%d) fail, ret=%d\n", 
			srcModId, srcDevId, srcChnId, dstModId, dstDevId, dstChnId, ret);
		return -1;
	}
	LOG_INFO("UnBind ModId:%d (devId:%d, chnId:%d) →→→ ModId:%d (devId:%d, chnId:%d) successful\n", 
		srcModId, srcDevId, srcChnId, dstModId, dstDevId, dstChnId);
	return ret;
}


/* --------------------------- Control ---------------------------*/
int rk3588_GrpSetZoom(RK_U32 u32Zoom) { // range 1 - 1000
	int s32GrpId = 0;
    int s32Ret = RK_SUCCESS;
    VPSS_CROP_INFO_S stCropInfo;

    s32Ret = RK_MPI_VPSS_GetGrpCrop(s32GrpId, &stCropInfo);
    if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VPSS_GetGrpCrop failed, ret=%d\n", s32Ret);
        return s32Ret;
    }

	uint8_t level = 2; // 1, 2, 4, 8, 16
	int width = 1920;
	int height = 1080;

    stCropInfo.bEnable = RK_TRUE;;
    stCropInfo.enCropCoordinate = VPSS_CROP_RATIO_COOR;//VPSS_CROP_RATIO_COOR; //VPSS_CROP_ABS_COOR
    stCropInfo.stCropRect.s32X = width / (level);//480;//500 - u32Zoom / 2;
    stCropInfo.stCropRect.s32Y = height / (level);//270;//500 - u32Zoom / 2;
    stCropInfo.stCropRect.u32Width = width / (level);
    stCropInfo.stCropRect.u32Height = height / (level);


    s32Ret = RK_MPI_VPSS_SetGrpCrop(s32GrpId, &stCropInfo);
    if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VPSS_SetGrpCrop failed, ret=%d\n", s32Ret);
        return s32Ret;
    }

    return s32Ret;
}

int rk3588_SetChnRotation(VPSS_CHN VpssChn, ROTATION_E enRotation) { //(ROTATION_E)pstCtx->s32Rotation
	int s32GrpId = 0;
    RK_S32 s32Ret = RK_SUCCESS;
    ROTATION_E rotation = ROTATION_0;

    s32Ret = RK_MPI_VPSS_GetChnRotation(s32GrpId, VpssChn, &rotation);
    if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VPSS_GetChnRotation failed, ret=%d\n", s32Ret);
        return s32Ret;
    }
    s32Ret = RK_MPI_VPSS_SetChnRotation(s32GrpId, VpssChn, enRotation);
    if (s32Ret != RK_SUCCESS) {
        LOG_ERROR("RK_MPI_VPSS_SetChnRotation, ret=%d\n", s32Ret);
        return s32Ret;
    }

    return s32Ret;
}

int rk3588_SetBrightness(const rk_aiq_sys_ctx_t* sys_ctx, int value) {
	if(value <= 0) value = 0;
	else if(value >= 100) value = 100;

	acp_attrib_t attrib;
	int ret = rk_aiq_user_api2_acp_GetAttrib(sys_ctx, &attrib);
	attrib.brightness = value * 2.55; // value[0,255]
	ret |= rk_aiq_user_api2_acp_SetAttrib(sys_ctx, &attrib);
	if(!ret){
		LOG_INFO("Set Brightness Level: %d success.\n", value);
	}else{
		LOG_ERROR("Set Brightness Level: %d fail, ret=%d\n", value, ret);
	}

	return ret;
}

int rk3588_SetContrast(const rk_aiq_sys_ctx_t* sys_ctx, int value) {
	if(value <= 0) value = 0;
	else if(value >= 100) value = 100;

	acp_attrib_t attrib;
	int ret = rk_aiq_user_api2_acp_GetAttrib(sys_ctx, &attrib);
	attrib.contrast = value * 2.55; // value[0,255]
	ret |= rk_aiq_user_api2_acp_SetAttrib(sys_ctx, &attrib);
	if(!ret){
		LOG_INFO("Set Contrast Level: %d success.\n", value);
	}else{
		LOG_ERROR("Set Contrast Level: %d fail, ret=%d\n", value, ret);
	}

	return ret;
}

int rk3488_SetSaturation(const rk_aiq_sys_ctx_t* sys_ctx, int value) {
	if(value <= 0) value = 0;
	else if(value >= 100) value = 100;

	acp_attrib_t attrib;
	int ret = rk_aiq_user_api2_acp_GetAttrib(sys_ctx, &attrib);
	attrib.saturation = value * 2.55; // value[0,255]
	ret |= rk_aiq_user_api2_acp_SetAttrib(sys_ctx, &attrib);
	if(!ret){
		LOG_INFO("Set Saturation Level: %d success.\n", value);
	}else{
		LOG_ERROR("Set Saturation Level: %d fail, ret=%d\n", value, ret);
	}

	return ret;
}

int rk3588_SetSharpness(const rk_aiq_sys_ctx_t* sys_ctx, int value) {
	if(value <= 0) value = 0;
	else if(value >= 100) value = 100;

	float fPercent = 0.0f;
	fPercent = value / 100.0f;
	rk_aiq_sharp_strength_v4_t sharpV4Strenght;
	sharpV4Strenght.sync.sync_mode = RK_AIQ_UAPI_MODE_SYNC;
	sharpV4Strenght.percent = fPercent;
	int ret = rk_aiq_user_api2_asharpV4_SetStrength(sys_ctx, &sharpV4Strenght);
	if(!ret){
		LOG_INFO("Set Sharpness Level: %d success.\n", value);
	}else{
		LOG_ERROR("Set Sharpness Level: %d fail, ret=%d\n", value, ret);
	}

	return ret;
}

int rk3588_SetExposureMode(const rk_aiq_sys_ctx_t* sys_ctx, const char *value) { //"auto"
	int mode;
	Uapi_ExpSwAttrV2_t expSwAttr;
	rk_aiq_user_api2_ae_getExpSwAttr(sys_ctx, &expSwAttr);
	if (!strcmp(value, "auto")) {
		expSwAttr.AecOpType = RK_AIQ_OP_MODE_AUTO;
	} else {
		if (mode != RK_AIQ_WORKING_MODE_NORMAL) {
			expSwAttr.AecOpType = RK_AIQ_OP_MODE_MANUAL;
			expSwAttr.stManual.HdrAE.ManualGainEn = true;
			expSwAttr.stManual.HdrAE.ManualTimeEn = true;
		} else {
			expSwAttr.AecOpType = RK_AIQ_OP_MODE_MANUAL;
			expSwAttr.stManual.LinearAE.ManualGainEn = true;
			expSwAttr.stManual.LinearAE.ManualTimeEn = true;
		}
	}
	int ret = rk_aiq_user_api2_ae_setExpSwAttr(sys_ctx, expSwAttr);
	if(!ret){
		LOG_INFO("Set Exposure Mode : %s success.\n", value);
	}else{
		LOG_ERROR("Set Exposure Mode : %s  fail, ret=%d\n", value, ret);
	}

	return ret;
}


