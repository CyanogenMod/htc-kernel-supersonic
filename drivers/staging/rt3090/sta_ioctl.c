/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2007, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************

    Module Name:
    sta_ioctl.c

    Abstract:
    IOCTL related subroutines

    Revision History:
    Who         When          What
    --------    ----------    ----------------------------------------------
    Rory Chen   01-03-2003    created
	Rory Chen   02-14-2005    modify to support RT61
*/

#include	"rt_config.h"

#ifdef DBG
extern ULONG    RTDebugLevel;
#endif

#define NR_WEP_KEYS				4
#define WEP_SMALL_KEY_LEN			(40/8)
#define WEP_LARGE_KEY_LEN			(104/8)

#define GROUP_KEY_NO                4

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
#define IWE_STREAM_ADD_EVENT(_A, _B, _C, _D, _E)		iwe_stream_add_event(_A, _B, _C, _D, _E)
#define IWE_STREAM_ADD_POINT(_A, _B, _C, _D, _E)		iwe_stream_add_point(_A, _B, _C, _D, _E)
#define IWE_STREAM_ADD_VALUE(_A, _B, _C, _D, _E, _F)	iwe_stream_add_value(_A, _B, _C, _D, _E, _F)
#else
#define IWE_STREAM_ADD_EVENT(_A, _B, _C, _D, _E)		iwe_stream_add_event(_B, _C, _D, _E)
#define IWE_STREAM_ADD_POINT(_A, _B, _C, _D, _E)		iwe_stream_add_point(_B, _C, _D, _E)
#define IWE_STREAM_ADD_VALUE(_A, _B, _C, _D, _E, _F)	iwe_stream_add_value(_B, _C, _D, _E, _F)
#endif

extern UCHAR    CipherWpa2Template[];

typedef struct PACKED _RT_VERSION_INFO{
    UCHAR       DriverVersionW;
    UCHAR       DriverVersionX;
    UCHAR       DriverVersionY;
    UCHAR       DriverVersionZ;
    UINT        DriverBuildYear;
    UINT        DriverBuildMonth;
    UINT        DriverBuildDay;
} RT_VERSION_INFO, *PRT_VERSION_INFO;

struct iw_priv_args privtab[] = {
{ RTPRIV_IOCTL_SET,
  IW_PRIV_TYPE_CHAR | 1024, 0,
  "set"},

{ RTPRIV_IOCTL_SHOW, IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
  ""},
/* --- sub-ioctls definitions --- */
    { SHOW_CONN_STATUS,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "connStatus" },
	{ SHOW_DRVIER_VERION,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "driverVer" },
    { SHOW_BA_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "bainfo" },
	{ SHOW_DESC_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "descinfo" },
    { RAIO_OFF,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "radio_off" },
	{ RAIO_ON,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "radio_on" },
#ifdef QOS_DLS_SUPPORT
	{ SHOW_DLS_ENTRY_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "dlsentryinfo" },
#endif // QOS_DLS_SUPPORT //
	{ SHOW_CFG_VALUE,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "show" },
	{ SHOW_ADHOC_ENTRY_INFO,
	  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, "adhocEntry" },
/* --- sub-ioctls relations --- */

#ifdef DBG
{ RTPRIV_IOCTL_BBP,
  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
  "bbp"},
{ RTPRIV_IOCTL_MAC,
  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
  "mac"},
#ifdef RTMP_RF_RW_SUPPORT
{ RTPRIV_IOCTL_RF,
  IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
  "rf"},
#endif // RTMP_RF_RW_SUPPORT //
{ RTPRIV_IOCTL_E2P,
  IW_PRIV_TYPE_CHAR | 1024, IW_PRIV_TYPE_CHAR | 1024,
  "e2p"},
#endif  /* DBG */

{ RTPRIV_IOCTL_STATISTICS,
  0, IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
  "stat"},
{ RTPRIV_IOCTL_GSITESURVEY,
  0, IW_PRIV_TYPE_CHAR | 1024,
  "get_site_survey"},


};

static __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,   24,  36, 48, 72, 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 104, 156, 208, 234, 260, // 20MHz, 800ns GI, MCS: 0 ~ 15
	39, 78,  117, 156, 234, 312, 351, 390,										  // 20MHz, 800ns GI, MCS: 16 ~ 23
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, // 40MHz, 800ns GI, MCS: 0 ~ 15
	81, 162, 243, 324, 486, 648, 729, 810,										  // 40MHz, 800ns GI, MCS: 16 ~ 23
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288, // 20MHz, 400ns GI, MCS: 0 ~ 15
	43, 87,  130, 173, 260, 317, 390, 433,										  // 20MHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600, // 40MHz, 400ns GI, MCS: 0 ~ 15
	90, 180, 270, 360, 540, 720, 810, 900};



INT Set_SSID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

#ifdef WMM_SUPPORT
INT	Set_WmmCapable_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg);
#endif

INT Set_NetworkType_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_AuthMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_EncrypType_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_DefaultKeyID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key1_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key2_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key3_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_Key4_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

INT Set_WPAPSK_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);


INT Set_PSMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

#ifdef RT3090
INT Set_PCIePSLevel_Proc(
IN  PRTMP_ADAPTER   pAdapter,
IN  PUCHAR          arg);
#endif // RT3090 //
#ifdef WPA_SUPPLICANT_SUPPORT
INT Set_Wpa_Support(
    IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg);
#endif // WPA_SUPPLICANT_SUPPORT //

#ifdef DBG

VOID RTMPIoctlMAC(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq);

VOID RTMPIoctlE2PROM(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  struct iwreq    *wrq);
#endif // DBG //


NDIS_STATUS RTMPWPANoneAddKeyProc(
    IN  PRTMP_ADAPTER   pAd,
    IN	PVOID			pBuf);

INT Set_FragTest_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);

#ifdef DOT11_N_SUPPORT
INT Set_TGnWifiTest_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING          arg);
#endif // DOT11_N_SUPPORT //

INT Set_LongRetryLimit_Proc(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	PSTRING			arg);

INT Set_ShortRetryLimit_Proc(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	PSTRING			arg);

#ifdef EXT_BUILD_CHANNEL_LIST
INT Set_Ieee80211dClientMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg);
#endif // EXT_BUILD_CHANNEL_LIST //

#ifdef CARRIER_DETECTION_SUPPORT
INT Set_CarrierDetect_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING          arg);
#endif // CARRIER_DETECTION_SUPPORT //

INT	Show_Adhoc_MacTable_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			extra);

#ifdef RTMP_RF_RW_SUPPORT
VOID RTMPIoctlRF(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq);
#endif // RTMP_RF_RW_SUPPORT //


INT Set_BeaconLostTime_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg);

INT Set_AutoRoaming_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg);

INT Set_SiteSurvey_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg);

INT Set_ForceTxBurst_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg);

#ifdef ANT_DIVERSITY_SUPPORT
INT	Set_Antenna_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PUCHAR			arg);
#endif // ANT_DIVERSITY_SUPPORT //

static struct {
	PSTRING name;
	INT (*set_proc)(PRTMP_ADAPTER pAdapter, PSTRING arg);
} *PRTMP_PRIVATE_SET_PROC, RTMP_PRIVATE_SUPPORT_PROC[] = {
	{"DriverVersion",				Set_DriverVersion_Proc},
	{"CountryRegion",				Set_CountryRegion_Proc},
	{"CountryRegionABand",			Set_CountryRegionABand_Proc},
	{"SSID",						Set_SSID_Proc},
	{"WirelessMode",				Set_WirelessMode_Proc},
	{"TxBurst",					Set_TxBurst_Proc},
	{"TxPreamble",				Set_TxPreamble_Proc},
	{"TxPower",					Set_TxPower_Proc},
	{"Channel",					Set_Channel_Proc},
	{"BGProtection",				Set_BGProtection_Proc},
	{"RTSThreshold",				Set_RTSThreshold_Proc},
	{"FragThreshold",				Set_FragThreshold_Proc},
#ifdef DOT11_N_SUPPORT
	{"HtBw",		                Set_HtBw_Proc},
	{"HtMcs",		                Set_HtMcs_Proc},
	{"HtGi",		                Set_HtGi_Proc},
	{"HtOpMode",		            Set_HtOpMode_Proc},
	{"HtExtcha",		            Set_HtExtcha_Proc},
	{"HtMpduDensity",		        Set_HtMpduDensity_Proc},
	{"HtBaWinSize",				Set_HtBaWinSize_Proc},
	{"HtRdg",					Set_HtRdg_Proc},
	{"HtAmsdu",					Set_HtAmsdu_Proc},
	{"HtAutoBa",				Set_HtAutoBa_Proc},
	{"HtBaDecline",					Set_BADecline_Proc},
	{"HtProtect",				Set_HtProtect_Proc},
	{"HtMimoPs",				Set_HtMimoPs_Proc},
	{"HtDisallowTKIP",				Set_HtDisallowTKIP_Proc},
#endif // DOT11_N_SUPPORT //

#ifdef AGGREGATION_SUPPORT
	{"PktAggregate",				Set_PktAggregate_Proc},
#endif // AGGREGATION_SUPPORT //

#ifdef WMM_SUPPORT
	{"WmmCapable",					Set_WmmCapable_Proc},
#endif
	{"IEEE80211H",					Set_IEEE80211H_Proc},
    {"NetworkType",                 Set_NetworkType_Proc},
	{"AuthMode",					Set_AuthMode_Proc},
	{"EncrypType",					Set_EncrypType_Proc},
	{"DefaultKeyID",				Set_DefaultKeyID_Proc},
	{"Key1",						Set_Key1_Proc},
	{"Key2",						Set_Key2_Proc},
	{"Key3",						Set_Key3_Proc},
	{"Key4",						Set_Key4_Proc},
	{"WPAPSK",						Set_WPAPSK_Proc},
	{"ResetCounter",				Set_ResetStatCounter_Proc},
	{"PSMode",                      Set_PSMode_Proc},
#ifdef DBG
	{"Debug",						Set_Debug_Proc},
#endif // DBG //

#ifdef RALINK_ATE
	{"ATE",							Set_ATE_Proc},
	{"ATEDA",						Set_ATE_DA_Proc},
	{"ATESA",						Set_ATE_SA_Proc},
	{"ATEBSSID",					Set_ATE_BSSID_Proc},
	{"ATECHANNEL",					Set_ATE_CHANNEL_Proc},
	{"ATETXPOW0",					Set_ATE_TX_POWER0_Proc},
	{"ATETXPOW1",					Set_ATE_TX_POWER1_Proc},
	{"ATETXANT",					Set_ATE_TX_Antenna_Proc},
	{"ATERXANT",					Set_ATE_RX_Antenna_Proc},
	{"ATETXFREQOFFSET",				Set_ATE_TX_FREQOFFSET_Proc},
	{"ATETXBW",						Set_ATE_TX_BW_Proc},
	{"ATETXLEN",					Set_ATE_TX_LENGTH_Proc},
	{"ATETXCNT",					Set_ATE_TX_COUNT_Proc},
	{"ATETXMCS",					Set_ATE_TX_MCS_Proc},
	{"ATETXMODE",					Set_ATE_TX_MODE_Proc},
	{"ATETXGI",						Set_ATE_TX_GI_Proc},
	{"ATERXFER",					Set_ATE_RX_FER_Proc},
	{"ATERRF",						Set_ATE_Read_RF_Proc},
	{"ATEWRF1",						Set_ATE_Write_RF1_Proc},
	{"ATEWRF2",						Set_ATE_Write_RF2_Proc},
	{"ATEWRF3",						Set_ATE_Write_RF3_Proc},
	{"ATEWRF4",						Set_ATE_Write_RF4_Proc},
	{"ATELDE2P",				    Set_ATE_Load_E2P_Proc},
	{"ATERE2P",						Set_ATE_Read_E2P_Proc},
	{"ATESHOW",						Set_ATE_Show_Proc},
	{"ATEHELP",						Set_ATE_Help_Proc},

#ifdef RALINK_28xx_QA
	{"TxStop",						Set_TxStop_Proc},
	{"RxStop",						Set_RxStop_Proc},
#endif // RALINK_28xx_QA //
#endif // RALINK_ATE //

#ifdef WPA_SUPPLICANT_SUPPORT
    {"WpaSupport",                  Set_Wpa_Support},
#endif // WPA_SUPPLICANT_SUPPORT //





	{"FixedTxMode",                 Set_FixedTxMode_Proc},
#ifdef CONFIG_APSTA_MIXED_SUPPORT
	{"OpMode",						Set_OpMode_Proc},
#endif // CONFIG_APSTA_MIXED_SUPPORT //
#ifdef DOT11_N_SUPPORT
    {"TGnWifiTest",                 Set_TGnWifiTest_Proc},
    {"ForceGF",					Set_ForceGF_Proc},
#endif // DOT11_N_SUPPORT //
#ifdef QOS_DLS_SUPPORT
	{"DlsAddEntry",					Set_DlsAddEntry_Proc},
	{"DlsTearDownEntry",			Set_DlsTearDownEntry_Proc},
#endif // QOS_DLS_SUPPORT //
	{"LongRetry",				Set_LongRetryLimit_Proc},
	{"ShortRetry",				Set_ShortRetryLimit_Proc},
#ifdef EXT_BUILD_CHANNEL_LIST
	{"11dClientMode",				Set_Ieee80211dClientMode_Proc},
#endif // EXT_BUILD_CHANNEL_LIST //
#ifdef CARRIER_DETECTION_SUPPORT
	{"CarrierDetect",				Set_CarrierDetect_Proc},
#endif // CARRIER_DETECTION_SUPPORT //


//2008/09/11:KH add to support efuse<--
#ifdef RT30xx
#ifdef RTMP_EFUSE_SUPPORT
	{"efuseFreeNumber",				set_eFuseGetFreeBlockCount_Proc},
	{"efuseDump",					set_eFusedump_Proc},
	{"efuseLoadFromBin",				set_eFuseLoadFromBin_Proc},
	{"efuseBufferModeWriteBack",		set_eFuseBufferModeWriteBack_Proc},
#endif // RTMP_EFUSE_SUPPORT //
#ifdef ANT_DIVERSITY_SUPPORT
	{"ant",					Set_Antenna_Proc},
#endif // ANT_DIVERSITY_SUPPORT //
#endif // RT30xx //
//2008/09/11:KH add to support efuse-->

	{"BeaconLostTime",				Set_BeaconLostTime_Proc},
	{"AutoRoaming",					Set_AutoRoaming_Proc},
	{"SiteSurvey",					Set_SiteSurvey_Proc},
	{"ForceTxBurst",				Set_ForceTxBurst_Proc},

	{NULL,}
};


VOID RTMPAddKey(
	IN	PRTMP_ADAPTER	    pAd,
	IN	PNDIS_802_11_KEY    pKey)
{
	ULONG				KeyIdx;
	MAC_TABLE_ENTRY		*pEntry;

    DBGPRINT(RT_DEBUG_TRACE, ("RTMPAddKey ------>\n"));

	if (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
	{
		if (pKey->KeyIndex & 0x80000000)
		{
		    if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone)
            {
                NdisZeroMemory(pAd->StaCfg.PMK, 32);
                NdisMoveMemory(pAd->StaCfg.PMK, pKey->KeyMaterial, pKey->KeyLength);
                goto end;
            }
		    // Update PTK
		    NdisZeroMemory(&pAd->SharedKey[BSS0][0], sizeof(CIPHER_KEY));
            pAd->SharedKey[BSS0][0].KeyLen = LEN_TKIP_EK;
            NdisMoveMemory(pAd->SharedKey[BSS0][0].Key, pKey->KeyMaterial, LEN_TKIP_EK);
#ifdef WPA_SUPPLICANT_SUPPORT
            if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
            {
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].TxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[BSS0][0].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }

            // Decide its ChiperAlg
		if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_TKIP;
		else if (pAd->StaCfg.PairCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_AES;
		else
			pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_NONE;

            // Update these related information to MAC_TABLE_ENTRY
		pEntry = &pAd->MacTab.Content[BSSID_WCID];
            NdisMoveMemory(pEntry->PairwiseKey.Key, pAd->SharedKey[BSS0][0].Key, LEN_TKIP_EK);
		NdisMoveMemory(pEntry->PairwiseKey.RxMic, pAd->SharedKey[BSS0][0].RxMic, LEN_TKIP_RXMICK);
		NdisMoveMemory(pEntry->PairwiseKey.TxMic, pAd->SharedKey[BSS0][0].TxMic, LEN_TKIP_TXMICK);
		pEntry->PairwiseKey.CipherAlg = pAd->SharedKey[BSS0][0].CipherAlg;

		// Update pairwise key information to ASIC Shared Key Table
		AsicAddSharedKeyEntry(pAd,
							  BSS0,
							  0,
							  pAd->SharedKey[BSS0][0].CipherAlg,
							  pAd->SharedKey[BSS0][0].Key,
							  pAd->SharedKey[BSS0][0].TxMic,
							  pAd->SharedKey[BSS0][0].RxMic);

		// Update ASIC WCID attribute table and IVEIV table
		RTMPAddWcidAttributeEntry(pAd,
								  BSS0,
								  0,
								  pAd->SharedKey[BSS0][0].CipherAlg,
								  pEntry);

            if (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA2)
            {
                // set 802.1x port control
	            //pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
				STA_PORT_SECURED(pAd);

                // Indicate Connected for GUI
                pAd->IndicateMediaState = NdisMediaStateConnected;
            }
		}
        else
        {
            // Update GTK
            pAd->StaCfg.DefaultKeyId = (pKey->KeyIndex & 0xFF);
            NdisZeroMemory(&pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId], sizeof(CIPHER_KEY));
            pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].KeyLen = LEN_TKIP_EK;
            NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key, pKey->KeyMaterial, LEN_TKIP_EK);
#ifdef WPA_SUPPLICANT_SUPPORT
            if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
            {
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }
            else
#endif // WPA_SUPPLICANT_SUPPORT //
            {
		NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic, pKey->KeyMaterial + LEN_TKIP_EK, LEN_TKIP_TXMICK);
                NdisMoveMemory(pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic, pKey->KeyMaterial + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
            }

            // Update Shared Key CipherAlg
		pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_NONE;
		if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption2Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_TKIP;
		else if (pAd->StaCfg.GroupCipher == Ndis802_11Encryption3Enabled)
			pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg = CIPHER_AES;

            // Update group key information to ASIC Shared Key Table
		AsicAddSharedKeyEntry(pAd,
							  BSS0,
							  pAd->StaCfg.DefaultKeyId,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].TxMic,
							  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].RxMic);

		// Update ASIC WCID attribute table and IVEIV table
		RTMPAddWcidAttributeEntry(pAd,
								  BSS0,
								  pAd->StaCfg.DefaultKeyId,
								  pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].CipherAlg,
								  NULL);

            // set 802.1x port control
	        //pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			STA_PORT_SECURED(pAd);

            // Indicate Connected for GUI
            pAd->IndicateMediaState = NdisMediaStateConnected;
        }
	}
	else	// dynamic WEP from wpa_supplicant
	{
		UCHAR	CipherAlg;
	PUCHAR	Key;

		if(pKey->KeyLength == 32)
			goto end;

		KeyIdx = pKey->KeyIndex & 0x0fffffff;

		if (KeyIdx < 4)
		{
			// it is a default shared key, for Pairwise key setting
			if (pKey->KeyIndex & 0x80000000)
			{
				pEntry = MacTableLookup(pAd, pKey->BSSID);

				if (pEntry)
				{
					DBGPRINT(RT_DEBUG_TRACE, ("RTMPAddKey: Set Pair-wise Key\n"));

					// set key material and key length
					pEntry->PairwiseKey.KeyLen = (UCHAR)pKey->KeyLength;
					NdisMoveMemory(pEntry->PairwiseKey.Key, &pKey->KeyMaterial, pKey->KeyLength);

					// set Cipher type
					if (pKey->KeyLength == 5)
						pEntry->PairwiseKey.CipherAlg = CIPHER_WEP64;
					else
						pEntry->PairwiseKey.CipherAlg = CIPHER_WEP128;

					// Add Pair-wise key to Asic
					AsicAddPairwiseKeyEntry(
						pAd,
						pEntry->Addr,
						(UCHAR)pEntry->Aid,
				&pEntry->PairwiseKey);

					// update WCID attribute table and IVEIV table for this entry
					RTMPAddWcidAttributeEntry(
						pAd,
						BSS0,
						KeyIdx, // The value may be not zero
						pEntry->PairwiseKey.CipherAlg,
						pEntry);

				}
			}
			else
            {
				// Default key for tx (shared key)
				pAd->StaCfg.DefaultKeyId = (UCHAR) KeyIdx;

				// set key material and key length
				pAd->SharedKey[BSS0][KeyIdx].KeyLen = (UCHAR) pKey->KeyLength;
				NdisMoveMemory(pAd->SharedKey[BSS0][KeyIdx].Key, &pKey->KeyMaterial, pKey->KeyLength);

				// Set Ciper type
				if (pKey->KeyLength == 5)
					pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_WEP64;
				else
					pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_WEP128;

			CipherAlg = pAd->SharedKey[BSS0][KeyIdx].CipherAlg;
			Key = pAd->SharedKey[BSS0][KeyIdx].Key;

				// Set Group key material to Asic
			AsicAddSharedKeyEntry(pAd, BSS0, KeyIdx, CipherAlg, Key, NULL, NULL);

				// Update WCID attribute table and IVEIV table for this group key table
				RTMPAddWcidAttributeEntry(pAd, BSS0, KeyIdx, CipherAlg, NULL);

			}
		}
	}
end:
	return;
}

char * rtstrchr(const char * s, int c)
{
    for(; *s != (char) c; ++s)
        if (*s == '\0')
            return NULL;
    return (char *) s;
}

/*
This is required for LinEX2004/kernel2.6.7 to provide iwlist scanning function
*/

int
rt_ioctl_giwname(struct net_device *dev,
		   struct iw_request_info *info,
		   char *name, char *extra)
{

#ifdef RTMP_MAC_PCI
    strncpy(name, "RT2860 Wireless", IFNAMSIZ);
#endif // RTMP_MAC_PCI //
	return 0;
}

int rt_ioctl_siwfreq(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_freq *freq, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	int	chan = -1;

    //check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
        return -ENETDOWN;
    }


	if (freq->e > 1)
		return -EINVAL;

	if((freq->e == 0) && (freq->m <= 1000))
		chan = freq->m;	// Setting by channel number
	else
		MAP_KHZ_TO_CHANNEL_ID( (freq->m /100) , chan); // Setting by frequency - search the table , like 2.412G, 2.422G,

    if (ChannelSanity(pAdapter, chan) == TRUE)
    {
	pAdapter->CommonCfg.Channel = chan;
	DBGPRINT(RT_DEBUG_ERROR, ("==>rt_ioctl_siwfreq::SIOCSIWFREQ[cmd=0x%x] (Channel=%d)\n", SIOCSIWFREQ, pAdapter->CommonCfg.Channel));
    }
    else
        return -EINVAL;

	return 0;
}


int rt_ioctl_giwfreq(struct net_device *dev,
		   struct iw_request_info *info,
		   struct iw_freq *freq, char *extra)
{
	PRTMP_ADAPTER pAdapter = NULL;
	UCHAR ch;
	ULONG	m = 2412000;

	pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

		ch = pAdapter->CommonCfg.Channel;

	DBGPRINT(RT_DEBUG_TRACE,("==>rt_ioctl_giwfreq  %d\n", ch));

	MAP_CHANNEL_ID_TO_KHZ(ch, m);
	freq->m = m * 100;
	freq->e = 1;
	return 0;
}


int rt_ioctl_siwmode(struct net_device *dev,
		   struct iw_request_info *info,
		   __u32 *mode, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	//check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
    }

	switch (*mode)
	{
		case IW_MODE_ADHOC:
			Set_NetworkType_Proc(pAdapter, "Adhoc");
			break;
		case IW_MODE_INFRA:
			Set_NetworkType_Proc(pAdapter, "Infra");
			break;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,20))
        case IW_MODE_MONITOR:
			Set_NetworkType_Proc(pAdapter, "Monitor");
			break;
#endif
		default:
			DBGPRINT(RT_DEBUG_TRACE, ("===>rt_ioctl_siwmode::SIOCSIWMODE (unknown %d)\n", *mode));
			return -EINVAL;
	}

	// Reset Ralink supplicant to not use, it will be set to start when UI set PMK key
	pAdapter->StaCfg.WpaState = SS_NOTUSE;

	return 0;
}


int rt_ioctl_giwmode(struct net_device *dev,
		   struct iw_request_info *info,
		   __u32 *mode, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (ADHOC_ON(pAdapter))
		*mode = IW_MODE_ADHOC;
    else if (INFRA_ON(pAdapter))
		*mode = IW_MODE_INFRA;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,20))
    else if (MONITOR_ON(pAdapter))
    {
        *mode = IW_MODE_MONITOR;
    }
#endif
    else
        *mode = IW_MODE_AUTO;

	DBGPRINT(RT_DEBUG_TRACE, ("==>rt_ioctl_giwmode(mode=%d)\n", *mode));
	return 0;
}

int rt_ioctl_siwsens(struct net_device *dev,
		   struct iw_request_info *info,
		   char *name, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	return 0;
}

int rt_ioctl_giwsens(struct net_device *dev,
		   struct iw_request_info *info,
		   char *name, char *extra)
{
	return 0;
}

int rt_ioctl_giwrange(struct net_device *dev,
		   struct iw_request_info *info,
		   struct iw_point *data, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_range *range = (struct iw_range *) extra;
	u16 val;
	int i;

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	DBGPRINT(RT_DEBUG_TRACE ,("===>rt_ioctl_giwrange\n"));
	data->length = sizeof(struct iw_range);
	memset(range, 0, sizeof(struct iw_range));

	range->txpower_capa = IW_TXPOW_DBM;

	if (INFRA_ON(pAdapter)||ADHOC_ON(pAdapter))
	{
		range->min_pmp = 1 * 1024;
		range->max_pmp = 65535 * 1024;
		range->min_pmt = 1 * 1024;
		range->max_pmt = 1000 * 1024;
		range->pmp_flags = IW_POWER_PERIOD;
		range->pmt_flags = IW_POWER_TIMEOUT;
		range->pm_capa = IW_POWER_PERIOD | IW_POWER_TIMEOUT |
			IW_POWER_UNICAST_R | IW_POWER_ALL_R;
	}

	range->we_version_compiled = WIRELESS_EXT;
	range->we_version_source = 14;

	range->retry_capa = IW_RETRY_LIMIT;
	range->retry_flags = IW_RETRY_LIMIT;
	range->min_retry = 0;
	range->max_retry = 255;

	range->num_channels =  pAdapter->ChannelListNum;

	val = 0;
	for (i = 1; i <= range->num_channels; i++)
	{
		u32 m = 2412000;
		range->freq[val].i = pAdapter->ChannelList[i-1].Channel;
		MAP_CHANNEL_ID_TO_KHZ(pAdapter->ChannelList[i-1].Channel, m);
		range->freq[val].m = m * 100; /* OS_HZ */

		range->freq[val].e = 1;
		val++;
		if (val == IW_MAX_FREQUENCIES)
			break;
	}
	range->num_frequency = val;

	range->max_qual.qual = 100; /* what is correct max? This was not
					* documented exactly. At least
					* 69 has been observed. */
	range->max_qual.level = 0; /* dB */
	range->max_qual.noise = 0; /* dB */

	/* What would be suitable values for "average/typical" qual? */
	range->avg_qual.qual = 20;
	range->avg_qual.level = -60;
	range->avg_qual.noise = -95;
	range->sensitivity = 3;

	range->max_encoding_tokens = NR_WEP_KEYS;
	range->num_encoding_sizes = 2;
	range->encoding_size[0] = 5;
	range->encoding_size[1] = 13;

	range->min_rts = 0;
	range->max_rts = 2347;
	range->min_frag = 256;
	range->max_frag = 2346;

#if WIRELESS_EXT > 17
	/* IW_ENC_CAPA_* bit field */
	range->enc_capa = IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2 |
					IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP;
#endif

	return 0;
}

int rt_ioctl_siwap(struct net_device *dev,
		      struct iw_request_info *info,
		      struct sockaddr *ap_addr, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
    NDIS_802_11_MAC_ADDRESS Bssid;

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
    }

	if (pAdapter->Mlme.CntlMachine.CurrState != CNTL_IDLE)
    {
        RTMP_MLME_RESET_STATE_MACHINE(pAdapter);
        DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
    }

    // tell CNTL state machine to call NdisMSetInformationComplete() after completing
    // this request, because this request is initiated by NDIS.
    pAdapter->MlmeAux.CurrReqIsFromNdis = FALSE;
	// Prevent to connect AP again in STAMlmePeriodicExec
	pAdapter->MlmeAux.AutoReconnectSsidLen= 32;

    memset(Bssid, 0, MAC_ADDR_LEN);
    memcpy(Bssid, ap_addr->sa_data, MAC_ADDR_LEN);
    MlmeEnqueue(pAdapter,
                MLME_CNTL_STATE_MACHINE,
                OID_802_11_BSSID,
                sizeof(NDIS_802_11_MAC_ADDRESS),
                (VOID *)&Bssid);

    DBGPRINT(RT_DEBUG_TRACE, ("IOCTL::SIOCSIWAP %02x:%02x:%02x:%02x:%02x:%02x\n",
        Bssid[0], Bssid[1], Bssid[2], Bssid[3], Bssid[4], Bssid[5]));

	return 0;
}

int rt_ioctl_giwap(struct net_device *dev,
		      struct iw_request_info *info,
		      struct sockaddr *ap_addr, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (INFRA_ON(pAdapter) || ADHOC_ON(pAdapter))
	{
		ap_addr->sa_family = ARPHRD_ETHER;
		memcpy(ap_addr->sa_data, &pAdapter->CommonCfg.Bssid, ETH_ALEN);
	}
#ifdef WPA_SUPPLICANT_SUPPORT
    // Add for RT2870
    else if (pAdapter->StaCfg.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE)
    {
        ap_addr->sa_family = ARPHRD_ETHER;
        memcpy(ap_addr->sa_data, &pAdapter->MlmeAux.Bssid, ETH_ALEN);
    }
#endif // WPA_SUPPLICANT_SUPPORT //
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("IOCTL::SIOCGIWAP(=EMPTY)\n"));
		return -ENOTCONN;
	}

	return 0;
}

/*
 * Units are in db above the noise floor. That means the
 * rssi values reported in the tx/rx descriptors in the
 * driver are the SNR expressed in db.
 *
 * If you assume that the noise floor is -95, which is an
 * excellent assumption 99.5 % of the time, then you can
 * derive the absolute signal level (i.e. -95 + rssi).
 * There are some other slight factors to take into account
 * depending on whether the rssi measurement is from 11b,
 * 11g, or 11a.   These differences are at most 2db and
 * can be documented.
 *
 * NB: various calculations are based on the orinoco/wavelan
 *     drivers for compatibility
 */
static void set_quality(PRTMP_ADAPTER pAdapter,
                        struct iw_quality *iq,
                        signed char rssi)
{
	__u8 ChannelQuality;

	// Normalize Rssi
	if (rssi >= -50)
        ChannelQuality = 100;
	else if (rssi >= -80) // between -50 ~ -80dbm
		ChannelQuality = (__u8)(24 + ((rssi + 80) * 26)/10);
	else if (rssi >= -90)   // between -80 ~ -90dbm
        ChannelQuality = (__u8)((rssi + 90) * 26)/10;
	else
		ChannelQuality = 0;

    iq->qual = (__u8)ChannelQuality;

    iq->level = (__u8)(rssi);
    iq->noise = (pAdapter->BbpWriteLatch[66] > pAdapter->BbpTuning.FalseCcaUpperThreshold) ? ((__u8)pAdapter->BbpTuning.FalseCcaUpperThreshold) : ((__u8) pAdapter->BbpWriteLatch[66]);		// noise level (dBm)
    iq->noise += 256 - 143;
    iq->updated = pAdapter->iw_stats.qual.updated;
}

int rt_ioctl_iwaplist(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	struct sockaddr addr[IW_MAX_AP];
	struct iw_quality qual[IW_MAX_AP];
	int i;

	//check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		data->length = 0;
		return 0;
        //return -ENETDOWN;
	}

	for (i = 0; i <IW_MAX_AP ; i++)
	{
		if (i >=  pAdapter->ScanTab.BssNr)
			break;
		addr[i].sa_family = ARPHRD_ETHER;
			memcpy(addr[i].sa_data, &pAdapter->ScanTab.BssEntry[i].Bssid, MAC_ADDR_LEN);
		set_quality(pAdapter, &qual[i], pAdapter->ScanTab.BssEntry[i].Rssi);
	}
	data->length = i;
	memcpy(extra, &addr, i*sizeof(addr[0]));
	data->flags = 1;		/* signal quality present (sort of) */
	memcpy(extra + i*sizeof(addr[0]), &qual, i*sizeof(qual[i]));

	return 0;
}

#ifdef SIOCGIWSCAN
int rt_ioctl_siwscan(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	ULONG								Now;
	int Status = NDIS_STATUS_SUCCESS;

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	if (MONITOR_ON(pAdapter))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("!!! Driver is in Monitor Mode now !!!\n"));
        return -EINVAL;
    }


#ifdef WPA_SUPPLICANT_SUPPORT
	if (pAdapter->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE)
	{
		pAdapter->StaCfg.WpaSupplicantScanCount++;
	}
#endif // WPA_SUPPLICANT_SUPPORT //

    pAdapter->StaCfg.bScanReqIsFromWebUI = TRUE;
	if (RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
		return NDIS_STATUS_SUCCESS;
	do{
		Now = jiffies;

#ifdef WPA_SUPPLICANT_SUPPORT
		if ((pAdapter->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE) &&
			(pAdapter->StaCfg.WpaSupplicantScanCount > 3))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("!!! WpaSupplicantScanCount > 3\n"));
			Status = NDIS_STATUS_SUCCESS;
			break;
		}
#endif // WPA_SUPPLICANT_SUPPORT //

		if ((OPSTATUS_TEST_FLAG(pAdapter, fOP_STATUS_MEDIA_STATE_CONNECTED)) &&
			((pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeWPA) ||
			(pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK)) &&
			(pAdapter->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("!!! Link UP, Port Not Secured! ignore this set::OID_802_11_BSSID_LIST_SCAN\n"));
			Status = NDIS_STATUS_SUCCESS;
			break;
		}

		if (pAdapter->Mlme.CntlMachine.CurrState != CNTL_IDLE)
		{
			RTMP_MLME_RESET_STATE_MACHINE(pAdapter);
			DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
		}

		// tell CNTL state machine to call NdisMSetInformationComplete() after completing
		// this request, because this request is initiated by NDIS.
		pAdapter->MlmeAux.CurrReqIsFromNdis = FALSE;
		// Reset allowed scan retries
		pAdapter->StaCfg.ScanCnt = 0;
		pAdapter->StaCfg.LastScanTime = Now;

		MlmeEnqueue(pAdapter,
			MLME_CNTL_STATE_MACHINE,
			OID_802_11_BSSID_LIST_SCAN,
			0,
			NULL);

		Status = NDIS_STATUS_SUCCESS;
		RTMP_MLME_HANDLER(pAdapter);
	}while(0);
	return NDIS_STATUS_SUCCESS;
}

int rt_ioctl_giwscan(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra)
{

	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	int i=0;
	PSTRING current_ev = extra, previous_ev = extra;
	PSTRING end_buf;
	PSTRING current_val;
	STRING custom[MAX_CUSTOM_LEN] = {0};
#ifndef IWEVGENIE
	unsigned char idx;
#endif // IWEVGENIE //
	struct iw_event iwe;

	if (RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
    {
		/*
		 * Still scanning, indicate the caller should try again.
		 */
		return -EAGAIN;
	}


#ifdef WPA_SUPPLICANT_SUPPORT
	if (pAdapter->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE)
	{
		pAdapter->StaCfg.WpaSupplicantScanCount = 0;
	}
#endif // WPA_SUPPLICANT_SUPPORT //

	if (pAdapter->ScanTab.BssNr == 0)
	{
		data->length = 0;
		return 0;
	}

#if WIRELESS_EXT >= 17
    if (data->length > 0)
        end_buf = extra + data->length;
    else
        end_buf = extra + IW_SCAN_MAX_DATA;
#else
    end_buf = extra + IW_SCAN_MAX_DATA;
#endif

	for (i = 0; i < pAdapter->ScanTab.BssNr; i++)
	{
		if (current_ev >= end_buf)
        {
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif
        }

		//MAC address
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWAP;
		iwe.u.ap_addr.sa_family = ARPHRD_ETHER;
				memcpy(iwe.u.ap_addr.sa_data, &pAdapter->ScanTab.BssEntry[i].Bssid, ETH_ALEN);

        previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_ADDR_LEN);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		/*
		Protocol:
			it will show scanned AP's WirelessMode .
			it might be
					802.11a
					802.11a/n
					802.11g/n
					802.11b/g/n
					802.11g
					802.11b/g
		*/
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWNAME;


	{
		PBSS_ENTRY pBssEntry=&pAdapter->ScanTab.BssEntry[i];
		BOOLEAN isGonly=FALSE;
		int rateCnt=0;

		if (pBssEntry->Channel>14)
		{
			if (pBssEntry->HtCapabilityLen!=0)
				strcpy(iwe.u.name,"802.11a/n");
			else
				strcpy(iwe.u.name,"802.11a");
		}
		else
		{
			/*
				if one of non B mode rate is set supported rate . it mean G only.
			*/
			for (rateCnt=0;rateCnt<pBssEntry->SupRateLen;rateCnt++)
			{
				/*
					6Mbps(140) 9Mbps(146) and >=12Mbps(152) are supported rate , it mean G only.
				*/
				if (pBssEntry->SupRate[rateCnt]==140 || pBssEntry->SupRate[rateCnt]==146 || pBssEntry->SupRate[rateCnt]>=152)
					isGonly=TRUE;
			}

			for (rateCnt=0;rateCnt<pBssEntry->ExtRateLen;rateCnt++)
			{
				if (pBssEntry->ExtRate[rateCnt]==140 || pBssEntry->ExtRate[rateCnt]==146 || pBssEntry->ExtRate[rateCnt]>=152)
					isGonly=TRUE;
			}


			if (pBssEntry->HtCapabilityLen!=0)
			{
				if (isGonly==TRUE)
					strcpy(iwe.u.name,"802.11g/n");
				else
					strcpy(iwe.u.name,"802.11b/g/n");
			}
			else
			{
				if (isGonly==TRUE)
					strcpy(iwe.u.name,"802.11g");
				else
				{
					if (pBssEntry->SupRateLen==4 && pBssEntry->ExtRateLen==0)
						strcpy(iwe.u.name,"802.11b");
					else
						strcpy(iwe.u.name,"802.11b/g");
				}
			}
		}
	}

		previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_ADDR_LEN);
		if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
			return -E2BIG;
#else
			break;
#endif

		//ESSID
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWESSID;
		iwe.u.data.length = pAdapter->ScanTab.BssEntry[i].SsidLen;
		iwe.u.data.flags = 1;

        previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_POINT(info, current_ev,end_buf, &iwe, (PSTRING) pAdapter->ScanTab.BssEntry[i].Ssid);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Network Type
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWMODE;
		if (pAdapter->ScanTab.BssEntry[i].BssType == Ndis802_11IBSS)
		{
			iwe.u.mode = IW_MODE_ADHOC;
		}
		else if (pAdapter->ScanTab.BssEntry[i].BssType == Ndis802_11Infrastructure)
		{
			iwe.u.mode = IW_MODE_INFRA;
		}
		else
		{
			iwe.u.mode = IW_MODE_AUTO;
		}
		iwe.len = IW_EV_UINT_LEN;

        previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev, end_buf, &iwe,  IW_EV_UINT_LEN);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Channel and Frequency
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWFREQ;
		if (INFRA_ON(pAdapter) || ADHOC_ON(pAdapter))
			iwe.u.freq.m = pAdapter->ScanTab.BssEntry[i].Channel;
		else
			iwe.u.freq.m = pAdapter->ScanTab.BssEntry[i].Channel;
		iwe.u.freq.e = 0;
		iwe.u.freq.i = 0;

		previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_EVENT(info, current_ev,end_buf, &iwe, IW_EV_FREQ_LEN);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

        //Add quality statistics
        //================================
        memset(&iwe, 0, sizeof(iwe));
	iwe.cmd = IWEVQUAL;
	iwe.u.qual.level = 0;
	iwe.u.qual.noise = 0;
	set_quality(pAdapter, &iwe.u.qual, pAdapter->ScanTab.BssEntry[i].Rssi);
	current_ev = IWE_STREAM_ADD_EVENT(info, current_ev, end_buf, &iwe, IW_EV_QUAL_LEN);
	if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Encyption key
		//================================
		memset(&iwe, 0, sizeof(iwe));
		iwe.cmd = SIOCGIWENCODE;
		if (CAP_IS_PRIVACY_ON (pAdapter->ScanTab.BssEntry[i].CapabilityInfo ))
			iwe.u.data.flags =IW_ENCODE_ENABLED | IW_ENCODE_NOKEY;
		else
			iwe.u.data.flags = IW_ENCODE_DISABLED;

        previous_ev = current_ev;
        current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf,&iwe, (char *)pAdapter->SharedKey[BSS0][(iwe.u.data.flags & IW_ENCODE_INDEX)-1].Key);
        if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
            return -E2BIG;
#else
			break;
#endif

		//Bit Rate
		//================================
		if (pAdapter->ScanTab.BssEntry[i].SupRateLen)
        {
            UCHAR tmpRate = pAdapter->ScanTab.BssEntry[i].SupRate[pAdapter->ScanTab.BssEntry[i].SupRateLen-1];
			memset(&iwe, 0, sizeof(iwe));
			iwe.cmd = SIOCGIWRATE;
		current_val = current_ev + IW_EV_LCP_LEN;
            if (tmpRate == 0x82)
                iwe.u.bitrate.value =  1 * 1000000;
            else if (tmpRate == 0x84)
                iwe.u.bitrate.value =  2 * 1000000;
            else if (tmpRate == 0x8B)
                iwe.u.bitrate.value =  5.5 * 1000000;
            else if (tmpRate == 0x96)
                iwe.u.bitrate.value =  11 * 1000000;
            else
		    iwe.u.bitrate.value =  (tmpRate/2) * 1000000;

			if (tmpRate == 0x6c && pAdapter->ScanTab.BssEntry[i].HtCapabilityLen > 0)
			{
				int rate_count = sizeof(ralinkrate)/sizeof(__s32);
				HT_CAP_INFO capInfo = pAdapter->ScanTab.BssEntry[i].HtCapability.HtCapInfo;
				int shortGI = capInfo.ChannelWidth ? capInfo.ShortGIfor40 : capInfo.ShortGIfor20;
				int maxMCS = pAdapter->ScanTab.BssEntry[i].HtCapability.MCSSet[1] ?  15 : 7;
				int rate_index = 12 + ((UCHAR)capInfo.ChannelWidth * 24) + ((UCHAR)shortGI *48) + ((UCHAR)maxMCS);
				if (rate_index < 0)
					rate_index = 0;
				if (rate_index > rate_count)
					rate_index = rate_count;
				iwe.u.bitrate.value	=  ralinkrate[rate_index] * 500000;
			}

			iwe.u.bitrate.disabled = 0;
			current_val = IWE_STREAM_ADD_VALUE(info, current_ev,
				current_val, end_buf, &iwe,
			IW_EV_PARAM_LEN);

		if((current_val-current_ev)>IW_EV_LCP_LEN)
		current_ev = current_val;
		else
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
        }

#ifdef IWEVGENIE
        //WPA IE
		if (pAdapter->ScanTab.BssEntry[i].WpaIE.IELen > 0)
        {
			memset(&iwe, 0, sizeof(iwe));
			memset(&custom[0], 0, MAX_CUSTOM_LEN);
			memcpy(custom, &(pAdapter->ScanTab.BssEntry[i].WpaIE.IE[0]),
						   pAdapter->ScanTab.BssEntry[i].WpaIE.IELen);
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = pAdapter->ScanTab.BssEntry[i].WpaIE.IELen;
			current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf, &iwe, custom);
			if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
		}

		//WPA2 IE
        if (pAdapter->ScanTab.BssEntry[i].RsnIE.IELen > 0)
        {
		memset(&iwe, 0, sizeof(iwe));
			memset(&custom[0], 0, MAX_CUSTOM_LEN);
			memcpy(custom, &(pAdapter->ScanTab.BssEntry[i].RsnIE.IE[0]),
						   pAdapter->ScanTab.BssEntry[i].RsnIE.IELen);
			iwe.cmd = IWEVGENIE;
			iwe.u.data.length = pAdapter->ScanTab.BssEntry[i].RsnIE.IELen;
			current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf, &iwe, custom);
			if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
        }
#else
        //WPA IE
		//================================
        if (pAdapter->ScanTab.BssEntry[i].WpaIE.IELen > 0)
        {
		NdisZeroMemory(&iwe, sizeof(iwe));
			memset(&custom[0], 0, MAX_CUSTOM_LEN);
		iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = (pAdapter->ScanTab.BssEntry[i].WpaIE.IELen * 2) + 7;
            NdisMoveMemory(custom, "wpa_ie=", 7);
            for (idx = 0; idx < pAdapter->ScanTab.BssEntry[i].WpaIE.IELen; idx++)
                sprintf(custom, "%s%02x", custom, pAdapter->ScanTab.BssEntry[i].WpaIE.IE[idx]);
            previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf, &iwe,  custom);
            if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
        }

        //WPA2 IE
        if (pAdapter->ScanTab.BssEntry[i].RsnIE.IELen > 0)
        {
		NdisZeroMemory(&iwe, sizeof(iwe));
			memset(&custom[0], 0, MAX_CUSTOM_LEN);
		iwe.cmd = IWEVCUSTOM;
            iwe.u.data.length = (pAdapter->ScanTab.BssEntry[i].RsnIE.IELen * 2) + 7;
            NdisMoveMemory(custom, "rsn_ie=", 7);
			for (idx = 0; idx < pAdapter->ScanTab.BssEntry[i].RsnIE.IELen; idx++)
                sprintf(custom, "%s%02x", custom, pAdapter->ScanTab.BssEntry[i].RsnIE.IE[idx]);
            previous_ev = current_ev;
		current_ev = IWE_STREAM_ADD_POINT(info, current_ev, end_buf, &iwe,  custom);
            if (current_ev == previous_ev)
#if WIRELESS_EXT >= 17
                return -E2BIG;
#else
			    break;
#endif
        }
#endif // IWEVGENIE //
	}

	data->length = current_ev - extra;
    pAdapter->StaCfg.bScanReqIsFromWebUI = FALSE;
	DBGPRINT(RT_DEBUG_ERROR ,("===>rt_ioctl_giwscan. %d(%d) BSS returned, data->length = %d\n",i , pAdapter->ScanTab.BssNr, data->length));
	return 0;
}
#endif

int rt_ioctl_siwessid(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *essid)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	//check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
	DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
    }

	if (data->flags)
	{
		PSTRING	pSsidString = NULL;

		// Includes null character.
		if (data->length > (IW_ESSID_MAX_SIZE + 1))
			return -E2BIG;

		pSsidString = kmalloc(MAX_LEN_OF_SSID+1, MEM_ALLOC_FLAG);
		if (pSsidString)
        {
			NdisZeroMemory(pSsidString, MAX_LEN_OF_SSID+1);
			NdisMoveMemory(pSsidString, essid, data->length);
			if (Set_SSID_Proc(pAdapter, pSsidString) == FALSE)
				return -EINVAL;
		}
		else
			return -ENOMEM;
		}
	else
    {
		// ANY ssid
		if (Set_SSID_Proc(pAdapter, "") == FALSE)
			return -EINVAL;
    }
	return 0;
}

int rt_ioctl_giwessid(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *essid)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	data->flags = 1;
    if (MONITOR_ON(pAdapter))
    {
        data->length  = 0;
        return 0;
    }

	if (OPSTATUS_TEST_FLAG(pAdapter, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		DBGPRINT(RT_DEBUG_TRACE ,("MediaState is connected\n"));
		data->length = pAdapter->CommonCfg.SsidLen;
		memcpy(essid, pAdapter->CommonCfg.Ssid, pAdapter->CommonCfg.SsidLen);
	}
	else
	{//the ANY ssid was specified
		data->length  = 0;
		DBGPRINT(RT_DEBUG_TRACE ,("MediaState is not connected, ess\n"));
	}

	return 0;

}

int rt_ioctl_siwnickn(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *nickname)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

    //check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE ,("INFO::Network is down!\n"));
        return -ENETDOWN;
    }

	if (data->length > IW_ESSID_MAX_SIZE)
		return -EINVAL;

	memset(pAdapter->nickname, 0, IW_ESSID_MAX_SIZE + 1);
	memcpy(pAdapter->nickname, nickname, data->length);


	return 0;
}

int rt_ioctl_giwnickn(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *nickname)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (data->length > strlen((PSTRING) pAdapter->nickname) + 1)
		data->length = strlen((PSTRING) pAdapter->nickname) + 1;
	if (data->length > 0) {
		memcpy(nickname, pAdapter->nickname, data->length-1);
		nickname[data->length-1] = '\0';
	}
	return 0;
}

int rt_ioctl_siwrts(struct net_device *dev,
		       struct iw_request_info *info,
		       struct iw_param *rts, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	u16 val;

    //check if the interface is down
    if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
        return -ENETDOWN;
    }

	if (rts->disabled)
		val = MAX_RTS_THRESHOLD;
	else if (rts->value < 0 || rts->value > MAX_RTS_THRESHOLD)
		return -EINVAL;
	else if (rts->value == 0)
	    val = MAX_RTS_THRESHOLD;
	else
		val = rts->value;

	if (val != pAdapter->CommonCfg.RtsThreshold)
		pAdapter->CommonCfg.RtsThreshold = val;

	return 0;
}

int rt_ioctl_giwrts(struct net_device *dev,
		       struct iw_request_info *info,
		       struct iw_param *rts, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	rts->value = pAdapter->CommonCfg.RtsThreshold;
	rts->disabled = (rts->value == MAX_RTS_THRESHOLD);
	rts->fixed = 1;

	return 0;
}

int rt_ioctl_siwfrag(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_param *frag, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	u16 val;

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	if (frag->disabled)
		val = MAX_FRAG_THRESHOLD;
	else if (frag->value >= MIN_FRAG_THRESHOLD || frag->value <= MAX_FRAG_THRESHOLD)
        val = __cpu_to_le16(frag->value & ~0x1); /* even numbers only */
	else if (frag->value == 0)
	    val = MAX_FRAG_THRESHOLD;
	else
		return -EINVAL;

	pAdapter->CommonCfg.FragmentThreshold = val;
	return 0;
}

int rt_ioctl_giwfrag(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_param *frag, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	frag->value = pAdapter->CommonCfg.FragmentThreshold;
	frag->disabled = (frag->value == MAX_FRAG_THRESHOLD);
	frag->fixed = 1;

	return 0;
}

#define MAX_WEP_KEY_SIZE 13
#define MIN_WEP_KEY_SIZE 5
int rt_ioctl_siwencode(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *erq, char *extra)
{
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	if ((erq->length == 0) &&
        (erq->flags & IW_ENCODE_DISABLED))
	{
		pAdapter->StaCfg.PairCipher = Ndis802_11WEPDisabled;
		pAdapter->StaCfg.GroupCipher = Ndis802_11WEPDisabled;
		pAdapter->StaCfg.WepStatus = Ndis802_11WEPDisabled;
        pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
        goto done;
	}
	else if (erq->flags & IW_ENCODE_RESTRICTED || erq->flags & IW_ENCODE_OPEN)
	{
	    //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
		STA_PORT_SECURED(pAdapter);
		pAdapter->StaCfg.PairCipher = Ndis802_11WEPEnabled;
		pAdapter->StaCfg.GroupCipher = Ndis802_11WEPEnabled;
		pAdapter->StaCfg.WepStatus = Ndis802_11WEPEnabled;
        pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
		if (erq->flags & IW_ENCODE_RESTRICTED)
			pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeShared;
	else
			pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
	}

    if (erq->length > 0)
	{
		int keyIdx = (erq->flags & IW_ENCODE_INDEX) - 1;
		/* Check the size of the key */
		if (erq->length > MAX_WEP_KEY_SIZE)
		{
			return -EINVAL;
		}
		/* Check key index */
		if ((keyIdx < 0) || (keyIdx >= NR_WEP_KEYS))
        {
            DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::Wrong keyIdx=%d! Using default key instead (%d)\n",
                                        keyIdx, pAdapter->StaCfg.DefaultKeyId));

            //Using default key
			keyIdx = pAdapter->StaCfg.DefaultKeyId;
        }
		else
			pAdapter->StaCfg.DefaultKeyId = keyIdx;

        NdisZeroMemory(pAdapter->SharedKey[BSS0][keyIdx].Key,  16);

		if (erq->length == MAX_WEP_KEY_SIZE)
        {
			pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MAX_WEP_KEY_SIZE;
            pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP128;
		}
		else if (erq->length == MIN_WEP_KEY_SIZE)
        {
            pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MIN_WEP_KEY_SIZE;
            pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP64;
		}
		else
			/* Disable the key */
			pAdapter->SharedKey[BSS0][keyIdx].KeyLen = 0;

		/* Check if the key is not marked as invalid */
		if(!(erq->flags & IW_ENCODE_NOKEY))
		{
			/* Copy the key in the driver */
			NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].Key, extra, erq->length);
        }
	}
    else
			{
		/* Do we want to just set the transmit key index ? */
		int index = (erq->flags & IW_ENCODE_INDEX) - 1;
		if ((index >= 0) && (index < 4))
        {
			pAdapter->StaCfg.DefaultKeyId = index;
            }
        else
			/* Don't complain if only change the mode */
		if (!(erq->flags & IW_ENCODE_MODE))
			return -EINVAL;
	}

done:
    DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::erq->flags=%x\n",erq->flags));
	DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::AuthMode=%x\n",pAdapter->StaCfg.AuthMode));
	DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::DefaultKeyId=%x, KeyLen = %d\n",pAdapter->StaCfg.DefaultKeyId , pAdapter->SharedKey[BSS0][pAdapter->StaCfg.DefaultKeyId].KeyLen));
	DBGPRINT(RT_DEBUG_TRACE ,("==>rt_ioctl_siwencode::WepStatus=%x\n",pAdapter->StaCfg.WepStatus));
	return 0;
}

int
rt_ioctl_giwencode(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_point *erq, char *key)
{
	int kid;
	PRTMP_ADAPTER pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);

	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
	}

	kid = erq->flags & IW_ENCODE_INDEX;
	DBGPRINT(RT_DEBUG_TRACE, ("===>rt_ioctl_giwencode %d\n", erq->flags & IW_ENCODE_INDEX));

	if (pAdapter->StaCfg.WepStatus == Ndis802_11WEPDisabled)
	{
		erq->length = 0;
		erq->flags = IW_ENCODE_DISABLED;
	}
	else if ((kid > 0) && (kid <=4))
	{
		// copy wep key
		erq->flags = kid ;			/* NB: base 1 */
		if (erq->length > pAdapter->SharedKey[BSS0][kid-1].KeyLen)
			erq->length = pAdapter->SharedKey[BSS0][kid-1].KeyLen;
		memcpy(key, pAdapter->SharedKey[BSS0][kid-1].Key, erq->length);
		//if ((kid == pAdapter->PortCfg.DefaultKeyId))
		//erq->flags |= IW_ENCODE_ENABLED;	/* XXX */
		if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared)
			erq->flags |= IW_ENCODE_RESTRICTED;		/* XXX */
		else
			erq->flags |= IW_ENCODE_OPEN;		/* XXX */

	}
	else if (kid == 0)
	{
		if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared)
			erq->flags |= IW_ENCODE_RESTRICTED;		/* XXX */
		else
			erq->flags |= IW_ENCODE_OPEN;		/* XXX */
		erq->length = pAdapter->SharedKey[BSS0][pAdapter->StaCfg.DefaultKeyId].KeyLen;
		memcpy(key, pAdapter->SharedKey[BSS0][pAdapter->StaCfg.DefaultKeyId].Key, erq->length);
		// copy default key ID
		if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared)
			erq->flags |= IW_ENCODE_RESTRICTED;		/* XXX */
		else
			erq->flags |= IW_ENCODE_OPEN;		/* XXX */
		erq->flags = pAdapter->StaCfg.DefaultKeyId + 1;			/* NB: base 1 */
		erq->flags |= IW_ENCODE_ENABLED;	/* XXX */
	}

	return 0;

}

static int
rt_ioctl_setparam(struct net_device *dev, struct iw_request_info *info,
			 void *w, char *extra)
{
	PRTMP_ADAPTER pAdapter;
	POS_COOKIE pObj;
	PSTRING this_char = extra;
	PSTRING value;
	int  Status=0;

	pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	if (pAdapter == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	pObj = (POS_COOKIE) pAdapter->OS_Cookie;
	{
		pObj->ioctl_if_type = INT_MAIN;
        pObj->ioctl_if = MAIN_MBSSID;
	}

	//check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
			return -ENETDOWN;
	}

	if (!*this_char)
		return -EINVAL;

	if ((value = rtstrchr(this_char, '=')) != NULL)
	    *value++ = 0;

	if (!value && (strcmp(this_char, "SiteSurvey") != 0))
	    return -EINVAL;
	else
		goto SET_PROC;

	// reject setting nothing besides ANY ssid(ssidLen=0)
    if (!*value && (strcmp(this_char, "SSID") != 0))
        return -EINVAL;

SET_PROC:
	for (PRTMP_PRIVATE_SET_PROC = RTMP_PRIVATE_SUPPORT_PROC; PRTMP_PRIVATE_SET_PROC->name; PRTMP_PRIVATE_SET_PROC++)
	{
	    if (strcmp(this_char, PRTMP_PRIVATE_SET_PROC->name) == 0)
	    {
	        if(!PRTMP_PRIVATE_SET_PROC->set_proc(pAdapter, value))
	        {	//FALSE:Set private failed then return Invalid argument
			    Status = -EINVAL;
	        }
		    break;	//Exit for loop.
	    }
	}

	if(PRTMP_PRIVATE_SET_PROC->name == NULL)
	{  //Not found argument
	    Status = -EINVAL;
	    DBGPRINT(RT_DEBUG_TRACE, ("===>rt_ioctl_setparam:: (iwpriv) Not Support Set Command [%s=%s]\n", this_char, value));
	}

    return Status;
}


static int
rt_private_get_statistics(struct net_device *dev, struct iw_request_info *info,
		struct iw_point *wrq, char *extra)
{
	INT				Status = 0;
    PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);

    if (extra == NULL)
    {
        wrq->length = 0;
        return -EIO;
    }

    memset(extra, 0x00, IW_PRIV_SIZE_MASK);
    sprintf(extra, "\n\n");

#ifdef RALINK_ATE
	if (ATE_ON(pAd))
	{
	    sprintf(extra+strlen(extra), "Tx success                      = %ld\n", (ULONG)pAd->ate.TxDoneCount);
	    //sprintf(extra+strlen(extra), "Tx success without retry        = %ld\n", (ULONG)pAd->ate.TxDoneCount);
	}
	else
#endif // RALINK_ATE //
	{
    sprintf(extra+strlen(extra), "Tx success                      = %ld\n", (ULONG)pAd->WlanCounters.TransmittedFragmentCount.QuadPart);
    sprintf(extra+strlen(extra), "Tx success without retry        = %ld\n", (ULONG)pAd->WlanCounters.TransmittedFragmentCount.QuadPart - (ULONG)pAd->WlanCounters.RetryCount.QuadPart);
	}
    sprintf(extra+strlen(extra), "Tx success after retry          = %ld\n", (ULONG)pAd->WlanCounters.RetryCount.QuadPart);
    sprintf(extra+strlen(extra), "Tx fail to Rcv ACK after retry  = %ld\n", (ULONG)pAd->WlanCounters.FailedCount.QuadPart);
    sprintf(extra+strlen(extra), "RTS Success Rcv CTS             = %ld\n", (ULONG)pAd->WlanCounters.RTSSuccessCount.QuadPart);
    sprintf(extra+strlen(extra), "RTS Fail Rcv CTS                = %ld\n", (ULONG)pAd->WlanCounters.RTSFailureCount.QuadPart);

    sprintf(extra+strlen(extra), "Rx success                      = %ld\n", (ULONG)pAd->WlanCounters.ReceivedFragmentCount.QuadPart);
    sprintf(extra+strlen(extra), "Rx with CRC                     = %ld\n", (ULONG)pAd->WlanCounters.FCSErrorCount.QuadPart);
    sprintf(extra+strlen(extra), "Rx drop due to out of resource  = %ld\n", (ULONG)pAd->Counters8023.RxNoBuffer);
    sprintf(extra+strlen(extra), "Rx duplicate frame              = %ld\n", (ULONG)pAd->WlanCounters.FrameDuplicateCount.QuadPart);

    sprintf(extra+strlen(extra), "False CCA (one second)          = %ld\n", (ULONG)pAd->RalinkCounters.OneSecFalseCCACnt);

#ifdef RALINK_ATE
	if (ATE_ON(pAd))
	{
		if (pAd->ate.RxAntennaSel == 0)
		{
		sprintf(extra+strlen(extra), "RSSI-A                          = %ld\n", (LONG)(pAd->ate.LastRssi0 - pAd->BbpRssiToDbmDelta));
			sprintf(extra+strlen(extra), "RSSI-B (if available)           = %ld\n", (LONG)(pAd->ate.LastRssi1 - pAd->BbpRssiToDbmDelta));
			sprintf(extra+strlen(extra), "RSSI-C (if available)           = %ld\n\n", (LONG)(pAd->ate.LastRssi2 - pAd->BbpRssiToDbmDelta));
		}
		else
		{
		sprintf(extra+strlen(extra), "RSSI                            = %ld\n", (LONG)(pAd->ate.LastRssi0 - pAd->BbpRssiToDbmDelta));
		}
	}
	else
#endif // RALINK_ATE //
	{
	sprintf(extra+strlen(extra), "RSSI-A                          = %ld\n", (LONG)(pAd->StaCfg.RssiSample.LastRssi0 - pAd->BbpRssiToDbmDelta));
        sprintf(extra+strlen(extra), "RSSI-B (if available)           = %ld\n", (LONG)(pAd->StaCfg.RssiSample.LastRssi1 - pAd->BbpRssiToDbmDelta));
        sprintf(extra+strlen(extra), "RSSI-C (if available)           = %ld\n\n", (LONG)(pAd->StaCfg.RssiSample.LastRssi2 - pAd->BbpRssiToDbmDelta));
	}
#ifdef WPA_SUPPLICANT_SUPPORT
    sprintf(extra+strlen(extra), "WpaSupplicantUP                 = %d\n\n", pAd->StaCfg.WpaSupplicantUP);
#endif // WPA_SUPPLICANT_SUPPORT //



    wrq->length = strlen(extra) + 1; // 1: size of '\0'
    DBGPRINT(RT_DEBUG_TRACE, ("<== rt_private_get_statistics, wrq->length = %d\n", wrq->length));

    return Status;
}

#ifdef DOT11_N_SUPPORT
void	getBaInfo(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			pOutBuf)
{
	INT i, j;
	BA_ORI_ENTRY *pOriBAEntry;
	BA_REC_ENTRY *pRecBAEntry;

	for (i=0; i<MAX_LEN_OF_MAC_TABLE; i++)
	{
		PMAC_TABLE_ENTRY pEntry = &pAd->MacTab.Content[i];
		if (((pEntry->ValidAsCLI || pEntry->ValidAsApCli) && (pEntry->Sst == SST_ASSOC))
			|| (pEntry->ValidAsWDS) || (pEntry->ValidAsMesh))
		{
			sprintf(pOutBuf, "%s\n%02X:%02X:%02X:%02X:%02X:%02X (Aid = %d) (AP) -\n",
                pOutBuf,
				pEntry->Addr[0], pEntry->Addr[1], pEntry->Addr[2],
				pEntry->Addr[3], pEntry->Addr[4], pEntry->Addr[5], pEntry->Aid);

			sprintf(pOutBuf, "%s[Recipient]\n", pOutBuf);
			for (j=0; j < NUM_OF_TID; j++)
			{
				if (pEntry->BARecWcidArray[j] != 0)
				{
					pRecBAEntry =&pAd->BATable.BARecEntry[pEntry->BARecWcidArray[j]];
					sprintf(pOutBuf, "%sTID=%d, BAWinSize=%d, LastIndSeq=%d, ReorderingPkts=%d\n", pOutBuf, j, pRecBAEntry->BAWinSize, pRecBAEntry->LastIndSeq, pRecBAEntry->list.qlen);
				}
			}
			sprintf(pOutBuf, "%s\n", pOutBuf);

			sprintf(pOutBuf, "%s[Originator]\n", pOutBuf);
			for (j=0; j < NUM_OF_TID; j++)
			{
				if (pEntry->BAOriWcidArray[j] != 0)
				{
					pOriBAEntry =&pAd->BATable.BAOriEntry[pEntry->BAOriWcidArray[j]];
					sprintf(pOutBuf, "%sTID=%d, BAWinSize=%d, StartSeq=%d, CurTxSeq=%d\n", pOutBuf, j, pOriBAEntry->BAWinSize, pOriBAEntry->Sequence, pEntry->TxSeq[j]);
				}
			}
			sprintf(pOutBuf, "%s\n\n", pOutBuf);
		}
        if (strlen(pOutBuf) > (IW_PRIV_SIZE_MASK - 30))
                break;
	}

	return;
}
#endif // DOT11_N_SUPPORT //

static int
rt_private_show(struct net_device *dev, struct iw_request_info *info,
		struct iw_point *wrq, PSTRING extra)
{
	INT				Status = 0;
	PRTMP_ADAPTER   pAd;
	POS_COOKIE		pObj;
	u32             subcmd = wrq->flags;

	pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	if (pAd == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	pObj = (POS_COOKIE) pAd->OS_Cookie;
	if (extra == NULL)
	{
		wrq->length = 0;
		return -EIO;
	}
	memset(extra, 0x00, IW_PRIV_SIZE_MASK);

	{
		pObj->ioctl_if_type = INT_MAIN;
		pObj->ioctl_if = MAIN_MBSSID;
	}

    switch(subcmd)
    {

        case SHOW_CONN_STATUS:
            if (MONITOR_ON(pAd))
            {
#ifdef DOT11_N_SUPPORT
                if (pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED &&
                    pAd->CommonCfg.RegTransmitSetting.field.BW)
                    sprintf(extra, "Monitor Mode(CentralChannel %d)\n", pAd->CommonCfg.CentralChannel);
                else
#endif // DOT11_N_SUPPORT //
                    sprintf(extra, "Monitor Mode(Channel %d)\n", pAd->CommonCfg.Channel);
            }
            else
            {
                if (pAd->IndicateMediaState == NdisMediaStateConnected)
		{
		    if (INFRA_ON(pAd))
                    {
                    sprintf(extra, "Connected(AP: %s[%02X:%02X:%02X:%02X:%02X:%02X])\n",
                                    pAd->CommonCfg.Ssid,
                                    pAd->CommonCfg.Bssid[0],
                                    pAd->CommonCfg.Bssid[1],
                                    pAd->CommonCfg.Bssid[2],
                                    pAd->CommonCfg.Bssid[3],
                                    pAd->CommonCfg.Bssid[4],
                                    pAd->CommonCfg.Bssid[5]);
			DBGPRINT(RT_DEBUG_TRACE ,("Ssid=%s ,Ssidlen = %d\n",pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen));
		}
                    else if (ADHOC_ON(pAd))
                        sprintf(extra, "Connected\n");
		}
		else
		{
		    sprintf(extra, "Disconnected\n");
			DBGPRINT(RT_DEBUG_TRACE ,("ConnStatus is not connected\n"));
		}
            }
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;
        case SHOW_DRVIER_VERION:
            sprintf(extra, "Driver version-%s, %s %s\n", STA_DRIVER_VERSION, __DATE__, __TIME__ );
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;
#ifdef DOT11_N_SUPPORT
        case SHOW_BA_INFO:
            getBaInfo(pAd, extra);
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;
#endif // DOT11_N_SUPPORT //
		case SHOW_DESC_INFO:
			{
				Show_DescInfo_Proc(pAd, NULL);
				wrq->length = 0; // 1: size of '\0'
			}
			break;
        case RAIO_OFF:
            if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
            {
                if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
		        {
		            RTMP_MLME_RESET_STATE_MACHINE(pAd);
		            DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
		        }
            }
            pAd->StaCfg.bSwRadio = FALSE;
            if (pAd->StaCfg.bRadio != (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio))
            {
                pAd->StaCfg.bRadio = (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio);
                if (pAd->StaCfg.bRadio == FALSE)
                {
                    MlmeRadioOff(pAd);
                    // Update extra information
					pAd->ExtraInfo = SW_RADIO_OFF;
                }
            }
            sprintf(extra, "Radio Off\n");
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;
        case RAIO_ON:
            pAd->StaCfg.bSwRadio = TRUE;
            //if (pAd->StaCfg.bRadio != (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio))
            {
                pAd->StaCfg.bRadio = (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio);
                if (pAd->StaCfg.bRadio == TRUE)
                {
                    MlmeRadioOn(pAd);
                    // Update extra information
					pAd->ExtraInfo = EXTRA_INFO_CLEAR;
                }
            }
            sprintf(extra, "Radio On\n");
            wrq->length = strlen(extra) + 1; // 1: size of '\0'
            break;


#ifdef QOS_DLS_SUPPORT
		case SHOW_DLS_ENTRY_INFO:
			{
				Set_DlsEntryInfo_Display_Proc(pAd, NULL);
				wrq->length = 0; // 1: size of '\0'
			}
			break;
#endif // QOS_DLS_SUPPORT //

		case SHOW_CFG_VALUE:
			{
				Status = RTMPShowCfgValue(pAd, (PSTRING) wrq->pointer, extra);
				if (Status == 0)
					wrq->length = strlen(extra) + 1; // 1: size of '\0'
			}
			break;
		case SHOW_ADHOC_ENTRY_INFO:
			Show_Adhoc_MacTable_Proc(pAd, extra);
			wrq->length = strlen(extra) + 1; // 1: size of '\0'
			break;
        default:
            DBGPRINT(RT_DEBUG_TRACE, ("%s - unknow subcmd = %d\n", __FUNCTION__, subcmd));
            break;
    }

    return Status;
}

#ifdef SIOCSIWMLME
int rt_ioctl_siwmlme(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_mlme *pMlme = (struct iw_mlme *)wrqu->data.pointer;
	MLME_QUEUE_ELEM				MsgElem;
	MLME_DISASSOC_REQ_STRUCT	DisAssocReq;
	MLME_DEAUTH_REQ_STRUCT      DeAuthReq;

	DBGPRINT(RT_DEBUG_TRACE, ("====> %s\n", __FUNCTION__));

	if (pMlme == NULL)
		return -EINVAL;

	switch(pMlme->cmd)
	{
#ifdef IW_MLME_DEAUTH
		case IW_MLME_DEAUTH:
			DBGPRINT(RT_DEBUG_TRACE, ("====> %s - IW_MLME_DEAUTH\n", __FUNCTION__));
			COPY_MAC_ADDR(DeAuthReq.Addr, pAd->CommonCfg.Bssid);
			DeAuthReq.Reason = pMlme->reason_code;
			MsgElem.MsgLen = sizeof(MLME_DEAUTH_REQ_STRUCT);
			NdisMoveMemory(MsgElem.Msg, &DeAuthReq, sizeof(MLME_DEAUTH_REQ_STRUCT));
			MlmeDeauthReqAction(pAd, &MsgElem);
			if (INFRA_ON(pAd))
			{
			    LinkDown(pAd, FALSE);
			    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			}
			break;
#endif // IW_MLME_DEAUTH //
#ifdef IW_MLME_DISASSOC
		case IW_MLME_DISASSOC:
			DBGPRINT(RT_DEBUG_TRACE, ("====> %s - IW_MLME_DISASSOC\n", __FUNCTION__));
			COPY_MAC_ADDR(DisAssocReq.Addr, pAd->CommonCfg.Bssid);
			DisAssocReq.Reason =  pMlme->reason_code;

			MsgElem.Machine = ASSOC_STATE_MACHINE;
			MsgElem.MsgType = MT2_MLME_DISASSOC_REQ;
			MsgElem.MsgLen = sizeof(MLME_DISASSOC_REQ_STRUCT);
			NdisMoveMemory(MsgElem.Msg, &DisAssocReq, sizeof(MLME_DISASSOC_REQ_STRUCT));

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
			MlmeDisassocReqAction(pAd, &MsgElem);
			break;
#endif // IW_MLME_DISASSOC //
		default:
			DBGPRINT(RT_DEBUG_TRACE, ("====> %s - Unknow Command\n", __FUNCTION__));
			break;
	}

	return 0;
}
#endif // SIOCSIWMLME //

#if WIRELESS_EXT > 17
int rt_ioctl_siwauth(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER   pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_param *param = &wrqu->param;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
	}
	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_WPA_VERSION:
            if (param->value == IW_AUTH_WPA_VERSION_WPA)
            {
                pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPAPSK;
				if (pAdapter->StaCfg.BssType == BSS_ADHOC)
					pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPANone;
            }
            else if (param->value == IW_AUTH_WPA_VERSION_WPA2)
                pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA2PSK;

            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_WPA_VERSION - param->value = %d!\n", __FUNCTION__, param->value));
            break;
	case IW_AUTH_CIPHER_PAIRWISE:
            if (param->value == IW_AUTH_CIPHER_NONE)
            {
                pAdapter->StaCfg.WepStatus = Ndis802_11WEPDisabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11WEPDisabled;
            }
            else if (param->value == IW_AUTH_CIPHER_WEP40 ||
                     param->value == IW_AUTH_CIPHER_WEP104)
            {
                pAdapter->StaCfg.WepStatus = Ndis802_11WEPEnabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11WEPEnabled;
#ifdef WPA_SUPPLICANT_SUPPORT
                pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
            }
            else if (param->value == IW_AUTH_CIPHER_TKIP)
            {
                pAdapter->StaCfg.WepStatus = Ndis802_11Encryption2Enabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;
            }
            else if (param->value == IW_AUTH_CIPHER_CCMP)
            {
                pAdapter->StaCfg.WepStatus = Ndis802_11Encryption3Enabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11Encryption3Enabled;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_CIPHER_PAIRWISE - param->value = %d!\n", __FUNCTION__, param->value));
            break;
	case IW_AUTH_CIPHER_GROUP:
            if (param->value == IW_AUTH_CIPHER_NONE)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11WEPDisabled;
            }
            else if (param->value == IW_AUTH_CIPHER_WEP40 ||
                     param->value == IW_AUTH_CIPHER_WEP104)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11WEPEnabled;
            }
            else if (param->value == IW_AUTH_CIPHER_TKIP)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11Encryption2Enabled;
            }
            else if (param->value == IW_AUTH_CIPHER_CCMP)
            {
                pAdapter->StaCfg.GroupCipher = Ndis802_11Encryption3Enabled;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_CIPHER_GROUP - param->value = %d!\n", __FUNCTION__, param->value));
            break;
	case IW_AUTH_KEY_MGMT:
            if (param->value == IW_AUTH_KEY_MGMT_802_1X)
            {
                if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK)
                {
                    pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA;
#ifdef WPA_SUPPLICANT_SUPPORT
                    pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
                }
                else if (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
                {
                    pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA2;
#ifdef WPA_SUPPLICANT_SUPPORT
                    pAdapter->StaCfg.IEEE8021X = FALSE;
#endif // WPA_SUPPLICANT_SUPPORT //
                }
#ifdef WPA_SUPPLICANT_SUPPORT
                else
                    // WEP 1x
                    pAdapter->StaCfg.IEEE8021X = TRUE;
#endif // WPA_SUPPLICANT_SUPPORT //
            }
            else if (param->value == 0)
            {
                //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
				STA_PORT_SECURED(pAdapter);
            }
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_KEY_MGMT - param->value = %d!\n", __FUNCTION__, param->value));
            break;
	case IW_AUTH_RX_UNENCRYPTED_EAPOL:
            break;
	case IW_AUTH_PRIVACY_INVOKED:
            /*if (param->value == 0)
			{
                pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
                pAdapter->StaCfg.WepStatus = Ndis802_11WEPDisabled;
                pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;
                pAdapter->StaCfg.PairCipher = Ndis802_11WEPDisabled;
		    pAdapter->StaCfg.GroupCipher = Ndis802_11WEPDisabled;
            }*/
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_PRIVACY_INVOKED - param->value = %d!\n", __FUNCTION__, param->value));
		break;
	case IW_AUTH_DROP_UNENCRYPTED:
            if (param->value != 0)
                pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
			else
			{
                //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
				STA_PORT_SECURED(pAdapter);
			}
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_WPA_VERSION - param->value = %d!\n", __FUNCTION__, param->value));
		break;
	case IW_AUTH_80211_AUTH_ALG:
			if (param->value & IW_AUTH_ALG_SHARED_KEY)
            {
				pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeShared;
			}
            else if (param->value & IW_AUTH_ALG_OPEN_SYSTEM)
            {
				pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
			}
            else
				return -EINVAL;
            DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_80211_AUTH_ALG - param->value = %d!\n", __FUNCTION__, param->value));
			break;
	case IW_AUTH_WPA_ENABLED:
		DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_AUTH_WPA_ENABLED - Driver supports WPA!(param->value = %d)\n", __FUNCTION__, param->value));
		break;
	default:
		return -EOPNOTSUPP;
}

	return 0;
}

int rt_ioctl_giwauth(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER   pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_param *param = &wrqu->param;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
    }

	switch (param->flags & IW_AUTH_INDEX) {
	case IW_AUTH_DROP_UNENCRYPTED:
        param->value = (pAdapter->StaCfg.WepStatus == Ndis802_11WEPDisabled) ? 0 : 1;
		break;

	case IW_AUTH_80211_AUTH_ALG:
        param->value = (pAdapter->StaCfg.AuthMode == Ndis802_11AuthModeShared) ? IW_AUTH_ALG_SHARED_KEY : IW_AUTH_ALG_OPEN_SYSTEM;
		break;

	case IW_AUTH_WPA_ENABLED:
		param->value = (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA) ? 1 : 0;
		break;

	default:
		return -EOPNOTSUPP;
	}
    DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_giwauth::param->value = %d!\n", param->value));
	return 0;
}

void fnSetCipherKey(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  INT             keyIdx,
    IN  UCHAR           CipherAlg,
    IN  BOOLEAN         bGTK,
    IN  struct iw_encode_ext *ext)
{
    NdisZeroMemory(&pAdapter->SharedKey[BSS0][keyIdx], sizeof(CIPHER_KEY));
    pAdapter->SharedKey[BSS0][keyIdx].KeyLen = LEN_TKIP_EK;
    NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].Key, ext->key, LEN_TKIP_EK);
    NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].TxMic, ext->key + LEN_TKIP_EK, LEN_TKIP_TXMICK);
    NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].RxMic, ext->key + LEN_TKIP_EK + LEN_TKIP_TXMICK, LEN_TKIP_RXMICK);
    pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CipherAlg;

    // Update group key information to ASIC Shared Key Table
	AsicAddSharedKeyEntry(pAdapter,
						  BSS0,
						  keyIdx,
						  pAdapter->SharedKey[BSS0][keyIdx].CipherAlg,
						  pAdapter->SharedKey[BSS0][keyIdx].Key,
						  pAdapter->SharedKey[BSS0][keyIdx].TxMic,
						  pAdapter->SharedKey[BSS0][keyIdx].RxMic);

    if (bGTK)
        // Update ASIC WCID attribute table and IVEIV table
	RTMPAddWcidAttributeEntry(pAdapter,
							  BSS0,
							  keyIdx,
							  pAdapter->SharedKey[BSS0][keyIdx].CipherAlg,
							  NULL);
    else
        // Update ASIC WCID attribute table and IVEIV table
	RTMPAddWcidAttributeEntry(pAdapter,
							  BSS0,
							  keyIdx,
							  pAdapter->SharedKey[BSS0][keyIdx].CipherAlg,
							  &pAdapter->MacTab.Content[BSSID_WCID]);
}

int rt_ioctl_siwencodeext(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
			{
    PRTMP_ADAPTER   pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_point *encoding = &wrqu->encoding;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
    int keyIdx, alg = ext->alg;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAdapter, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
	}

    if (encoding->flags & IW_ENCODE_DISABLED)
	{
        keyIdx = (encoding->flags & IW_ENCODE_INDEX) - 1;
        // set BSSID wcid entry of the Pair-wise Key table as no-security mode
	    AsicRemovePairwiseKeyEntry(pAdapter, BSS0, BSSID_WCID);
        pAdapter->SharedKey[BSS0][keyIdx].KeyLen = 0;
		pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_NONE;
		AsicRemoveSharedKeyEntry(pAdapter, 0, (UCHAR)keyIdx);
        NdisZeroMemory(&pAdapter->SharedKey[BSS0][keyIdx], sizeof(CIPHER_KEY));
        DBGPRINT(RT_DEBUG_TRACE, ("%s::Remove all keys!(encoding->flags = %x)\n", __FUNCTION__, encoding->flags));
    }
					else
    {
        // Get Key Index and convet to our own defined key index
	keyIdx = (encoding->flags & IW_ENCODE_INDEX) - 1;
	if((keyIdx < 0) || (keyIdx >= NR_WEP_KEYS))
		return -EINVAL;

        if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
        {
            pAdapter->StaCfg.DefaultKeyId = keyIdx;
            DBGPRINT(RT_DEBUG_TRACE, ("%s::DefaultKeyId = %d\n", __FUNCTION__, pAdapter->StaCfg.DefaultKeyId));
        }

        switch (alg) {
		case IW_ENCODE_ALG_NONE:
                DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_ENCODE_ALG_NONE\n", __FUNCTION__));
			break;
		case IW_ENCODE_ALG_WEP:
                DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_ENCODE_ALG_WEP - ext->key_len = %d, keyIdx = %d\n", __FUNCTION__, ext->key_len, keyIdx));
			if (ext->key_len == MAX_WEP_KEY_SIZE)
                {
				pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MAX_WEP_KEY_SIZE;
                    pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP128;
				}
			else if (ext->key_len == MIN_WEP_KEY_SIZE)
                {
                    pAdapter->SharedKey[BSS0][keyIdx].KeyLen = MIN_WEP_KEY_SIZE;
                    pAdapter->SharedKey[BSS0][keyIdx].CipherAlg = CIPHER_WEP64;
			}
			else
                    return -EINVAL;

                NdisZeroMemory(pAdapter->SharedKey[BSS0][keyIdx].Key,  16);
			    NdisMoveMemory(pAdapter->SharedKey[BSS0][keyIdx].Key, ext->key, ext->key_len);

				if (pAdapter->StaCfg.GroupCipher == Ndis802_11GroupWEP40Enabled ||
					pAdapter->StaCfg.GroupCipher == Ndis802_11GroupWEP104Enabled)
				{
					// Set Group key material to Asic
					AsicAddSharedKeyEntry(pAdapter, BSS0, keyIdx, pAdapter->SharedKey[BSS0][keyIdx].CipherAlg, pAdapter->SharedKey[BSS0][keyIdx].Key, NULL, NULL);
					// Update WCID attribute table and IVEIV table for this group key table
					RTMPAddWcidAttributeEntry(pAdapter, BSS0, keyIdx, pAdapter->SharedKey[BSS0][keyIdx].CipherAlg, NULL);
					STA_PORT_SECURED(pAdapter);
					// Indicate Connected for GUI
					pAdapter->IndicateMediaState = NdisMediaStateConnected;
				}
			break;
            case IW_ENCODE_ALG_TKIP:
                DBGPRINT(RT_DEBUG_TRACE, ("%s::IW_ENCODE_ALG_TKIP - keyIdx = %d, ext->key_len = %d\n", __FUNCTION__, keyIdx, ext->key_len));
                if (ext->key_len == 32)
                {
                    if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
                    {
                        fnSetCipherKey(pAdapter, keyIdx, CIPHER_TKIP, FALSE, ext);
                        if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA2)
                        {
                            //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
                            STA_PORT_SECURED(pAdapter);
                            pAdapter->IndicateMediaState = NdisMediaStateConnected;
                        }
		}
                    else if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
                    {
                        fnSetCipherKey(pAdapter, keyIdx, CIPHER_TKIP, TRUE, ext);

                        // set 802.1x port control
		        //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
		        STA_PORT_SECURED(pAdapter);
		        pAdapter->IndicateMediaState = NdisMediaStateConnected;
                    }
                }
                else
                    return -EINVAL;
                break;
            case IW_ENCODE_ALG_CCMP:
                if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
		{
                    fnSetCipherKey(pAdapter, keyIdx, CIPHER_AES, FALSE, ext);
                    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA2)
			//pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			STA_PORT_SECURED(pAdapter);
			pAdapter->IndicateMediaState = NdisMediaStateConnected;
                }
                else if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY)
                {
                    fnSetCipherKey(pAdapter, keyIdx, CIPHER_AES, TRUE, ext);

                    // set 802.1x port control
		        //pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
		        STA_PORT_SECURED(pAdapter);
		        pAdapter->IndicateMediaState = NdisMediaStateConnected;
                }
                break;
		default:
			return -EINVAL;
		}
    }

    return 0;
}

int
rt_ioctl_giwencodeext(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	PCHAR pKey = NULL;
	struct iw_point *encoding = &wrqu->encoding;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	int idx, max_key_len;

	DBGPRINT(RT_DEBUG_TRACE ,("===> rt_ioctl_giwencodeext\n"));

	max_key_len = encoding->length - sizeof(*ext);
	if (max_key_len < 0)
		return -EINVAL;

	idx = encoding->flags & IW_ENCODE_INDEX;
	if (idx)
	{
		if (idx < 1 || idx > 4)
			return -EINVAL;
		idx--;

		if ((pAd->StaCfg.WepStatus == Ndis802_11Encryption2Enabled) ||
			(pAd->StaCfg.WepStatus == Ndis802_11Encryption3Enabled))
		{
			if (idx != pAd->StaCfg.DefaultKeyId)
			{
				ext->key_len = 0;
				return 0;
			}
		}
	}
	else
		idx = pAd->StaCfg.DefaultKeyId;

	encoding->flags = idx + 1;
	memset(ext, 0, sizeof(*ext));

	ext->key_len = 0;
	switch(pAd->StaCfg.WepStatus) {
		case Ndis802_11WEPDisabled:
			ext->alg = IW_ENCODE_ALG_NONE;
			encoding->flags |= IW_ENCODE_DISABLED;
			break;
		case Ndis802_11WEPEnabled:
			ext->alg = IW_ENCODE_ALG_WEP;
			if (pAd->SharedKey[BSS0][idx].KeyLen > max_key_len)
				return -E2BIG;
			else
			{
				ext->key_len = pAd->SharedKey[BSS0][idx].KeyLen;
				pKey = (PCHAR)&(pAd->SharedKey[BSS0][idx].Key[0]);
			}
			break;
		case Ndis802_11Encryption2Enabled:
		case Ndis802_11Encryption3Enabled:
			if (pAd->StaCfg.WepStatus == Ndis802_11Encryption2Enabled)
				ext->alg = IW_ENCODE_ALG_TKIP;
			else
				ext->alg = IW_ENCODE_ALG_CCMP;

			if (max_key_len < 32)
				return -E2BIG;
			else
			{
				ext->key_len = 32;
				pKey = (PCHAR)&pAd->StaCfg.PMK[0];
			}
			break;
		default:
			return -EINVAL;
	}

	if (ext->key_len && pKey)
	{
		encoding->flags |= IW_ENCODE_ENABLED;
		memcpy(ext->key, pKey, ext->key_len);
	}

	return 0;
}

#ifdef SIOCSIWGENIE
int rt_ioctl_siwgenie(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);

	DBGPRINT(RT_DEBUG_TRACE ,("===> rt_ioctl_siwgenie\n"));
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
	pAd->StaCfg.bRSN_IE_FromWpaSupplicant = FALSE;
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
	if (wrqu->data.length > MAX_LEN_OF_RSNIE ||
	    (wrqu->data.length && extra == NULL))
		return -EINVAL;

	if (wrqu->data.length)
	{
		pAd->StaCfg.RSNIE_Len = wrqu->data.length;
		NdisMoveMemory(&pAd->StaCfg.RSN_IE[0], extra, pAd->StaCfg.RSNIE_Len);
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
		pAd->StaCfg.bRSN_IE_FromWpaSupplicant = TRUE;
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
	}
	else
	{
		pAd->StaCfg.RSNIE_Len = 0;
		NdisZeroMemory(&pAd->StaCfg.RSN_IE[0], MAX_LEN_OF_RSNIE);
	}

	return 0;
}
#endif // SIOCSIWGENIE //

int rt_ioctl_giwgenie(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);

	if ((pAd->StaCfg.RSNIE_Len == 0) ||
		(pAd->StaCfg.AuthMode < Ndis802_11AuthModeWPA))
	{
		wrqu->data.length = 0;
		return 0;
	}

#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
#ifdef SIOCSIWGENIE
	if (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_ENABLE)
	{
	if (wrqu->data.length < pAd->StaCfg.RSNIE_Len)
		return -E2BIG;

	wrqu->data.length = pAd->StaCfg.RSNIE_Len;
	memcpy(extra, &pAd->StaCfg.RSN_IE[0], pAd->StaCfg.RSNIE_Len);
	}
	else
#endif // SIOCSIWGENIE //
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
	{
		UCHAR RSNIe = IE_WPA;

		if (wrqu->data.length < (pAd->StaCfg.RSNIE_Len + 2)) // ID, Len
			return -E2BIG;
		wrqu->data.length = pAd->StaCfg.RSNIE_Len + 2;

		if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK) ||
            (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2))
			RSNIe = IE_RSN;

		extra[0] = (char)RSNIe;
		extra[1] = pAd->StaCfg.RSNIE_Len;
		memcpy(extra+2, &pAd->StaCfg.RSN_IE[0], pAd->StaCfg.RSNIE_Len);
	}

	return 0;
}

int rt_ioctl_siwpmksa(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
{
	PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
	struct iw_pmksa *pPmksa = (struct iw_pmksa *)wrqu->data.pointer;
	INT	CachedIdx = 0, idx = 0;

	if (pPmksa == NULL)
		return -EINVAL;

	DBGPRINT(RT_DEBUG_TRACE ,("===> rt_ioctl_siwpmksa\n"));
	switch(pPmksa->cmd)
	{
		case IW_PMKSA_FLUSH:
			NdisZeroMemory(pAd->StaCfg.SavedPMK, sizeof(BSSID_INFO)*PMKID_NO);
			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - IW_PMKSA_FLUSH\n"));
			break;
		case IW_PMKSA_REMOVE:
			for (CachedIdx = 0; CachedIdx < pAd->StaCfg.SavedPMKNum; CachedIdx++)
			{
		        // compare the BSSID
		        if (NdisEqualMemory(pPmksa->bssid.sa_data, pAd->StaCfg.SavedPMK[CachedIdx].BSSID, MAC_ADDR_LEN))
		        {
				NdisZeroMemory(pAd->StaCfg.SavedPMK[CachedIdx].BSSID, MAC_ADDR_LEN);
					NdisZeroMemory(pAd->StaCfg.SavedPMK[CachedIdx].PMKID, 16);
					for (idx = CachedIdx; idx < (pAd->StaCfg.SavedPMKNum - 1); idx++)
					{
						NdisMoveMemory(&pAd->StaCfg.SavedPMK[idx].BSSID[0], &pAd->StaCfg.SavedPMK[idx+1].BSSID[0], MAC_ADDR_LEN);
						NdisMoveMemory(&pAd->StaCfg.SavedPMK[idx].PMKID[0], &pAd->StaCfg.SavedPMK[idx+1].PMKID[0], 16);
					}
					pAd->StaCfg.SavedPMKNum--;
			        break;
		        }
	        }

			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - IW_PMKSA_REMOVE\n"));
			break;
		case IW_PMKSA_ADD:
			for (CachedIdx = 0; CachedIdx < pAd->StaCfg.SavedPMKNum; CachedIdx++)
			{
		        // compare the BSSID
		        if (NdisEqualMemory(pPmksa->bssid.sa_data, pAd->StaCfg.SavedPMK[CachedIdx].BSSID, MAC_ADDR_LEN))
			        break;
	        }

	        // Found, replace it
	        if (CachedIdx < PMKID_NO)
	        {
		        DBGPRINT(RT_DEBUG_OFF, ("Update PMKID, idx = %d\n", CachedIdx));
		        NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx].BSSID[0], pPmksa->bssid.sa_data, MAC_ADDR_LEN);
				NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx].PMKID[0], pPmksa->pmkid, 16);
		        pAd->StaCfg.SavedPMKNum++;
	        }
	        // Not found, replace the last one
	        else
	        {
		        // Randomly replace one
		        CachedIdx = (pPmksa->bssid.sa_data[5] % PMKID_NO);
		        DBGPRINT(RT_DEBUG_OFF, ("Update PMKID, idx = %d\n", CachedIdx));
		        NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx].BSSID[0], pPmksa->bssid.sa_data, MAC_ADDR_LEN);
				NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx].PMKID[0], pPmksa->pmkid, 16);
	        }

			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - IW_PMKSA_ADD\n"));
			break;
		default:
			DBGPRINT(RT_DEBUG_TRACE ,("rt_ioctl_siwpmksa - Unknow Command!!\n"));
			break;
	}

	return 0;
}
#endif // #if WIRELESS_EXT > 17

#ifdef DBG
static int
rt_private_ioctl_bbp(struct net_device *dev, struct iw_request_info *info,
		struct iw_point *wrq, char *extra)
			{
	PSTRING				this_char;
	PSTRING				value = NULL;
	UCHAR				regBBP = 0;
//	CHAR				arg[255]={0};
	UINT32				bbpId;
	UINT32				bbpValue;
	BOOLEAN				bIsPrintAllBBP = FALSE;
	INT					Status = 0;
    PRTMP_ADAPTER       pAdapter = RTMP_OS_NETDEV_GET_PRIV(dev);


	memset(extra, 0x00, IW_PRIV_SIZE_MASK);

	if (wrq->length > 1) //No parameters.
				{
		sprintf(extra, "\n");

		//Parsing Read or Write
		this_char = wrq->pointer;
		DBGPRINT(RT_DEBUG_TRACE, ("this_char=%s\n", this_char));
		if (!*this_char)
			goto next;

		if ((value = rtstrchr(this_char, '=')) != NULL)
			*value++ = 0;

		if (!value || !*value)
		{ //Read
			DBGPRINT(RT_DEBUG_TRACE, ("this_char=%s, value=%s\n", this_char, value));
			if (sscanf(this_char, "%d", &(bbpId)) == 1)
			{
				if (bbpId <= MAX_BBP_ID)
				{
#ifdef RALINK_ATE
					if (ATE_ON(pAdapter))
					{
						ATE_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
					}
					else
#endif // RALINK_ATE //
					{
					RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
					}
					sprintf(extra+strlen(extra), "R%02d[0x%02X]:%02X\n", bbpId, bbpId, regBBP);
                    wrq->length = strlen(extra) + 1; // 1: size of '\0'
					DBGPRINT(RT_DEBUG_TRACE, ("msg=%s\n", extra));
				}
				else
				{//Invalid parametes, so default printk all bbp
					bIsPrintAllBBP = TRUE;
					goto next;
				}
			}
			else
			{ //Invalid parametes, so default printk all bbp
				bIsPrintAllBBP = TRUE;
				goto next;
			}
		}
		else
		{ //Write
			if ((sscanf(this_char, "%d", &(bbpId)) == 1) && (sscanf(value, "%x", &(bbpValue)) == 1))
			{
				if (bbpId <= MAX_BBP_ID)
				{
#ifdef RALINK_ATE
					if (ATE_ON(pAdapter))
					{
						ATE_BBP_IO_WRITE8_BY_REG_ID(pAdapter, bbpId, bbpValue);
						/* read it back for showing */
						ATE_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
					}
					else
#endif // RALINK_ATE //
					{
					    RTMP_BBP_IO_WRITE8_BY_REG_ID(pAdapter, bbpId, bbpValue);
					/* read it back for showing */
					RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
			}
					sprintf(extra+strlen(extra), "R%02d[0x%02X]:%02X\n", bbpId, bbpId, regBBP);
                    wrq->length = strlen(extra) + 1; // 1: size of '\0'
					DBGPRINT(RT_DEBUG_TRACE, ("msg=%s\n", extra));
				}
				else
				{//Invalid parametes, so default printk all bbp
					bIsPrintAllBBP = TRUE;
					goto next;
				}
			}
			else
			{ //Invalid parametes, so default printk all bbp
				bIsPrintAllBBP = TRUE;
				goto next;
			}
		}
		}
	else
		bIsPrintAllBBP = TRUE;

next:
	if (bIsPrintAllBBP)
	{
		memset(extra, 0x00, IW_PRIV_SIZE_MASK);
		sprintf(extra, "\n");
		for (bbpId = 0; bbpId <= MAX_BBP_ID; bbpId++)
		{
		    if (strlen(extra) >= (IW_PRIV_SIZE_MASK - 20))
                break;
#ifdef RALINK_ATE
			if (ATE_ON(pAdapter))
			{
				ATE_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
			}
			else
#endif // RALINK_ATE //
			RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
			sprintf(extra+strlen(extra), "R%02d[0x%02X]:%02X    ", bbpId, bbpId, regBBP);
			if (bbpId%5 == 4)
			sprintf(extra+strlen(extra), "%03d = %02X\n", bbpId, regBBP);  // edit by johnli, change display format
		}

        wrq->length = strlen(extra) + 1; // 1: size of '\0'
        DBGPRINT(RT_DEBUG_TRACE, ("wrq->length = %d\n", wrq->length));
	}

	DBGPRINT(RT_DEBUG_TRACE, ("<==rt_private_ioctl_bbp\n\n"));

    return Status;
}
#endif // DBG //

int rt_ioctl_siwrate(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
    PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
    UINT32          rate = wrqu->bitrate.value, fixed = wrqu->bitrate.fixed;

    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_siwrate::Network is down!\n"));
	return -ENETDOWN;
	}

    DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_siwrate::(rate = %d, fixed = %d)\n", rate, fixed));
    /* rate = -1 => auto rate
       rate = X, fixed = 1 => (fixed rate X)
    */
    if (rate == -1)
    {
        //Auto Rate
        pAd->StaCfg.DesiredTransmitSetting.field.MCS = MCS_AUTO;
		pAd->StaCfg.bAutoTxRateSwitch = TRUE;
		if ((pAd->CommonCfg.PhyMode <= PHY_11G) ||
		    (pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE <= MODE_OFDM))
            RTMPSetDesiredRates(pAd, -1);

#ifdef DOT11_N_SUPPORT
            SetCommonHT(pAd);
#endif // DOT11_N_SUPPORT //
    }
    else
    {
        if (fixed)
        {
		pAd->StaCfg.bAutoTxRateSwitch = FALSE;
            if ((pAd->CommonCfg.PhyMode <= PHY_11G) ||
                (pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE <= MODE_OFDM))
                RTMPSetDesiredRates(pAd, rate);
            else
            {
                pAd->StaCfg.DesiredTransmitSetting.field.MCS = MCS_AUTO;
#ifdef DOT11_N_SUPPORT
                SetCommonHT(pAd);
#endif // DOT11_N_SUPPORT //
            }
            DBGPRINT(RT_DEBUG_TRACE, ("rt_ioctl_siwrate::(HtMcs=%d)\n",pAd->StaCfg.DesiredTransmitSetting.field.MCS));
        }
        else
        {
            // TODO: rate = X, fixed = 0 => (rates <= X)
            return -EOPNOTSUPP;
        }
    }

    return 0;
}

int rt_ioctl_giwrate(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
    PRTMP_ADAPTER   pAd = RTMP_OS_NETDEV_GET_PRIV(dev);
    int rate_index = 0, rate_count = 0;
    HTTRANSMIT_SETTING ht_setting;
/* Remove to global variable
    __s32 ralinkrate[] =
	{2,  4,   11,  22, // CCK
	12, 18,   24,  36, 48, 72, 96, 108, // OFDM
	13, 26,   39,  52,  78, 104, 117, 130, 26,  52,  78, 104, 156, 208, 234, 260, // 20MHz, 800ns GI, MCS: 0 ~ 15
	39, 78,  117, 156, 234, 312, 351, 390,										  // 20MHz, 800ns GI, MCS: 16 ~ 23
	27, 54,   81, 108, 162, 216, 243, 270, 54, 108, 162, 216, 324, 432, 486, 540, // 40MHz, 800ns GI, MCS: 0 ~ 15
	81, 162, 243, 324, 486, 648, 729, 810,										  // 40MHz, 800ns GI, MCS: 16 ~ 23
	14, 29,   43,  57,  87, 115, 130, 144, 29, 59,   87, 115, 173, 230, 260, 288, // 20MHz, 400ns GI, MCS: 0 ~ 15
	43, 87,  130, 173, 260, 317, 390, 433,										  // 20MHz, 400ns GI, MCS: 16 ~ 23
	30, 60,   90, 120, 180, 240, 270, 300, 60, 120, 180, 240, 360, 480, 540, 600, // 40MHz, 400ns GI, MCS: 0 ~ 15
	90, 180, 270, 360, 540, 720, 810, 900};										  // 40MHz, 400ns GI, MCS: 16 ~ 23
*/

    rate_count = sizeof(ralinkrate)/sizeof(__s32);
    //check if the interface is down
	if(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
	return -ENETDOWN;
	}

    if ((pAd->StaCfg.bAutoTxRateSwitch == FALSE) &&
        (INFRA_ON(pAd)) &&
        ((pAd->CommonCfg.PhyMode <= PHY_11G) || (pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE <= MODE_OFDM)))
        ht_setting.word = pAd->StaCfg.HTPhyMode.word;
    else
        ht_setting.word = pAd->MacTab.Content[BSSID_WCID].HTPhyMode.word;

#ifdef DOT11_N_SUPPORT
    if (ht_setting.field.MODE >= MODE_HTMIX)
    {
//	rate_index = 12 + ((UCHAR)ht_setting.field.BW *16) + ((UCHAR)ht_setting.field.ShortGI *32) + ((UCHAR)ht_setting.field.MCS);
	rate_index = 12 + ((UCHAR)ht_setting.field.BW *24) + ((UCHAR)ht_setting.field.ShortGI *48) + ((UCHAR)ht_setting.field.MCS);
    }
    else
#endif // DOT11_N_SUPPORT //
    if (ht_setting.field.MODE == MODE_OFDM)
	rate_index = (UCHAR)(ht_setting.field.MCS) + 4;
    else if (ht_setting.field.MODE == MODE_CCK)
	rate_index = (UCHAR)(ht_setting.field.MCS);

    if (rate_index < 0)
        rate_index = 0;

    if (rate_index > rate_count)
        rate_index = rate_count;

    wrqu->bitrate.value = ralinkrate[rate_index] * 500000;
    wrqu->bitrate.disabled = 0;

    return 0;
}

static const iw_handler rt_handler[] =
{
	(iw_handler) NULL,			            /* SIOCSIWCOMMIT */
	(iw_handler) rt_ioctl_giwname,			/* SIOCGIWNAME   */
	(iw_handler) NULL,			            /* SIOCSIWNWID   */
	(iw_handler) NULL,			            /* SIOCGIWNWID   */
	(iw_handler) rt_ioctl_siwfreq,		    /* SIOCSIWFREQ   */
	(iw_handler) rt_ioctl_giwfreq,		    /* SIOCGIWFREQ   */
	(iw_handler) rt_ioctl_siwmode,		    /* SIOCSIWMODE   */
	(iw_handler) rt_ioctl_giwmode,		    /* SIOCGIWMODE   */
	(iw_handler) NULL,		                /* SIOCSIWSENS   */
	(iw_handler) NULL,		                /* SIOCGIWSENS   */
	(iw_handler) NULL /* not used */,		/* SIOCSIWRANGE  */
	(iw_handler) rt_ioctl_giwrange,		    /* SIOCGIWRANGE  */
	(iw_handler) NULL /* not used */,		/* SIOCSIWPRIV   */
	(iw_handler) NULL /* kernel code */,    /* SIOCGIWPRIV   */
	(iw_handler) NULL /* not used */,		/* SIOCSIWSTATS  */
	(iw_handler) rt28xx_get_wireless_stats /* kernel code */,    /* SIOCGIWSTATS  */
	(iw_handler) NULL,		                /* SIOCSIWSPY    */
	(iw_handler) NULL,		                /* SIOCGIWSPY    */
	(iw_handler) NULL,				        /* SIOCSIWTHRSPY */
	(iw_handler) NULL,				        /* SIOCGIWTHRSPY */
	(iw_handler) rt_ioctl_siwap,            /* SIOCSIWAP     */
	(iw_handler) rt_ioctl_giwap,		    /* SIOCGIWAP     */
#ifdef SIOCSIWMLME
	(iw_handler) rt_ioctl_siwmlme,	        /* SIOCSIWMLME   */
#else
	(iw_handler) NULL,				        /* SIOCSIWMLME */
#endif // SIOCSIWMLME //
	(iw_handler) rt_ioctl_iwaplist,		    /* SIOCGIWAPLIST */
#ifdef SIOCGIWSCAN
	(iw_handler) rt_ioctl_siwscan,		    /* SIOCSIWSCAN   */
	(iw_handler) rt_ioctl_giwscan,		    /* SIOCGIWSCAN   */
#else
	(iw_handler) NULL,				        /* SIOCSIWSCAN   */
	(iw_handler) NULL,				        /* SIOCGIWSCAN   */
#endif /* SIOCGIWSCAN */
	(iw_handler) rt_ioctl_siwessid,		    /* SIOCSIWESSID  */
	(iw_handler) rt_ioctl_giwessid,		    /* SIOCGIWESSID  */
	(iw_handler) rt_ioctl_siwnickn,		    /* SIOCSIWNICKN  */
	(iw_handler) rt_ioctl_giwnickn,		    /* SIOCGIWNICKN  */
	(iw_handler) NULL,				        /* -- hole --    */
	(iw_handler) NULL,				        /* -- hole --    */
	(iw_handler) rt_ioctl_siwrate,          /* SIOCSIWRATE   */
	(iw_handler) rt_ioctl_giwrate,          /* SIOCGIWRATE   */
	(iw_handler) rt_ioctl_siwrts,		    /* SIOCSIWRTS    */
	(iw_handler) rt_ioctl_giwrts,		    /* SIOCGIWRTS    */
	(iw_handler) rt_ioctl_siwfrag,		    /* SIOCSIWFRAG   */
	(iw_handler) rt_ioctl_giwfrag,		    /* SIOCGIWFRAG   */
	(iw_handler) NULL,		                /* SIOCSIWTXPOW  */
	(iw_handler) NULL,		                /* SIOCGIWTXPOW  */
	(iw_handler) NULL,		                /* SIOCSIWRETRY  */
	(iw_handler) NULL,		                /* SIOCGIWRETRY  */
	(iw_handler) rt_ioctl_siwencode,		/* SIOCSIWENCODE */
	(iw_handler) rt_ioctl_giwencode,		/* SIOCGIWENCODE */
	(iw_handler) NULL,		                /* SIOCSIWPOWER  */
	(iw_handler) NULL,		                /* SIOCGIWPOWER  */
	(iw_handler) NULL,						/* -- hole -- */
	(iw_handler) NULL,						/* -- hole -- */
#if WIRELESS_EXT > 17
    (iw_handler) rt_ioctl_siwgenie,         /* SIOCSIWGENIE  */
	(iw_handler) rt_ioctl_giwgenie,         /* SIOCGIWGENIE  */
	(iw_handler) rt_ioctl_siwauth,		    /* SIOCSIWAUTH   */
	(iw_handler) rt_ioctl_giwauth,		    /* SIOCGIWAUTH   */
	(iw_handler) rt_ioctl_siwencodeext,	    /* SIOCSIWENCODEEXT */
	(iw_handler) rt_ioctl_giwencodeext,		/* SIOCGIWENCODEEXT */
	(iw_handler) rt_ioctl_siwpmksa,         /* SIOCSIWPMKSA  */
#endif
};

static const iw_handler rt_priv_handlers[] = {
	(iw_handler) NULL, /* + 0x00 */
	(iw_handler) NULL, /* + 0x01 */
	(iw_handler) rt_ioctl_setparam, /* + 0x02 */
#ifdef DBG
	(iw_handler) rt_private_ioctl_bbp, /* + 0x03 */
#else
	(iw_handler) NULL, /* + 0x03 */
#endif
	(iw_handler) NULL, /* + 0x04 */
	(iw_handler) NULL, /* + 0x05 */
	(iw_handler) NULL, /* + 0x06 */
	(iw_handler) NULL, /* + 0x07 */
	(iw_handler) NULL, /* + 0x08 */
	(iw_handler) rt_private_get_statistics, /* + 0x09 */
	(iw_handler) NULL, /* + 0x0A */
	(iw_handler) NULL, /* + 0x0B */
	(iw_handler) NULL, /* + 0x0C */
	(iw_handler) NULL, /* + 0x0D */
	(iw_handler) NULL, /* + 0x0E */
	(iw_handler) NULL, /* + 0x0F */
	(iw_handler) NULL, /* + 0x10 */
	(iw_handler) rt_private_show, /* + 0x11 */
    (iw_handler) NULL, /* + 0x12 */
	(iw_handler) NULL, /* + 0x13 */
    (iw_handler) NULL, /* + 0x14 */
	(iw_handler) NULL, /* + 0x15 */
    (iw_handler) NULL, /* + 0x16 */
	(iw_handler) NULL, /* + 0x17 */
	(iw_handler) NULL, /* + 0x18 */
};

const struct iw_handler_def rt28xx_iw_handler_def =
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
	.standard	= (iw_handler *) rt_handler,
	.num_standard	= sizeof(rt_handler) / sizeof(iw_handler),
	.private	= (iw_handler *) rt_priv_handlers,
	.num_private		= N(rt_priv_handlers),
	.private_args	= (struct iw_priv_args *) privtab,
	.num_private_args	= N(privtab),
#if IW_HANDLER_VERSION >= 7
    .get_wireless_stats = rt28xx_get_wireless_stats,
#endif
};

INT RTMPSetInformation(
    IN  PRTMP_ADAPTER pAd,
    IN  OUT struct ifreq    *rq,
    IN  INT                 cmd)
{
    struct iwreq                        *wrq = (struct iwreq *) rq;
    NDIS_802_11_SSID                    Ssid;
    NDIS_802_11_MAC_ADDRESS             Bssid;
    RT_802_11_PHY_MODE                  PhyMode;
    RT_802_11_STA_CONFIG                StaConfig;
    NDIS_802_11_RATES                   aryRates;
    RT_802_11_PREAMBLE                  Preamble;
    NDIS_802_11_WEP_STATUS              WepStatus;
    NDIS_802_11_AUTHENTICATION_MODE     AuthMode = Ndis802_11AuthModeMax;
    NDIS_802_11_NETWORK_INFRASTRUCTURE  BssType;
    NDIS_802_11_RTS_THRESHOLD           RtsThresh;
    NDIS_802_11_FRAGMENTATION_THRESHOLD FragThresh;
    NDIS_802_11_POWER_MODE              PowerMode;
    PNDIS_802_11_KEY                    pKey = NULL;
    PNDIS_802_11_WEP			        pWepKey =NULL;
    PNDIS_802_11_REMOVE_KEY             pRemoveKey = NULL;
    NDIS_802_11_CONFIGURATION           Config, *pConfig = NULL;
    NDIS_802_11_NETWORK_TYPE            NetType;
    ULONG                               Now;
    UINT                                KeyIdx = 0;
    INT                                 Status = NDIS_STATUS_SUCCESS, MaxPhyMode = PHY_11G;
    ULONG                               PowerTemp;
    BOOLEAN                             RadioState;
    BOOLEAN                             StateMachineTouched = FALSE;
     PNDIS_802_11_PASSPHRASE                    ppassphrase = NULL;
#ifdef DOT11_N_SUPPORT
	OID_SET_HT_PHYMODE					HT_PhyMode;	//11n ,kathy
#endif // DOT11_N_SUPPORT //
#ifdef WPA_SUPPLICANT_SUPPORT
    PNDIS_802_11_PMKID                  pPmkId = NULL;
    BOOLEAN				                IEEE8021xState = FALSE;
    BOOLEAN				                IEEE8021x_required_keys = FALSE;
    UCHAR                               wpa_supplicant_enable = 0;
#endif // WPA_SUPPLICANT_SUPPORT //

#ifdef SNMP_SUPPORT
	TX_RTY_CFG_STRUC			tx_rty_cfg;
	ULONG						ShortRetryLimit, LongRetryLimit;
	UCHAR						ctmp;
#endif // SNMP_SUPPORT //




#ifdef DOT11_N_SUPPORT
	MaxPhyMode = PHY_11N_5G;
#endif // DOT11_N_SUPPORT //

	DBGPRINT(RT_DEBUG_TRACE, ("-->RTMPSetInformation(),	0x%08x\n", cmd&0x7FFF));
	switch(cmd & 0x7FFF) {
		case RT_OID_802_11_COUNTRY_REGION:
			if (wrq->u.data.length < sizeof(UCHAR))
				Status = -EINVAL;
			// Only avaliable when EEPROM not programming
            else if (!(pAd->CommonCfg.CountryRegion & 0x80) && !(pAd->CommonCfg.CountryRegionForABand & 0x80))
			{
				ULONG   Country;
				UCHAR	TmpPhy;

				Status = copy_from_user(&Country, wrq->u.data.pointer, wrq->u.data.length);
				pAd->CommonCfg.CountryRegion = (UCHAR)(Country & 0x000000FF);
				pAd->CommonCfg.CountryRegionForABand = (UCHAR)((Country >> 8) & 0x000000FF);
                TmpPhy = pAd->CommonCfg.PhyMode;
				pAd->CommonCfg.PhyMode = 0xff;
				// Build all corresponding channel information
				RTMPSetPhyMode(pAd, TmpPhy);
#ifdef DOT11_N_SUPPORT
				SetCommonHT(pAd);
#endif // DOT11_N_SUPPORT //
				DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_COUNTRY_REGION (A:%d  B/G:%d)\n", pAd->CommonCfg.CountryRegionForABand,
				    pAd->CommonCfg.CountryRegion));
            }
            break;
        case OID_802_11_BSSID_LIST_SCAN:
            Now = jiffies;
			DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_BSSID_LIST_SCAN, TxCnt = %d \n", pAd->RalinkCounters.LastOneSecTotalTxCount));

            if (MONITOR_ON(pAd))
            {
                DBGPRINT(RT_DEBUG_TRACE, ("!!! Driver is in Monitor Mode now !!!\n"));
                break;
            }

			//Benson add 20080527, when radio off, sta don't need to scan
			if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
				break;

			if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
			{
                DBGPRINT(RT_DEBUG_TRACE, ("!!! Driver is scanning now !!!\n"));
				pAd->StaCfg.bScanReqIsFromWebUI = TRUE;
				Status = NDIS_STATUS_SUCCESS;
                break;
            }

			if (pAd->RalinkCounters.LastOneSecTotalTxCount > 100)
            {
                DBGPRINT(RT_DEBUG_TRACE, ("!!! Link UP, ignore this set::OID_802_11_BSSID_LIST_SCAN\n"));
				Status = NDIS_STATUS_SUCCESS;
				pAd->StaCfg.ScanCnt = 99;		// Prevent auto scan triggered by this OID
				break;
            }

            if ((OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED)) &&
				((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA) ||
				(pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
				(pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) ||
				(pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)) &&
                (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED))
            {
                DBGPRINT(RT_DEBUG_TRACE, ("!!! Link UP, Port Not Secured! ignore this set::OID_802_11_BSSID_LIST_SCAN\n"));
				Status = NDIS_STATUS_SUCCESS;
				pAd->StaCfg.ScanCnt = 99;		// Prevent auto scan triggered by this OID
				break;
            }


            if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
            {
                RTMP_MLME_RESET_STATE_MACHINE(pAd);
                DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
            }

            // tell CNTL state machine to call NdisMSetInformationComplete() after completing
            // this request, because this request is initiated by NDIS.
            pAd->MlmeAux.CurrReqIsFromNdis = FALSE;
            // Reset allowed scan retries
            pAd->StaCfg.ScanCnt = 0;
            pAd->StaCfg.LastScanTime = Now;

			pAd->StaCfg.bScanReqIsFromWebUI = TRUE;
            RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);
            MlmeEnqueue(pAd,
                        MLME_CNTL_STATE_MACHINE,
                        OID_802_11_BSSID_LIST_SCAN,
                        0,
                        NULL);

            Status = NDIS_STATUS_SUCCESS;
            StateMachineTouched = TRUE;
            break;
        case OID_802_11_SSID:
            if (wrq->u.data.length != sizeof(NDIS_802_11_SSID))
                Status = -EINVAL;
            else
            {
		PSTRING pSsidString = NULL;
                Status = copy_from_user(&Ssid, wrq->u.data.pointer, wrq->u.data.length);

				DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_SSID (Len=%d,Ssid=%s)\n", Ssid.SsidLength, Ssid.Ssid));
                if (Ssid.SsidLength > MAX_LEN_OF_SSID)
                    Status = -EINVAL;
                else
                {
			if (Ssid.SsidLength == 0)
				{
				Set_SSID_Proc(pAd, "");
				}
					else
                    {
				pSsidString = (PSTRING)kmalloc(MAX_LEN_OF_SSID+1, MEM_ALLOC_FLAG);
						if (pSsidString)
						{
							NdisZeroMemory(pSsidString, MAX_LEN_OF_SSID+1);
							NdisMoveMemory(pSsidString, Ssid.Ssid, Ssid.SsidLength);
							Set_SSID_Proc(pAd, pSsidString);
							kfree(pSsidString);
						}
						else
							Status = -ENOMEM;
                    }
                }
            }
            break;
		case OID_802_11_SET_PASSPHRASE:
	    ppassphrase= kmalloc(wrq->u.data.length, MEM_ALLOC_FLAG);

	    if(ppassphrase== NULL)
            {
		Status = -ENOMEM;
				DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_SET_PASSPHRASE, Failed!!\n"));
		break;
            }
			else
		{
		Status = copy_from_user(ppassphrase, wrq->u.data.pointer, wrq->u.data.length);

				if (Status)
		{
			Status  = -EINVAL;
			DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_SET_PASSPHRASE, Failed (length mismatch)!!\n"));
			}
			else
			{
					if(ppassphrase->KeyLength < 8 || ppassphrase->KeyLength > 64)
					{
						Status  = -EINVAL;
			DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_SET_PASSPHRASE, Failed (len less than 8 or greater than 64)!!\n"));
					}
					else
					{
						// set key passphrase and length
						NdisZeroMemory(pAd->StaCfg.WpaPassPhrase, 64);
					NdisMoveMemory(pAd->StaCfg.WpaPassPhrase, &ppassphrase->KeyMaterial, ppassphrase->KeyLength);
						pAd->StaCfg.WpaPassPhraseLen = ppassphrase->KeyLength;
						hex_dump("pAd->StaCfg.WpaPassPhrase", pAd->StaCfg.WpaPassPhrase, 64);
						printk("WpaPassPhrase=%s\n",pAd->StaCfg.WpaPassPhrase);
					}
                }
            }
		kfree(ppassphrase);
			break;

        case OID_802_11_BSSID:
            if (wrq->u.data.length != sizeof(NDIS_802_11_MAC_ADDRESS))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&Bssid, wrq->u.data.pointer, wrq->u.data.length);

                // tell CNTL state machine to call NdisMSetInformationComplete() after completing
                // this request, because this request is initiated by NDIS.
                pAd->MlmeAux.CurrReqIsFromNdis = FALSE;

				// Prevent to connect AP again in STAMlmePeriodicExec
				pAd->MlmeAux.AutoReconnectSsidLen= 32;

                // Reset allowed scan retries
				pAd->StaCfg.ScanCnt = 0;

                if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
                {
                    RTMP_MLME_RESET_STATE_MACHINE(pAd);
                    DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
                }
                MlmeEnqueue(pAd,
                            MLME_CNTL_STATE_MACHINE,
                            OID_802_11_BSSID,
                            sizeof(NDIS_802_11_MAC_ADDRESS),
                            (VOID *)&Bssid);
                Status = NDIS_STATUS_SUCCESS;
                StateMachineTouched = TRUE;

                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_BSSID %02x:%02x:%02x:%02x:%02x:%02x\n",
                                        Bssid[0], Bssid[1], Bssid[2], Bssid[3], Bssid[4], Bssid[5]));
            }
            break;
        case RT_OID_802_11_RADIO:
            if (wrq->u.data.length != sizeof(BOOLEAN))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&RadioState, wrq->u.data.pointer, wrq->u.data.length);
                DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_RADIO (=%d)\n", RadioState));
                if (pAd->StaCfg.bSwRadio != RadioState)
                {
                    pAd->StaCfg.bSwRadio = RadioState;
                    if (pAd->StaCfg.bRadio != (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio))
                    {
                        pAd->StaCfg.bRadio = (pAd->StaCfg.bHwRadio && pAd->StaCfg.bSwRadio);
                        if (pAd->StaCfg.bRadio == TRUE)
                        {
                            MlmeRadioOn(pAd);
                            // Update extra information
							pAd->ExtraInfo = EXTRA_INFO_CLEAR;
                        }
                        else
                        {
				if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
				            {
				                if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
						        {
						            RTMP_MLME_RESET_STATE_MACHINE(pAd);
						            DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
						        }
				            }

                            MlmeRadioOff(pAd);
                            // Update extra information
							pAd->ExtraInfo = SW_RADIO_OFF;
                        }
                    }
                }
            }
            break;
        case RT_OID_802_11_PHY_MODE:
            if (wrq->u.data.length != sizeof(RT_802_11_PHY_MODE))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&PhyMode, wrq->u.data.pointer, wrq->u.data.length);
				if (PhyMode <= MaxPhyMode)
				{
                RTMPSetPhyMode(pAd, PhyMode);
#ifdef DOT11_N_SUPPORT
				SetCommonHT(pAd);
#endif // DOT11_N_SUPPORT //
				}
                DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_PHY_MODE (=%d)\n", PhyMode));
            }
            break;
        case RT_OID_802_11_STA_CONFIG:
            if (wrq->u.data.length != sizeof(RT_802_11_STA_CONFIG))
                Status  = -EINVAL;
            else
            {
		UINT32	Value;

                Status = copy_from_user(&StaConfig, wrq->u.data.pointer, wrq->u.data.length);
                pAd->CommonCfg.bEnableTxBurst = StaConfig.EnableTxBurst;
                pAd->CommonCfg.UseBGProtection = StaConfig.UseBGProtection;
                pAd->CommonCfg.bUseShortSlotTime = 1; // 2003-10-30 always SHORT SLOT capable
                if ((pAd->CommonCfg.PhyMode != StaConfig.AdhocMode) &&
					(StaConfig.AdhocMode <= MaxPhyMode))
                {
                    // allow dynamic change of "USE OFDM rate or not" in ADHOC mode
                    // if setting changed, need to reset current TX rate as well as BEACON frame format
                    if (pAd->StaCfg.BssType == BSS_ADHOC)
                    {
			pAd->CommonCfg.PhyMode = StaConfig.AdhocMode;
			RTMPSetPhyMode(pAd, PhyMode);
                        MlmeUpdateTxRates(pAd, FALSE, 0);
                        MakeIbssBeacon(pAd);           // re-build BEACON frame
                        AsicEnableIbssSync(pAd);   // copy to on-chip memory
                    }
                }
                DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_SET_STA_CONFIG (Burst=%d, Protection=%ld,ShortSlot=%d\n",
                                        pAd->CommonCfg.bEnableTxBurst,
                                        pAd->CommonCfg.UseBGProtection,
                                        pAd->CommonCfg.bUseShortSlotTime));

				if (pAd->CommonCfg.PSPXlink)
					Value = PSPXLINK;
				else
					Value = STANORMAL;
				RTMP_IO_WRITE32(pAd, RX_FILTR_CFG, Value);
				Value = 0;
				RTMP_IO_READ32(pAd, MAC_SYS_CTRL, &Value);
				Value &= (~0x80);
				RTMP_IO_WRITE32(pAd, MAC_SYS_CTRL, Value);
            }
            break;
        case OID_802_11_DESIRED_RATES:
            if (wrq->u.data.length != sizeof(NDIS_802_11_RATES))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&aryRates, wrq->u.data.pointer, wrq->u.data.length);
                NdisZeroMemory(pAd->CommonCfg.DesireRate, MAX_LEN_OF_SUPPORTED_RATES);
                NdisMoveMemory(pAd->CommonCfg.DesireRate, &aryRates, sizeof(NDIS_802_11_RATES));
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_DESIRED_RATES (%02x,%02x,%02x,%02x,%02x,%02x,%02x,%02x)\n",
                    pAd->CommonCfg.DesireRate[0],pAd->CommonCfg.DesireRate[1],
                    pAd->CommonCfg.DesireRate[2],pAd->CommonCfg.DesireRate[3],
                    pAd->CommonCfg.DesireRate[4],pAd->CommonCfg.DesireRate[5],
                    pAd->CommonCfg.DesireRate[6],pAd->CommonCfg.DesireRate[7] ));
                // Changing DesiredRate may affect the MAX TX rate we used to TX frames out
                MlmeUpdateTxRates(pAd, FALSE, 0);
            }
            break;
        case RT_OID_802_11_PREAMBLE:
            if (wrq->u.data.length != sizeof(RT_802_11_PREAMBLE))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&Preamble, wrq->u.data.pointer, wrq->u.data.length);
                if (Preamble == Rt802_11PreambleShort)
                {
                    pAd->CommonCfg.TxPreamble = Preamble;
                    MlmeSetTxPreamble(pAd, Rt802_11PreambleShort);
                }
                else if ((Preamble == Rt802_11PreambleLong) || (Preamble == Rt802_11PreambleAuto))
                {
                    // if user wants AUTO, initialize to LONG here, then change according to AP's
                    // capability upon association.
                    pAd->CommonCfg.TxPreamble = Preamble;
                    MlmeSetTxPreamble(pAd, Rt802_11PreambleLong);
                }
                else
                {
                    Status = -EINVAL;
                    break;
                }
                DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_PREAMBLE (=%d)\n", Preamble));
            }
            break;
        case OID_802_11_WEP_STATUS:
            if (wrq->u.data.length != sizeof(NDIS_802_11_WEP_STATUS))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&WepStatus, wrq->u.data.pointer, wrq->u.data.length);
                // Since TKIP, AES, WEP are all supported. It should not have any invalid setting
                if (WepStatus <= Ndis802_11Encryption3KeyAbsent)
                {
                    if (pAd->StaCfg.WepStatus != WepStatus)
                    {
                        // Config has changed
                        pAd->bConfigChanged = TRUE;
                    }
                    pAd->StaCfg.WepStatus     = WepStatus;
                    pAd->StaCfg.OrigWepStatus = WepStatus;
                    pAd->StaCfg.PairCipher    = WepStatus;
			pAd->StaCfg.GroupCipher   = WepStatus;
                }
                else
                {
                    Status  = -EINVAL;
                    break;
                }
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_WEP_STATUS (=%d)\n",WepStatus));
            }
            break;
        case OID_802_11_AUTHENTICATION_MODE:
            if (wrq->u.data.length != sizeof(NDIS_802_11_AUTHENTICATION_MODE))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&AuthMode, wrq->u.data.pointer, wrq->u.data.length);
                if (AuthMode > Ndis802_11AuthModeMax)
                {
                    Status  = -EINVAL;
                    break;
                }
                else
                {
                    if (pAd->StaCfg.AuthMode != AuthMode)
                    {
                        // Config has changed
                        pAd->bConfigChanged = TRUE;
                    }
                    pAd->StaCfg.AuthMode = AuthMode;
                }
                pAd->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_AUTHENTICATION_MODE (=%d) \n",pAd->StaCfg.AuthMode));
            }
            break;
        case OID_802_11_INFRASTRUCTURE_MODE:
            if (wrq->u.data.length != sizeof(NDIS_802_11_NETWORK_INFRASTRUCTURE))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&BssType, wrq->u.data.pointer, wrq->u.data.length);

				if (BssType == Ndis802_11IBSS)
					Set_NetworkType_Proc(pAd, "Adhoc");
				else if (BssType == Ndis802_11Infrastructure)
					Set_NetworkType_Proc(pAd, "Infra");
                else if (BssType == Ndis802_11Monitor)
					Set_NetworkType_Proc(pAd, "Monitor");
                else
                {
                    Status  = -EINVAL;
                    DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_INFRASTRUCTURE_MODE (unknown)\n"));
                }
            }
            break;
	 case OID_802_11_REMOVE_WEP:
            DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_REMOVE_WEP\n"));
            if (wrq->u.data.length != sizeof(NDIS_802_11_KEY_INDEX))
            {
				Status = -EINVAL;
            }
            else
            {
		KeyIdx = *(NDIS_802_11_KEY_INDEX *) wrq->u.data.pointer;

		if (KeyIdx & 0x80000000)
		{
			// Should never set default bit when remove key
			Status = -EINVAL;
		}
		else
		{
			KeyIdx = KeyIdx & 0x0fffffff;
			if (KeyIdx >= 4){
				Status = -EINVAL;
			}
			else
			{
						pAd->SharedKey[BSS0][KeyIdx].KeyLen = 0;
						pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_NONE;
						AsicRemoveSharedKeyEntry(pAd, 0, (UCHAR)KeyIdx);
			}
		}
            }
            break;
        case RT_OID_802_11_RESET_COUNTERS:
            NdisZeroMemory(&pAd->WlanCounters, sizeof(COUNTER_802_11));
            NdisZeroMemory(&pAd->Counters8023, sizeof(COUNTER_802_3));
            NdisZeroMemory(&pAd->RalinkCounters, sizeof(COUNTER_RALINK));
            pAd->Counters8023.RxNoBuffer   = 0;
			pAd->Counters8023.GoodReceives = 0;
			pAd->Counters8023.RxNoBuffer   = 0;
            DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_RESET_COUNTERS \n"));
            break;
        case OID_802_11_RTS_THRESHOLD:
            if (wrq->u.data.length != sizeof(NDIS_802_11_RTS_THRESHOLD))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&RtsThresh, wrq->u.data.pointer, wrq->u.data.length);
                if (RtsThresh > MAX_RTS_THRESHOLD)
                    Status  = -EINVAL;
                else
                    pAd->CommonCfg.RtsThreshold = (USHORT)RtsThresh;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_RTS_THRESHOLD (=%ld)\n",RtsThresh));
            break;
        case OID_802_11_FRAGMENTATION_THRESHOLD:
            if (wrq->u.data.length != sizeof(NDIS_802_11_FRAGMENTATION_THRESHOLD))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&FragThresh, wrq->u.data.pointer, wrq->u.data.length);
                pAd->CommonCfg.bUseZeroToDisableFragment = FALSE;
                if (FragThresh > MAX_FRAG_THRESHOLD || FragThresh < MIN_FRAG_THRESHOLD)
                {
                    if (FragThresh == 0)
                    {
                        pAd->CommonCfg.FragmentThreshold = MAX_FRAG_THRESHOLD;
                        pAd->CommonCfg.bUseZeroToDisableFragment = TRUE;
                    }
                    else
                        Status  = -EINVAL;
                }
                else
                    pAd->CommonCfg.FragmentThreshold = (USHORT)FragThresh;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_FRAGMENTATION_THRESHOLD (=%ld) \n",FragThresh));
            break;
        case OID_802_11_POWER_MODE:
            if (wrq->u.data.length != sizeof(NDIS_802_11_POWER_MODE))
                Status = -EINVAL;
            else
            {
                Status = copy_from_user(&PowerMode, wrq->u.data.pointer, wrq->u.data.length);
                if (PowerMode == Ndis802_11PowerModeCAM)
			Set_PSMode_Proc(pAd, "CAM");
                else if (PowerMode == Ndis802_11PowerModeMAX_PSP)
			Set_PSMode_Proc(pAd, "Max_PSP");
                else if (PowerMode == Ndis802_11PowerModeFast_PSP)
					Set_PSMode_Proc(pAd, "Fast_PSP");
                else if (PowerMode == Ndis802_11PowerModeLegacy_PSP)
					Set_PSMode_Proc(pAd, "Legacy_PSP");
                else
                    Status = -EINVAL;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_POWER_MODE (=%d)\n",PowerMode));
            break;
         case RT_OID_802_11_TX_POWER_LEVEL_1:
			if (wrq->u.data.length  < sizeof(ULONG))
				Status = -EINVAL;
			else
			{
				Status = copy_from_user(&PowerTemp, wrq->u.data.pointer, wrq->u.data.length);
				if (PowerTemp > 100)
					PowerTemp = 0xffffffff;  // AUTO
				pAd->CommonCfg.TxPowerDefault = PowerTemp; //keep current setting.
				pAd->CommonCfg.TxPowerPercentage = pAd->CommonCfg.TxPowerDefault;
                DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_TX_POWER_LEVEL_1 (=%ld)\n", pAd->CommonCfg.TxPowerPercentage));
			}
	        break;
		case OID_802_11_NETWORK_TYPE_IN_USE:
			if (wrq->u.data.length != sizeof(NDIS_802_11_NETWORK_TYPE))
				Status = -EINVAL;
			else
			{
				Status = copy_from_user(&NetType, wrq->u.data.pointer, wrq->u.data.length);

				if (NetType == Ndis802_11DS)
					RTMPSetPhyMode(pAd, PHY_11B);
				else if (NetType == Ndis802_11OFDM24)
					RTMPSetPhyMode(pAd, PHY_11BG_MIXED);
				else if (NetType == Ndis802_11OFDM5)
					RTMPSetPhyMode(pAd, PHY_11A);
				else
					Status = -EINVAL;
#ifdef DOT11_N_SUPPORT
				if (Status == NDIS_STATUS_SUCCESS)
					SetCommonHT(pAd);
#endif // DOT11_N_SUPPORT //
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_NETWORK_TYPE_IN_USE (=%d)\n",NetType));
		    }
			break;
        // For WPA PSK PMK key
        case RT_OID_802_11_ADD_WPA:
            pKey = kmalloc(wrq->u.data.length, MEM_ALLOC_FLAG);
            if(pKey == NULL)
            {
                Status = -ENOMEM;
                break;
            }

            Status = copy_from_user(pKey, wrq->u.data.pointer, wrq->u.data.length);
            if (pKey->Length != wrq->u.data.length)
            {
                Status  = -EINVAL;
                DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_ADD_WPA, Failed!!\n"));
            }
            else
            {
                if ((pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPAPSK) &&
				    (pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPA2PSK) &&
				    (pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPANone) )
                {
                    Status = -EOPNOTSUPP;
                    DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_ADD_WPA, Failed!! [AuthMode != WPAPSK/WPA2PSK/WPANONE]\n"));
                }
                else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
						 (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK) ||
						 (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone) )     // Only for WPA PSK mode
				{
                    NdisMoveMemory(pAd->StaCfg.PMK, &pKey->KeyMaterial, pKey->KeyLength);
                    // Use RaConfig as PSK agent.
                    // Start STA supplicant state machine
                    if (pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPANone)
                        pAd->StaCfg.WpaState = SS_START;

                    DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_ADD_WPA (id=0x%x, Len=%d-byte)\n", pKey->KeyIndex, pKey->KeyLength));
                }
                else
                {
                    pAd->StaCfg.WpaState = SS_NOTUSE;
                    DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_ADD_WPA (id=0x%x, Len=%d-byte)\n", pKey->KeyIndex, pKey->KeyLength));
                }
            }
            kfree(pKey);
            break;
        case OID_802_11_REMOVE_KEY:
            pRemoveKey = kmalloc(wrq->u.data.length, MEM_ALLOC_FLAG);
            if(pRemoveKey == NULL)
            {
                Status = -ENOMEM;
                break;
            }

            Status = copy_from_user(pRemoveKey, wrq->u.data.pointer, wrq->u.data.length);
            if (pRemoveKey->Length != wrq->u.data.length)
            {
                Status  = -EINVAL;
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_REMOVE_KEY, Failed!!\n"));
            }
            else
            {
                if (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
                {
                    RTMPWPARemoveKeyProc(pAd, pRemoveKey);
                    DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_REMOVE_KEY, Remove WPA Key!!\n"));
                }
                else
                {
                    KeyIdx = pRemoveKey->KeyIndex;

                    if (KeyIdx & 0x80000000)
                    {
                        // Should never set default bit when remove key
                        Status  = -EINVAL;
                        DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_REMOVE_KEY, Failed!!(Should never set default bit when remove key)\n"));
                    }
                    else
                    {
                        KeyIdx = KeyIdx & 0x0fffffff;
                        if (KeyIdx > 3)
                        {
                            Status  = -EINVAL;
                            DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_REMOVE_KEY, Failed!!(KeyId[%d] out of range)\n", KeyIdx));
                        }
                        else
                        {
                            pAd->SharedKey[BSS0][KeyIdx].KeyLen = 0;
                            pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CIPHER_NONE;
                            AsicRemoveSharedKeyEntry(pAd, 0, (UCHAR)KeyIdx);
                            DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_REMOVE_KEY (id=0x%x, Len=%d-byte)\n", pRemoveKey->KeyIndex, pRemoveKey->Length));
                        }
                    }
                }
            }
            kfree(pRemoveKey);
            break;
        // New for WPA
        case OID_802_11_ADD_KEY:
            pKey = kmalloc(wrq->u.data.length, MEM_ALLOC_FLAG);
            if(pKey == NULL)
            {
                Status = -ENOMEM;
                break;
            }
            Status = copy_from_user(pKey, wrq->u.data.pointer, wrq->u.data.length);
            if (pKey->Length != wrq->u.data.length)
            {
                Status  = -EINVAL;
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_ADD_KEY, Failed!!\n"));
            }
            else
            {
                RTMPAddKey(pAd, pKey);
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_ADD_KEY (id=0x%x, Len=%d-byte)\n", pKey->KeyIndex, pKey->KeyLength));
            }
            kfree(pKey);
            break;
        case OID_802_11_CONFIGURATION:
            if (wrq->u.data.length != sizeof(NDIS_802_11_CONFIGURATION))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&Config, wrq->u.data.pointer, wrq->u.data.length);
                pConfig = &Config;

                if ((pConfig->BeaconPeriod >= 20) && (pConfig->BeaconPeriod <=400))
                     pAd->CommonCfg.BeaconPeriod = (USHORT) pConfig->BeaconPeriod;

                pAd->StaActive.AtimWin = (USHORT) pConfig->ATIMWindow;
                MAP_KHZ_TO_CHANNEL_ID(pConfig->DSConfig, pAd->CommonCfg.Channel);
                //
				// Save the channel on MlmeAux for CntlOidRTBssidProc used.
				//
				pAd->MlmeAux.Channel = pAd->CommonCfg.Channel;

                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_CONFIGURATION (BeacnPeriod=%ld,AtimW=%ld,Ch=%d)\n",
                    pConfig->BeaconPeriod, pConfig->ATIMWindow, pAd->CommonCfg.Channel));
                // Config has changed
                pAd->bConfigChanged = TRUE;
            }
            break;
#ifdef DOT11_N_SUPPORT
		case RT_OID_802_11_SET_HT_PHYMODE:
			if (wrq->u.data.length	!= sizeof(OID_SET_HT_PHYMODE))
				Status = -EINVAL;
			else
			{
			    POID_SET_HT_PHYMODE	pHTPhyMode = &HT_PhyMode;

				Status = copy_from_user(&HT_PhyMode, wrq->u.data.pointer, wrq->u.data.length);
				DBGPRINT(RT_DEBUG_TRACE, ("Set::pHTPhyMode	(PhyMode = %d,TransmitNo = %d, HtMode =	%d,	ExtOffset =	%d , MCS = %d, BW =	%d,	STBC = %d, SHORTGI = %d) \n",
				pHTPhyMode->PhyMode, pHTPhyMode->TransmitNo,pHTPhyMode->HtMode,pHTPhyMode->ExtOffset,
				pHTPhyMode->MCS, pHTPhyMode->BW, pHTPhyMode->STBC,	pHTPhyMode->SHORTGI));
				if (pAd->CommonCfg.PhyMode	>= PHY_11ABGN_MIXED)
					RTMPSetHT(pAd,	pHTPhyMode);
			}
			DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_SET_HT_PHYMODE(MCS=%d,BW=%d,SGI=%d,STBC=%d)\n",
				pAd->StaCfg.HTPhyMode.field.MCS, pAd->StaCfg.HTPhyMode.field.BW, pAd->StaCfg.HTPhyMode.field.ShortGI,
				pAd->StaCfg.HTPhyMode.field.STBC));
			break;
#endif // DOT11_N_SUPPORT //
		case RT_OID_802_11_SET_APSD_SETTING:
			if (wrq->u.data.length != sizeof(ULONG))
				Status = -EINVAL;
			else
			{
				ULONG apsd ;
				Status = copy_from_user(&apsd, wrq->u.data.pointer,	wrq->u.data.length);

				/*-------------------------------------------------------------------
				|B31~B7	|	B6~B5	 |	 B4	 |	 B3	 |	B2	 |	B1	 |	   B0		|
				---------------------------------------------------------------------
				| Rsvd	| Max SP Len | AC_VO | AC_VI | AC_BK | AC_BE | APSD	Capable	|
				---------------------------------------------------------------------*/
				pAd->CommonCfg.bAPSDCapable = (apsd & 0x00000001) ? TRUE :	FALSE;
				pAd->CommonCfg.bAPSDAC_BE = ((apsd	& 0x00000002) >> 1)	? TRUE : FALSE;
				pAd->CommonCfg.bAPSDAC_BK = ((apsd	& 0x00000004) >> 2)	? TRUE : FALSE;
				pAd->CommonCfg.bAPSDAC_VI = ((apsd	& 0x00000008) >> 3)	? TRUE : FALSE;
				pAd->CommonCfg.bAPSDAC_VO = ((apsd	& 0x00000010) >> 4)	? TRUE : FALSE;
				pAd->CommonCfg.MaxSPLength	= (UCHAR)((apsd	& 0x00000060) >> 5);

				DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_SET_APSD_SETTING (apsd=0x%lx, APSDCap=%d, [BE,BK,VI,VO]=[%d/%d/%d/%d],	MaxSPLen=%d)\n", apsd, pAd->CommonCfg.bAPSDCapable,
					pAd->CommonCfg.bAPSDAC_BE,	pAd->CommonCfg.bAPSDAC_BK,	pAd->CommonCfg.bAPSDAC_VI,	pAd->CommonCfg.bAPSDAC_VO,	pAd->CommonCfg.MaxSPLength));
			}
			break;

		case RT_OID_802_11_SET_APSD_PSM:
			if (wrq->u.data.length	!= sizeof(ULONG))
				Status = -EINVAL;
			else
			{
				// Driver needs	to notify AP when PSM changes
				Status = copy_from_user(&pAd->CommonCfg.bAPSDForcePowerSave, wrq->u.data.pointer, wrq->u.data.length);
				if (pAd->CommonCfg.bAPSDForcePowerSave	!= pAd->StaCfg.Psm)
				{
					RTMP_SET_PSM_BIT(pAd,	pAd->CommonCfg.bAPSDForcePowerSave);
					RTMPSendNullFrame(pAd,	pAd->CommonCfg.TxRate,	TRUE);
				}
				DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_SET_APSD_PSM (bAPSDForcePowerSave:%d)\n",	pAd->CommonCfg.bAPSDForcePowerSave));
			}
			break;
#ifdef QOS_DLS_SUPPORT
		case RT_OID_802_11_SET_DLS:
			if (wrq->u.data.length != sizeof(ULONG))
				Status = -EINVAL;
			else
			{
				BOOLEAN	oldvalue = pAd->CommonCfg.bDLSCapable;
				Status = copy_from_user(&pAd->CommonCfg.bDLSCapable, wrq->u.data.pointer, wrq->u.data.length);
				if (oldvalue &&	!pAd->CommonCfg.bDLSCapable)
				{
					int	i;
					// tear	down local dls table entry
					for	(i=0; i<MAX_NUM_OF_INIT_DLS_ENTRY; i++)
					{
						if (pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH))
						{
							pAd->StaCfg.DLSEntry[i].Status	= DLS_NONE;
							pAd->StaCfg.DLSEntry[i].Valid	= FALSE;
							RTMPSendDLSTearDownFrame(pAd, pAd->StaCfg.DLSEntry[i].MacAddr);
						}
					}

					// tear	down peer dls table	entry
					for	(i=MAX_NUM_OF_INIT_DLS_ENTRY; i<MAX_NUM_OF_DLS_ENTRY; i++)
					{
						if (pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH))
						{
							pAd->StaCfg.DLSEntry[i].Status	= DLS_NONE;
							pAd->StaCfg.DLSEntry[i].Valid	= FALSE;
							RTMPSendDLSTearDownFrame(pAd, pAd->StaCfg.DLSEntry[i].MacAddr);
						}
					}
				}

				DBGPRINT(RT_DEBUG_TRACE,("Set::RT_OID_802_11_SET_DLS (=%d)\n", pAd->CommonCfg.bDLSCapable));
			}
			break;

		case RT_OID_802_11_SET_DLS_PARAM:
			if (wrq->u.data.length	!= sizeof(RT_802_11_DLS_UI))
				Status = -EINVAL;
			else
			{
				RT_802_11_DLS	Dls;

				NdisZeroMemory(&Dls, sizeof(RT_802_11_DLS));
				RTMPMoveMemory(&Dls, wrq->u.data.pointer, sizeof(RT_802_11_DLS_UI));
				MlmeEnqueue(pAd,
							MLME_CNTL_STATE_MACHINE,
							RT_OID_802_11_SET_DLS_PARAM,
							sizeof(RT_802_11_DLS),
							&Dls);
				DBGPRINT(RT_DEBUG_TRACE,("Set::RT_OID_802_11_SET_DLS_PARAM \n"));
			}
			break;
#endif // QOS_DLS_SUPPORT //
		case RT_OID_802_11_SET_WMM:
			if (wrq->u.data.length	!= sizeof(BOOLEAN))
				Status = -EINVAL;
			else
			{
				Status = copy_from_user(&pAd->CommonCfg.bWmmCapable, wrq->u.data.pointer, wrq->u.data.length);
				DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_SET_WMM (=%d)	\n", pAd->CommonCfg.bWmmCapable));
			}
			break;

		case OID_802_11_DISASSOCIATE:
			//
			// Set NdisRadioStateOff to	TRUE, instead of called	MlmeRadioOff.
			// Later on, NDIS_802_11_BSSID_LIST_EX->NumberOfItems should be	0
			// when	query OID_802_11_BSSID_LIST.
			//
			// TRUE:  NumberOfItems	will set to	0.
			// FALSE: NumberOfItems	no change.
			//
			pAd->CommonCfg.NdisRadioStateOff =	TRUE;
			// Set to immediately send the media disconnect	event
			pAd->MlmeAux.CurrReqIsFromNdis	= TRUE;
			DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_DISASSOCIATE	\n"));


			if (INFRA_ON(pAd))
			{
				if (pAd->Mlme.CntlMachine.CurrState !=	CNTL_IDLE)
				{
					RTMP_MLME_RESET_STATE_MACHINE(pAd);
					DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME	busy, reset	MLME state machine !!!\n"));
				}

				MlmeEnqueue(pAd,
					MLME_CNTL_STATE_MACHINE,
					OID_802_11_DISASSOCIATE,
					0,
					NULL);

				StateMachineTouched	= TRUE;
			}
			break;

#ifdef DOT11_N_SUPPORT
		case RT_OID_802_11_SET_IMME_BA_CAP:
				if (wrq->u.data.length != sizeof(OID_BACAP_STRUC))
					Status = -EINVAL;
				else
				{
					OID_BACAP_STRUC Orde ;
					Status = copy_from_user(&Orde, wrq->u.data.pointer, wrq->u.data.length);
					if (Orde.Policy > BA_NOTUSE)
					{
						Status = NDIS_STATUS_INVALID_DATA;
					}
					else if (Orde.Policy == BA_NOTUSE)
					{
						pAd->CommonCfg.BACapability.field.Policy = BA_NOTUSE;
						pAd->CommonCfg.BACapability.field.MpduDensity = Orde.MpduDensity;
						pAd->CommonCfg.DesiredHtPhy.MpduDensity = Orde.MpduDensity;
						pAd->CommonCfg.DesiredHtPhy.AmsduEnable = Orde.AmsduEnable;
						pAd->CommonCfg.DesiredHtPhy.AmsduSize= Orde.AmsduSize;
						pAd->CommonCfg.DesiredHtPhy.MimoPs= Orde.MMPSmode;
						pAd->CommonCfg.BACapability.field.MMPSmode = Orde.MMPSmode;
						// UPdata to HT IE
						pAd->CommonCfg.HtCapability.HtCapInfo.MimoPs = Orde.MMPSmode;
						pAd->CommonCfg.HtCapability.HtCapInfo.AMsduSize = Orde.AmsduSize;
						pAd->CommonCfg.HtCapability.HtCapParm.MpduDensity = Orde.MpduDensity;
					}
					else
					{
                        pAd->CommonCfg.BACapability.field.AutoBA = Orde.AutoBA;
						pAd->CommonCfg.BACapability.field.Policy = IMMED_BA; // we only support immediate BA.
						pAd->CommonCfg.BACapability.field.MpduDensity = Orde.MpduDensity;
						pAd->CommonCfg.DesiredHtPhy.MpduDensity = Orde.MpduDensity;
						pAd->CommonCfg.DesiredHtPhy.AmsduEnable = Orde.AmsduEnable;
						pAd->CommonCfg.DesiredHtPhy.AmsduSize= Orde.AmsduSize;
						pAd->CommonCfg.DesiredHtPhy.MimoPs = Orde.MMPSmode;
						pAd->CommonCfg.BACapability.field.MMPSmode = Orde.MMPSmode;

						// UPdata to HT IE
						pAd->CommonCfg.HtCapability.HtCapInfo.MimoPs = Orde.MMPSmode;
						pAd->CommonCfg.HtCapability.HtCapInfo.AMsduSize = Orde.AmsduSize;
						pAd->CommonCfg.HtCapability.HtCapParm.MpduDensity = Orde.MpduDensity;

						if (pAd->CommonCfg.BACapability.field.RxBAWinLimit > MAX_RX_REORDERBUF)
							pAd->CommonCfg.BACapability.field.RxBAWinLimit = MAX_RX_REORDERBUF;

					}

					pAd->CommonCfg.REGBACapability.word = pAd->CommonCfg.BACapability.word;
					DBGPRINT(RT_DEBUG_TRACE, ("Set::(Orde.AutoBA = %d) (Policy=%d)(ReBAWinLimit=%d)(TxBAWinLimit=%d)(AutoMode=%d)\n",Orde.AutoBA, pAd->CommonCfg.BACapability.field.Policy,
						pAd->CommonCfg.BACapability.field.RxBAWinLimit,pAd->CommonCfg.BACapability.field.TxBAWinLimit, pAd->CommonCfg.BACapability.field.AutoBA));
					DBGPRINT(RT_DEBUG_TRACE, ("Set::(MimoPs = %d)(AmsduEnable = %d) (AmsduSize=%d)(MpduDensity=%d)\n",pAd->CommonCfg.DesiredHtPhy.MimoPs, pAd->CommonCfg.DesiredHtPhy.AmsduEnable,
						pAd->CommonCfg.DesiredHtPhy.AmsduSize, pAd->CommonCfg.DesiredHtPhy.MpduDensity));
				}

				break;
		case RT_OID_802_11_ADD_IMME_BA:
			DBGPRINT(RT_DEBUG_TRACE, (" Set :: RT_OID_802_11_ADD_IMME_BA \n"));
			if (wrq->u.data.length != sizeof(OID_ADD_BA_ENTRY))
					Status = -EINVAL;
			else
			{
				UCHAR		        index;
				OID_ADD_BA_ENTRY    BA;
				MAC_TABLE_ENTRY     *pEntry;

				Status = copy_from_user(&BA, wrq->u.data.pointer, wrq->u.data.length);
				if (BA.TID > 15)
				{
					Status = NDIS_STATUS_INVALID_DATA;
					break;
				}
				else
				{
					//BATableInsertEntry
					//As ad-hoc mode, BA pair is not limited to only BSSID. so add via OID.
					index = BA.TID;
					// in ad hoc mode, when adding BA pair, we should insert this entry into MACEntry too
					pEntry = MacTableLookup(pAd, BA.MACAddr);
					if (!pEntry)
					{
						DBGPRINT(RT_DEBUG_TRACE, ("RT_OID_802_11_ADD_IMME_BA. break on no connection.----:%x:%x\n", BA.MACAddr[4], BA.MACAddr[5]));
						break;
					}
					if (BA.IsRecipient == FALSE)
					{
					    if (pEntry->bIAmBadAtheros == TRUE)
							pAd->CommonCfg.BACapability.field.RxBAWinLimit = 0x10;

						BAOriSessionSetUp(pAd, pEntry, index, 0, 100, TRUE);
					}
					else
					{
						//BATableInsertEntry(pAd, pEntry->Aid, BA.MACAddr, 0, 0xffff, BA.TID, BA.nMSDU, BA.IsRecipient);
					}

					DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_802_11_ADD_IMME_BA. Rec = %d. Mac = %x:%x:%x:%x:%x:%x . \n",
						BA.IsRecipient, BA.MACAddr[0], BA.MACAddr[1], BA.MACAddr[2], BA.MACAddr[2]
						, BA.MACAddr[4], BA.MACAddr[5]));
				}
			}
			break;

		case RT_OID_802_11_TEAR_IMME_BA:
			DBGPRINT(RT_DEBUG_TRACE, ("Set :: RT_OID_802_11_TEAR_IMME_BA \n"));
			if (wrq->u.data.length != sizeof(OID_ADD_BA_ENTRY))
					Status = -EINVAL;
			else
			{
				POID_ADD_BA_ENTRY	pBA;
				MAC_TABLE_ENTRY *pEntry;

				pBA = kmalloc(wrq->u.data.length, MEM_ALLOC_FLAG);

				if (pBA == NULL)
				{
					DBGPRINT(RT_DEBUG_TRACE, ("Set :: RT_OID_802_11_TEAR_IMME_BA kmalloc() can't allocate enough memory\n"));
					Status = NDIS_STATUS_FAILURE;
				}
				else
				{
					Status = copy_from_user(pBA, wrq->u.data.pointer, wrq->u.data.length);
					DBGPRINT(RT_DEBUG_TRACE, ("Set :: RT_OID_802_11_TEAR_IMME_BA(TID=%d, bAllTid=%d)\n", pBA->TID, pBA->bAllTid));

					if (!pBA->bAllTid && (pBA->TID > NUM_OF_TID))
					{
						Status = NDIS_STATUS_INVALID_DATA;
						break;
					}

					if (pBA->IsRecipient == FALSE)
					{
						pEntry = MacTableLookup(pAd, pBA->MACAddr);
						DBGPRINT(RT_DEBUG_TRACE, (" pBA->IsRecipient == FALSE\n"));
						if (pEntry)
						{
							DBGPRINT(RT_DEBUG_TRACE, (" pBA->pEntry\n"));
							BAOriSessionTearDown(pAd, pEntry->Aid, pBA->TID, FALSE, TRUE);
						}
						else
							DBGPRINT(RT_DEBUG_TRACE, ("Set :: Not found pEntry \n"));
					}
					else
					{
						pEntry = MacTableLookup(pAd, pBA->MACAddr);
						if (pEntry)
						{
							BARecSessionTearDown( pAd, (UCHAR)pEntry->Aid, pBA->TID, TRUE);
						}
						else
							DBGPRINT(RT_DEBUG_TRACE, ("Set :: Not found pEntry \n"));
					}
					kfree(pBA);
				}
            }
            break;
#endif // DOT11_N_SUPPORT //

        // For WPA_SUPPLICANT to set static wep key
	case OID_802_11_ADD_WEP:
	    pWepKey = kmalloc(wrq->u.data.length, MEM_ALLOC_FLAG);

	    if(pWepKey == NULL)
            {
                Status = -ENOMEM;
				DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_ADD_WEP, Failed!!\n"));
                break;
            }
            Status = copy_from_user(pWepKey, wrq->u.data.pointer, wrq->u.data.length);
            if (Status)
            {
                Status  = -EINVAL;
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_ADD_WEP, Failed (length mismatch)!!\n"));
            }
            else
            {
		        KeyIdx = pWepKey->KeyIndex & 0x0fffffff;
                // KeyIdx must be 0 ~ 3
                if (KeyIdx > 4)
			{
                    Status  = -EINVAL;
                    DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_ADD_WEP, Failed (KeyIdx must be smaller than 4)!!\n"));
                }
                else
                {
                    UCHAR CipherAlg = 0;
                    PUCHAR Key;

                    // set key material and key length
                    NdisZeroMemory(pAd->SharedKey[BSS0][KeyIdx].Key, 16);
                    pAd->SharedKey[BSS0][KeyIdx].KeyLen = (UCHAR) pWepKey->KeyLength;
                    NdisMoveMemory(pAd->SharedKey[BSS0][KeyIdx].Key, &pWepKey->KeyMaterial, pWepKey->KeyLength);

                    switch(pWepKey->KeyLength)
                    {
                        case 5:
                            CipherAlg = CIPHER_WEP64;
                            break;
                        case 13:
                            CipherAlg = CIPHER_WEP128;
                            break;
                        default:
                            DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_ADD_WEP, only support CIPHER_WEP64(len:5) & CIPHER_WEP128(len:13)!!\n"));
                            Status = -EINVAL;
                            break;
                    }
                    pAd->SharedKey[BSS0][KeyIdx].CipherAlg = CipherAlg;

                    // Default key for tx (shared key)
                    if (pWepKey->KeyIndex & 0x80000000)
                    {
#ifdef WPA_SUPPLICANT_SUPPORT
                        // set key material and key length
                        NdisZeroMemory(pAd->StaCfg.DesireSharedKey[KeyIdx].Key, 16);
                        pAd->StaCfg.DesireSharedKey[KeyIdx].KeyLen = (UCHAR) pWepKey->KeyLength;
                        NdisMoveMemory(pAd->StaCfg.DesireSharedKey[KeyIdx].Key, &pWepKey->KeyMaterial, pWepKey->KeyLength);
                        pAd->StaCfg.DesireSharedKeyId = KeyIdx;
                        pAd->StaCfg.DesireSharedKey[KeyIdx].CipherAlg = CipherAlg;
#endif // WPA_SUPPLICANT_SUPPORT //
                        pAd->StaCfg.DefaultKeyId = (UCHAR) KeyIdx;
                    }

#ifdef WPA_SUPPLICANT_SUPPORT
					if ((pAd->StaCfg.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE) &&
						(pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA))
					{
						Key = pWepKey->KeyMaterial;

						// Set Group key material to Asic
					AsicAddSharedKeyEntry(pAd, BSS0, KeyIdx, CipherAlg, Key, NULL, NULL);

						// Update WCID attribute table and IVEIV table for this group key table
						RTMPAddWcidAttributeEntry(pAd, BSS0, KeyIdx, CipherAlg, NULL);

						STA_PORT_SECURED(pAd);

					// Indicate Connected for GUI
					pAd->IndicateMediaState = NdisMediaStateConnected;
					}
                    else if (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_SECURED)
#endif // WPA_SUPPLICANT_SUPPORT
                    {
                        Key = pAd->SharedKey[BSS0][KeyIdx].Key;

                        // Set key material and cipherAlg to Asic
					AsicAddSharedKeyEntry(pAd, BSS0, KeyIdx, CipherAlg, Key, NULL, NULL);

                        if (pWepKey->KeyIndex & 0x80000000)
                        {
                            PMAC_TABLE_ENTRY pEntry = &pAd->MacTab.Content[BSSID_WCID];
                            // Assign group key info
						RTMPAddWcidAttributeEntry(pAd, BSS0, KeyIdx, CipherAlg, NULL);
						// Assign pairwise key info
						RTMPAddWcidAttributeEntry(pAd, BSS0, KeyIdx, CipherAlg, pEntry);
                        }
                    }
					DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_ADD_WEP (id=0x%x, Len=%d-byte), %s\n", pWepKey->KeyIndex, pWepKey->KeyLength, (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_SECURED) ? "Port Secured":"Port NOT Secured"));
				}
            }
            kfree(pWepKey);
            break;
#ifdef WPA_SUPPLICANT_SUPPORT
	    case OID_SET_COUNTERMEASURES:
            if (wrq->u.data.length != sizeof(int))
                Status  = -EINVAL;
            else
            {
                int enabled = 0;
                Status = copy_from_user(&enabled, wrq->u.data.pointer, wrq->u.data.length);
                if (enabled == 1)
                    pAd->StaCfg.bBlockAssoc = TRUE;
                else
                    // WPA MIC error should block association attempt for 60 seconds
                    pAd->StaCfg.bBlockAssoc = FALSE;
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_SET_COUNTERMEASURES bBlockAssoc=%s\n", pAd->StaCfg.bBlockAssoc ? "TRUE":"FALSE"));
            }
	        break;
        case RT_OID_WPA_SUPPLICANT_SUPPORT:
			if (wrq->u.data.length != sizeof(UCHAR))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&wpa_supplicant_enable, wrq->u.data.pointer, wrq->u.data.length);
			pAd->StaCfg.WpaSupplicantUP = wpa_supplicant_enable;
			DBGPRINT(RT_DEBUG_TRACE, ("Set::RT_OID_WPA_SUPPLICANT_SUPPORT (=%d)\n", pAd->StaCfg.WpaSupplicantUP));
			}
            break;
        case OID_802_11_DEAUTHENTICATION:
            if (wrq->u.data.length != sizeof(MLME_DEAUTH_REQ_STRUCT))
                Status  = -EINVAL;
            else
            {
                MLME_DEAUTH_REQ_STRUCT      *pInfo;
		MLME_QUEUE_ELEM *MsgElem = (MLME_QUEUE_ELEM *) kmalloc(sizeof(MLME_QUEUE_ELEM), MEM_ALLOC_FLAG);
                if (MsgElem == NULL)
                {
			DBGPRINT(RT_DEBUG_ERROR, ("%s():alloc memory failed!\n", __FUNCTION__));
                        return -EINVAL;
                }

                pInfo = (MLME_DEAUTH_REQ_STRUCT *) MsgElem->Msg;
                Status = copy_from_user(pInfo, wrq->u.data.pointer, wrq->u.data.length);
                MlmeDeauthReqAction(pAd, MsgElem);
				kfree(MsgElem);

                if (INFRA_ON(pAd))
                {
                    LinkDown(pAd, FALSE);
                    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
                }
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_DEAUTHENTICATION (Reason=%d)\n", pInfo->Reason));
            }
            break;
        case OID_802_11_DROP_UNENCRYPTED:
            if (wrq->u.data.length != sizeof(int))
                Status  = -EINVAL;
            else
            {
                int enabled = 0;
                Status = copy_from_user(&enabled, wrq->u.data.pointer, wrq->u.data.length);
                if (enabled == 1)
                    pAd->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
                else
                    pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
				NdisAcquireSpinLock(&pAd->MacTabLock);
				pAd->MacTab.Content[BSSID_WCID].PortSecured = pAd->StaCfg.PortSecured;
				NdisReleaseSpinLock(&pAd->MacTabLock);
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_DROP_UNENCRYPTED (=%d)\n", enabled));
            }
            break;
        case OID_802_11_SET_IEEE8021X:
            if (wrq->u.data.length != sizeof(BOOLEAN))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&IEEE8021xState, wrq->u.data.pointer, wrq->u.data.length);
		        pAd->StaCfg.IEEE8021X = IEEE8021xState;
                DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_SET_IEEE8021X (=%d)\n", IEEE8021xState));
            }
            break;
        case OID_802_11_SET_IEEE8021X_REQUIRE_KEY:
			if (wrq->u.data.length != sizeof(BOOLEAN))
				 Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&IEEE8021x_required_keys, wrq->u.data.pointer, wrq->u.data.length);
				pAd->StaCfg.IEEE8021x_required_keys = IEEE8021x_required_keys;
				DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_SET_IEEE8021X_REQUIRE_KEY (%d)\n", IEEE8021x_required_keys));
			}
			break;
        case OID_802_11_PMKID:
	        pPmkId = kmalloc(wrq->u.data.length, MEM_ALLOC_FLAG);

	        if(pPmkId == NULL) {
                Status = -ENOMEM;
                break;
            }
            Status = copy_from_user(pPmkId, wrq->u.data.pointer, wrq->u.data.length);

	        // check the PMKID information
	        if (pPmkId->BSSIDInfoCount == 0)
                NdisZeroMemory(pAd->StaCfg.SavedPMK, sizeof(BSSID_INFO)*PMKID_NO);
	        else
	        {
		        PBSSID_INFO	pBssIdInfo;
		        UINT		BssIdx;
		        UINT		CachedIdx;

		        for (BssIdx = 0; BssIdx < pPmkId->BSSIDInfoCount; BssIdx++)
		        {
			        // point to the indexed BSSID_INFO structure
			        pBssIdInfo = (PBSSID_INFO) ((PUCHAR) pPmkId + 2 * sizeof(UINT) + BssIdx * sizeof(BSSID_INFO));
			        // Find the entry in the saved data base.
			        for (CachedIdx = 0; CachedIdx < pAd->StaCfg.SavedPMKNum; CachedIdx++)
			        {
				        // compare the BSSID
				        if (NdisEqualMemory(pBssIdInfo->BSSID, pAd->StaCfg.SavedPMK[CachedIdx].BSSID, sizeof(NDIS_802_11_MAC_ADDRESS)))
					        break;
			        }

			        // Found, replace it
			        if (CachedIdx < PMKID_NO)
			        {
				        DBGPRINT(RT_DEBUG_OFF, ("Update OID_802_11_PMKID, idx = %d\n", CachedIdx));
				        NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx], pBssIdInfo, sizeof(BSSID_INFO));
				        pAd->StaCfg.SavedPMKNum++;
			        }
			        // Not found, replace the last one
			        else
			        {
				        // Randomly replace one
				        CachedIdx = (pBssIdInfo->BSSID[5] % PMKID_NO);
				        DBGPRINT(RT_DEBUG_OFF, ("Update OID_802_11_PMKID, idx = %d\n", CachedIdx));
				        NdisMoveMemory(&pAd->StaCfg.SavedPMK[CachedIdx], pBssIdInfo, sizeof(BSSID_INFO));
			        }
		        }
			}
			if(pPmkId)
				kfree(pPmkId);
	        break;
#endif // WPA_SUPPLICANT_SUPPORT //



#ifdef SNMP_SUPPORT
		case OID_802_11_SHORTRETRYLIMIT:
			if (wrq->u.data.length != sizeof(ULONG))
				Status = -EINVAL;
			else
			{
				Status = copy_from_user(&ShortRetryLimit, wrq->u.data.pointer, wrq->u.data.length);
				RTMP_IO_READ32(pAd, TX_RTY_CFG, &tx_rty_cfg.word);
				tx_rty_cfg.field.ShortRtyLimit = ShortRetryLimit;
				RTMP_IO_WRITE32(pAd, TX_RTY_CFG, tx_rty_cfg.word);
				DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_SHORTRETRYLIMIT (tx_rty_cfg.field.ShortRetryLimit=%d, ShortRetryLimit=%ld)\n", tx_rty_cfg.field.ShortRtyLimit, ShortRetryLimit));
			}
			break;

		case OID_802_11_LONGRETRYLIMIT:
			DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_LONGRETRYLIMIT \n"));
			if (wrq->u.data.length != sizeof(ULONG))
				Status = -EINVAL;
			else
			{
				Status = copy_from_user(&LongRetryLimit, wrq->u.data.pointer, wrq->u.data.length);
				RTMP_IO_READ32(pAd, TX_RTY_CFG, &tx_rty_cfg.word);
				tx_rty_cfg.field.LongRtyLimit = LongRetryLimit;
				RTMP_IO_WRITE32(pAd, TX_RTY_CFG, tx_rty_cfg.word);
				DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_LONGRETRYLIMIT (tx_rty_cfg.field.LongRetryLimit= %d,LongRetryLimit=%ld)\n", tx_rty_cfg.field.LongRtyLimit, LongRetryLimit));
			}
			break;

		case OID_802_11_WEPDEFAULTKEYVALUE:
			DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_WEPDEFAULTKEYVALUE\n"));
			pKey = kmalloc(wrq->u.data.length, GFP_KERNEL);
			Status = copy_from_user(pKey, wrq->u.data.pointer, wrq->u.data.length);
			//pKey = &WepKey;

			if ( pKey->Length != wrq->u.data.length)
			{
				Status = -EINVAL;
				DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_WEPDEFAULTKEYVALUE, Failed!!\n"));
			}
			KeyIdx = pKey->KeyIndex & 0x0fffffff;
			DBGPRINT(RT_DEBUG_TRACE,("pKey->KeyIndex =%d, pKey->KeyLength=%d\n", pKey->KeyIndex, pKey->KeyLength));

			// it is a shared key
			if (KeyIdx > 4)
				Status = -EINVAL;
			else
			{
				pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].KeyLen = (UCHAR) pKey->KeyLength;
				NdisMoveMemory(&pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key, &pKey->KeyMaterial, pKey->KeyLength);
				if (pKey->KeyIndex & 0x80000000)
				{
					// Default key for tx (shared key)
					pAd->StaCfg.DefaultKeyId = (UCHAR) KeyIdx;
				}
				//RestartAPIsRequired = TRUE;
			}
			break;


		case OID_802_11_WEPDEFAULTKEYID:
			DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_WEPDEFAULTKEYID \n"));

			if (wrq->u.data.length != sizeof(UCHAR))
				Status = -EINVAL;
			else
				Status = copy_from_user(&pAd->StaCfg.DefaultKeyId, wrq->u.data.pointer, wrq->u.data.length);

			break;


		case OID_802_11_CURRENTCHANNEL:
			DBGPRINT(RT_DEBUG_TRACE, ("Set::OID_802_11_CURRENTCHANNEL \n"));
			if (wrq->u.data.length != sizeof(UCHAR))
				Status = -EINVAL;
			else
			{
				Status = copy_from_user(&ctmp, wrq->u.data.pointer, wrq->u.data.length);
				sprintf((PSTRING)&ctmp,"%d", ctmp);
				Set_Channel_Proc(pAd, (PSTRING)&ctmp);
			}
			break;
#endif



		case RT_OID_802_11_SET_PSPXLINK_MODE:
			if (wrq->u.data.length != sizeof(BOOLEAN))
                Status  = -EINVAL;
            else
            {
                Status = copy_from_user(&pAd->CommonCfg.PSPXlink, wrq->u.data.pointer, wrq->u.data.length);
				/*if (pAd->CommonCfg.PSPXlink)
					RX_FILTER_SET_FLAG(pAd, fRX_FILTER_ACCEPT_PROMISCUOUS)*/
				DBGPRINT(RT_DEBUG_TRACE,("Set::RT_OID_802_11_SET_PSPXLINK_MODE(=%d) \n", pAd->CommonCfg.PSPXlink));
            }
			break;


        default:
            DBGPRINT(RT_DEBUG_TRACE, ("Set::unknown IOCTL's subcmd = 0x%08x\n", cmd));
            Status = -EOPNOTSUPP;
            break;
    }


    return Status;
}

INT RTMPQueryInformation(
    IN  PRTMP_ADAPTER pAd,
    IN  OUT struct ifreq    *rq,
    IN  INT                 cmd)
{
    struct iwreq                        *wrq = (struct iwreq *) rq;
    NDIS_802_11_BSSID_LIST_EX           *pBssidList = NULL;
    PNDIS_WLAN_BSSID_EX                 pBss;
    NDIS_802_11_SSID                    Ssid;
    NDIS_802_11_CONFIGURATION           *pConfiguration = NULL;
    RT_802_11_LINK_STATUS               *pLinkStatus = NULL;
    RT_802_11_STA_CONFIG                *pStaConfig = NULL;
    NDIS_802_11_STATISTICS              *pStatistics = NULL;
    NDIS_802_11_RTS_THRESHOLD           RtsThresh;
    NDIS_802_11_FRAGMENTATION_THRESHOLD FragThresh;
    NDIS_802_11_POWER_MODE              PowerMode;
    NDIS_802_11_NETWORK_INFRASTRUCTURE  BssType;
    RT_802_11_PREAMBLE                  PreamType;
    NDIS_802_11_AUTHENTICATION_MODE     AuthMode;
    NDIS_802_11_WEP_STATUS              WepStatus;
    NDIS_MEDIA_STATE                    MediaState;
    ULONG                               BssBufSize, ulInfo=0, NetworkTypeList[4], apsd = 0;
    USHORT                              BssLen = 0;
    PUCHAR                              pBuf = NULL, pPtr;
    INT                                 Status = NDIS_STATUS_SUCCESS;
    UINT                                we_version_compiled;
    UCHAR                               i, Padding = 0;
    BOOLEAN                             RadioState;
	STRING								driverVersion[8];
    OID_SET_HT_PHYMODE			        *pHTPhyMode = NULL;


#ifdef SNMP_SUPPORT
	//for snmp, kathy
	DefaultKeyIdxValue			*pKeyIdxValue;
	INT							valueLen;
	TX_RTY_CFG_STRUC			tx_rty_cfg;
	ULONG						ShortRetryLimit, LongRetryLimit;
	UCHAR						tmp[64];
#endif //SNMP

    switch(cmd)
    {
        case RT_OID_DEVICE_NAME:
            wrq->u.data.length = sizeof(pAd->nickname);
            Status = copy_to_user(wrq->u.data.pointer, pAd->nickname, wrq->u.data.length);
            break;
        case RT_OID_VERSION_INFO:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_VERSION_INFO \n"));
			wrq->u.data.length = 8*sizeof(CHAR);
			sprintf(&driverVersion[0], "%s", STA_DRIVER_VERSION);
			driverVersion[7] = '\0';
			if (copy_to_user(wrq->u.data.pointer, &driverVersion[0], wrq->u.data.length))
            {
				Status = -EFAULT;
            }
            break;

        case OID_802_11_BSSID_LIST:
            if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
            {
		/*
		 * Still scanning, indicate the caller should try again.
		 */
		DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_BSSID_LIST (Still scanning)\n"));
				return -EAGAIN;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_BSSID_LIST (%d BSS returned)\n",pAd->ScanTab.BssNr));
			pAd->StaCfg.bScanReqIsFromWebUI = FALSE;
            // Claculate total buffer size required
            BssBufSize = sizeof(ULONG);

            for (i = 0; i < pAd->ScanTab.BssNr; i++)
            {
                // Align pointer to 4 bytes boundary.
                //Padding = 4 - (pAdapter->ScanTab.BssEntry[i].VarIELen & 0x0003);
                //if (Padding == 4)
                //    Padding = 0;
                BssBufSize += (sizeof(NDIS_WLAN_BSSID_EX) - 1 + sizeof(NDIS_802_11_FIXED_IEs) + pAd->ScanTab.BssEntry[i].VarIELen + Padding);
            }

            // For safety issue, we add 256 bytes just in case
            BssBufSize += 256;
            // Allocate the same size as passed from higher layer
            pBuf = kmalloc(BssBufSize, MEM_ALLOC_FLAG);
            if(pBuf == NULL)
            {
                Status = -ENOMEM;
                break;
            }
            // Init 802_11_BSSID_LIST_EX structure
            NdisZeroMemory(pBuf, BssBufSize);
            pBssidList = (PNDIS_802_11_BSSID_LIST_EX) pBuf;
            pBssidList->NumberOfItems = pAd->ScanTab.BssNr;

            // Calculate total buffer length
            BssLen = 4; // Consist of NumberOfItems
            // Point to start of NDIS_WLAN_BSSID_EX
            // pPtr = pBuf + sizeof(ULONG);
            pPtr = (PUCHAR) &pBssidList->Bssid[0];
            for (i = 0; i < pAd->ScanTab.BssNr; i++)
            {
                pBss = (PNDIS_WLAN_BSSID_EX) pPtr;
                NdisMoveMemory(&pBss->MacAddress, &pAd->ScanTab.BssEntry[i].Bssid, MAC_ADDR_LEN);
                if ((pAd->ScanTab.BssEntry[i].Hidden == 1) && (pAd->StaCfg.bShowHiddenSSID == FALSE))
                {
                    //
					// We must return this SSID during 4way handshaking, otherwise Aegis will failed to parse WPA infomation
					// and then failed to send EAPOl farame.
					//
					if ((pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA) && (pAd->StaCfg.PortSecured != WPA_802_1X_PORT_SECURED))
					{
						pBss->Ssid.SsidLength = pAd->ScanTab.BssEntry[i].SsidLen;
						NdisMoveMemory(pBss->Ssid.Ssid, pAd->ScanTab.BssEntry[i].Ssid, pAd->ScanTab.BssEntry[i].SsidLen);
					}
					else
			pBss->Ssid.SsidLength = 0;
                }
                else
                {
                    pBss->Ssid.SsidLength = pAd->ScanTab.BssEntry[i].SsidLen;
                    NdisMoveMemory(pBss->Ssid.Ssid, pAd->ScanTab.BssEntry[i].Ssid, pAd->ScanTab.BssEntry[i].SsidLen);
                }
                pBss->Privacy = pAd->ScanTab.BssEntry[i].Privacy;
                pBss->Rssi = pAd->ScanTab.BssEntry[i].Rssi - pAd->BbpRssiToDbmDelta;
                pBss->NetworkTypeInUse = NetworkTypeInUseSanity(&pAd->ScanTab.BssEntry[i]);
                pBss->Configuration.Length = sizeof(NDIS_802_11_CONFIGURATION);
                pBss->Configuration.BeaconPeriod = pAd->ScanTab.BssEntry[i].BeaconPeriod;
                pBss->Configuration.ATIMWindow = pAd->ScanTab.BssEntry[i].AtimWin;

                MAP_CHANNEL_ID_TO_KHZ(pAd->ScanTab.BssEntry[i].Channel, pBss->Configuration.DSConfig);

                if (pAd->ScanTab.BssEntry[i].BssType == BSS_INFRA)
                    pBss->InfrastructureMode = Ndis802_11Infrastructure;
                else
                    pBss->InfrastructureMode = Ndis802_11IBSS;

                NdisMoveMemory(pBss->SupportedRates, pAd->ScanTab.BssEntry[i].SupRate, pAd->ScanTab.BssEntry[i].SupRateLen);
                NdisMoveMemory(pBss->SupportedRates + pAd->ScanTab.BssEntry[i].SupRateLen,
                               pAd->ScanTab.BssEntry[i].ExtRate,
                               pAd->ScanTab.BssEntry[i].ExtRateLen);

                if (pAd->ScanTab.BssEntry[i].VarIELen == 0)
                {
                    pBss->IELength = sizeof(NDIS_802_11_FIXED_IEs);
                    NdisMoveMemory(pBss->IEs, &pAd->ScanTab.BssEntry[i].FixIEs, sizeof(NDIS_802_11_FIXED_IEs));
                    pPtr = pPtr + sizeof(NDIS_WLAN_BSSID_EX) - 1 + sizeof(NDIS_802_11_FIXED_IEs);
                }
                else
                {
                    pBss->IELength = (ULONG)(sizeof(NDIS_802_11_FIXED_IEs) + pAd->ScanTab.BssEntry[i].VarIELen);
                    pPtr = pPtr + sizeof(NDIS_WLAN_BSSID_EX) - 1 + sizeof(NDIS_802_11_FIXED_IEs);
                    NdisMoveMemory(pBss->IEs, &pAd->ScanTab.BssEntry[i].FixIEs, sizeof(NDIS_802_11_FIXED_IEs));
                    NdisMoveMemory(pBss->IEs + sizeof(NDIS_802_11_FIXED_IEs), pAd->ScanTab.BssEntry[i].VarIEs, pAd->ScanTab.BssEntry[i].VarIELen);
                    pPtr += pAd->ScanTab.BssEntry[i].VarIELen;
                }
                pBss->Length = (ULONG)(sizeof(NDIS_WLAN_BSSID_EX) - 1 + sizeof(NDIS_802_11_FIXED_IEs) + pAd->ScanTab.BssEntry[i].VarIELen + Padding);

#if WIRELESS_EXT < 17
                if ((BssLen + pBss->Length) < wrq->u.data.length)
                BssLen += pBss->Length;
                else
                {
                    pBssidList->NumberOfItems = i;
                    break;
                }
#else
                BssLen += pBss->Length;
#endif
            }

#if WIRELESS_EXT < 17
            wrq->u.data.length = BssLen;
#else
            if (BssLen > wrq->u.data.length)
            {
                kfree(pBssidList);
                return -E2BIG;
            }
            else
                wrq->u.data.length = BssLen;
#endif
            Status = copy_to_user(wrq->u.data.pointer, pBssidList, BssLen);
            kfree(pBssidList);
            break;
        case OID_802_3_CURRENT_ADDRESS:
            wrq->u.data.length = MAC_ADDR_LEN;
            Status = copy_to_user(wrq->u.data.pointer, &pAd->CurrentAddress, wrq->u.data.length);
            break;
        case OID_GEN_MEDIA_CONNECT_STATUS:
            if (pAd->IndicateMediaState == NdisMediaStateConnected)
                MediaState = NdisMediaStateConnected;
            else
                MediaState = NdisMediaStateDisconnected;

            wrq->u.data.length = sizeof(NDIS_MEDIA_STATE);
            Status = copy_to_user(wrq->u.data.pointer, &MediaState, wrq->u.data.length);
            break;
        case OID_802_11_BSSID:
            if (INFRA_ON(pAd) || ADHOC_ON(pAd))
            {
                Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.Bssid, sizeof(NDIS_802_11_MAC_ADDRESS));

            }
            else
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_BSSID(=EMPTY)\n"));
                Status = -ENOTCONN;
            }
            break;
        case OID_802_11_SSID:
			NdisZeroMemory(&Ssid, sizeof(NDIS_802_11_SSID));
			NdisZeroMemory(Ssid.Ssid, MAX_LEN_OF_SSID);
            Ssid.SsidLength = pAd->CommonCfg.SsidLen;
			memcpy(Ssid.Ssid, pAd->CommonCfg.Ssid,	Ssid.SsidLength);
            wrq->u.data.length = sizeof(NDIS_802_11_SSID);
            Status = copy_to_user(wrq->u.data.pointer, &Ssid, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_SSID (Len=%d, ssid=%s)\n", Ssid.SsidLength,Ssid.Ssid));
            break;
        case RT_OID_802_11_QUERY_LINK_STATUS:
            pLinkStatus = (RT_802_11_LINK_STATUS *) kmalloc(sizeof(RT_802_11_LINK_STATUS), MEM_ALLOC_FLAG);
            if (pLinkStatus)
            {
                pLinkStatus->CurrTxRate = RateIdTo500Kbps[pAd->CommonCfg.TxRate];   // unit : 500 kbps
                pLinkStatus->ChannelQuality = pAd->Mlme.ChannelQuality;
                pLinkStatus->RxByteCount = pAd->RalinkCounters.ReceivedByteCount;
                pLinkStatus->TxByteCount = pAd->RalinkCounters.TransmittedByteCount;
			pLinkStatus->CentralChannel = pAd->CommonCfg.CentralChannel;
                wrq->u.data.length = sizeof(RT_802_11_LINK_STATUS);
                Status = copy_to_user(wrq->u.data.pointer, pLinkStatus, wrq->u.data.length);
                kfree(pLinkStatus);
                DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_LINK_STATUS\n"));
            }
            else
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_LINK_STATUS(kmalloc failed)\n"));
                Status = -EFAULT;
            }
            break;
        case OID_802_11_CONFIGURATION:
            pConfiguration = (NDIS_802_11_CONFIGURATION *) kmalloc(sizeof(NDIS_802_11_CONFIGURATION), MEM_ALLOC_FLAG);
            if (pConfiguration)
            {
                pConfiguration->Length = sizeof(NDIS_802_11_CONFIGURATION);
                pConfiguration->BeaconPeriod = pAd->CommonCfg.BeaconPeriod;
                pConfiguration->ATIMWindow = pAd->StaActive.AtimWin;
                MAP_CHANNEL_ID_TO_KHZ(pAd->CommonCfg.Channel, pConfiguration->DSConfig);
                wrq->u.data.length = sizeof(NDIS_802_11_CONFIGURATION);
                Status = copy_to_user(wrq->u.data.pointer, pConfiguration, wrq->u.data.length);
                DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_CONFIGURATION(BeaconPeriod=%ld,AtimW=%ld,Channel=%d) \n",
                                        pConfiguration->BeaconPeriod, pConfiguration->ATIMWindow, pAd->CommonCfg.Channel));
				kfree(pConfiguration);
            }
            else
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_CONFIGURATION(kmalloc failed)\n"));
                Status = -EFAULT;
            }
            break;
		case RT_OID_802_11_SNR_0:
			if ((pAd->StaCfg.LastSNR0 > 0))
			{
				ulInfo = ((0xeb	- pAd->StaCfg.LastSNR0) * 3) /	16 ;
				wrq->u.data.length = sizeof(ulInfo);
				Status = copy_to_user(wrq->u.data.pointer, &ulInfo,	wrq->u.data.length);
				DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_SNR_0(0x=%lx)\n", ulInfo));
			}
            else
			    Status = -EFAULT;
			break;
		case RT_OID_802_11_SNR_1:
			if ((pAd->Antenna.field.RxPath	> 1) &&
                (pAd->StaCfg.LastSNR1 > 0))
			{
				ulInfo = ((0xeb	- pAd->StaCfg.LastSNR1) * 3) /	16 ;
				wrq->u.data.length = sizeof(ulInfo);
				Status = copy_to_user(wrq->u.data.pointer, &ulInfo,	wrq->u.data.length);
				DBGPRINT(RT_DEBUG_TRACE,("Query::RT_OID_802_11_SNR_1(0x=%lx)\n",ulInfo));
			}
			else
				Status = -EFAULT;
            DBGPRINT(RT_DEBUG_TRACE,("Query::RT_OID_802_11_SNR_1(pAd->StaCfg.LastSNR1=%d)\n",pAd->StaCfg.LastSNR1));
			break;
        case OID_802_11_RSSI_TRIGGER:
            ulInfo = pAd->StaCfg.RssiSample.LastRssi0 - pAd->BbpRssiToDbmDelta;
            wrq->u.data.length = sizeof(ulInfo);
            Status = copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_RSSI_TRIGGER(=%ld)\n", ulInfo));
            break;
		case OID_802_11_RSSI:
        case RT_OID_802_11_RSSI:
			ulInfo = pAd->StaCfg.RssiSample.LastRssi0;
			wrq->u.data.length = sizeof(ulInfo);
			Status = copy_to_user(wrq->u.data.pointer, &ulInfo,	wrq->u.data.length);
			break;
		case RT_OID_802_11_RSSI_1:
            ulInfo = pAd->StaCfg.RssiSample.LastRssi1;
			wrq->u.data.length = sizeof(ulInfo);
			Status = copy_to_user(wrq->u.data.pointer, &ulInfo,	wrq->u.data.length);
			break;
        case RT_OID_802_11_RSSI_2:
            ulInfo = pAd->StaCfg.RssiSample.LastRssi2;
			wrq->u.data.length = sizeof(ulInfo);
			Status = copy_to_user(wrq->u.data.pointer, &ulInfo,	wrq->u.data.length);
			break;
        case OID_802_11_STATISTICS:
            pStatistics = (NDIS_802_11_STATISTICS *) kmalloc(sizeof(NDIS_802_11_STATISTICS), MEM_ALLOC_FLAG);
            if (pStatistics)
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_STATISTICS \n"));
                // add the most up-to-date h/w raw counters into software counters
			    NICUpdateRawCounters(pAd);

                // Sanity check for calculation of sucessful count
                if (pAd->WlanCounters.TransmittedFragmentCount.QuadPart < pAd->WlanCounters.RetryCount.QuadPart)
                    pAd->WlanCounters.TransmittedFragmentCount.QuadPart = pAd->WlanCounters.RetryCount.QuadPart;

                pStatistics->TransmittedFragmentCount.QuadPart = pAd->WlanCounters.TransmittedFragmentCount.QuadPart;
                pStatistics->MulticastTransmittedFrameCount.QuadPart = pAd->WlanCounters.MulticastTransmittedFrameCount.QuadPart;
                pStatistics->FailedCount.QuadPart = pAd->WlanCounters.FailedCount.QuadPart;
                pStatistics->RetryCount.QuadPart = pAd->WlanCounters.RetryCount.QuadPart;
                pStatistics->MultipleRetryCount.QuadPart = pAd->WlanCounters.MultipleRetryCount.QuadPart;
                pStatistics->RTSSuccessCount.QuadPart = pAd->WlanCounters.RTSSuccessCount.QuadPart;
                pStatistics->RTSFailureCount.QuadPart = pAd->WlanCounters.RTSFailureCount.QuadPart;
                pStatistics->ACKFailureCount.QuadPart = pAd->WlanCounters.ACKFailureCount.QuadPart;
                pStatistics->FrameDuplicateCount.QuadPart = pAd->WlanCounters.FrameDuplicateCount.QuadPart;
                pStatistics->ReceivedFragmentCount.QuadPart = pAd->WlanCounters.ReceivedFragmentCount.QuadPart;
                pStatistics->MulticastReceivedFrameCount.QuadPart = pAd->WlanCounters.MulticastReceivedFrameCount.QuadPart;
#ifdef DBG
                pStatistics->FCSErrorCount = pAd->RalinkCounters.RealFcsErrCount;
#else
                pStatistics->FCSErrorCount.QuadPart = pAd->WlanCounters.FCSErrorCount.QuadPart;
                pStatistics->FrameDuplicateCount.u.LowPart = pAd->WlanCounters.FrameDuplicateCount.u.LowPart / 100;
#endif
                wrq->u.data.length = sizeof(NDIS_802_11_STATISTICS);
                Status = copy_to_user(wrq->u.data.pointer, pStatistics, wrq->u.data.length);
                kfree(pStatistics);
            }
            else
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_STATISTICS(kmalloc failed)\n"));
                Status = -EFAULT;
            }
            break;
        case OID_GEN_RCV_OK:
            ulInfo = pAd->Counters8023.GoodReceives;
            wrq->u.data.length = sizeof(ulInfo);
            Status = copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length);
            break;
        case OID_GEN_RCV_NO_BUFFER:
            ulInfo = pAd->Counters8023.RxNoBuffer;
            wrq->u.data.length = sizeof(ulInfo);
            Status = copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length);
            break;
        case RT_OID_802_11_PHY_MODE:
            ulInfo = (ULONG)pAd->CommonCfg.PhyMode;
            wrq->u.data.length = sizeof(ulInfo);
            Status = copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_PHY_MODE (=%ld)\n", ulInfo));
            break;
        case RT_OID_802_11_STA_CONFIG:
            pStaConfig = (RT_802_11_STA_CONFIG *) kmalloc(sizeof(RT_802_11_STA_CONFIG), MEM_ALLOC_FLAG);
            if (pStaConfig)
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_STA_CONFIG\n"));
                pStaConfig->EnableTxBurst = pAd->CommonCfg.bEnableTxBurst;
                pStaConfig->EnableTurboRate = 0;
                pStaConfig->UseBGProtection = pAd->CommonCfg.UseBGProtection;
                pStaConfig->UseShortSlotTime = pAd->CommonCfg.bUseShortSlotTime;
                //pStaConfig->AdhocMode = pAd->StaCfg.AdhocMode;
                pStaConfig->HwRadioStatus = (pAd->StaCfg.bHwRadio == TRUE) ? 1 : 0;
                pStaConfig->Rsv1 = 0;
                pStaConfig->SystemErrorBitmap = pAd->SystemErrorBitmap;
                wrq->u.data.length = sizeof(RT_802_11_STA_CONFIG);
                Status = copy_to_user(wrq->u.data.pointer, pStaConfig, wrq->u.data.length);
                kfree(pStaConfig);
            }
            else
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_STA_CONFIG(kmalloc failed)\n"));
                Status = -EFAULT;
            }
            break;
        case OID_802_11_RTS_THRESHOLD:
            RtsThresh = pAd->CommonCfg.RtsThreshold;
            wrq->u.data.length = sizeof(RtsThresh);
            Status = copy_to_user(wrq->u.data.pointer, &RtsThresh, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_RTS_THRESHOLD(=%ld)\n", RtsThresh));
            break;
        case OID_802_11_FRAGMENTATION_THRESHOLD:
            FragThresh = pAd->CommonCfg.FragmentThreshold;
            if (pAd->CommonCfg.bUseZeroToDisableFragment == TRUE)
                FragThresh = 0;
            wrq->u.data.length = sizeof(FragThresh);
            Status = copy_to_user(wrq->u.data.pointer, &FragThresh, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_FRAGMENTATION_THRESHOLD(=%ld)\n", FragThresh));
            break;
        case OID_802_11_POWER_MODE:
            PowerMode = pAd->StaCfg.WindowsPowerMode;
            wrq->u.data.length = sizeof(PowerMode);
            Status = copy_to_user(wrq->u.data.pointer, &PowerMode, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_POWER_MODE(=%d)\n", PowerMode));
            break;
        case RT_OID_802_11_RADIO:
            RadioState = (BOOLEAN) pAd->StaCfg.bSwRadio;
            wrq->u.data.length = sizeof(RadioState);
            Status = copy_to_user(wrq->u.data.pointer, &RadioState, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_RADIO (=%d)\n", RadioState));
            break;
        case OID_802_11_INFRASTRUCTURE_MODE:
            if (pAd->StaCfg.BssType == BSS_ADHOC)
                BssType = Ndis802_11IBSS;
            else if (pAd->StaCfg.BssType == BSS_INFRA)
                BssType = Ndis802_11Infrastructure;
            else if (pAd->StaCfg.BssType == BSS_MONITOR)
                BssType = Ndis802_11Monitor;
            else
                BssType = Ndis802_11AutoUnknown;

            wrq->u.data.length = sizeof(BssType);
            Status = copy_to_user(wrq->u.data.pointer, &BssType, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_INFRASTRUCTURE_MODE(=%d)\n", BssType));
            break;
        case RT_OID_802_11_PREAMBLE:
            PreamType = pAd->CommonCfg.TxPreamble;
            wrq->u.data.length = sizeof(PreamType);
            Status = copy_to_user(wrq->u.data.pointer, &PreamType, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_PREAMBLE(=%d)\n", PreamType));
            break;
        case OID_802_11_AUTHENTICATION_MODE:
            AuthMode = pAd->StaCfg.AuthMode;
            wrq->u.data.length = sizeof(AuthMode);
            Status = copy_to_user(wrq->u.data.pointer, &AuthMode, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_AUTHENTICATION_MODE(=%d)\n", AuthMode));
            break;
        case OID_802_11_WEP_STATUS:
            WepStatus = pAd->StaCfg.WepStatus;
            wrq->u.data.length = sizeof(WepStatus);
            Status = copy_to_user(wrq->u.data.pointer, &WepStatus, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_WEP_STATUS(=%d)\n", WepStatus));
            break;
        case OID_802_11_TX_POWER_LEVEL:
			wrq->u.data.length = sizeof(ULONG);
			Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.TxPower, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_TX_POWER_LEVEL %x\n",pAd->CommonCfg.TxPower));
			break;
        case RT_OID_802_11_TX_POWER_LEVEL_1:
            wrq->u.data.length = sizeof(ULONG);
            Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.TxPowerPercentage, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_TX_POWER_LEVEL_1 (=%ld)\n", pAd->CommonCfg.TxPowerPercentage));
			break;
        case OID_802_11_NETWORK_TYPES_SUPPORTED:
			if ((pAd->RfIcType	== RFIC_2850) || (pAd->RfIcType ==	RFIC_2750) || (pAd->RfIcType == RFIC_3052))
			{
				NetworkTypeList[0] = 3;                 // NumberOfItems = 3
				NetworkTypeList[1] = Ndis802_11DS;      // NetworkType[1] = 11b
				NetworkTypeList[2] = Ndis802_11OFDM24;  // NetworkType[2] = 11g
				NetworkTypeList[3] = Ndis802_11OFDM5;   // NetworkType[3] = 11a
                wrq->u.data.length = 16;
				Status = copy_to_user(wrq->u.data.pointer, &NetworkTypeList[0], wrq->u.data.length);
			}
			else
			{
				NetworkTypeList[0] = 2;                 // NumberOfItems = 2
				NetworkTypeList[1] = Ndis802_11DS;      // NetworkType[1] = 11b
				NetworkTypeList[2] = Ndis802_11OFDM24;  // NetworkType[2] = 11g
			    wrq->u.data.length = 12;
				Status = copy_to_user(wrq->u.data.pointer, &NetworkTypeList[0], wrq->u.data.length);
			}
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_NETWORK_TYPES_SUPPORTED\n"));
				break;
	    case OID_802_11_NETWORK_TYPE_IN_USE:
            wrq->u.data.length = sizeof(ULONG);
			if (pAd->CommonCfg.PhyMode == PHY_11A)
				ulInfo = Ndis802_11OFDM5;
			else if ((pAd->CommonCfg.PhyMode == PHY_11BG_MIXED) || (pAd->CommonCfg.PhyMode == PHY_11G))
				ulInfo = Ndis802_11OFDM24;
			else
				ulInfo = Ndis802_11DS;
            Status = copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length);
			break;
        case RT_OID_802_11_QUERY_LAST_RX_RATE:
            ulInfo = (ULONG)pAd->LastRxRate;
            wrq->u.data.length = sizeof(ulInfo);
			Status = copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_LAST_RX_RATE (=%ld)\n", ulInfo));
			break;
		case RT_OID_802_11_QUERY_LAST_TX_RATE:
			//ulInfo = (ULONG)pAd->LastTxRate;
			ulInfo = (ULONG)pAd->MacTab.Content[BSSID_WCID].HTPhyMode.word;
			wrq->u.data.length = sizeof(ulInfo);
			Status = copy_to_user(wrq->u.data.pointer, &ulInfo,	wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_LAST_TX_RATE (=%lx)\n", ulInfo));
			break;
        case RT_OID_802_11_QUERY_EEPROM_VERSION:
            wrq->u.data.length = sizeof(ULONG);
            Status = copy_to_user(wrq->u.data.pointer, &pAd->EepromVersion, wrq->u.data.length);
            break;
        case RT_OID_802_11_QUERY_FIRMWARE_VERSION:
            wrq->u.data.length = sizeof(ULONG);
            Status = copy_to_user(wrq->u.data.pointer, &pAd->FirmwareVersion, wrq->u.data.length);
			break;
	    case RT_OID_802_11_QUERY_NOISE_LEVEL:
			wrq->u.data.length = sizeof(UCHAR);
			Status = copy_to_user(wrq->u.data.pointer, &pAd->BbpWriteLatch[66], wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_NOISE_LEVEL (=%d)\n", pAd->BbpWriteLatch[66]));
			break;
	    case RT_OID_802_11_EXTRA_INFO:
			wrq->u.data.length = sizeof(ULONG);
			Status = copy_to_user(wrq->u.data.pointer, &pAd->ExtraInfo, wrq->u.data.length);
	        DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_EXTRA_INFO (=%ld)\n", pAd->ExtraInfo));
	        break;
	    case RT_OID_WE_VERSION_COMPILED:
	        wrq->u.data.length = sizeof(UINT);
	        we_version_compiled = WIRELESS_EXT;
	        Status = copy_to_user(wrq->u.data.pointer, &we_version_compiled, wrq->u.data.length);
	        break;
		case RT_OID_802_11_QUERY_APSD_SETTING:
			apsd = (pAd->CommonCfg.bAPSDCapable | (pAd->CommonCfg.bAPSDAC_BE << 1) | (pAd->CommonCfg.bAPSDAC_BK << 2)
				| (pAd->CommonCfg.bAPSDAC_VI << 3)	| (pAd->CommonCfg.bAPSDAC_VO << 4)	| (pAd->CommonCfg.MaxSPLength << 5));

			wrq->u.data.length = sizeof(ULONG);
			Status = copy_to_user(wrq->u.data.pointer, &apsd, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_APSD_SETTING (=0x%lx,APSDCap=%d,AC_BE=%d,AC_BK=%d,AC_VI=%d,AC_VO=%d,MAXSPLen=%d)\n",
				apsd,pAd->CommonCfg.bAPSDCapable,pAd->CommonCfg.bAPSDAC_BE,pAd->CommonCfg.bAPSDAC_BK,pAd->CommonCfg.bAPSDAC_VI,pAd->CommonCfg.bAPSDAC_VO,pAd->CommonCfg.MaxSPLength));
			break;
		case RT_OID_802_11_QUERY_APSD_PSM:
			wrq->u.data.length = sizeof(ULONG);
			Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.bAPSDForcePowerSave, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_APSD_PSM (=%d)\n", pAd->CommonCfg.bAPSDForcePowerSave));
			break;
		case RT_OID_802_11_QUERY_WMM:
			wrq->u.data.length = sizeof(BOOLEAN);
			Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.bWmmCapable, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_WMM (=%d)\n",	pAd->CommonCfg.bWmmCapable));
			break;
#ifdef WPA_SUPPLICANT_SUPPORT
        case RT_OID_NEW_DRIVER:
            {
                UCHAR enabled = 1;
	        wrq->u.data.length = sizeof(UCHAR);
	        Status = copy_to_user(wrq->u.data.pointer, &enabled, wrq->u.data.length);
                DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_NEW_DRIVER (=%d)\n", enabled));
            }
	        break;
        case RT_OID_WPA_SUPPLICANT_SUPPORT:
	        wrq->u.data.length = sizeof(UCHAR);
	        Status = copy_to_user(wrq->u.data.pointer, &pAd->StaCfg.WpaSupplicantUP, wrq->u.data.length);
            DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_WPA_SUPPLICANT_SUPPORT (=%d)\n", pAd->StaCfg.WpaSupplicantUP));
	        break;
#endif // WPA_SUPPLICANT_SUPPORT //

        case RT_OID_DRIVER_DEVICE_NAME:
            DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_DRIVER_DEVICE_NAME \n"));
			wrq->u.data.length = 16;
			if (copy_to_user(wrq->u.data.pointer, pAd->StaCfg.dev_name, wrq->u.data.length))
			{
				Status = -EFAULT;
			}
            break;
        case RT_OID_802_11_QUERY_HT_PHYMODE:
            pHTPhyMode = (OID_SET_HT_PHYMODE *) kmalloc(sizeof(OID_SET_HT_PHYMODE), MEM_ALLOC_FLAG);
            if (pHTPhyMode)
            {
                pHTPhyMode->PhyMode = pAd->CommonCfg.PhyMode;
			pHTPhyMode->HtMode = (UCHAR)pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MODE;
			pHTPhyMode->BW = (UCHAR)pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.BW;
			pHTPhyMode->MCS= (UCHAR)pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.MCS;
			pHTPhyMode->SHORTGI= (UCHAR)pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.ShortGI;
			pHTPhyMode->STBC= (UCHAR)pAd->MacTab.Content[BSSID_WCID].HTPhyMode.field.STBC;

			pHTPhyMode->ExtOffset = ((pAd->CommonCfg.CentralChannel < pAd->CommonCfg.Channel) ? (EXTCHA_BELOW) : (EXTCHA_ABOVE));
                wrq->u.data.length = sizeof(OID_SET_HT_PHYMODE);
                if (copy_to_user(wrq->u.data.pointer, pHTPhyMode, wrq->u.data.length))
			{
				Status = -EFAULT;
			}
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_HT_PHYMODE (PhyMode = %d, MCS =%d, BW = %d, STBC = %d, ExtOffset=%d)\n",
				pHTPhyMode->HtMode, pHTPhyMode->MCS, pHTPhyMode->BW, pHTPhyMode->STBC, pHTPhyMode->ExtOffset));
			DBGPRINT(RT_DEBUG_TRACE, (" MlmeUpdateTxRates (.word = %x )\n", pAd->MacTab.Content[BSSID_WCID].HTPhyMode.word));
            }
            else
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_STA_CONFIG(kmalloc failed)\n"));
                Status = -EFAULT;
            }
            break;
        case RT_OID_802_11_COUNTRY_REGION:
            DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_COUNTRY_REGION \n"));
			wrq->u.data.length = sizeof(ulInfo);
            ulInfo = pAd->CommonCfg.CountryRegionForABand;
            ulInfo = (ulInfo << 8)|(pAd->CommonCfg.CountryRegion);
			if (copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length))
            {
				Status = -EFAULT;
            }
            break;
        case RT_OID_802_11_QUERY_DAT_HT_PHYMODE:
            pHTPhyMode = (OID_SET_HT_PHYMODE *) kmalloc(sizeof(OID_SET_HT_PHYMODE), MEM_ALLOC_FLAG);
            if (pHTPhyMode)
            {
                pHTPhyMode->PhyMode = pAd->CommonCfg.PhyMode;
			pHTPhyMode->HtMode = (UCHAR)pAd->CommonCfg.RegTransmitSetting.field.HTMODE;
			pHTPhyMode->BW = (UCHAR)pAd->CommonCfg.RegTransmitSetting.field.BW;
			pHTPhyMode->MCS= (UCHAR)pAd->StaCfg.DesiredTransmitSetting.field.MCS;
			pHTPhyMode->SHORTGI= (UCHAR)pAd->CommonCfg.RegTransmitSetting.field.ShortGI;
			pHTPhyMode->STBC= (UCHAR)pAd->CommonCfg.RegTransmitSetting.field.STBC;

                wrq->u.data.length = sizeof(OID_SET_HT_PHYMODE);
                if (copy_to_user(wrq->u.data.pointer, pHTPhyMode, wrq->u.data.length))
			{
				Status = -EFAULT;
			}
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_HT_PHYMODE (PhyMode = %d, MCS =%d, BW = %d, STBC = %d, ExtOffset=%d)\n",
				pHTPhyMode->HtMode, pHTPhyMode->MCS, pHTPhyMode->BW, pHTPhyMode->STBC, pHTPhyMode->ExtOffset));
			DBGPRINT(RT_DEBUG_TRACE, (" MlmeUpdateTxRates (.word = %x )\n", pAd->MacTab.Content[BSSID_WCID].HTPhyMode.word));
            }
            else
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_STA_CONFIG(kmalloc failed)\n"));
                Status = -EFAULT;
            }
            break;
        case RT_OID_QUERY_MULTIPLE_CARD_SUPPORT:
			wrq->u.data.length = sizeof(UCHAR);
            i = 0;
#ifdef MULTIPLE_CARD_SUPPORT
            i = 1;
#endif // MULTIPLE_CARD_SUPPORT //
			if (copy_to_user(wrq->u.data.pointer, &i, wrq->u.data.length))
            {
				Status = -EFAULT;
            }
            DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_QUERY_MULTIPLE_CARD_SUPPORT(=%d) \n", i));
            break;
#ifdef SNMP_SUPPORT
		case RT_OID_802_11_MAC_ADDRESS:
            wrq->u.data.length = MAC_ADDR_LEN;
            Status = copy_to_user(wrq->u.data.pointer, &pAd->CurrentAddress, wrq->u.data.length);
			break;

		case RT_OID_802_11_MANUFACTUREROUI:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_MANUFACTUREROUI \n"));
			wrq->u.data.length = ManufacturerOUI_LEN;
			Status = copy_to_user(wrq->u.data.pointer, &pAd->CurrentAddress, wrq->u.data.length);
			break;

		case RT_OID_802_11_MANUFACTURERNAME:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_MANUFACTURERNAME \n"));
			wrq->u.data.length = strlen(ManufacturerNAME);
			Status = copy_to_user(wrq->u.data.pointer, ManufacturerNAME, wrq->u.data.length);
			break;

		case RT_OID_802_11_RESOURCETYPEIDNAME:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_RESOURCETYPEIDNAME \n"));
			wrq->u.data.length = strlen(ResourceTypeIdName);
			Status = copy_to_user(wrq->u.data.pointer, ResourceTypeIdName, wrq->u.data.length);
			break;

		case RT_OID_802_11_PRIVACYOPTIONIMPLEMENTED:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_PRIVACYOPTIONIMPLEMENTED \n"));
			ulInfo = 1; // 1 is support wep else 2 is not support.
			wrq->u.data.length = sizeof(ulInfo);
			Status = copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length);
			break;

		case RT_OID_802_11_POWERMANAGEMENTMODE:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_POWERMANAGEMENTMODE \n"));
			if (pAd->StaCfg.Psm == PSMP_ACTION)
				ulInfo = 1; // 1 is power active else 2 is power save.
			else
				ulInfo = 2;

			wrq->u.data.length = sizeof(ulInfo);
			Status = copy_to_user(wrq->u.data.pointer, &ulInfo, wrq->u.data.length);
			break;

		case OID_802_11_WEPDEFAULTKEYVALUE:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_WEPDEFAULTKEYVALUE \n"));
			//KeyIdxValue.KeyIdx = pAd->PortCfg.MBSSID[pAd->IoctlIF].DefaultKeyId;
			pKeyIdxValue = wrq->u.data.pointer;
			DBGPRINT(RT_DEBUG_TRACE,("KeyIdxValue.KeyIdx = %d, \n",pKeyIdxValue->KeyIdx));
			valueLen = pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].KeyLen;
			NdisMoveMemory(pKeyIdxValue->Value,
						   &pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].Key,
						   valueLen);
			pKeyIdxValue->Value[valueLen]='\0';

			wrq->u.data.length = sizeof(DefaultKeyIdxValue);

			Status = copy_to_user(wrq->u.data.pointer, pKeyIdxValue, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE,("DefaultKeyId = %d, total len = %d, str len=%d, KeyValue= %02x %02x %02x %02x \n",
										pAd->StaCfg.DefaultKeyId,
										wrq->u.data.length,
										pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].KeyLen,
										pAd->SharedKey[BSS0][0].Key[0],
										pAd->SharedKey[BSS0][1].Key[0],
										pAd->SharedKey[BSS0][2].Key[0],
										pAd->SharedKey[BSS0][3].Key[0]));
			break;

		case OID_802_11_WEPDEFAULTKEYID:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_WEPDEFAULTKEYID \n"));
			wrq->u.data.length = sizeof(UCHAR);
			Status = copy_to_user(wrq->u.data.pointer, &pAd->StaCfg.DefaultKeyId, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("DefaultKeyId =%d \n", pAd->StaCfg.DefaultKeyId));
			break;

		case RT_OID_802_11_WEPKEYMAPPINGLENGTH:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_WEPKEYMAPPINGLENGTH \n"));
			wrq->u.data.length = sizeof(UCHAR);
			Status = copy_to_user(wrq->u.data.pointer,
									&pAd->SharedKey[BSS0][pAd->StaCfg.DefaultKeyId].KeyLen,
									wrq->u.data.length);
			break;

		case OID_802_11_SHORTRETRYLIMIT:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_SHORTRETRYLIMIT \n"));
			wrq->u.data.length = sizeof(ULONG);
			RTMP_IO_READ32(pAd, TX_RTY_CFG, &tx_rty_cfg.word);
			ShortRetryLimit = tx_rty_cfg.field.ShortRtyLimit;
			DBGPRINT(RT_DEBUG_TRACE, ("ShortRetryLimit =%ld,  tx_rty_cfg.field.ShortRetryLimit=%d\n", ShortRetryLimit, tx_rty_cfg.field.ShortRtyLimit));
			Status = copy_to_user(wrq->u.data.pointer, &ShortRetryLimit, wrq->u.data.length);
			break;

		case OID_802_11_LONGRETRYLIMIT:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_LONGRETRYLIMIT \n"));
			wrq->u.data.length = sizeof(ULONG);
			RTMP_IO_READ32(pAd, TX_RTY_CFG, &tx_rty_cfg.word);
			LongRetryLimit = tx_rty_cfg.field.LongRtyLimit;
			DBGPRINT(RT_DEBUG_TRACE, ("LongRetryLimit =%ld,  tx_rty_cfg.field.LongRtyLimit=%d\n", LongRetryLimit, tx_rty_cfg.field.LongRtyLimit));
			Status = copy_to_user(wrq->u.data.pointer, &LongRetryLimit, wrq->u.data.length);
			break;

		case RT_OID_802_11_PRODUCTID:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_PRODUCTID \n"));

#ifdef RTMP_MAC_PCI
			{

				USHORT  device_id;
				if (((POS_COOKIE)pAd->OS_Cookie)->pci_dev != NULL)
				pci_read_config_word(((POS_COOKIE)pAd->OS_Cookie)->pci_dev, PCI_DEVICE_ID, &device_id);
				else
					DBGPRINT(RT_DEBUG_TRACE, (" pci_dev = NULL\n"));
				sprintf((PSTRING)tmp, "%04x %04x\n", NIC_PCI_VENDOR_ID, device_id);
			}
#endif // RTMP_MAC_PCI //
			wrq->u.data.length = strlen((PSTRING)tmp);
			Status = copy_to_user(wrq->u.data.pointer, tmp, wrq->u.data.length);
			break;

		case RT_OID_802_11_MANUFACTUREID:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_MANUFACTUREID \n"));
			wrq->u.data.length = strlen(ManufacturerNAME);
			Status = copy_to_user(wrq->u.data.pointer, ManufacturerNAME, wrq->u.data.length);
			break;

		case OID_802_11_CURRENTCHANNEL:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_CURRENTCHANNEL \n"));
			wrq->u.data.length = sizeof(UCHAR);
			DBGPRINT(RT_DEBUG_TRACE, ("sizeof UCHAR=%d, channel=%d \n", sizeof(UCHAR), pAd->CommonCfg.Channel));
			Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.Channel, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Status=%d\n", Status));
			break;
#endif //SNMP_SUPPORT

		case OID_802_11_BUILD_CHANNEL_EX:
			{
				UCHAR value;
				DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_BUILD_CHANNEL_EX \n"));
				wrq->u.data.length = sizeof(UCHAR);
#ifdef EXT_BUILD_CHANNEL_LIST
				DBGPRINT(RT_DEBUG_TRACE, ("Support EXT_BUILD_CHANNEL_LIST.\n"));
				value = 1;
#else
				DBGPRINT(RT_DEBUG_TRACE, ("Doesn't support EXT_BUILD_CHANNEL_LIST.\n"));
				value = 0;
#endif // EXT_BUILD_CHANNEL_LIST //
				Status = copy_to_user(wrq->u.data.pointer, &value, 1);
				DBGPRINT(RT_DEBUG_TRACE, ("Status=%d\n", Status));
			}
			break;

		case OID_802_11_GET_CH_LIST:
			{
				PRT_CHANNEL_LIST_INFO pChListBuf;

				DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_GET_CH_LIST \n"));
				if (pAd->ChannelListNum == 0)
				{
					wrq->u.data.length = 0;
					break;
				}

				pChListBuf = (RT_CHANNEL_LIST_INFO *) kmalloc(sizeof(RT_CHANNEL_LIST_INFO), MEM_ALLOC_FLAG);
				if (pChListBuf == NULL)
				{
					wrq->u.data.length = 0;
					break;
				}

				pChListBuf->ChannelListNum = pAd->ChannelListNum;
				for (i = 0; i < pChListBuf->ChannelListNum; i++)
					pChListBuf->ChannelList[i] = pAd->ChannelList[i].Channel;

				wrq->u.data.length = sizeof(RT_CHANNEL_LIST_INFO);
				Status = copy_to_user(wrq->u.data.pointer, pChListBuf, sizeof(RT_CHANNEL_LIST_INFO));
				DBGPRINT(RT_DEBUG_TRACE, ("Status=%d\n", Status));

				if (pChListBuf)
					kfree(pChListBuf);
			}
			break;

		case OID_802_11_GET_COUNTRY_CODE:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_GET_COUNTRY_CODE \n"));
			wrq->u.data.length = 2;
			Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.CountryCode, 2);
			DBGPRINT(RT_DEBUG_TRACE, ("Status=%d\n", Status));
			break;

		case OID_802_11_GET_CHANNEL_GEOGRAPHY:
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_GET_CHANNEL_GEOGRAPHY \n"));
			wrq->u.data.length = 1;
			Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.Geography, 1);
			DBGPRINT(RT_DEBUG_TRACE, ("Status=%d\n", Status));
			break;


#ifdef QOS_DLS_SUPPORT
		case RT_OID_802_11_QUERY_DLS:
			wrq->u.data.length = sizeof(BOOLEAN);
			Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.bDLSCapable, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_DLS(=%d)\n", pAd->CommonCfg.bDLSCapable));
			break;

		case RT_OID_802_11_QUERY_DLS_PARAM:
			{
				PRT_802_11_DLS_INFO	pDlsInfo = kmalloc(sizeof(RT_802_11_DLS_INFO), GFP_ATOMIC);
				if (pDlsInfo == NULL)
					break;

				for (i=0; i<MAX_NUM_OF_DLS_ENTRY; i++)
				{
					RTMPMoveMemory(&pDlsInfo->Entry[i], &pAd->StaCfg.DLSEntry[i], sizeof(RT_802_11_DLS_UI));
				}

				pDlsInfo->num = MAX_NUM_OF_DLS_ENTRY;
				wrq->u.data.length = sizeof(RT_802_11_DLS_INFO);
				Status = copy_to_user(wrq->u.data.pointer, pDlsInfo, wrq->u.data.length);
				DBGPRINT(RT_DEBUG_TRACE, ("Query::RT_OID_802_11_QUERY_DLS_PARAM\n"));

				if (pDlsInfo)
					kfree(pDlsInfo);
			}
			break;
#endif // QOS_DLS_SUPPORT //

		case OID_802_11_SET_PSPXLINK_MODE:
			wrq->u.data.length = sizeof(BOOLEAN);
            Status = copy_to_user(wrq->u.data.pointer, &pAd->CommonCfg.PSPXlink, wrq->u.data.length);
			DBGPRINT(RT_DEBUG_TRACE, ("Query::OID_802_11_SET_PSPXLINK_MODE(=%d)\n", pAd->CommonCfg.PSPXlink));
			break;


        default:
            DBGPRINT(RT_DEBUG_TRACE, ("Query::unknown IOCTL's subcmd = 0x%08x\n", cmd));
            Status = -EOPNOTSUPP;
            break;
    }
    return Status;
}

INT rt28xx_sta_ioctl(
	IN	struct net_device	*net_dev,
	IN	OUT	struct ifreq	*rq,
	IN	INT					cmd)
{
	POS_COOKIE			pObj;
	RTMP_ADAPTER        *pAd = NULL;
	struct iwreq        *wrq = (struct iwreq *) rq;
	BOOLEAN				StateMachineTouched = FALSE;
	INT					Status = NDIS_STATUS_SUCCESS;
	USHORT				subcmd;


	pAd = RTMP_OS_NETDEV_GET_PRIV(net_dev);
	if (pAd == NULL)
	{
		/* if 1st open fail, pAd will be free;
		   So the net_dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}
	pObj = (POS_COOKIE) pAd->OS_Cookie;

    //check if the interface is down
    if(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
    {
#ifdef CONFIG_APSTA_MIXED_SUPPORT
	    if (wrq->u.data.pointer == NULL)
	    {
		    return Status;
	    }

	    if (strstr(wrq->u.data.pointer, "OpMode") == NULL)
#endif // CONFIG_APSTA_MIXED_SUPPORT //
		{
            DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		    return -ENETDOWN;
        }
    }

	{	// determine this ioctl command is comming from which interface.
		pObj->ioctl_if_type = INT_MAIN;
		pObj->ioctl_if = MAIN_MBSSID;
	}

	switch(cmd)
	{
#ifdef RALINK_ATE
#ifdef RALINK_28xx_QA
		case RTPRIV_IOCTL_ATE:
			{
				RtmpDoAte(pAd, wrq);
			}
			break;
#endif // RALINK_28xx_QA //
#endif // RALINK_ATE //
        case SIOCGIFHWADDR:
			DBGPRINT(RT_DEBUG_TRACE, ("IOCTL::SIOCGIFHWADDR\n"));
			memcpy(wrq->u.name, pAd->CurrentAddress, ETH_ALEN);
			break;
		case SIOCGIWNAME:
        {
		char *name=&wrq->u.name[0];
		rt_ioctl_giwname(net_dev, NULL, name, NULL);
            break;
		}
		case SIOCGIWESSID:  //Get ESSID
        {
		struct iw_point *essid=&wrq->u.essid;
		rt_ioctl_giwessid(net_dev, NULL, essid, essid->pointer);
            break;
		}
		case SIOCSIWESSID:  //Set ESSID
		{
		struct iw_point	*essid=&wrq->u.essid;
		rt_ioctl_siwessid(net_dev, NULL, essid, essid->pointer);
            break;
		}
		case SIOCSIWNWID:   // set network id (the cell)
		case SIOCGIWNWID:   // get network id
			Status = -EOPNOTSUPP;
			break;
		case SIOCSIWFREQ:   //set channel/frequency (Hz)
		{
		struct iw_freq *freq=&wrq->u.freq;
		rt_ioctl_siwfreq(net_dev, NULL, freq, NULL);
			break;
		}
		case SIOCGIWFREQ:   // get channel/frequency (Hz)
		{
		struct iw_freq *freq=&wrq->u.freq;
		rt_ioctl_giwfreq(net_dev, NULL, freq, NULL);
			break;
		}
		case SIOCSIWNICKN: //set node name/nickname
		{
		//struct iw_point *data=&wrq->u.data;
		//rt_ioctl_siwnickn(net_dev, NULL, data, NULL);
			break;
			}
		case SIOCGIWNICKN: //get node name/nickname
        {
			struct iw_point	*erq = NULL;
		erq = &wrq->u.data;
            erq->length = strlen((PSTRING) pAd->nickname);
            Status = copy_to_user(erq->pointer, pAd->nickname, erq->length);
			break;
		}
		case SIOCGIWRATE:   //get default bit rate (bps)
		    rt_ioctl_giwrate(net_dev, NULL, &wrq->u, NULL);
            break;
	    case SIOCSIWRATE:  //set default bit rate (bps)
	        rt_ioctl_siwrate(net_dev, NULL, &wrq->u, NULL);
            break;
        case SIOCGIWRTS:  // get RTS/CTS threshold (bytes)
		{
		struct iw_param *rts=&wrq->u.rts;
		rt_ioctl_giwrts(net_dev, NULL, rts, NULL);
            break;
		}
        case SIOCSIWRTS:  //set RTS/CTS threshold (bytes)
		{
		struct iw_param *rts=&wrq->u.rts;
		rt_ioctl_siwrts(net_dev, NULL, rts, NULL);
            break;
		}
        case SIOCGIWFRAG:  //get fragmentation thr (bytes)
		{
		struct iw_param *frag=&wrq->u.frag;
		rt_ioctl_giwfrag(net_dev, NULL, frag, NULL);
            break;
		}
        case SIOCSIWFRAG:  //set fragmentation thr (bytes)
		{
		struct iw_param *frag=&wrq->u.frag;
		rt_ioctl_siwfrag(net_dev, NULL, frag, NULL);
            break;
		}
        case SIOCGIWENCODE:  //get encoding token & mode
		{
		struct iw_point *erq=&wrq->u.encoding;
		if(erq)
			rt_ioctl_giwencode(net_dev, NULL, erq, erq->pointer);
            break;
		}
        case SIOCSIWENCODE:  //set encoding token & mode
		{
		struct iw_point *erq=&wrq->u.encoding;
		if(erq)
			rt_ioctl_siwencode(net_dev, NULL, erq, erq->pointer);
            break;
		}
		case SIOCGIWAP:     //get access point MAC addresses
		{
		struct sockaddr *ap_addr=&wrq->u.ap_addr;
		rt_ioctl_giwap(net_dev, NULL, ap_addr, ap_addr->sa_data);
			break;
		}
	    case SIOCSIWAP:  //set access point MAC addresses
		{
		struct sockaddr *ap_addr=&wrq->u.ap_addr;
		rt_ioctl_siwap(net_dev, NULL, ap_addr, ap_addr->sa_data);
            break;
		}
		case SIOCGIWMODE:   //get operation mode
		{
		__u32 *mode=&wrq->u.mode;
		rt_ioctl_giwmode(net_dev, NULL, mode, NULL);
            break;
		}
		case SIOCSIWMODE:   //set operation mode
		{
		__u32 *mode=&wrq->u.mode;
		rt_ioctl_siwmode(net_dev, NULL, mode, NULL);
            break;
		}
		case SIOCGIWSENS:   //get sensitivity (dBm)
		case SIOCSIWSENS:	//set sensitivity (dBm)
		case SIOCGIWPOWER:  //get Power Management settings
		case SIOCSIWPOWER:  //set Power Management settings
		case SIOCGIWTXPOW:  //get transmit power (dBm)
		case SIOCSIWTXPOW:  //set transmit power (dBm)
		case SIOCGIWRANGE:	//Get range of parameters
		case SIOCGIWRETRY:	//get retry limits and lifetime
		case SIOCSIWRETRY:	//set retry limits and lifetime
			Status = -EOPNOTSUPP;
			break;
		case RT_PRIV_IOCTL:
        case RT_PRIV_IOCTL_EXT:
			subcmd = wrq->u.data.flags;
			if( subcmd & OID_GET_SET_TOGGLE)
				Status = RTMPSetInformation(pAd, rq, subcmd);
			else
				Status = RTMPQueryInformation(pAd, rq, subcmd);
			break;
		case SIOCGIWPRIV:
			if (wrq->u.data.pointer)
			{
				if ( access_ok(VERIFY_WRITE, wrq->u.data.pointer, sizeof(privtab)) != TRUE)
					break;
				wrq->u.data.length = sizeof(privtab) / sizeof(privtab[0]);
				if (copy_to_user(wrq->u.data.pointer, privtab, sizeof(privtab)))
					Status = -EFAULT;
			}
			break;
		case RTPRIV_IOCTL_SET:
			if(access_ok(VERIFY_READ, wrq->u.data.pointer, wrq->u.data.length) != TRUE)
					break;
			rt_ioctl_setparam(net_dev, NULL, NULL, wrq->u.data.pointer);
			break;
		case RTPRIV_IOCTL_GSITESURVEY:
			RTMPIoctlGetSiteSurvey(pAd, wrq);
		    break;
#ifdef DBG
		case RTPRIV_IOCTL_MAC:
			RTMPIoctlMAC(pAd, wrq);
			break;
		case RTPRIV_IOCTL_E2P:
			RTMPIoctlE2PROM(pAd, wrq);
			break;
#ifdef RTMP_RF_RW_SUPPORT
		case RTPRIV_IOCTL_RF:
			RTMPIoctlRF(pAd, wrq);
			break;
#endif // RTMP_RF_RW_SUPPORT //
#endif // DBG //

        case SIOCETHTOOL:
                break;
		default:
			DBGPRINT(RT_DEBUG_ERROR, ("IOCTL::unknown IOCTL's cmd = 0x%08x\n", cmd));
			Status = -EOPNOTSUPP;
			break;
	}

    if(StateMachineTouched) // Upper layer sent a MLME-related operations
	RTMP_MLME_HANDLER(pAd);

	return Status;
}

/*
    ==========================================================================
    Description:
        Set SSID
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_SSID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    NDIS_802_11_SSID                    Ssid, *pSsid=NULL;
    BOOLEAN                             StateMachineTouched = FALSE;
    int                                 success = TRUE;

    if( strlen(arg) <= MAX_LEN_OF_SSID)
    {
        NdisZeroMemory(&Ssid, sizeof(NDIS_802_11_SSID));
        if (strlen(arg) != 0)
        {
            NdisMoveMemory(Ssid.Ssid, arg, strlen(arg));
            Ssid.SsidLength = strlen(arg);
        }
        else   //ANY ssid
        {
            Ssid.SsidLength = 0;
	    memcpy(Ssid.Ssid, "", 0);
		pAdapter->StaCfg.BssType = BSS_INFRA;
		pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
        pAdapter->StaCfg.WepStatus  = Ndis802_11EncryptionDisabled;
	}
        pSsid = &Ssid;

        if (pAdapter->Mlme.CntlMachine.CurrState != CNTL_IDLE)
        {
            RTMP_MLME_RESET_STATE_MACHINE(pAdapter);
            DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
        }

		if ((pAdapter->StaCfg.WpaPassPhraseLen >= 8) &&
			(pAdapter->StaCfg.WpaPassPhraseLen <= 64))
		{
			STRING passphrase_str[65] = {0};
			UCHAR keyMaterial[40];

			RTMPMoveMemory(passphrase_str, pAdapter->StaCfg.WpaPassPhrase, pAdapter->StaCfg.WpaPassPhraseLen);
			RTMPZeroMemory(pAdapter->StaCfg.PMK, 32);
			if (pAdapter->StaCfg.WpaPassPhraseLen == 64)
			{
			    AtoH((PSTRING) pAdapter->StaCfg.WpaPassPhrase, pAdapter->StaCfg.PMK, 32);
			}
			else
			{
			    PasswordHash((PSTRING) pAdapter->StaCfg.WpaPassPhrase, Ssid.Ssid, Ssid.SsidLength, keyMaterial);
			    NdisMoveMemory(pAdapter->StaCfg.PMK, keyMaterial, 32);
			}
		}

        pAdapter->MlmeAux.CurrReqIsFromNdis = TRUE;
        pAdapter->StaCfg.bScanReqIsFromWebUI = FALSE;
		pAdapter->bConfigChanged = TRUE;

        MlmeEnqueue(pAdapter,
                    MLME_CNTL_STATE_MACHINE,
                    OID_802_11_SSID,
                    sizeof(NDIS_802_11_SSID),
                    (VOID *)pSsid);

        StateMachineTouched = TRUE;
        DBGPRINT(RT_DEBUG_TRACE, ("Set_SSID_Proc::(Len=%d,Ssid=%s)\n", Ssid.SsidLength, Ssid.Ssid));
    }
    else
        success = FALSE;

    if (StateMachineTouched) // Upper layer sent a MLME-related operations
	RTMP_MLME_HANDLER(pAdapter);

    return success;
}

#ifdef WMM_SUPPORT
/*
    ==========================================================================
    Description:
        Set WmmCapable Enable or Disable
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT	Set_WmmCapable_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	BOOLEAN	bWmmCapable;

	bWmmCapable = simple_strtol(arg, 0, 10);

	if ((bWmmCapable == 1)
		)
		pAd->CommonCfg.bWmmCapable = TRUE;
	else if (bWmmCapable == 0)
		pAd->CommonCfg.bWmmCapable = FALSE;
	else
		return FALSE;  //Invalid argument

	DBGPRINT(RT_DEBUG_TRACE, ("Set_WmmCapable_Proc::(bWmmCapable=%d)\n",
		pAd->CommonCfg.bWmmCapable));

	return TRUE;
}
#endif // WMM_SUPPORT //

/*
    ==========================================================================
    Description:
        Set Network Type(Infrastructure/Adhoc mode)
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_NetworkType_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    UINT32	Value = 0;

    if (strcmp(arg, "Adhoc") == 0)
	{
		if (pAdapter->StaCfg.BssType != BSS_ADHOC)
		{
			// Config has changed
			pAdapter->bConfigChanged = TRUE;
            if (MONITOR_ON(pAdapter))
            {
                RTMP_IO_WRITE32(pAdapter, RX_FILTR_CFG, STANORMAL);
                RTMP_IO_READ32(pAdapter, MAC_SYS_CTRL, &Value);
				Value &= (~0x80);
				RTMP_IO_WRITE32(pAdapter, MAC_SYS_CTRL, Value);
                OPSTATUS_CLEAR_FLAG(pAdapter, fOP_STATUS_MEDIA_STATE_CONNECTED);
                pAdapter->StaCfg.bAutoReconnect = TRUE;
                LinkDown(pAdapter, FALSE);
            }
			if (INFRA_ON(pAdapter))
			{
				//BOOLEAN Cancelled;
				// Set the AutoReconnectSsid to prevent it reconnect to old SSID
				// Since calling this indicate user don't want to connect to that SSID anymore.
				pAdapter->MlmeAux.AutoReconnectSsidLen= 32;
				NdisZeroMemory(pAdapter->MlmeAux.AutoReconnectSsid, pAdapter->MlmeAux.AutoReconnectSsidLen);

				LinkDown(pAdapter, FALSE);

				DBGPRINT(RT_DEBUG_TRACE, ("NDIS_STATUS_MEDIA_DISCONNECT Event BB!\n"));
			}
		}
		pAdapter->StaCfg.BssType = BSS_ADHOC;
        pAdapter->net_dev->type = pAdapter->StaCfg.OriDevType;
		DBGPRINT(RT_DEBUG_TRACE, ("===>Set_NetworkType_Proc::(AD-HOC)\n"));
	}
    else if (strcmp(arg, "Infra") == 0)
	{
		if (pAdapter->StaCfg.BssType != BSS_INFRA)
		{
			// Config has changed
			pAdapter->bConfigChanged = TRUE;
            if (MONITOR_ON(pAdapter))
            {
                RTMP_IO_WRITE32(pAdapter, RX_FILTR_CFG, STANORMAL);
                RTMP_IO_READ32(pAdapter, MAC_SYS_CTRL, &Value);
				Value &= (~0x80);
				RTMP_IO_WRITE32(pAdapter, MAC_SYS_CTRL, Value);
                OPSTATUS_CLEAR_FLAG(pAdapter, fOP_STATUS_MEDIA_STATE_CONNECTED);
                pAdapter->StaCfg.bAutoReconnect = TRUE;
                LinkDown(pAdapter, FALSE);
            }
			if (ADHOC_ON(pAdapter))
			{
				// Set the AutoReconnectSsid to prevent it reconnect to old SSID
				// Since calling this indicate user don't want to connect to that SSID anymore.
				pAdapter->MlmeAux.AutoReconnectSsidLen= 32;
				NdisZeroMemory(pAdapter->MlmeAux.AutoReconnectSsid, pAdapter->MlmeAux.AutoReconnectSsidLen);

				LinkDown(pAdapter, FALSE);
			}
		}
		pAdapter->StaCfg.BssType = BSS_INFRA;
        pAdapter->net_dev->type = pAdapter->StaCfg.OriDevType;
		DBGPRINT(RT_DEBUG_TRACE, ("===>Set_NetworkType_Proc::(INFRA)\n"));
	}
    else if (strcmp(arg, "Monitor") == 0)
    {
			UCHAR	bbpValue = 0;
			BCN_TIME_CFG_STRUC csr;
			OPSTATUS_CLEAR_FLAG(pAdapter, fOP_STATUS_INFRA_ON);
            OPSTATUS_CLEAR_FLAG(pAdapter, fOP_STATUS_ADHOC_ON);
			OPSTATUS_SET_FLAG(pAdapter, fOP_STATUS_MEDIA_STATE_CONNECTED);
			// disable all periodic state machine
			pAdapter->StaCfg.bAutoReconnect = FALSE;
			// reset all mlme state machine
			RTMP_MLME_RESET_STATE_MACHINE(pAdapter);
			DBGPRINT(RT_DEBUG_TRACE, ("fOP_STATUS_MEDIA_STATE_CONNECTED \n"));
            if (pAdapter->CommonCfg.CentralChannel == 0)
            {
#ifdef DOT11_N_SUPPORT
                if (pAdapter->CommonCfg.PhyMode == PHY_11AN_MIXED)
                    pAdapter->CommonCfg.CentralChannel = 36;
                else
#endif // DOT11_N_SUPPORT //
                    pAdapter->CommonCfg.CentralChannel = 6;
            }
#ifdef DOT11_N_SUPPORT
            else
                N_ChannelCheck(pAdapter);
#endif // DOT11_N_SUPPORT //

#ifdef DOT11_N_SUPPORT
			if (pAdapter->CommonCfg.PhyMode >= PHY_11ABGN_MIXED &&
                pAdapter->CommonCfg.RegTransmitSetting.field.BW == BW_40 &&
                pAdapter->CommonCfg.RegTransmitSetting.field.EXTCHA == EXTCHA_ABOVE)
			{
				// 40MHz ,control channel at lower
				RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, BBP_R4, &bbpValue);
				bbpValue &= (~0x18);
				bbpValue |= 0x10;
				RTMP_BBP_IO_WRITE8_BY_REG_ID(pAdapter, BBP_R4, bbpValue);
				pAdapter->CommonCfg.BBPCurrentBW = BW_40;
				//  RX : control channel at lower
				RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, BBP_R3, &bbpValue);
				bbpValue &= (~0x20);
				RTMP_BBP_IO_WRITE8_BY_REG_ID(pAdapter, BBP_R3, bbpValue);

				RTMP_IO_READ32(pAdapter, TX_BAND_CFG, &Value);
				Value &= 0xfffffffe;
				RTMP_IO_WRITE32(pAdapter, TX_BAND_CFG, Value);
				pAdapter->CommonCfg.CentralChannel = pAdapter->CommonCfg.Channel + 2;
                AsicSwitchChannel(pAdapter, pAdapter->CommonCfg.CentralChannel, FALSE);
			    AsicLockChannel(pAdapter, pAdapter->CommonCfg.CentralChannel);
                DBGPRINT(RT_DEBUG_TRACE, ("BW_40 ,control_channel(%d), CentralChannel(%d) \n",
                                           pAdapter->CommonCfg.Channel,
                                           pAdapter->CommonCfg.CentralChannel));
			}
			else if (pAdapter->CommonCfg.PhyMode >= PHY_11ABGN_MIXED &&
                     pAdapter->CommonCfg.RegTransmitSetting.field.BW == BW_40 &&
                     pAdapter->CommonCfg.RegTransmitSetting.field.EXTCHA == EXTCHA_BELOW)
			{
				// 40MHz ,control channel at upper
				RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, BBP_R4, &bbpValue);
				bbpValue &= (~0x18);
				bbpValue |= 0x10;
				RTMP_BBP_IO_WRITE8_BY_REG_ID(pAdapter, BBP_R4, bbpValue);
				pAdapter->CommonCfg.BBPCurrentBW = BW_40;
				RTMP_IO_READ32(pAdapter, TX_BAND_CFG, &Value);
				Value |= 0x1;
				RTMP_IO_WRITE32(pAdapter, TX_BAND_CFG, Value);

				RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, BBP_R3, &bbpValue);
				bbpValue |= (0x20);
				RTMP_BBP_IO_WRITE8_BY_REG_ID(pAdapter, BBP_R3, bbpValue);
				pAdapter->CommonCfg.CentralChannel = pAdapter->CommonCfg.Channel - 2;
                AsicSwitchChannel(pAdapter, pAdapter->CommonCfg.CentralChannel, FALSE);
			    AsicLockChannel(pAdapter, pAdapter->CommonCfg.CentralChannel);
                DBGPRINT(RT_DEBUG_TRACE, ("BW_40 ,control_channel(%d), CentralChannel(%d) \n",
                                           pAdapter->CommonCfg.Channel,
                                           pAdapter->CommonCfg.CentralChannel));
			}
			else
#endif // DOT11_N_SUPPORT //
			{
				// 20MHz
				RTMP_BBP_IO_READ8_BY_REG_ID(pAdapter, BBP_R4, &bbpValue);
				bbpValue &= (~0x18);
				RTMP_BBP_IO_WRITE8_BY_REG_ID(pAdapter, BBP_R4, bbpValue);
				pAdapter->CommonCfg.BBPCurrentBW = BW_20;
                AsicSwitchChannel(pAdapter, pAdapter->CommonCfg.Channel, FALSE);
			    AsicLockChannel(pAdapter, pAdapter->CommonCfg.Channel);
				DBGPRINT(RT_DEBUG_TRACE, ("BW_20, Channel(%d)\n", pAdapter->CommonCfg.Channel));
			}
			// Enable Rx with promiscuous reception
			RTMP_IO_WRITE32(pAdapter, RX_FILTR_CFG, 0x3);
			// ASIC supporsts sniffer function with replacing RSSI with timestamp.
			//RTMP_IO_READ32(pAdapter, MAC_SYS_CTRL, &Value);
			//Value |= (0x80);
			//RTMP_IO_WRITE32(pAdapter, MAC_SYS_CTRL, Value);
			// disable sync
			RTMP_IO_READ32(pAdapter, BCN_TIME_CFG, &csr.word);
			csr.field.bBeaconGen = 0;
			csr.field.bTBTTEnable = 0;
			csr.field.TsfSyncMode = 0;
			RTMP_IO_WRITE32(pAdapter, BCN_TIME_CFG, csr.word);

			pAdapter->StaCfg.BssType = BSS_MONITOR;
            pAdapter->net_dev->type = ARPHRD_IEEE80211_PRISM; //ARPHRD_IEEE80211; // IEEE80211
			DBGPRINT(RT_DEBUG_TRACE, ("===>Set_NetworkType_Proc::(MONITOR)\n"));
    }

    // Reset Ralink supplicant to not use, it will be set to start when UI set PMK key
    pAdapter->StaCfg.WpaState = SS_NOTUSE;

    DBGPRINT(RT_DEBUG_TRACE, ("Set_NetworkType_Proc::(NetworkType=%d)\n", pAdapter->StaCfg.BssType));

    return TRUE;
}

/*
    ==========================================================================
    Description:
        Set Authentication mode
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_AuthMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    if ((strcmp(arg, "WEPAUTO") == 0) || (strcmp(arg, "wepauto") == 0))
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeAutoSwitch;
    else if ((strcmp(arg, "OPEN") == 0) || (strcmp(arg, "open") == 0))
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeOpen;
    else if ((strcmp(arg, "SHARED") == 0) || (strcmp(arg, "shared") == 0))
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeShared;
    else if ((strcmp(arg, "WPAPSK") == 0) || (strcmp(arg, "wpapsk") == 0))
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPAPSK;
    else if ((strcmp(arg, "WPANONE") == 0) || (strcmp(arg, "wpanone") == 0))
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPANone;
    else if ((strcmp(arg, "WPA2PSK") == 0) || (strcmp(arg, "wpa2psk") == 0))
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA2PSK;
#ifdef WPA_SUPPLICANT_SUPPORT
    else if ((strcmp(arg, "WPA") == 0) || (strcmp(arg, "wpa") == 0))
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA;
    else if ((strcmp(arg, "WPA2") == 0) || (strcmp(arg, "wpa2") == 0))
        pAdapter->StaCfg.AuthMode = Ndis802_11AuthModeWPA2;
#endif // WPA_SUPPLICANT_SUPPORT //
    else
        return FALSE;

    pAdapter->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;

    DBGPRINT(RT_DEBUG_TRACE, ("Set_AuthMode_Proc::(AuthMode=%d)\n", pAdapter->StaCfg.AuthMode));

    return TRUE;
}

/*
    ==========================================================================
    Description:
        Set Encryption Type
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_EncrypType_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    if ((strcmp(arg, "NONE") == 0) || (strcmp(arg, "none") == 0))
    {
        if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
            return TRUE;    // do nothing

        pAdapter->StaCfg.WepStatus     = Ndis802_11WEPDisabled;
        pAdapter->StaCfg.PairCipher    = Ndis802_11WEPDisabled;
	    pAdapter->StaCfg.GroupCipher   = Ndis802_11WEPDisabled;
    }
    else if ((strcmp(arg, "WEP") == 0) || (strcmp(arg, "wep") == 0))
    {
        if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
            return TRUE;    // do nothing

        pAdapter->StaCfg.WepStatus     = Ndis802_11WEPEnabled;
        pAdapter->StaCfg.PairCipher    = Ndis802_11WEPEnabled;
	    pAdapter->StaCfg.GroupCipher   = Ndis802_11WEPEnabled;
    }
    else if ((strcmp(arg, "TKIP") == 0) || (strcmp(arg, "tkip") == 0))
    {
        if (pAdapter->StaCfg.AuthMode < Ndis802_11AuthModeWPA)
            return TRUE;    // do nothing

        pAdapter->StaCfg.WepStatus     = Ndis802_11Encryption2Enabled;
        pAdapter->StaCfg.PairCipher    = Ndis802_11Encryption2Enabled;
	    pAdapter->StaCfg.GroupCipher   = Ndis802_11Encryption2Enabled;
    }
    else if ((strcmp(arg, "AES") == 0) || (strcmp(arg, "aes") == 0))
    {
        if (pAdapter->StaCfg.AuthMode < Ndis802_11AuthModeWPA)
            return TRUE;    // do nothing

        pAdapter->StaCfg.WepStatus     = Ndis802_11Encryption3Enabled;
        pAdapter->StaCfg.PairCipher    = Ndis802_11Encryption3Enabled;
	    pAdapter->StaCfg.GroupCipher   = Ndis802_11Encryption3Enabled;
    }
    else
        return FALSE;

    pAdapter->StaCfg.OrigWepStatus = pAdapter->StaCfg.WepStatus;

    DBGPRINT(RT_DEBUG_TRACE, ("Set_EncrypType_Proc::(EncrypType=%d)\n", pAdapter->StaCfg.WepStatus));

    return TRUE;
}

/*
    ==========================================================================
    Description:
        Set Default Key ID
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_DefaultKeyID_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    ULONG                               KeyIdx;

    KeyIdx = simple_strtol(arg, 0, 10);
    if((KeyIdx >= 1 ) && (KeyIdx <= 4))
        pAdapter->StaCfg.DefaultKeyId = (UCHAR) (KeyIdx - 1 );
    else
        return FALSE;  //Invalid argument

    DBGPRINT(RT_DEBUG_TRACE, ("Set_DefaultKeyID_Proc::(DefaultKeyID=%d)\n", pAdapter->StaCfg.DefaultKeyId));

    return TRUE;
}

/*
    ==========================================================================
    Description:
        Set WEP KEY1
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_Key1_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    int                                 KeyLen;
    int                                 i;
    UCHAR                               CipherAlg=CIPHER_WEP64;

    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
        return TRUE;    // do nothing

    KeyLen = strlen(arg);

    switch (KeyLen)
    {
        case 5: //wep 40 Ascii type
            pAdapter->SharedKey[BSS0][0].KeyLen = KeyLen;
            memcpy(pAdapter->SharedKey[BSS0][0].Key, arg, KeyLen);
            CipherAlg = CIPHER_WEP64;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key1_Proc::(Key1=%s and type=%s)\n", arg, "Ascii"));
            break;
        case 10: //wep 40 Hex type
            for(i=0; i < KeyLen; i++)
            {
                if( !isxdigit(*(arg+i)) )
                    return FALSE;  //Not Hex value;
            }
            pAdapter->SharedKey[BSS0][0].KeyLen = KeyLen / 2 ;
            AtoH(arg, pAdapter->SharedKey[BSS0][0].Key, KeyLen / 2);
            CipherAlg = CIPHER_WEP64;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key1_Proc::(Key1=%s and type=%s)\n", arg, "Hex"));
            break;
        case 13: //wep 104 Ascii type
            pAdapter->SharedKey[BSS0][0].KeyLen = KeyLen;
            memcpy(pAdapter->SharedKey[BSS0][0].Key, arg, KeyLen);
            CipherAlg = CIPHER_WEP128;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key1_Proc::(Key1=%s and type=%s)\n", arg, "Ascii"));
            break;
        case 26: //wep 104 Hex type
            for(i=0; i < KeyLen; i++)
            {
                if( !isxdigit(*(arg+i)) )
                    return FALSE;  //Not Hex value;
            }
            pAdapter->SharedKey[BSS0][0].KeyLen = KeyLen / 2 ;
            AtoH(arg, pAdapter->SharedKey[BSS0][0].Key, KeyLen / 2);
            CipherAlg = CIPHER_WEP128;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key1_Proc::(Key1=%s and type=%s)\n", arg, "Hex"));
            break;
        default: //Invalid argument
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key1_Proc::Invalid argument (=%s)\n", arg));
            return FALSE;
    }

    pAdapter->SharedKey[BSS0][0].CipherAlg = CipherAlg;

    // Set keys (into ASIC)
    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
        ;   // not support
    else    // Old WEP stuff
    {
        AsicAddSharedKeyEntry(pAdapter,
                              0,
                              0,
                              pAdapter->SharedKey[BSS0][0].CipherAlg,
                              pAdapter->SharedKey[BSS0][0].Key,
                              NULL,
                              NULL);
    }

    return TRUE;
}
/*
    ==========================================================================

    Description:
        Set WEP KEY2
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_Key2_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    int                                 KeyLen;
    int                                 i;
    UCHAR                               CipherAlg=CIPHER_WEP64;

    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
        return TRUE;    // do nothing

    KeyLen = strlen(arg);

    switch (KeyLen)
    {
        case 5: //wep 40 Ascii type
            pAdapter->SharedKey[BSS0][1].KeyLen = KeyLen;
            memcpy(pAdapter->SharedKey[BSS0][1].Key, arg, KeyLen);
            CipherAlg = CIPHER_WEP64;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key2_Proc::(Key2=%s and type=%s)\n", arg, "Ascii"));
            break;
        case 10: //wep 40 Hex type
            for(i=0; i < KeyLen; i++)
            {
                if( !isxdigit(*(arg+i)) )
                    return FALSE;  //Not Hex value;
            }
            pAdapter->SharedKey[BSS0][1].KeyLen = KeyLen / 2 ;
            AtoH(arg, pAdapter->SharedKey[BSS0][1].Key, KeyLen / 2);
            CipherAlg = CIPHER_WEP64;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key2_Proc::(Key2=%s and type=%s)\n", arg, "Hex"));
            break;
        case 13: //wep 104 Ascii type
            pAdapter->SharedKey[BSS0][1].KeyLen = KeyLen;
            memcpy(pAdapter->SharedKey[BSS0][1].Key, arg, KeyLen);
            CipherAlg = CIPHER_WEP128;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key2_Proc::(Key2=%s and type=%s)\n", arg, "Ascii"));
            break;
        case 26: //wep 104 Hex type
            for(i=0; i < KeyLen; i++)
            {
                if( !isxdigit(*(arg+i)) )
                    return FALSE;  //Not Hex value;
            }
            pAdapter->SharedKey[BSS0][1].KeyLen = KeyLen / 2 ;
            AtoH(arg, pAdapter->SharedKey[BSS0][1].Key, KeyLen / 2);
            CipherAlg = CIPHER_WEP128;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key2_Proc::(Key2=%s and type=%s)\n", arg, "Hex"));
            break;
        default: //Invalid argument
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key2_Proc::Invalid argument (=%s)\n", arg));
            return FALSE;
    }
    pAdapter->SharedKey[BSS0][1].CipherAlg = CipherAlg;

    // Set keys (into ASIC)
    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
        ;   // not support
    else    // Old WEP stuff
    {
        AsicAddSharedKeyEntry(pAdapter,
                              0,
                              1,
                              pAdapter->SharedKey[BSS0][1].CipherAlg,
                              pAdapter->SharedKey[BSS0][1].Key,
                              NULL,
                              NULL);
    }

    return TRUE;
}
/*
    ==========================================================================
    Description:
        Set WEP KEY3
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_Key3_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    int                                 KeyLen;
    int                                 i;
    UCHAR                               CipherAlg=CIPHER_WEP64;

    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
        return TRUE;    // do nothing

    KeyLen = strlen(arg);

    switch (KeyLen)
    {
        case 5: //wep 40 Ascii type
            pAdapter->SharedKey[BSS0][2].KeyLen = KeyLen;
            memcpy(pAdapter->SharedKey[BSS0][2].Key, arg, KeyLen);
            CipherAlg = CIPHER_WEP64;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key3_Proc::(Key3=%s and type=Ascii)\n", arg));
            break;
        case 10: //wep 40 Hex type
            for(i=0; i < KeyLen; i++)
            {
                if( !isxdigit(*(arg+i)) )
                    return FALSE;  //Not Hex value;
            }
            pAdapter->SharedKey[BSS0][2].KeyLen = KeyLen / 2 ;
            AtoH(arg, pAdapter->SharedKey[BSS0][2].Key, KeyLen / 2);
            CipherAlg = CIPHER_WEP64;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key3_Proc::(Key3=%s and type=Hex)\n", arg));
            break;
        case 13: //wep 104 Ascii type
            pAdapter->SharedKey[BSS0][2].KeyLen = KeyLen;
            memcpy(pAdapter->SharedKey[BSS0][2].Key, arg, KeyLen);
            CipherAlg = CIPHER_WEP128;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key3_Proc::(Key3=%s and type=Ascii)\n", arg));
            break;
        case 26: //wep 104 Hex type
            for(i=0; i < KeyLen; i++)
            {
                if( !isxdigit(*(arg+i)) )
                    return FALSE;  //Not Hex value;
            }
            pAdapter->SharedKey[BSS0][2].KeyLen = KeyLen / 2 ;
            AtoH(arg, pAdapter->SharedKey[BSS0][2].Key, KeyLen / 2);
            CipherAlg = CIPHER_WEP128;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key3_Proc::(Key3=%s and type=Hex)\n", arg));
            break;
        default: //Invalid argument
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key3_Proc::Invalid argument (=%s)\n", arg));
            return FALSE;
    }
    pAdapter->SharedKey[BSS0][2].CipherAlg = CipherAlg;

    // Set keys (into ASIC)
    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
        ;   // not support
    else    // Old WEP stuff
    {
        AsicAddSharedKeyEntry(pAdapter,
                              0,
                              2,
                              pAdapter->SharedKey[BSS0][2].CipherAlg,
                              pAdapter->SharedKey[BSS0][2].Key,
                              NULL,
                              NULL);
    }

    return TRUE;
}
/*
    ==========================================================================
    Description:
        Set WEP KEY4
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_Key4_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    int                                 KeyLen;
    int                                 i;
    UCHAR                               CipherAlg=CIPHER_WEP64;

    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
        return TRUE;    // do nothing

    KeyLen = strlen(arg);

    switch (KeyLen)
    {
        case 5: //wep 40 Ascii type
            pAdapter->SharedKey[BSS0][3].KeyLen = KeyLen;
            memcpy(pAdapter->SharedKey[BSS0][3].Key, arg, KeyLen);
            CipherAlg = CIPHER_WEP64;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key4_Proc::(Key4=%s and type=%s)\n", arg, "Ascii"));
            break;
        case 10: //wep 40 Hex type
            for(i=0; i < KeyLen; i++)
            {
                if( !isxdigit(*(arg+i)) )
                    return FALSE;  //Not Hex value;
            }
            pAdapter->SharedKey[BSS0][3].KeyLen = KeyLen / 2 ;
            AtoH(arg, pAdapter->SharedKey[BSS0][3].Key, KeyLen / 2);
            CipherAlg = CIPHER_WEP64;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key4_Proc::(Key4=%s and type=%s)\n", arg, "Hex"));
            break;
        case 13: //wep 104 Ascii type
            pAdapter->SharedKey[BSS0][3].KeyLen = KeyLen;
            memcpy(pAdapter->SharedKey[BSS0][3].Key, arg, KeyLen);
            CipherAlg = CIPHER_WEP128;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key4_Proc::(Key4=%s and type=%s)\n", arg, "Ascii"));
            break;
        case 26: //wep 104 Hex type
            for(i=0; i < KeyLen; i++)
            {
                if( !isxdigit(*(arg+i)) )
                    return FALSE;  //Not Hex value;
            }
            pAdapter->SharedKey[BSS0][3].KeyLen = KeyLen / 2 ;
            AtoH(arg, pAdapter->SharedKey[BSS0][3].Key, KeyLen / 2);
            CipherAlg = CIPHER_WEP128;
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key4_Proc::(Key4=%s and type=%s)\n", arg, "Hex"));
            break;
        default: //Invalid argument
            DBGPRINT(RT_DEBUG_TRACE, ("Set_Key4_Proc::Invalid argument (=%s)\n", arg));
            return FALSE;
    }
    pAdapter->SharedKey[BSS0][3].CipherAlg = CipherAlg;

    // Set keys (into ASIC)
    if (pAdapter->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
        ;   // not support
    else    // Old WEP stuff
    {
        AsicAddSharedKeyEntry(pAdapter,
                              0,
                              3,
                              pAdapter->SharedKey[BSS0][3].CipherAlg,
                              pAdapter->SharedKey[BSS0][3].Key,
                              NULL,
                              NULL);
    }

    return TRUE;
}

/*
    ==========================================================================
    Description:
        Set WPA PSK key
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_WPAPSK_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING          arg)
{
    int status;

    if ((pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPAPSK) &&
        (pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPA2PSK) &&
	    (pAd->StaCfg.AuthMode != Ndis802_11AuthModeWPANone)
		)
        return TRUE;    // do nothing

    DBGPRINT(RT_DEBUG_TRACE, ("Set_WPAPSK_Proc::(WPAPSK=%s)\n", arg));

	status = RT_CfgSetWPAPSKKey(pAd, arg, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen, pAd->StaCfg.PMK);
	if (status == FALSE)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("Set_WPAPSK_Proc(): Set key failed!\n"));
		return FALSE;
	}
	NdisZeroMemory(pAd->StaCfg.WpaPassPhrase, 64);
    NdisMoveMemory(pAd->StaCfg.WpaPassPhrase, arg, strlen(arg));
    pAd->StaCfg.WpaPassPhraseLen = (UINT)strlen(arg);



    if(pAd->StaCfg.BssType == BSS_ADHOC &&
       pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone)
    {
        pAd->StaCfg.WpaState = SS_NOTUSE;
    }
    else
    {
        // Start STA supplicant state machine
        pAd->StaCfg.WpaState = SS_START;
    }

    return TRUE;
}

/*
    ==========================================================================
    Description:
        Set Power Saving mode
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_PSMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    if (pAdapter->StaCfg.BssType == BSS_INFRA)
    {
        if ((strcmp(arg, "Max_PSP") == 0) ||
			(strcmp(arg, "max_psp") == 0) ||
			(strcmp(arg, "MAX_PSP") == 0))
        {
            // do NOT turn on PSM bit here, wait until MlmeCheckPsmChange()
            // to exclude certain situations.
            if (pAdapter->StaCfg.bWindowsACCAMEnable == FALSE)
                pAdapter->StaCfg.WindowsPowerMode = Ndis802_11PowerModeMAX_PSP;
            pAdapter->StaCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeMAX_PSP;
            OPSTATUS_SET_FLAG(pAdapter, fOP_STATUS_RECEIVE_DTIM);
            pAdapter->StaCfg.DefaultListenCount = 5;

        }
        else if ((strcmp(arg, "Fast_PSP") == 0) ||
				 (strcmp(arg, "fast_psp") == 0) ||
                 (strcmp(arg, "FAST_PSP") == 0))
        {
            // do NOT turn on PSM bit here, wait until MlmeCheckPsmChange()
            // to exclude certain situations.
            OPSTATUS_SET_FLAG(pAdapter, fOP_STATUS_RECEIVE_DTIM);
            if (pAdapter->StaCfg.bWindowsACCAMEnable == FALSE)
                pAdapter->StaCfg.WindowsPowerMode = Ndis802_11PowerModeFast_PSP;
            pAdapter->StaCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeFast_PSP;
            pAdapter->StaCfg.DefaultListenCount = 3;
        }
        else if ((strcmp(arg, "Legacy_PSP") == 0) ||
                 (strcmp(arg, "legacy_psp") == 0) ||
                 (strcmp(arg, "LEGACY_PSP") == 0))
        {
            // do NOT turn on PSM bit here, wait until MlmeCheckPsmChange()
            // to exclude certain situations.
            OPSTATUS_SET_FLAG(pAdapter, fOP_STATUS_RECEIVE_DTIM);
            if (pAdapter->StaCfg.bWindowsACCAMEnable == FALSE)
                pAdapter->StaCfg.WindowsPowerMode = Ndis802_11PowerModeLegacy_PSP;
            pAdapter->StaCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeLegacy_PSP;
            pAdapter->StaCfg.DefaultListenCount = 3;
        }
        else
        {
            //Default Ndis802_11PowerModeCAM
            // clear PSM bit immediately
            RTMP_SET_PSM_BIT(pAdapter, PWR_ACTIVE);
            OPSTATUS_SET_FLAG(pAdapter, fOP_STATUS_RECEIVE_DTIM);
            if (pAdapter->StaCfg.bWindowsACCAMEnable == FALSE)
                pAdapter->StaCfg.WindowsPowerMode = Ndis802_11PowerModeCAM;
            pAdapter->StaCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeCAM;
        }

        DBGPRINT(RT_DEBUG_TRACE, ("Set_PSMode_Proc::(PSMode=%ld)\n", pAdapter->StaCfg.WindowsPowerMode));
    }
    else
        return FALSE;


    return TRUE;
}
//Add to suport the function which clould dynamicallly enable/disable PCIe Power Saving

#ifdef WPA_SUPPLICANT_SUPPORT
/*
    ==========================================================================
    Description:
        Set WpaSupport flag.
    Value:
        0: Driver ignore wpa_supplicant.
        1: wpa_supplicant initiates scanning and AP selection.
        2: driver takes care of scanning, AP selection, and IEEE 802.11 association parameters.
    Return:
        TRUE if all parameters are OK, FALSE otherwise
    ==========================================================================
*/
INT Set_Wpa_Support(
    IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{

    if ( simple_strtol(arg, 0, 10) == 0)
        pAd->StaCfg.WpaSupplicantUP = WPA_SUPPLICANT_DISABLE;
    else if ( simple_strtol(arg, 0, 10) == 1)
        pAd->StaCfg.WpaSupplicantUP = WPA_SUPPLICANT_ENABLE;
    else if ( simple_strtol(arg, 0, 10) == 2)
        pAd->StaCfg.WpaSupplicantUP = WPA_SUPPLICANT_ENABLE_WITH_WEB_UI;
    else
        pAd->StaCfg.WpaSupplicantUP = WPA_SUPPLICANT_DISABLE;

    DBGPRINT(RT_DEBUG_TRACE, ("Set_Wpa_Support::(WpaSupplicantUP=%d)\n", pAd->StaCfg.WpaSupplicantUP));

    return TRUE;
}
#endif // WPA_SUPPLICANT_SUPPORT //

#ifdef DBG
/*
    ==========================================================================
    Description:
        Read / Write MAC
    Arguments:
        pAdapter                    Pointer to our adapter
        wrq                         Pointer to the ioctl argument

    Return Value:
        None

    Note:
        Usage:
               1.) iwpriv ra0 mac 0        ==> read MAC where Addr=0x0
               2.) iwpriv ra0 mac 0=12     ==> write MAC where Addr=0x0, value=12
    ==========================================================================
*/
VOID RTMPIoctlMAC(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq)
{
	PSTRING				this_char;
	PSTRING				value;
	INT					j = 0, k = 0;
	STRING				msg[1024];
	STRING				arg[255];
	ULONG				macAddr = 0;
	UCHAR				temp[16];
	STRING				temp2[16];
	UINT32				macValue = 0;
	INT					Status;
	BOOLEAN				bIsPrintAllMAC = FALSE;


	memset(msg, 0x00, 1024);
	if (wrq->u.data.length > 1) //No parameters.
	{
	    Status = copy_from_user(arg, wrq->u.data.pointer, (wrq->u.data.length > 255) ? 255 : wrq->u.data.length);
		sprintf(msg, "\n");

		//Parsing Read or Write
	    this_char = arg;
		if (!*this_char)
			goto next;

		if ((value = rtstrchr(this_char, '=')) != NULL)
			*value++ = 0;

		if (!value || !*value)
		{ //Read
			// Sanity check
			if(strlen(this_char) > 4)
				goto next;

			j = strlen(this_char);
			while(j-- > 0)
			{
				if(this_char[j] > 'f' || this_char[j] < '0')
					return;
			}

			// Mac Addr
			k = j = strlen(this_char);
			while(j-- > 0)
			{
				this_char[4-k+j] = this_char[j];
			}

			while(k < 4)
				this_char[3-k++]='0';
			this_char[4]='\0';

			if(strlen(this_char) == 4)
			{
				AtoH(this_char, temp, 2);
				macAddr = *temp*256 + temp[1];
				if (macAddr < 0xFFFF)
				{
					RTMP_IO_READ32(pAdapter, macAddr, &macValue);
					DBGPRINT(RT_DEBUG_TRACE, ("MacAddr=%lx, MacValue=%x\n", macAddr, macValue));
					sprintf(msg+strlen(msg), "[0x%08lX]:%08X  ", macAddr , macValue);
				}
				else
				{//Invalid parametes, so default printk all mac
					bIsPrintAllMAC = TRUE;
					goto next;
				}
			}
		}
		else
		{ //Write
			memcpy(&temp2, value, strlen(value));
			temp2[strlen(value)] = '\0';

			// Sanity check
			if((strlen(this_char) > 4) || strlen(temp2) > 8)
				goto next;

			j = strlen(this_char);
			while(j-- > 0)
			{
				if(this_char[j] > 'f' || this_char[j] < '0')
					return;
			}

			j = strlen(temp2);
			while(j-- > 0)
			{
				if(temp2[j] > 'f' || temp2[j] < '0')
					return;
			}

			//MAC Addr
			k = j = strlen(this_char);
			while(j-- > 0)
			{
				this_char[4-k+j] = this_char[j];
			}

			while(k < 4)
				this_char[3-k++]='0';
			this_char[4]='\0';

			//MAC value
			k = j = strlen(temp2);
			while(j-- > 0)
			{
				temp2[8-k+j] = temp2[j];
			}

			while(k < 8)
				temp2[7-k++]='0';
			temp2[8]='\0';

			{
				AtoH(this_char, temp, 2);
				macAddr = *temp*256 + temp[1];

				AtoH(temp2, temp, 4);
				macValue = *temp*256*256*256 + temp[1]*256*256 + temp[2]*256 + temp[3];

				// debug mode
				if (macAddr == (HW_DEBUG_SETTING_BASE + 4))
				{
					// 0x2bf4: byte0 non-zero: enable R17 tuning, 0: disable R17 tuning
                    if (macValue & 0x000000ff)
                    {
                        pAdapter->BbpTuning.bEnable = TRUE;
                        DBGPRINT(RT_DEBUG_TRACE,("turn on R17 tuning\n"));
                    }
                    else
                    {
                        UCHAR R66;
                        pAdapter->BbpTuning.bEnable = FALSE;
                        R66 = 0x26 + GET_LNA_GAIN(pAdapter);
#ifdef RALINK_ATE
						if (ATE_ON(pAdapter))
						{
							ATE_BBP_IO_WRITE8_BY_REG_ID(pAdapter, BBP_R66, (0x26 + GET_LNA_GAIN(pAdapter)));
						}
						else
#endif // RALINK_ATE //

							RTMP_BBP_IO_WRITE8_BY_REG_ID(pAdapter, BBP_R66, (0x26 + GET_LNA_GAIN(pAdapter)));
                        DBGPRINT(RT_DEBUG_TRACE,("turn off R17 tuning, restore to 0x%02x\n", R66));
                    }
					return;
				}

				DBGPRINT(RT_DEBUG_TRACE, ("MacAddr=%02lx, MacValue=0x%x\n", macAddr, macValue));

				RTMP_IO_WRITE32(pAdapter, macAddr, macValue);
				sprintf(msg+strlen(msg), "[0x%08lX]:%08X  ", macAddr, macValue);
			}
		}
	}
	else
		bIsPrintAllMAC = TRUE;
next:
	if (bIsPrintAllMAC)
	{
		struct file		*file_w;
		PSTRING			fileName = "MacDump.txt";
		mm_segment_t	orig_fs;

		orig_fs = get_fs();
		set_fs(KERNEL_DS);

		// open file
		file_w = filp_open(fileName, O_WRONLY|O_CREAT, 0);
		if (IS_ERR(file_w))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("-->2) %s: Error %ld opening %s\n", __FUNCTION__, -PTR_ERR(file_w), fileName));
		}
		else
		{
			if (file_w->f_op && file_w->f_op->write)
			{
				file_w->f_pos = 0;
				macAddr = 0x1000;

				while (macAddr <= 0x1800)
				{
					RTMP_IO_READ32(pAdapter, macAddr, &macValue);
					sprintf(msg, "%08lx = %08X\n", macAddr, macValue);

					// write data to file
					file_w->f_op->write(file_w, msg, strlen(msg), &file_w->f_pos);

					printk("%s", msg);
					macAddr += 4;
				}
				sprintf(msg, "\nDump all MAC values to %s\n", fileName);
			}
			filp_close(file_w, NULL);
		}
		set_fs(orig_fs);
	}
	if(strlen(msg) == 1)
		sprintf(msg+strlen(msg), "===>Error command format!");

	// Copy the information into the user buffer
	wrq->u.data.length = strlen(msg);
	Status = copy_to_user(wrq->u.data.pointer, msg, wrq->u.data.length);

	DBGPRINT(RT_DEBUG_TRACE, ("<==RTMPIoctlMAC\n\n"));
}

/*
    ==========================================================================
    Description:
        Read / Write E2PROM
    Arguments:
        pAdapter                    Pointer to our adapter
        wrq                         Pointer to the ioctl argument

    Return Value:
        None

    Note:
        Usage:
               1.) iwpriv ra0 e2p 0		==> read E2PROM where Addr=0x0
               2.) iwpriv ra0 e2p 0=1234    ==> write E2PROM where Addr=0x0, value=1234
    ==========================================================================
*/
VOID RTMPIoctlE2PROM(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq)
{
	PSTRING				this_char;
	PSTRING				value;
	INT					j = 0, k = 0;
	STRING				msg[1024];
	STRING				arg[255];
	USHORT				eepAddr = 0;
	UCHAR				temp[16];
	STRING				temp2[16];
	USHORT				eepValue;
	int					Status;
	BOOLEAN				bIsPrintAllE2P = FALSE;


	memset(msg, 0x00, 1024);
	if (wrq->u.data.length > 1) //No parameters.
	{
	    Status = copy_from_user(arg, wrq->u.data.pointer, (wrq->u.data.length > 255) ? 255 : wrq->u.data.length);
		sprintf(msg, "\n");

	    //Parsing Read or Write
		this_char = arg;


		if (!*this_char)
			goto next;

		if ((value = rtstrchr(this_char, '=')) != NULL)
			*value++ = 0;

		if (!value || !*value)
		{ //Read

			// Sanity check
			if(strlen(this_char) > 4)
				goto next;

			j = strlen(this_char);
			while(j-- > 0)
			{
				if(this_char[j] > 'f' || this_char[j] < '0')
					return;
			}

			// E2PROM addr
			k = j = strlen(this_char);
			while(j-- > 0)
			{
				this_char[4-k+j] = this_char[j];
			}

			while(k < 4)
				this_char[3-k++]='0';
			this_char[4]='\0';

			if(strlen(this_char) == 4)
			{
				AtoH(this_char, temp, 2);
				eepAddr = *temp*256 + temp[1];
				if (eepAddr < 0xFFFF)
				{
					RT28xx_EEPROM_READ16(pAdapter, eepAddr, eepValue);
					sprintf(msg+strlen(msg), "[0x%04X]:0x%04X  ", eepAddr , eepValue);
				}
				else
				{//Invalid parametes, so default printk all bbp
					bIsPrintAllE2P = TRUE;
					goto next;
				}
			}
		}
		else
		{ //Write
			memcpy(&temp2, value, strlen(value));
			temp2[strlen(value)] = '\0';

			// Sanity check
			if((strlen(this_char) > 4) || strlen(temp2) > 8)
				goto next;

			j = strlen(this_char);
			while(j-- > 0)
			{
				if(this_char[j] > 'f' || this_char[j] < '0')
					return;
			}
			j = strlen(temp2);
			while(j-- > 0)
			{
				if(temp2[j] > 'f' || temp2[j] < '0')
					return;
			}

			//MAC Addr
			k = j = strlen(this_char);
			while(j-- > 0)
			{
				this_char[4-k+j] = this_char[j];
			}

			while(k < 4)
				this_char[3-k++]='0';
			this_char[4]='\0';

			//MAC value
			k = j = strlen(temp2);
			while(j-- > 0)
			{
				temp2[4-k+j] = temp2[j];
			}

			while(k < 4)
				temp2[3-k++]='0';
			temp2[4]='\0';

			AtoH(this_char, temp, 2);
			eepAddr = *temp*256 + temp[1];

			AtoH(temp2, temp, 2);
			eepValue = *temp*256 + temp[1];

			RT28xx_EEPROM_WRITE16(pAdapter, eepAddr, eepValue);
			sprintf(msg+strlen(msg), "[0x%02X]:%02X  ", eepAddr, eepValue);
		}
	}
	else
		bIsPrintAllE2P = TRUE;
next:
	if (bIsPrintAllE2P)
	{
		struct file		*file_w;
		PSTRING			fileName = "EEPROMDump.txt";
		mm_segment_t	orig_fs;

		orig_fs = get_fs();
		set_fs(KERNEL_DS);

		// open file
		file_w = filp_open(fileName, O_WRONLY|O_CREAT, 0);
		if (IS_ERR(file_w))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("-->2) %s: Error %ld opening %s\n", __FUNCTION__, -PTR_ERR(file_w), fileName));
		}
		else
		{
			if (file_w->f_op && file_w->f_op->write)
			{
				file_w->f_pos = 0;
				eepAddr = 0x00;

				while (eepAddr <= 0xFE)
				{
					RT28xx_EEPROM_READ16(pAdapter, eepAddr, eepValue);
					sprintf(msg, "%08x = %04x\n", eepAddr , eepValue);

					// write data to file
					file_w->f_op->write(file_w, msg, strlen(msg), &file_w->f_pos);

					printk("%s", msg);
					eepAddr += 2;
				}
				sprintf(msg, "\nDump all EEPROM values to %s\n", fileName);
			}
			filp_close(file_w, NULL);
		}
		set_fs(orig_fs);
	}
	if(strlen(msg) == 1)
		sprintf(msg+strlen(msg), "===>Error command format!");


	// Copy the information into the user buffer
	wrq->u.data.length = strlen(msg);
	Status = copy_to_user(wrq->u.data.pointer, msg, wrq->u.data.length);

	DBGPRINT(RT_DEBUG_TRACE, ("<==RTMPIoctlE2PROM\n"));
}


#ifdef RT30xx
/*
    ==========================================================================
    Description:
        Read / Write RF register
Arguments:
    pAdapter                    Pointer to our adapter
    wrq                         Pointer to the ioctl argument

    Return Value:
        None

    Note:
        Usage:
               1.) iwpriv ra0 rf                ==> read all RF registers
               2.) iwpriv ra0 rf 1              ==> read RF where RegID=1
               3.) iwpriv ra0 rf 1=10		    ==> write RF R1=0x10
    ==========================================================================
*/
VOID RTMPIoctlRF(
	IN	PRTMP_ADAPTER	pAdapter,
	IN	struct iwreq	*wrq)
{
	CHAR				*this_char;
	CHAR				*value;
	UCHAR				regRF = 0;
	STRING				msg[2048];
	CHAR				arg[255];
	INT					rfId;
	LONG				rfValue;
	int					Status;
	BOOLEAN				bIsPrintAllRF = FALSE;


	memset(msg, 0x00, 2048);
	if (wrq->u.data.length > 1) //No parameters.
	{
	    Status = copy_from_user(arg, wrq->u.data.pointer, (wrq->u.data.length > 255) ? 255 : wrq->u.data.length);
		sprintf(msg, "\n");

	    //Parsing Read or Write
		this_char = arg;
		if (!*this_char)
			goto next;

		if ((value = strchr(this_char, '=')) != NULL)
			*value++ = 0;

		if (!value || !*value)
		{ //Read
			if (sscanf((PSTRING) this_char, "%d", &(rfId)) == 1)
			{
				if (rfId <= 31)
				{
#ifdef RALINK_ATE
					/*
						In RT2860 ATE mode, we do not load 8051 firmware.
	                    We must access RF directly.
						For RT2870 ATE mode, ATE_RF_IO_WRITE8(/READ8)_BY_REG_ID are redefined.
					*/
					if (ATE_ON(pAdapter))
					{
						ATE_RF_IO_READ8_BY_REG_ID(pAdapter, rfId, &regRF);
					}
					else
#endif // RALINK_ATE //
					// according to Andy, Gary, David require.
					// the command rf shall read rf register directly for dubug.
					// BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
					RT30xxReadRFRegister(pAdapter, rfId, &regRF);

					sprintf(msg+strlen(msg), "R%02d[0x%02x]:%02X  ", rfId, rfId, regRF);
				}
				else
				{//Invalid parametes, so default printk all RF
					bIsPrintAllRF = TRUE;
					goto next;
				}
			}
			else
			{ //Invalid parametes, so default printk all RF
				bIsPrintAllRF = TRUE;
				goto next;
			}
		}
		else
		{ //Write
			if ((sscanf((PSTRING) this_char, "%d", &(rfId)) == 1) && (sscanf((PSTRING) value, "%lx", &(rfValue)) == 1))
			{
				if (rfId <= 31)
				{
#ifdef RALINK_ATE
						/*
							In RT2860 ATE mode, we do not load 8051 firmware.
		                    We must access RF directly.
							For RT2870 ATE mode, ATE_RF_IO_WRITE8(/READ8)_BY_REG_ID are redefined.
						*/
						if (ATE_ON(pAdapter))
						{
							ATE_RF_IO_READ8_BY_REG_ID(pAdapter, rfId, &regRF);
							ATE_RF_IO_WRITE8_BY_REG_ID(pAdapter, (UCHAR)rfId,(UCHAR) rfValue);
							//Read it back for showing
							ATE_RF_IO_READ8_BY_REG_ID(pAdapter, rfId, &regRF);
							sprintf(msg+strlen(msg), "R%02d[0x%02X]:%02X\n", rfId, rfId, regRF);
						}
						else
#endif // RALINK_ATE //
						{
							// according to Andy, Gary, David require.
							// the command RF shall read/write RF register directly for dubug.
							//BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
							//BBP_IO_WRITE8_BY_REG_ID(pAdapter, (UCHAR)bbpId,(UCHAR) bbpValue);
							RT30xxReadRFRegister(pAdapter, rfId, &regRF);
							RT30xxWriteRFRegister(pAdapter, (UCHAR)rfId,(UCHAR) rfValue);
							//Read it back for showing
							//BBP_IO_READ8_BY_REG_ID(pAdapter, bbpId, &regBBP);
							RT30xxReadRFRegister(pAdapter, rfId, &regRF);
			                sprintf(msg+strlen(msg), "R%02d[0x%02X]:%02X\n", rfId, rfId, regRF);
				                }
				}
				else
				{//Invalid parametes, so default printk all RF
					bIsPrintAllRF = TRUE;
				}
			}
			else
			{ //Invalid parametes, so default printk all RF
				bIsPrintAllRF = TRUE;
			}
		}
	}
	else
		bIsPrintAllRF = TRUE;
next:
	if (bIsPrintAllRF)
	{
		memset(msg, 0x00, 2048);
		sprintf(msg, "\n");
		for (rfId = 0; rfId <= 31; rfId++)
		{
#ifdef RALINK_ATE
			/*
				In RT2860 ATE mode, we do not load 8051 firmware.
                We must access RF directly.
				For RT2870 ATE mode, ATE_RF_IO_WRITE8(/READ8)_BY_REG_ID are redefined.
			*/
			if (ATE_ON(pAdapter))
			{
				ATE_RF_IO_READ8_BY_REG_ID(pAdapter, rfId, &regRF);
			}
			else
#endif // RALINK_ATE //

			// according to Andy, Gary, David require.
			// the command RF shall read/write RF register directly for dubug.
			RT30xxReadRFRegister(pAdapter, rfId, &regRF);
			sprintf(msg+strlen(msg), "%03d = %02X\n", rfId, regRF);
		}
		// Copy the information into the user buffer
		DBGPRINT(RT_DEBUG_TRACE, ("strlen(msg)=%d\n", (UINT32)strlen(msg)));
		wrq->u.data.length = strlen(msg);
		if (copy_to_user(wrq->u.data.pointer, msg, wrq->u.data.length))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("%s: copy_to_user() fail\n", __FUNCTION__));
		}
	}
	else
	{
		if(strlen(msg) == 1)
			sprintf(msg+strlen(msg), "===>Error command format!");

		DBGPRINT(RT_DEBUG_TRACE, ("copy to user [msg=%s]\n", msg));
		// Copy the information into the user buffer
		DBGPRINT(RT_DEBUG_TRACE, ("strlen(msg) =%d\n", (UINT32)strlen(msg)));

		// Copy the information into the user buffer
		wrq->u.data.length = strlen(msg);
		Status = copy_to_user(wrq->u.data.pointer, msg, wrq->u.data.length);
	}

	DBGPRINT(RT_DEBUG_TRACE, ("<==RTMPIoctlRF\n\n"));
}
#endif // RT30xx //
#endif // DBG //




INT Set_TGnWifiTest_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING          arg)
{
    if (simple_strtol(arg, 0, 10) == 0)
        pAd->StaCfg.bTGnWifiTest = FALSE;
    else
        pAd->StaCfg.bTGnWifiTest = TRUE;

    DBGPRINT(RT_DEBUG_TRACE, ("IF Set_TGnWifiTest_Proc::(bTGnWifiTest=%d)\n", pAd->StaCfg.bTGnWifiTest));
	return TRUE;
}

#ifdef EXT_BUILD_CHANNEL_LIST
INT Set_Ieee80211dClientMode_Proc(
    IN  PRTMP_ADAPTER   pAdapter,
    IN  PSTRING          arg)
{
    if (simple_strtol(arg, 0, 10) == 0)
        pAdapter->StaCfg.IEEE80211dClientMode = Rt802_11_D_None;
    else if (simple_strtol(arg, 0, 10) == 1)
        pAdapter->StaCfg.IEEE80211dClientMode = Rt802_11_D_Flexible;
    else if (simple_strtol(arg, 0, 10) == 2)
        pAdapter->StaCfg.IEEE80211dClientMode = Rt802_11_D_Strict;
    else
        return FALSE;

    DBGPRINT(RT_DEBUG_TRACE, ("Set_Ieee802dMode_Proc::(IEEEE0211dMode=%d)\n", pAdapter->StaCfg.IEEE80211dClientMode));
    return TRUE;
}
#endif // EXT_BUILD_CHANNEL_LIST //

#ifdef CARRIER_DETECTION_SUPPORT
INT Set_CarrierDetect_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg)
{
    if (simple_strtol(arg, 0, 10) == 0)
        pAd->CommonCfg.CarrierDetect.Enable = FALSE;
    else
        pAd->CommonCfg.CarrierDetect.Enable = TRUE;

    DBGPRINT(RT_DEBUG_TRACE, ("IF Set_CarrierDetect_Proc::(CarrierDetect.Enable=%d)\n", pAd->CommonCfg.CarrierDetect.Enable));
	return TRUE;
}
#endif // CARRIER_DETECTION_SUPPORT //


INT	Show_Adhoc_MacTable_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			extra)
{
	INT i;

	sprintf(extra, "\n");

#ifdef DOT11_N_SUPPORT
	sprintf(extra, "%sHT Operating Mode : %d\n", extra, pAd->CommonCfg.AddHTInfo.AddHtInfo2.OperaionMode);
#endif // DOT11_N_SUPPORT //

	sprintf(extra, "%s\n%-19s%-4s%-4s%-7s%-7s%-7s%-10s%-6s%-6s%-6s%-6s\n", extra,
			"MAC", "AID", "BSS", "RSSI0", "RSSI1", "RSSI2", "PhMd", "BW", "MCS", "SGI", "STBC");

	for (i=1; i<MAX_LEN_OF_MAC_TABLE; i++)
	{
		PMAC_TABLE_ENTRY pEntry = &pAd->MacTab.Content[i];

		if (strlen(extra) > (IW_PRIV_SIZE_MASK - 30))
		    break;
		if ((pEntry->ValidAsCLI || pEntry->ValidAsApCli) && (pEntry->Sst == SST_ASSOC))
		{
			sprintf(extra, "%s%02X:%02X:%02X:%02X:%02X:%02X  ", extra,
				pEntry->Addr[0], pEntry->Addr[1], pEntry->Addr[2],
				pEntry->Addr[3], pEntry->Addr[4], pEntry->Addr[5]);
			sprintf(extra, "%s%-4d", extra, (int)pEntry->Aid);
			sprintf(extra, "%s%-4d", extra, (int)pEntry->apidx);
			sprintf(extra, "%s%-7d", extra, pEntry->RssiSample.AvgRssi0);
			sprintf(extra, "%s%-7d", extra, pEntry->RssiSample.AvgRssi1);
			sprintf(extra, "%s%-7d", extra, pEntry->RssiSample.AvgRssi2);
			sprintf(extra, "%s%-10s", extra, GetPhyMode(pEntry->HTPhyMode.field.MODE));
			sprintf(extra, "%s%-6s", extra, GetBW(pEntry->HTPhyMode.field.BW));
			sprintf(extra, "%s%-6d", extra, pEntry->HTPhyMode.field.MCS);
			sprintf(extra, "%s%-6d", extra, pEntry->HTPhyMode.field.ShortGI);
			sprintf(extra, "%s%-6d", extra, pEntry->HTPhyMode.field.STBC);
			sprintf(extra, "%s%-10d, %d, %d%%\n", extra, pEntry->DebugFIFOCount, pEntry->DebugTxCount,
						(pEntry->DebugTxCount) ? ((pEntry->DebugTxCount-pEntry->DebugFIFOCount)*100/pEntry->DebugTxCount) : 0);
			sprintf(extra, "%s\n", extra);
		}
	}

	return TRUE;
}


INT Set_BeaconLostTime_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg)
{
	ULONG ltmp = (ULONG)simple_strtol(arg, 0, 10);

	if ((ltmp != 0) && (ltmp <= 60))
		pAd->StaCfg.BeaconLostTime = (ltmp * OS_HZ);

    DBGPRINT(RT_DEBUG_TRACE, ("IF Set_BeaconLostTime_Proc::(BeaconLostTime=%ld)\n", pAd->StaCfg.BeaconLostTime));
	return TRUE;
}

INT Set_AutoRoaming_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg)
{
    if (simple_strtol(arg, 0, 10) == 0)
        pAd->StaCfg.bAutoRoaming = FALSE;
    else
        pAd->StaCfg.bAutoRoaming = TRUE;

    DBGPRINT(RT_DEBUG_TRACE, ("IF Set_AutoRoaming_Proc::(bAutoRoaming=%d)\n", pAd->StaCfg.bAutoRoaming));
	return TRUE;
}


/*
    ==========================================================================
    Description:
        Issue a site survey command to driver
	Arguments:
	    pAdapter                    Pointer to our adapter
	    wrq                         Pointer to the ioctl argument

    Return Value:
        None

    Note:
        Usage:
               1.) iwpriv ra0 set site_survey
    ==========================================================================
*/
INT Set_SiteSurvey_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PSTRING			arg)
{
	NDIS_802_11_SSID Ssid;

	//check if the interface is down
	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_INTERRUPT_IN_USE))
	{
		DBGPRINT(RT_DEBUG_TRACE, ("INFO::Network is down!\n"));
		return -ENETDOWN;
	}

	if (MONITOR_ON(pAd))
    {
        DBGPRINT(RT_DEBUG_TRACE, ("!!! Driver is in Monitor Mode now !!!\n"));
        return -EINVAL;
    }

	RTMPZeroMemory(&Ssid, sizeof(NDIS_802_11_SSID));
	Ssid.SsidLength = 0;
	if ((arg != NULL) &&
		(strlen(arg) <= MAX_LEN_OF_SSID))
    {
        RTMPMoveMemory(Ssid.Ssid, arg, strlen(arg));
        Ssid.SsidLength = strlen(arg);
	}

	pAd->StaCfg.bScanReqIsFromWebUI = TRUE;

	if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)
	{
		RTMP_MLME_RESET_STATE_MACHINE(pAd);
		DBGPRINT(RT_DEBUG_TRACE, ("!!! MLME busy, reset MLME state machine !!!\n"));
	}

	// tell CNTL state machine to call NdisMSetInformationComplete() after completing
	// this request, because this request is initiated by NDIS.
	pAd->MlmeAux.CurrReqIsFromNdis = FALSE;
	// Reset allowed scan retries
	pAd->StaCfg.ScanCnt = 0;
	NdisGetSystemUpTime(&pAd->StaCfg.LastScanTime);

	MlmeEnqueue(pAd,
		MLME_CNTL_STATE_MACHINE,
		OID_802_11_BSSID_LIST_SCAN,
		Ssid.SsidLength,
		Ssid.Ssid);

	RTMP_MLME_HANDLER(pAd);

    DBGPRINT(RT_DEBUG_TRACE, ("Set_SiteSurvey_Proc\n"));

    return TRUE;
}

INT Set_ForceTxBurst_Proc(
    IN  PRTMP_ADAPTER   pAd,
    IN  PSTRING         arg)
{
    if (simple_strtol(arg, 0, 10) == 0)
        pAd->StaCfg.bForceTxBurst = FALSE;
    else
        pAd->StaCfg.bForceTxBurst = TRUE;

    DBGPRINT(RT_DEBUG_TRACE, ("IF Set_ForceTxBurst_Proc::(bForceTxBurst=%d)\n", pAd->StaCfg.bForceTxBurst));
	return TRUE;
}

#ifdef ANT_DIVERSITY_SUPPORT
INT	Set_Antenna_Proc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PUCHAR			arg)
{
    UCHAR UsedAnt;
	DBGPRINT(RT_DEBUG_TRACE, ("==> Set_Antenna_Proc *******************\n"));

    if(simple_strtol(arg, 0, 10) <= 3)
        UsedAnt = simple_strtol(arg, 0, 10);

    pAd->CommonCfg.bRxAntDiversity = UsedAnt; // Auto switch
    if (UsedAnt == ANT_DIVERSITY_ENABLE)
    {
            pAd->RxAnt.EvaluateStableCnt = 0;
            DBGPRINT(RT_DEBUG_TRACE, ("<== Set_Antenna_Proc(Auto Switch Mode), (%d,%d)\n", pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair1SecondaryRxAnt));
    }
    /* 2: Fix in the PHY Antenna CON1*/
    if (UsedAnt == ANT_FIX_ANT1)
    {
            AsicSetRxAnt(pAd, 0);
            DBGPRINT(RT_DEBUG_TRACE, ("<== Set_Antenna_Proc(Fix in Ant CON1), (%d,%d)\n", pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair1SecondaryRxAnt));
    }
    /* 3: Fix in the PHY Antenna CON2*/
    if (UsedAnt == ANT_FIX_ANT2)
    {
            AsicSetRxAnt(pAd, 1);
            DBGPRINT(RT_DEBUG_TRACE, ("<== Set_Antenna_Proc(Fix in Ant CON2), (%d,%d)\n", pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair1SecondaryRxAnt));
    }

	return TRUE;
}
#endif // ANT_DIVERSITY_SUPPORT //