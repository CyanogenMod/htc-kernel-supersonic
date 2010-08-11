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
	connect.c

	Abstract:

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
	John			2004-08-08			Major modification from RT2560
*/

#include "../rt_config.h"


UCHAR	CipherSuiteWpaNoneTkip[] = {
		0x00, 0x50, 0xf2, 0x01,	// oui
		0x01, 0x00,				// Version
		0x00, 0x50, 0xf2, 0x02,	// Multicast
		0x01, 0x00,				// Number of unicast
		0x00, 0x50, 0xf2, 0x02,	// unicast
		0x01, 0x00,				// number of authentication method
		0x00, 0x50, 0xf2, 0x00	// authentication
		};
UCHAR	CipherSuiteWpaNoneTkipLen = (sizeof(CipherSuiteWpaNoneTkip) / sizeof(UCHAR));

UCHAR	CipherSuiteWpaNoneAes[] = {
		0x00, 0x50, 0xf2, 0x01,	// oui
		0x01, 0x00,				// Version
		0x00, 0x50, 0xf2, 0x04,	// Multicast
		0x01, 0x00,				// Number of unicast
		0x00, 0x50, 0xf2, 0x04,	// unicast
		0x01, 0x00,				// number of authentication method
		0x00, 0x50, 0xf2, 0x00	// authentication
		};
UCHAR	CipherSuiteWpaNoneAesLen = (sizeof(CipherSuiteWpaNoneAes) / sizeof(UCHAR));

// The following MACRO is called after 1. starting an new IBSS, 2. succesfully JOIN an IBSS,
// or 3. succesfully ASSOCIATE to a BSS, 4. successfully RE_ASSOCIATE to a BSS
// All settings successfuly negotiated furing MLME state machines become final settings
// and are copied to pAd->StaActive
#define COPY_SETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(_pAd)                                 \
{                                                                                       \
	NdisZeroMemory((_pAd)->CommonCfg.Ssid, MAX_LEN_OF_SSID);							\
	(_pAd)->CommonCfg.SsidLen = (_pAd)->MlmeAux.SsidLen;                                \
	NdisMoveMemory((_pAd)->CommonCfg.Ssid, (_pAd)->MlmeAux.Ssid, (_pAd)->MlmeAux.SsidLen); \
	COPY_MAC_ADDR((_pAd)->CommonCfg.Bssid, (_pAd)->MlmeAux.Bssid);                      \
	(_pAd)->CommonCfg.Channel = (_pAd)->MlmeAux.Channel;                                \
	(_pAd)->CommonCfg.CentralChannel = (_pAd)->MlmeAux.CentralChannel;                  \
	(_pAd)->StaActive.Aid = (_pAd)->MlmeAux.Aid;                                        \
	(_pAd)->StaActive.AtimWin = (_pAd)->MlmeAux.AtimWin;                                \
	(_pAd)->StaActive.CapabilityInfo = (_pAd)->MlmeAux.CapabilityInfo;                  \
	(_pAd)->CommonCfg.BeaconPeriod = (_pAd)->MlmeAux.BeaconPeriod;                      \
	(_pAd)->StaActive.CfpMaxDuration = (_pAd)->MlmeAux.CfpMaxDuration;                  \
	(_pAd)->StaActive.CfpPeriod = (_pAd)->MlmeAux.CfpPeriod;                            \
	(_pAd)->StaActive.SupRateLen = (_pAd)->MlmeAux.SupRateLen;                          \
	NdisMoveMemory((_pAd)->StaActive.SupRate, (_pAd)->MlmeAux.SupRate, (_pAd)->MlmeAux.SupRateLen);\
	(_pAd)->StaActive.ExtRateLen = (_pAd)->MlmeAux.ExtRateLen;                          \
	NdisMoveMemory((_pAd)->StaActive.ExtRate, (_pAd)->MlmeAux.ExtRate, (_pAd)->MlmeAux.ExtRateLen);\
	NdisMoveMemory(&(_pAd)->CommonCfg.APEdcaParm, &(_pAd)->MlmeAux.APEdcaParm, sizeof(EDCA_PARM));\
	NdisMoveMemory(&(_pAd)->CommonCfg.APQosCapability, &(_pAd)->MlmeAux.APQosCapability, sizeof(QOS_CAPABILITY_PARM));\
	NdisMoveMemory(&(_pAd)->CommonCfg.APQbssLoad, &(_pAd)->MlmeAux.APQbssLoad, sizeof(QBSS_LOAD_PARM));\
	COPY_MAC_ADDR((_pAd)->MacTab.Content[BSSID_WCID].Addr, (_pAd)->MlmeAux.Bssid);      \
	(_pAd)->MacTab.Content[BSSID_WCID].Aid = (_pAd)->MlmeAux.Aid;                       \
	(_pAd)->MacTab.Content[BSSID_WCID].PairwiseKey.CipherAlg = (_pAd)->StaCfg.PairCipher;\
	COPY_MAC_ADDR((_pAd)->MacTab.Content[BSSID_WCID].PairwiseKey.BssId, (_pAd)->MlmeAux.Bssid);\
	(_pAd)->MacTab.Content[BSSID_WCID].RateLen = (_pAd)->StaActive.SupRateLen + (_pAd)->StaActive.ExtRateLen;\
}

/*
	==========================================================================
	Description:

	IRQL = PASSIVE_LEVEL

	==========================================================================
*/
VOID MlmeCntlInit(
	IN PRTMP_ADAPTER pAd,
	IN STATE_MACHINE *S,
	OUT STATE_MACHINE_FUNC Trans[])
{
	// Control state machine differs from other state machines, the interface
	// follows the standard interface
	pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID MlmeCntlMachinePerformAction(
	IN PRTMP_ADAPTER pAd,
	IN STATE_MACHINE *S,
	IN MLME_QUEUE_ELEM *Elem)
{
	switch(pAd->Mlme.CntlMachine.CurrState)
	{
		case CNTL_IDLE:
			CntlIdleProc(pAd, Elem);
			break;
		case CNTL_WAIT_DISASSOC:
			CntlWaitDisassocProc(pAd, Elem);
			break;
		case CNTL_WAIT_JOIN:
			CntlWaitJoinProc(pAd, Elem);
			break;

		// CNTL_WAIT_REASSOC is the only state in CNTL machine that does
		// not triggered directly or indirectly by "RTMPSetInformation(OID_xxx)".
		// Therefore not protected by NDIS's "only one outstanding OID request"
		// rule. Which means NDIS may SET OID in the middle of ROAMing attempts.
		// Current approach is to block new SET request at RTMPSetInformation()
		// when CntlMachine.CurrState is not CNTL_IDLE
		case CNTL_WAIT_REASSOC:
			CntlWaitReassocProc(pAd, Elem);
			break;

		case CNTL_WAIT_START:
			CntlWaitStartProc(pAd, Elem);
			break;
		case CNTL_WAIT_AUTH:
			CntlWaitAuthProc(pAd, Elem);
			break;
		case CNTL_WAIT_AUTH2:
			CntlWaitAuthProc2(pAd, Elem);
			break;
		case CNTL_WAIT_ASSOC:
			CntlWaitAssocProc(pAd, Elem);
			break;

		case CNTL_WAIT_OID_LIST_SCAN:
			if(Elem->MsgType == MT2_SCAN_CONF)
			{
				// Resume TxRing after SCANING complete. We hope the out-of-service time
				// won't be too long to let upper layer time-out the waiting frames
				RTMPResumeMsduTransmission(pAd);

				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;

                //
				// Set LED status to previous status.
				//
				if (pAd->bLedOnScanning)
				{
					pAd->bLedOnScanning = FALSE;
					RTMPSetLED(pAd, pAd->LedStatus);
				}
#ifdef DOT11N_DRAFT3
				// AP sent a 2040Coexistence mgmt frame, then station perform a scan, and then send back the respone.
				if (pAd->CommonCfg.BSSCoexist2040.field.InfoReq == 1)
				{
					Update2040CoexistFrameAndNotify(pAd, BSSID_WCID, TRUE);
				}
#endif // DOT11N_DRAFT3 //
			}
			break;

		case CNTL_WAIT_OID_DISASSOC:
			if (Elem->MsgType == MT2_DISASSOC_CONF)
			{
				LinkDown(pAd, FALSE);
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			}
			break;
		default:
			DBGPRINT_ERR(("!ERROR! CNTL - Illegal message type(=%ld)", Elem->MsgType));
			break;
	}
}


/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlIdleProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	MLME_DISASSOC_REQ_STRUCT   DisassocReq;

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))
		return;

	switch(Elem->MsgType)
	{
		case OID_802_11_SSID:
			CntlOidSsidProc(pAd, Elem);
			break;

		case OID_802_11_BSSID:
			CntlOidRTBssidProc(pAd,Elem);
			break;

		case OID_802_11_BSSID_LIST_SCAN:
			CntlOidScanProc(pAd,Elem);
			break;

		case OID_802_11_DISASSOCIATE:
			DisassocParmFill(pAd, &DisassocReq, pAd->CommonCfg.Bssid, REASON_DISASSOC_STA_LEAVING);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ, sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_DISASSOC;
#ifdef WPA_SUPPLICANT_SUPPORT
            if (pAd->StaCfg.WpaSupplicantUP != WPA_SUPPLICANT_ENABLE_WITH_WEB_UI)
#endif // WPA_SUPPLICANT_SUPPORT //
            {
			// Set the AutoReconnectSsid to prevent it reconnect to old SSID
			// Since calling this indicate user don't want to connect to that SSID anymore.
			pAd->MlmeAux.AutoReconnectSsidLen= 32;
			NdisZeroMemory(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen);
            }
			break;

		case MT2_MLME_ROAMING_REQ:
			CntlMlmeRoamingProc(pAd, Elem);
			break;

        case OID_802_11_MIC_FAILURE_REPORT_FRAME:
            WpaMicFailureReportFrame(pAd, Elem);
            break;

#ifdef QOS_DLS_SUPPORT
		case RT_OID_802_11_SET_DLS_PARAM:
			CntlOidDLSSetupProc(pAd, Elem);
			break;
#endif // QOS_DLS_SUPPORT //

		default:
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Illegal message in CntlIdleProc(MsgType=%ld)\n",Elem->MsgType));
			break;
	}
}

VOID CntlOidScanProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	MLME_SCAN_REQ_STRUCT       ScanReq;
	ULONG                      BssIdx = BSS_NOT_FOUND;
	BSS_ENTRY                  CurrBss;

#ifdef RALINK_ATE
/* Disable scanning when ATE is running. */
	if (ATE_ON(pAd))
		return;
#endif // RALINK_ATE //


	// record current BSS if network is connected.
	// 2003-2-13 do not include current IBSS if this is the only STA in this IBSS.
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		BssIdx = BssSsidTableSearch(&pAd->ScanTab, pAd->CommonCfg.Bssid, (PUCHAR)pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen, pAd->CommonCfg.Channel);
		if (BssIdx != BSS_NOT_FOUND)
		{
			NdisMoveMemory(&CurrBss, &pAd->ScanTab.BssEntry[BssIdx], sizeof(BSS_ENTRY));
		}
	}

	// clean up previous SCAN result, add current BSS back to table if any
	BssTableInit(&pAd->ScanTab);
	if (BssIdx != BSS_NOT_FOUND)
	{
		// DDK Note: If the NIC is associated with a particular BSSID and SSID
		//    that are not contained in the list of BSSIDs generated by this scan, the
		//    BSSID description of the currently associated BSSID and SSID should be
		//    appended to the list of BSSIDs in the NIC's database.
		// To ensure this, we append this BSS as the first entry in SCAN result
		NdisMoveMemory(&pAd->ScanTab.BssEntry[0], &CurrBss, sizeof(BSS_ENTRY));
		pAd->ScanTab.BssNr = 1;
	}

	ScanParmFill(pAd, &ScanReq, (PSTRING) Elem->Msg, Elem->MsgLen, BSS_ANY, SCAN_ACTIVE);
	MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ,
		sizeof(MLME_SCAN_REQ_STRUCT), &ScanReq);
	pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
}

/*
	==========================================================================
	Description:
		Before calling this routine, user desired SSID should already been
		recorded in CommonCfg.Ssid[]
	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlOidSsidProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM * Elem)
{
	PNDIS_802_11_SSID          pOidSsid = (NDIS_802_11_SSID *)Elem->Msg;
	MLME_DISASSOC_REQ_STRUCT   DisassocReq;
	ULONG					   Now;


	// Step 1. record the desired user settings to MlmeAux
	NdisZeroMemory(pAd->MlmeAux.Ssid, MAX_LEN_OF_SSID);
	NdisMoveMemory(pAd->MlmeAux.Ssid, pOidSsid->Ssid, pOidSsid->SsidLength);
	pAd->MlmeAux.SsidLen = (UCHAR)pOidSsid->SsidLength;
	NdisZeroMemory(pAd->MlmeAux.Bssid, MAC_ADDR_LEN);
	pAd->MlmeAux.BssType = pAd->StaCfg.BssType;

	pAd->StaCfg.bAutoConnectByBssid = FALSE;

	//
	// Update Reconnect Ssid, that user desired to connect.
	//
	NdisZeroMemory(pAd->MlmeAux.AutoReconnectSsid, MAX_LEN_OF_SSID);
	NdisMoveMemory(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
	pAd->MlmeAux.AutoReconnectSsidLen = pAd->MlmeAux.SsidLen;

	// step 2. find all matching BSS in the lastest SCAN result (inBssTab)
	//    & log them into MlmeAux.SsidBssTab for later-on iteration. Sort by RSSI order
	BssTableSsidSort(pAd, &pAd->MlmeAux.SsidBssTab, (PCHAR)pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);

	DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():CNTL - %d BSS of %d BSS match the desire (%d)SSID - %s\n",
			pAd->MlmeAux.SsidBssTab.BssNr, pAd->ScanTab.BssNr, pAd->MlmeAux.SsidLen, pAd->MlmeAux.Ssid));
	NdisGetSystemUpTime(&Now);

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) &&
		(pAd->CommonCfg.SsidLen == pAd->MlmeAux.SsidBssTab.BssEntry[0].SsidLen) &&
		NdisEqualMemory(pAd->CommonCfg.Ssid, pAd->MlmeAux.SsidBssTab.BssEntry[0].Ssid, pAd->CommonCfg.SsidLen) &&
		MAC_ADDR_EQUAL(pAd->CommonCfg.Bssid, pAd->MlmeAux.SsidBssTab.BssEntry[0].Bssid))
	{
		// Case 1. already connected with an AP who has the desired SSID
		//         with highest RSSI

		// Add checking Mode "LEAP" for CCX 1.0
		if (((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA) ||
			 (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
			 (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) ||
			 (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
			 ) &&
			(pAd->StaCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED))
		{
			// case 1.1 For WPA, WPA-PSK, if the 1x port is not secured, we have to redo
			//          connection process
			DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():CNTL - disassociate with current AP...\n"));
			DisassocParmFill(pAd, &DisassocReq, pAd->CommonCfg.Bssid, REASON_DISASSOC_STA_LEAVING);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
						sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
		}
		else if (pAd->bConfigChanged == TRUE)
		{
			// case 1.2 Important Config has changed, we have to reconnect to the same AP
			DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():CNTL - disassociate with current AP Because config changed...\n"));
			DisassocParmFill(pAd, &DisassocReq, pAd->CommonCfg.Bssid, REASON_DISASSOC_STA_LEAVING);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
						sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
		}
		else
		{
			// case 1.3. already connected to the SSID with highest RSSI.
			DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():CNTL - already with this BSSID. ignore this SET_SSID request\n"));
			//
			// (HCT 12.1) 1c_wlan_mediaevents required
			// media connect events are indicated when associating with the same AP
			//
			if (INFRA_ON(pAd))
			{
				//
				// Since MediaState already is NdisMediaStateConnected
				// We just indicate the connect event again to meet the WHQL required.
				//
				pAd->IndicateMediaState = NdisMediaStateConnected;
				RTMP_IndicateMediaState(pAd);
                pAd->ExtraInfo = GENERAL_LINK_UP;   // Update extra information to link is up
			}

			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
			RtmpOSWrielessEventSend(pAd, SIOCGIWAP, -1, &pAd->MlmeAux.Bssid[0], NULL, 0);
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
		}
	}
	else if (INFRA_ON(pAd))
	{
		//
		// For RT61
		// [88888] OID_802_11_SSID should have returned NDTEST_WEP_AP2(Returned: )
		// RT61 may lost SSID, and not connect to NDTEST_WEP_AP2 and will connect to NDTEST_WEP_AP2 by Autoreconnect
		// But media status is connected, so the SSID not report correctly.
		//
		if (!SSID_EQUAL(pAd->CommonCfg.Ssid, pAd->CommonCfg.SsidLen, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen))
		{
			//
			// Different SSID means not Roaming case, so we let LinkDown() to Indicate a disconnect event.
			//
			pAd->MlmeAux.CurrReqIsFromNdis = TRUE;
		}
		// case 2. active INFRA association existent
		//    roaming is done within miniport driver, nothing to do with configuration
		//    utility. so upon a new SET(OID_802_11_SSID) is received, we just
		//    disassociate with the current associated AP,
		//    then perform a new association with this new SSID, no matter the
		//    new/old SSID are the same or not.
		DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():CNTL - disassociate with current AP...\n"));
		DisassocParmFill(pAd, &DisassocReq, pAd->CommonCfg.Bssid, REASON_DISASSOC_STA_LEAVING);
		MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
					sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);
		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
	}
	else
	{
		if (ADHOC_ON(pAd))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():CNTL - drop current ADHOC\n"));
			LinkDown(pAd, FALSE);
			OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
			pAd->IndicateMediaState = NdisMediaStateDisconnected;
			RTMP_IndicateMediaState(pAd);
            pAd->ExtraInfo = GENERAL_LINK_DOWN;
			DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():NDIS_STATUS_MEDIA_DISCONNECT Event C!\n"));
		}

		if ((pAd->MlmeAux.SsidBssTab.BssNr == 0) &&
			(pAd->StaCfg.bAutoReconnect == TRUE) &&
			(pAd->MlmeAux.BssType == BSS_INFRA) &&
			(MlmeValidateSSID(pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen) == TRUE)
			)
		{
			MLME_SCAN_REQ_STRUCT       ScanReq;

			DBGPRINT(RT_DEBUG_TRACE, ("CntlOidSsidProc():CNTL - No matching BSS, start a new scan\n"));
			ScanParmFill(pAd, &ScanReq, (PSTRING) pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen, BSS_ANY, SCAN_ACTIVE);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ, sizeof(MLME_SCAN_REQ_STRUCT), &ScanReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
			// Reset Missed scan number
			pAd->StaCfg.LastScanTime = Now;
		}
		else
		{

			pAd->MlmeAux.BssIdx = 0;
			IterateOnBssTab(pAd);
		}
	}
}


/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlOidRTBssidProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM * Elem)
{
	ULONG       BssIdx;
	PUCHAR      pOidBssid = (PUCHAR)Elem->Msg;
	MLME_DISASSOC_REQ_STRUCT    DisassocReq;
	MLME_JOIN_REQ_STRUCT        JoinReq;

#ifdef RALINK_ATE
/* No need to perform this routine when ATE is running. */
	if (ATE_ON(pAd))
		return;
#endif // RALINK_ATE //

	// record user desired settings
	COPY_MAC_ADDR(pAd->MlmeAux.Bssid, pOidBssid);
	pAd->MlmeAux.BssType = pAd->StaCfg.BssType;

	// find the desired BSS in the latest SCAN result table
	BssIdx = BssTableSearch(&pAd->ScanTab, pOidBssid, pAd->MlmeAux.Channel);
	if (BssIdx == BSS_NOT_FOUND)
	{
		MLME_SCAN_REQ_STRUCT       ScanReq;

		DBGPRINT(RT_DEBUG_TRACE, ("CNTL - BSSID not found. reply NDIS_STATUS_NOT_ACCEPTED\n"));
		//pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;

		DBGPRINT(RT_DEBUG_TRACE, ("CNTL - BSSID not found. start a new scan\n"));
		ScanParmFill(pAd, &ScanReq, (PSTRING) pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen, BSS_ANY, SCAN_ACTIVE);
		MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ, sizeof(MLME_SCAN_REQ_STRUCT), &ScanReq);
		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
		// Reset Missed scan number
		NdisGetSystemUpTime(&pAd->StaCfg.LastScanTime);
		return;
	}

	//
	// Update Reconnect Ssid, that user desired to connect.
	//
	NdisZeroMemory(pAd->MlmeAux.AutoReconnectSsid, MAX_LEN_OF_SSID);
	pAd->MlmeAux.AutoReconnectSsidLen = pAd->ScanTab.BssEntry[BssIdx].SsidLen;
	NdisMoveMemory(pAd->MlmeAux.AutoReconnectSsid, pAd->ScanTab.BssEntry[BssIdx].Ssid, pAd->ScanTab.BssEntry[BssIdx].SsidLen);

	// copy the matched BSS entry from ScanTab to MlmeAux.SsidBssTab. Why?
	// Because we need this entry to become the JOIN target in later on SYNC state machine
	pAd->MlmeAux.BssIdx = 0;
	pAd->MlmeAux.SsidBssTab.BssNr = 1;
	NdisMoveMemory(&pAd->MlmeAux.SsidBssTab.BssEntry[0], &pAd->ScanTab.BssEntry[BssIdx], sizeof(BSS_ENTRY));

	// Add SSID into MlmeAux for site surey joining hidden SSID
	pAd->MlmeAux.SsidLen = pAd->ScanTab.BssEntry[BssIdx].SsidLen;
	NdisMoveMemory(pAd->MlmeAux.Ssid, pAd->ScanTab.BssEntry[BssIdx].Ssid, pAd->MlmeAux.SsidLen);

	{
		if (INFRA_ON(pAd))
		{
			// disassoc from current AP first
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - disassociate with current AP ...\n"));
			DisassocParmFill(pAd, &DisassocReq, pAd->CommonCfg.Bssid, REASON_DISASSOC_STA_LEAVING);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
						sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
		}
		else
		{
			if (ADHOC_ON(pAd))
			{
				DBGPRINT(RT_DEBUG_TRACE, ("CNTL - drop current ADHOC\n"));
				LinkDown(pAd, FALSE);
				OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
				pAd->IndicateMediaState = NdisMediaStateDisconnected;
				RTMP_IndicateMediaState(pAd);
                pAd->ExtraInfo = GENERAL_LINK_DOWN;
				DBGPRINT(RT_DEBUG_TRACE, ("NDIS_STATUS_MEDIA_DISCONNECT Event C!\n"));
			}

			// Change the wepstatus to original wepstatus
			pAd->StaCfg.WepStatus   = pAd->StaCfg.OrigWepStatus;
			pAd->StaCfg.PairCipher  = pAd->StaCfg.OrigWepStatus;
			pAd->StaCfg.GroupCipher = pAd->StaCfg.OrigWepStatus;

			// Check cipher suite, AP must have more secured cipher than station setting
			// Set the Pairwise and Group cipher to match the intended AP setting
			// We can only connect to AP with less secured cipher setting
			if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA) || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK))
			{
				pAd->StaCfg.GroupCipher = pAd->ScanTab.BssEntry[BssIdx].WPA.GroupCipher;

				if (pAd->StaCfg.WepStatus == pAd->ScanTab.BssEntry[BssIdx].WPA.PairCipher)
					pAd->StaCfg.PairCipher = pAd->ScanTab.BssEntry[BssIdx].WPA.PairCipher;
				else if (pAd->ScanTab.BssEntry[BssIdx].WPA.PairCipherAux != Ndis802_11WEPDisabled)
					pAd->StaCfg.PairCipher = pAd->ScanTab.BssEntry[BssIdx].WPA.PairCipherAux;
				else	// There is no PairCipher Aux, downgrade our capability to TKIP
					pAd->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;
			}
			else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK))
			{
				pAd->StaCfg.GroupCipher = pAd->ScanTab.BssEntry[BssIdx].WPA2.GroupCipher;

				if (pAd->StaCfg.WepStatus == pAd->ScanTab.BssEntry[BssIdx].WPA2.PairCipher)
					pAd->StaCfg.PairCipher = pAd->ScanTab.BssEntry[BssIdx].WPA2.PairCipher;
				else if (pAd->ScanTab.BssEntry[BssIdx].WPA2.PairCipherAux != Ndis802_11WEPDisabled)
					pAd->StaCfg.PairCipher = pAd->ScanTab.BssEntry[BssIdx].WPA2.PairCipherAux;
				else	// There is no PairCipher Aux, downgrade our capability to TKIP
					pAd->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;

				// RSN capability
				pAd->StaCfg.RsnCapability = pAd->ScanTab.BssEntry[BssIdx].WPA2.RsnCapability;
			}

			// Set Mix cipher flag
			pAd->StaCfg.bMixCipher = (pAd->StaCfg.PairCipher == pAd->StaCfg.GroupCipher) ? FALSE : TRUE;
			/*if (pAd->StaCfg.bMixCipher == TRUE)
			{
				// If mix cipher, re-build RSNIE
				RTMPMakeRSNIE(pAd, pAd->StaCfg.AuthMode, pAd->StaCfg.WepStatus, 0);
			}*/
			// No active association, join the BSS immediately
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - joining %02x:%02x:%02x:%02x:%02x:%02x ...\n",
				pOidBssid[0],pOidBssid[1],pOidBssid[2],pOidBssid[3],pOidBssid[4],pOidBssid[5]));

			JoinParmFill(pAd, &JoinReq, pAd->MlmeAux.BssIdx);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_JOIN_REQ, sizeof(MLME_JOIN_REQ_STRUCT), &JoinReq);

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_JOIN;
		}
	}
}

// Roaming is the only external request triggering CNTL state machine
// despite of other "SET OID" operation. All "SET OID" related oerations
// happen in sequence, because no other SET OID will be sent to this device
// until the the previous SET operation is complete (successful o failed).
// So, how do we quarantee this ROAMING request won't corrupt other "SET OID"?
// or been corrupted by other "SET OID"?
//
// IRQL = DISPATCH_LEVEL
VOID CntlMlmeRoamingProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	UCHAR BBPValue = 0;

	DBGPRINT(RT_DEBUG_TRACE,("CNTL - Roaming in MlmeAux.RoamTab...\n"));

	{
		//Let BBP register at 20MHz to do (fast) roaming.
		RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &BBPValue);
		BBPValue &= (~0x18);
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, BBPValue);

		NdisMoveMemory(&pAd->MlmeAux.SsidBssTab, &pAd->MlmeAux.RoamTab, sizeof(pAd->MlmeAux.RoamTab));
		pAd->MlmeAux.SsidBssTab.BssNr = pAd->MlmeAux.RoamTab.BssNr;

		BssTableSortByRssi(&pAd->MlmeAux.SsidBssTab);
		pAd->MlmeAux.BssIdx = 0;
		IterateOnBssTab(pAd);
	}
}

#ifdef QOS_DLS_SUPPORT
/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlOidDLSSetupProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	PRT_802_11_DLS		pDLS = (PRT_802_11_DLS)Elem->Msg;
	MLME_DLS_REQ_STRUCT	MlmeDlsReq;
	INT					i;
	USHORT				reason = REASON_UNSPECIFY;

	DBGPRINT(RT_DEBUG_TRACE,("CNTL - (OID set %02x:%02x:%02x:%02x:%02x:%02x with Valid=%d, Status=%d, TimeOut=%d, CountDownTimer=%d)\n",
		pDLS->MacAddr[0], pDLS->MacAddr[1], pDLS->MacAddr[2], pDLS->MacAddr[3], pDLS->MacAddr[4], pDLS->MacAddr[5],
		pDLS->Valid, pDLS->Status, pDLS->TimeOut, pDLS->CountDownTimer));

	if (!pAd->CommonCfg.bDLSCapable)
		return;

	// DLS will not be supported when Adhoc mode
	if (INFRA_ON(pAd))
	{
		for (i = 0; i < MAX_NUM_OF_DLS_ENTRY; i++)
		{
			if (pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH) &&
				(pDLS->TimeOut == pAd->StaCfg.DLSEntry[i].TimeOut) && MAC_ADDR_EQUAL(pDLS->MacAddr, pAd->StaCfg.DLSEntry[i].MacAddr))
			{
				// 1. Same setting, just drop it
				DBGPRINT(RT_DEBUG_TRACE,("CNTL - setting unchanged\n"));
				break;
			}
			else if (!pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH) &&
				MAC_ADDR_EQUAL(pDLS->MacAddr, pAd->StaCfg.DLSEntry[i].MacAddr))
			{
				// 2. Disable DLS link case, just tear down DLS link
				reason = REASON_QOS_UNWANTED_MECHANISM;
				pAd->StaCfg.DLSEntry[i].Valid	= FALSE;
				pAd->StaCfg.DLSEntry[i].Status	= DLS_NONE;
				DlsParmFill(pAd, &MlmeDlsReq, &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE, MT2_MLME_DLS_TEAR_DOWN, sizeof(MLME_DLS_REQ_STRUCT), &MlmeDlsReq);
				DBGPRINT(RT_DEBUG_TRACE,("CNTL - start tear down procedure\n"));
				break;
			}
			else if ((i < MAX_NUM_OF_DLS_ENTRY) && pDLS->Valid && !pAd->StaCfg.DLSEntry[i].Valid)
			{
				// 3. Enable case, start DLS setup procedure
				NdisMoveMemory(&pAd->StaCfg.DLSEntry[i], pDLS, sizeof(RT_802_11_DLS_UI));

				//Update countdown timer
				pAd->StaCfg.DLSEntry[i].CountDownTimer = pAd->StaCfg.DLSEntry[i].TimeOut;
				DlsParmFill(pAd, &MlmeDlsReq, &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE, MT2_MLME_DLS_REQ, sizeof(MLME_DLS_REQ_STRUCT), &MlmeDlsReq);
				DBGPRINT(RT_DEBUG_TRACE,("CNTL - DLS setup case\n"));
				break;
			}
			else if ((i < MAX_NUM_OF_DLS_ENTRY) && pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid &&
				(pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH) && !MAC_ADDR_EQUAL(pDLS->MacAddr, pAd->StaCfg.DLSEntry[i].MacAddr))
			{
				// 4. update mac case, tear down old DLS and setup new DLS
				reason = REASON_QOS_UNWANTED_MECHANISM;
				pAd->StaCfg.DLSEntry[i].Valid	= FALSE;
				pAd->StaCfg.DLSEntry[i].Status	= DLS_NONE;
				DlsParmFill(pAd, &MlmeDlsReq, &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE, MT2_MLME_DLS_TEAR_DOWN, sizeof(MLME_DLS_REQ_STRUCT), &MlmeDlsReq);
				NdisMoveMemory(&pAd->StaCfg.DLSEntry[i], pDLS, sizeof(RT_802_11_DLS_UI));
				DlsParmFill(pAd, &MlmeDlsReq, &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE, MT2_MLME_DLS_REQ, sizeof(MLME_DLS_REQ_STRUCT), &MlmeDlsReq);
				DBGPRINT(RT_DEBUG_TRACE,("CNTL - DLS tear down and restart case\n"));
				break;
			}
			else if (pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid &&
				MAC_ADDR_EQUAL(pDLS->MacAddr, pAd->StaCfg.DLSEntry[i].MacAddr) && (pAd->StaCfg.DLSEntry[i].TimeOut != pDLS->TimeOut))
			{
				// 5. update timeout case, start DLS setup procedure (no tear down)
				pAd->StaCfg.DLSEntry[i].TimeOut	= pDLS->TimeOut;
				//Update countdown timer
				pAd->StaCfg.DLSEntry[i].CountDownTimer = pAd->StaCfg.DLSEntry[i].TimeOut;
				DlsParmFill(pAd, &MlmeDlsReq, &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE, MT2_MLME_DLS_REQ, sizeof(MLME_DLS_REQ_STRUCT), &MlmeDlsReq);
				DBGPRINT(RT_DEBUG_TRACE,("CNTL - DLS update timeout case\n"));
				break;
			}
			else if (pDLS->Valid && pAd->StaCfg.DLSEntry[i].Valid &&
				(pAd->StaCfg.DLSEntry[i].Status != DLS_FINISH) && MAC_ADDR_EQUAL(pDLS->MacAddr, pAd->StaCfg.DLSEntry[i].MacAddr))
			{
				// 6. re-setup case, start DLS setup procedure (no tear down)
				DlsParmFill(pAd, &MlmeDlsReq, &pAd->StaCfg.DLSEntry[i], reason);
				MlmeEnqueue(pAd, DLS_STATE_MACHINE, MT2_MLME_DLS_REQ, sizeof(MLME_DLS_REQ_STRUCT), &MlmeDlsReq);
				DBGPRINT(RT_DEBUG_TRACE,("CNTL - DLS retry setup procedure\n"));
				break;
			}
			else
			{
				DBGPRINT(RT_DEBUG_WARN,("CNTL - DLS not changed in entry - %d - Valid=%d, Status=%d, TimeOut=%d\n",
					i, pAd->StaCfg.DLSEntry[i].Valid, pAd->StaCfg.DLSEntry[i].Status, pAd->StaCfg.DLSEntry[i].TimeOut));
			}
		}
	}
}
#endif // QOS_DLS_SUPPORT //

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlWaitDisassocProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	MLME_START_REQ_STRUCT     StartReq;

	if (Elem->MsgType == MT2_DISASSOC_CONF)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Dis-associate successful\n"));

	    if (pAd->CommonCfg.bWirelessEvent)
		{
			RTMPSendWirelessEvent(pAd, IW_DISASSOC_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0);
		}

		LinkDown(pAd, FALSE);

		// case 1. no matching BSS, and user wants ADHOC, so we just start a new one
		if ((pAd->MlmeAux.SsidBssTab.BssNr==0) && (pAd->StaCfg.BssType == BSS_ADHOC))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - No matching BSS, start a new ADHOC (Ssid=%s)...\n",pAd->MlmeAux.Ssid));
			StartParmFill(pAd, &StartReq, (PCHAR)pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ, sizeof(MLME_START_REQ_STRUCT), &StartReq);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_START;
		}
		// case 2. try each matched BSS
		else
		{
			pAd->MlmeAux.BssIdx = 0;

			IterateOnBssTab(pAd);
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlWaitJoinProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT                      Reason;
	MLME_AUTH_REQ_STRUCT        AuthReq;

	if (Elem->MsgType == MT2_JOIN_CONF)
	{
		NdisMoveMemory(&Reason, Elem->Msg, sizeof(USHORT));
		if (Reason == MLME_SUCCESS)
		{
			// 1. joined an IBSS, we are pretty much done here
			if (pAd->MlmeAux.BssType == BSS_ADHOC)
			{
			    //
				// 5G bands rules of Japan:
				// Ad hoc must be disabled in W53(ch52,56,60,64) channels.
				//
				if ( (pAd->CommonCfg.bIEEE80211H == 1) &&
                      RadarChannelCheck(pAd, pAd->CommonCfg.Channel)
				   )
				{
					pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
					DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Channel=%d, Join adhoc on W53(52,56,60,64) Channels are not accepted\n", pAd->CommonCfg.Channel));
					return;
				}

				LinkUp(pAd, BSS_ADHOC);
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
				DBGPRINT(RT_DEBUG_TRACE, ("CNTL - join the IBSS = %02x:%02x:%02x:%02x:%02x:%02x ...\n",
				pAd->CommonCfg.Bssid[0],pAd->CommonCfg.Bssid[1],pAd->CommonCfg.Bssid[2],
				pAd->CommonCfg.Bssid[3],pAd->CommonCfg.Bssid[4],pAd->CommonCfg.Bssid[5]));

                pAd->IndicateMediaState = NdisMediaStateConnected;
                pAd->ExtraInfo = GENERAL_LINK_UP;
			}
			// 2. joined a new INFRA network, start from authentication
			else
			{
				{
					// either Ndis802_11AuthModeShared or Ndis802_11AuthModeAutoSwitch, try shared key first
					if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeShared) ||
						(pAd->StaCfg.AuthMode == Ndis802_11AuthModeAutoSwitch))
					{
						AuthParmFill(pAd, &AuthReq, pAd->MlmeAux.Bssid, AUTH_MODE_KEY);
					}
					else
					{
						AuthParmFill(pAd, &AuthReq, pAd->MlmeAux.Bssid, AUTH_MODE_OPEN);
					}
					MlmeEnqueue(pAd, AUTH_STATE_MACHINE, MT2_MLME_AUTH_REQ,
							sizeof(MLME_AUTH_REQ_STRUCT), &AuthReq);
				}

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_AUTH;
			}
		}
		else
		{
			// 3. failed, try next BSS
			pAd->MlmeAux.BssIdx++;
			IterateOnBssTab(pAd);
		}
	}
}


/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlWaitStartProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT      Result;

	if (Elem->MsgType == MT2_START_CONF)
	{
		NdisMoveMemory(&Result, Elem->Msg, sizeof(USHORT));
		if (Result == MLME_SUCCESS)
		{
		    //
			// 5G bands rules of Japan:
			// Ad hoc must be disabled in W53(ch52,56,60,64) channels.
			//
			if ( (pAd->CommonCfg.bIEEE80211H == 1) &&
                  RadarChannelCheck(pAd, pAd->CommonCfg.Channel)
			   )
			{
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
				DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Channel=%d, Start adhoc on W53(52,56,60,64) Channels are not accepted\n", pAd->CommonCfg.Channel));
				return;
			}
#ifdef DOT11_N_SUPPORT
			NdisZeroMemory(&pAd->StaActive.SupportedPhyInfo.MCSSet[0], 16);
			if (pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED)
			{
				N_ChannelCheck(pAd);
				SetCommonHT(pAd);
				NdisMoveMemory(&pAd->MlmeAux.AddHtInfo, &pAd->CommonCfg.AddHTInfo, sizeof(ADD_HT_INFO_IE));
				RTMPCheckHt(pAd, BSSID_WCID, &pAd->CommonCfg.HtCapability, &pAd->CommonCfg.AddHTInfo);
				pAd->StaActive.SupportedPhyInfo.bHtEnable = TRUE;
				NdisMoveMemory(&pAd->StaActive.SupportedPhyInfo.MCSSet[0], &pAd->CommonCfg.HtCapability.MCSSet[0], 16);
				COPY_HTSETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(pAd);

				if ((pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth  == BW_40) &&
					(pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset == EXTCHA_ABOVE))
				{
					pAd->MlmeAux.CentralChannel = pAd->CommonCfg.Channel + 2;
				}
				else if ((pAd->CommonCfg.HtCapability.HtCapInfo.ChannelWidth  == BW_40) &&
						 (pAd->CommonCfg.AddHTInfo.AddHtInfo.ExtChanOffset == EXTCHA_BELOW))
				{
					pAd->MlmeAux.CentralChannel = pAd->CommonCfg.Channel - 2;
				}
			}
			else
#endif // DOT11_N_SUPPORT //
			{
				pAd->StaActive.SupportedPhyInfo.bHtEnable = FALSE;
			}
			LinkUp(pAd, BSS_ADHOC);
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			// Before send beacon, driver need do radar detection
			if ((pAd->CommonCfg.Channel > 14 )
				&& (pAd->CommonCfg.bIEEE80211H == 1)
				&& RadarChannelCheck(pAd, pAd->CommonCfg.Channel))
			{
				pAd->CommonCfg.RadarDetect.RDMode = RD_SILENCE_MODE;
				pAd->CommonCfg.RadarDetect.RDCount = 0;
#ifdef DFS_SUPPORT
				BbpRadarDetectionStart(pAd);
#endif // DFS_SUPPORT //
			}

			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - start a new IBSS = %02x:%02x:%02x:%02x:%02x:%02x ...\n",
				pAd->CommonCfg.Bssid[0],pAd->CommonCfg.Bssid[1],pAd->CommonCfg.Bssid[2],
				pAd->CommonCfg.Bssid[3],pAd->CommonCfg.Bssid[4],pAd->CommonCfg.Bssid[5]));
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Start IBSS fail. BUG!!!!!\n"));
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlWaitAuthProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT                       Reason;
	MLME_ASSOC_REQ_STRUCT        AssocReq;
	MLME_AUTH_REQ_STRUCT         AuthReq;

	if (Elem->MsgType == MT2_AUTH_CONF)
	{
		NdisMoveMemory(&Reason, Elem->Msg, sizeof(USHORT));
		if (Reason == MLME_SUCCESS)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - AUTH OK\n"));
			AssocParmFill(pAd, &AssocReq, pAd->MlmeAux.Bssid, pAd->MlmeAux.CapabilityInfo,
						  ASSOC_TIMEOUT, pAd->StaCfg.DefaultListenCount);

			{
				MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_ASSOC_REQ,
							sizeof(MLME_ASSOC_REQ_STRUCT), &AssocReq);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_ASSOC;
			}
		}
		else
		{
			// This fail may because of the AP already keep us in its MAC table without
			// ageing-out. The previous authentication attempt must have let it remove us.
			// so try Authentication again may help. For D-Link DWL-900AP+ compatibility.
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - AUTH FAIL, try again...\n"));
			{
				if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeShared) ||
					(pAd->StaCfg.AuthMode == Ndis802_11AuthModeAutoSwitch))
				{
					// either Ndis802_11AuthModeShared or Ndis802_11AuthModeAutoSwitch, try shared key first
					AuthParmFill(pAd, &AuthReq, pAd->MlmeAux.Bssid, AUTH_MODE_KEY);
				}
				else
				{
					AuthParmFill(pAd, &AuthReq, pAd->MlmeAux.Bssid, AUTH_MODE_OPEN);
				}
				MlmeEnqueue(pAd, AUTH_STATE_MACHINE, MT2_MLME_AUTH_REQ,
							sizeof(MLME_AUTH_REQ_STRUCT), &AuthReq);

			}
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_AUTH2;
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlWaitAuthProc2(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT                       Reason;
	MLME_ASSOC_REQ_STRUCT        AssocReq;
	MLME_AUTH_REQ_STRUCT         AuthReq;

	if (Elem->MsgType == MT2_AUTH_CONF)
	{
		NdisMoveMemory(&Reason, Elem->Msg, sizeof(USHORT));
		if (Reason == MLME_SUCCESS)
		{
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - AUTH OK\n"));
			AssocParmFill(pAd, &AssocReq, pAd->MlmeAux.Bssid, pAd->MlmeAux.CapabilityInfo,
							  ASSOC_TIMEOUT, pAd->StaCfg.DefaultListenCount);
			{
				MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_ASSOC_REQ,
							sizeof(MLME_ASSOC_REQ_STRUCT), &AssocReq);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_ASSOC;
			}
		}
		else
		{
			if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeAutoSwitch) &&
				 (pAd->MlmeAux.Alg == Ndis802_11AuthModeShared))
			{
				DBGPRINT(RT_DEBUG_TRACE, ("CNTL - AUTH FAIL, try OPEN system...\n"));
				AuthParmFill(pAd, &AuthReq, pAd->MlmeAux.Bssid, Ndis802_11AuthModeOpen);
				MlmeEnqueue(pAd, AUTH_STATE_MACHINE, MT2_MLME_AUTH_REQ,
							sizeof(MLME_AUTH_REQ_STRUCT), &AuthReq);

				pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_AUTH2;
			}
			else
			{
				// not success, try next BSS
				DBGPRINT(RT_DEBUG_TRACE, ("CNTL - AUTH FAIL, give up; try next BSS\n"));
				pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE; //???????
				pAd->MlmeAux.BssIdx++;
				IterateOnBssTab(pAd);
			}
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlWaitAssocProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT      Reason;

	if (Elem->MsgType == MT2_ASSOC_CONF)
	{
		NdisMoveMemory(&Reason, Elem->Msg, sizeof(USHORT));
		if (Reason == MLME_SUCCESS)
		{
			if (pAd->CommonCfg.bWirelessEvent)
			{
				RTMPSendWirelessEvent(pAd, IW_ASSOC_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0);
			}

			LinkUp(pAd, BSS_INFRA);
			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Association successful on BSS #%ld\n",pAd->MlmeAux.BssIdx));
		}
		else
		{
			// not success, try next BSS
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Association fails on BSS #%ld\n",pAd->MlmeAux.BssIdx));
			pAd->MlmeAux.BssIdx++;
			IterateOnBssTab(pAd);
		}
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID CntlWaitReassocProc(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	USHORT      Result;

	if (Elem->MsgType == MT2_REASSOC_CONF)
	{
		NdisMoveMemory(&Result, Elem->Msg, sizeof(USHORT));
		if (Result == MLME_SUCCESS)
		{
			// send wireless event - for association
			if (pAd->CommonCfg.bWirelessEvent)
				RTMPSendWirelessEvent(pAd, IW_ASSOC_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0);

			//
			// NDIS requires a new Link UP indication but no Link Down for RE-ASSOC
			//
			LinkUp(pAd, BSS_INFRA);

			pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Re-assocition successful on BSS #%ld\n", pAd->MlmeAux.RoamIdx));
		}
		else
		{
			// reassoc failed, try to pick next BSS in the BSS Table
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - Re-assocition fails on BSS #%ld\n", pAd->MlmeAux.RoamIdx));
			{
				pAd->MlmeAux.RoamIdx++;
				IterateOnBssTab2(pAd);
			}
		}
	}
}


VOID	AdhocTurnOnQos(
	IN  PRTMP_ADAPTER pAd)
{
#define AC0_DEF_TXOP		0
#define AC1_DEF_TXOP		0
#define AC2_DEF_TXOP		94
#define AC3_DEF_TXOP		47

	// Turn on QOs if use HT rate.
	if (pAd->CommonCfg.APEdcaParm.bValid == FALSE)
	{
		pAd->CommonCfg.APEdcaParm.bValid = TRUE;
		pAd->CommonCfg.APEdcaParm.Aifsn[0] = 3;
		pAd->CommonCfg.APEdcaParm.Aifsn[1] = 7;
		pAd->CommonCfg.APEdcaParm.Aifsn[2] = 1;
		pAd->CommonCfg.APEdcaParm.Aifsn[3] = 1;

		pAd->CommonCfg.APEdcaParm.Cwmin[0] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmin[1] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmin[2] = 3;
		pAd->CommonCfg.APEdcaParm.Cwmin[3] = 2;

		pAd->CommonCfg.APEdcaParm.Cwmax[0] = 10;
		pAd->CommonCfg.APEdcaParm.Cwmax[1] = 6;
		pAd->CommonCfg.APEdcaParm.Cwmax[2] = 4;
		pAd->CommonCfg.APEdcaParm.Cwmax[3] = 3;

		pAd->CommonCfg.APEdcaParm.Txop[0]  = 0;
		pAd->CommonCfg.APEdcaParm.Txop[1]  = 0;
		pAd->CommonCfg.APEdcaParm.Txop[2]  = AC2_DEF_TXOP;
		pAd->CommonCfg.APEdcaParm.Txop[3]  = AC3_DEF_TXOP;
	}
	AsicSetEdcaParm(pAd, &pAd->CommonCfg.APEdcaParm);
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID LinkUp(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR BssType)
{
	ULONG	Now;
	UINT32	Data;
	BOOLEAN	Cancelled;
	UCHAR	Value = 0, idx = 0, HashIdx = 0;
	MAC_TABLE_ENTRY *pEntry = NULL, *pCurrEntry = NULL;

	// Init ChannelQuality to prevent DEAD_CQI at initial LinkUp
	pAd->Mlme.ChannelQuality = 50;

	pEntry = MacTableLookup(pAd, pAd->CommonCfg.Bssid);
	if (pEntry)
	{
		MacTableDeleteEntry(pAd, pEntry->Aid, pEntry->Addr);
		pEntry = NULL;
	}


	pEntry = &pAd->MacTab.Content[BSSID_WCID];

	//
	// ASSOC - DisassocTimeoutAction
	// CNTL - Dis-associate successful
	// !!! LINK DOWN !!!
	// [88888] OID_802_11_SSID should have returned NDTEST_WEP_AP2(Returned: )
	//
	// To prevent DisassocTimeoutAction to call Link down after we link up,
	// cancel the DisassocTimer no matter what it start or not.
	//
	RTMPCancelTimer(&pAd->MlmeAux.DisassocTimer,  &Cancelled);

	COPY_SETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(pAd);

#ifdef DOT11_N_SUPPORT
	COPY_HTSETTINGS_FROM_MLME_AUX_TO_ACTIVE_CFG(pAd);
#endif // DOT11_N_SUPPORT //

#ifdef RTMP_MAC_PCI
	// Before power save before link up function, We will force use 1R.
	// So after link up, check Rx antenna # again.
	RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &Value);
	if(pAd->Antenna.field.RxPath == 3)
	{
		Value |= (0x10);
	}
	else if(pAd->Antenna.field.RxPath == 2)
	{
		Value |= (0x8);
	}
	else if(pAd->Antenna.field.RxPath == 1)
	{
		Value |= (0x0);
	}
	RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, Value);
	pAd->StaCfg.BBPR3 = Value;
#endif // RTMP_MAC_PCI //

	if (BssType == BSS_ADHOC)
	{
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_ADHOC_ON);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_INFRA_ON);

#ifdef CARRIER_DETECTION_SUPPORT // Roger sync Carrier
		// No carrier detection when adhoc
		// CarrierDetectionStop(pAd);
		pAd->CommonCfg.CarrierDetect.CD_State = CD_NORMAL;
#endif // CARRIER_DETECTION_SUPPORT //

#ifdef DOT11_N_SUPPORT
		if (pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED)
			AdhocTurnOnQos(pAd);
#endif // DOT11_N_SUPPORT //

		DBGPRINT(RT_DEBUG_TRACE, ("!!!Adhoc LINK UP !!! \n" ));
	}
	else
	{
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_INFRA_ON);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_ADHOC_ON);

		DBGPRINT(RT_DEBUG_TRACE, ("!!!Infra LINK UP !!! \n" ));
	}

		// 3*3
		// reset Tx beamforming bit
		RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &Value);
		Value &= (~0x01);
		Value |= pAd->CommonCfg.RegTransmitSetting.field.TxBF;
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, Value);

#ifdef DOT11_N_SUPPORT
	// Change to AP channel
        if ((pAd->CommonCfg.CentralChannel > pAd->CommonCfg.Channel) && (pAd->MlmeAux.HtCapability.HtCapInfo.ChannelWidth == BW_40))
	{
		// Must using 40MHz.
		pAd->CommonCfg.BBPCurrentBW = BW_40;
		AsicSwitchChannel(pAd, pAd->CommonCfg.CentralChannel, FALSE);
		AsicLockChannel(pAd, pAd->CommonCfg.CentralChannel);

		RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &Value);
		Value &= (~0x18);
		Value |= 0x10;
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, Value);

		//  RX : control channel at lower
		RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &Value);
		Value &= (~0x20);
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, Value);
#ifdef RTMP_MAC_PCI
            pAd->StaCfg.BBPR3 = Value;
#endif // RTMP_MAC_PCI //

		RTMP_IO_READ32(pAd, TX_BAND_CFG, &Data);
		Data &= 0xfffffffe;
		RTMP_IO_WRITE32(pAd, TX_BAND_CFG, Data);

		if (pAd->MACVersion == 0x28600100)
		{
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R69, 0x1A);
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R70, 0x0A);
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R73, 0x16);
                DBGPRINT(RT_DEBUG_TRACE, ("!!!rt2860C !!! \n" ));
		}

		DBGPRINT(RT_DEBUG_TRACE, ("!!!40MHz Lower LINK UP !!! Control Channel at Below. Central = %d \n", pAd->CommonCfg.CentralChannel ));
	}
		else if ((pAd->CommonCfg.CentralChannel < pAd->CommonCfg.Channel) && (pAd->MlmeAux.HtCapability.HtCapInfo.ChannelWidth == BW_40))
	    {
		    // Must using 40MHz.
			pAd->CommonCfg.BBPCurrentBW = BW_40;
			AsicSwitchChannel(pAd, pAd->CommonCfg.CentralChannel, FALSE);
		    AsicLockChannel(pAd, pAd->CommonCfg.CentralChannel);

			RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &Value);
		Value &= (~0x18);
		Value |= 0x10;
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, Value);

		RTMP_IO_READ32(pAd, TX_BAND_CFG, &Data);
		Data |= 0x1;
		RTMP_IO_WRITE32(pAd, TX_BAND_CFG, Data);

			RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &Value);
		    Value |= (0x20);
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, Value);
#ifdef RTMP_MAC_PCI
            pAd->StaCfg.BBPR3 = Value;
#endif // RTMP_MAC_PCI //

		if (pAd->MACVersion == 0x28600100)
		{
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R69, 0x1A);
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R70, 0x0A);
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R73, 0x16);
			    DBGPRINT(RT_DEBUG_TRACE, ("!!!rt2860C !!! \n" ));
		}

		    DBGPRINT(RT_DEBUG_TRACE, ("!!! 40MHz Upper LINK UP !!! Control Channel at UpperCentral = %d \n", pAd->CommonCfg.CentralChannel ));
	    }
	    else
#endif // DOT11_N_SUPPORT //
	    {
		    pAd->CommonCfg.BBPCurrentBW = BW_20;
		pAd->CommonCfg.CentralChannel = pAd->CommonCfg.Channel;
			AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);
			AsicLockChannel(pAd, pAd->CommonCfg.Channel);

			RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &Value);
			Value &= (~0x18);
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, Value);

			RTMP_IO_READ32(pAd, TX_BAND_CFG, &Data);
			Data &= 0xfffffffe;
			RTMP_IO_WRITE32(pAd, TX_BAND_CFG, Data);

			RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R3, &Value);
			Value &= (~0x20);
			RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R3, Value);
#ifdef RTMP_MAC_PCI
            pAd->StaCfg.BBPR3 = Value;
#endif // RTMP_MAC_PCI //

			if (pAd->MACVersion == 0x28600100)
			{
				RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R69, 0x16);
				RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R70, 0x08);
				RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R73, 0x11);
				DBGPRINT(RT_DEBUG_TRACE, ("!!!rt2860C !!! \n" ));
			}

		    DBGPRINT(RT_DEBUG_TRACE, ("!!! 20MHz LINK UP !!! \n" ));
	}

	RTMPSetAGCInitValue(pAd, pAd->CommonCfg.BBPCurrentBW);
	//
	// Save BBP_R66 value, it will be used in RTUSBResumeMsduTransmission
	//
	RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R66, &pAd->BbpTuning.R66CurrentValue);

	DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK UP !!! (BssType=%d, AID=%d, ssid=%s, Channel=%d, CentralChannel = %d)\n",
		BssType, pAd->StaActive.Aid, pAd->CommonCfg.Ssid, pAd->CommonCfg.Channel, pAd->CommonCfg.CentralChannel));

#ifdef DOT11_N_SUPPORT
	DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK UP !!! (Density =%d, )\n", pAd->MacTab.Content[BSSID_WCID].MpduDensity));
#endif // DOT11_N_SUPPORT //

		AsicSetBssid(pAd, pAd->CommonCfg.Bssid);

	AsicSetSlotTime(pAd, TRUE);
	AsicSetEdcaParm(pAd, &pAd->CommonCfg.APEdcaParm);


	// Call this for RTS protectionfor legacy rate, we will always enable RTS threshold, but normally it will not hit
	AsicUpdateProtect(pAd, 0, (OFDMSETPROTECT | CCKSETPROTECT), TRUE, FALSE);

#ifdef DOT11_N_SUPPORT
	if ((pAd->StaActive.SupportedPhyInfo.bHtEnable == TRUE))
	{
		// Update HT protectionfor based on AP's operating mode.
	if (pAd->MlmeAux.AddHtInfo.AddHtInfo2.NonGfPresent == 1)
	{
		AsicUpdateProtect(pAd, pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode,  ALLN_SETPROTECT, FALSE, TRUE);
	}
	else
			AsicUpdateProtect(pAd, pAd->MlmeAux.AddHtInfo.AddHtInfo2.OperaionMode,  ALLN_SETPROTECT, FALSE, FALSE);
	}
#endif // DOT11_N_SUPPORT //

	NdisZeroMemory(&pAd->DrsCounters, sizeof(COUNTER_DRS));

	NdisGetSystemUpTime(&Now);
	pAd->StaCfg.LastBeaconRxTime = Now;   // last RX timestamp

	if ((pAd->CommonCfg.TxPreamble != Rt802_11PreambleLong) &&
		CAP_IS_SHORT_PREAMBLE_ON(pAd->StaActive.CapabilityInfo))
	{
		MlmeSetTxPreamble(pAd, Rt802_11PreambleShort);
	}

	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);

	if (pAd->CommonCfg.RadarDetect.RDMode == RD_SILENCE_MODE)
	{
#ifdef DFS_SUPPORT
		RadarDetectionStop(pAd);
#endif // DFS_SUPPORT //
	}
	pAd->CommonCfg.RadarDetect.RDMode = RD_NORMAL_MODE;

	if (BssType == BSS_ADHOC)
	{
		MakeIbssBeacon(pAd);
		if ((pAd->CommonCfg.Channel > 14)
			&& (pAd->CommonCfg.bIEEE80211H == 1)
			&& RadarChannelCheck(pAd, pAd->CommonCfg.Channel))
		{
			; //Do nothing
		}
		else
		{
			AsicEnableIbssSync(pAd);
		}

		// In ad hoc mode, use MAC table from index 1.
		// p.s ASIC use all 0xff as termination of WCID table search.To prevent it's 0xff-ff-ff-ff-ff-ff, Write 0 here.
		RTMP_IO_WRITE32(pAd, MAC_WCID_BASE, 0x00);
		RTMP_IO_WRITE32(pAd, 0x1808, 0x00);

		// If WEP is enabled, add key material and cipherAlg into Asic
		// Fill in Shared Key Table(offset: 0x6c00) and Shared Key Mode(offset: 0x7000)

		if (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)
		{
			PUCHAR	Key;
			UCHAR	CipherAlg;

			for (idx=0; idx < SHARE_KEY_NUM; idx++)
		{
				CipherAlg = pAd->SharedKey[BSS0][idx].CipherAlg;
			Key = pAd->SharedKey[BSS0][idx].Key;

				if (pAd->SharedKey[BSS0][idx].KeyLen > 0)
				{
					// Set key material and cipherAlg to Asic
				AsicAddSharedKeyEntry(pAd, BSS0, idx, CipherAlg, Key, NULL, NULL);

                    if (idx == pAd->StaCfg.DefaultKeyId)
					{
						// Update WCID attribute table and IVEIV table for this group key table
						RTMPAddWcidAttributeEntry(pAd, BSS0, idx, CipherAlg, NULL);
					}
				}


			}
		}
		// If WPANone is enabled, add key material and cipherAlg into Asic
		// Fill in Shared Key Table(offset: 0x6c00) and Shared Key Mode(offset: 0x7000)
		else if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone)
		{
			pAd->StaCfg.DefaultKeyId = 0;	// always be zero

            NdisZeroMemory(&pAd->SharedKey[BSS0][0], sizeof(CIPHER_KEY));
							pAd->SharedKey[BSS0][0].KeyLen = LEN_TKIP_EK;
			NdisMoveMemory(pAd->SharedKey[BSS0][0].Key, pAd->StaCfg.PMK, LEN_TKIP_EK);

            if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
            {
			NdisMoveMemory(pAd->SharedKey[BSS0][0].RxMic, &pAd->StaCfg.PMK[16], LEN_TKIP_RXMICK);
			NdisMoveMemory(pAd->SharedKey[BSS0][0].TxMic, &pAd->StaCfg.PMK[16], LEN_TKIP_TXMICK);
            }

			// Decide its ChiperAlg
			if (pAd->StaCfg.PairCipher == Ndis802_11Encryption2Enabled)
				pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_TKIP;
			else if (pAd->StaCfg.PairCipher == Ndis802_11Encryption3Enabled)
				pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_AES;
			else
            {
                DBGPRINT(RT_DEBUG_TRACE, ("Unknow Cipher (=%d), set Cipher to AES\n", pAd->StaCfg.PairCipher));
				pAd->SharedKey[BSS0][0].CipherAlg = CIPHER_AES;
            }

			// Set key material and cipherAlg to Asic
			AsicAddSharedKeyEntry(pAd,
								  BSS0,
								  0,
								  pAd->SharedKey[BSS0][0].CipherAlg,
								  pAd->SharedKey[BSS0][0].Key,
								  pAd->SharedKey[BSS0][0].TxMic,
								  pAd->SharedKey[BSS0][0].RxMic);

            // Update WCID attribute table and IVEIV table for this group key table
			RTMPAddWcidAttributeEntry(pAd, BSS0, 0, pAd->SharedKey[BSS0][0].CipherAlg, NULL);

		}

	}
	else // BSS_INFRA
	{
		// Check the new SSID with last SSID
		while (Cancelled == TRUE)
		{
			if (pAd->CommonCfg.LastSsidLen == pAd->CommonCfg.SsidLen)
			{
				if (RTMPCompareMemory(pAd->CommonCfg.LastSsid, pAd->CommonCfg.Ssid, pAd->CommonCfg.LastSsidLen) == 0)
				{
					// Link to the old one no linkdown is required.
					break;
				}
			}
			// Send link down event before set to link up
			pAd->IndicateMediaState = NdisMediaStateDisconnected;
			RTMP_IndicateMediaState(pAd);
            pAd->ExtraInfo = GENERAL_LINK_DOWN;
			DBGPRINT(RT_DEBUG_TRACE, ("NDIS_STATUS_MEDIA_DISCONNECT Event AA!\n"));
			break;
		}

		//
		// On WPA mode, Remove All Keys if not connect to the last BSSID
		// Key will be set after 4-way handshake.
		//
		if (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA)
		{
			ULONG		IV;

			// Remove all WPA keys
			RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP);
			RTMPWPARemoveAllKeys(pAd);
			pAd->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
			pAd->StaCfg.PrivacyFilter = Ndis802_11PrivFilter8021xWEP;

			// Fixed connection failed with Range Maximizer - 515 AP (Marvell Chip) when security is WPAPSK/TKIP
			// If IV related values are too large in GroupMsg2, AP would ignore this message.
			IV = 1;
			IV |= (pAd->StaCfg.DefaultKeyId << 30);
			AsicUpdateWCIDIVEIV(pAd, BSSID_WCID, IV, 0);
			//RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP);
		}

		// NOTE:
		// the decision of using "short slot time" or not may change dynamically due to
		// new STA association to the AP. so we have to decide that upon parsing BEACON, not here

		// NOTE:
		// the decision to use "RTC/CTS" or "CTS-to-self" protection or not may change dynamically
		// due to new STA association to the AP. so we have to decide that upon parsing BEACON, not here

		ComposePsPoll(pAd);
		ComposeNullFrame(pAd);

			AsicEnableBssSync(pAd);

		// Add BSSID to WCID search table
		AsicUpdateRxWCIDTable(pAd, BSSID_WCID, pAd->CommonCfg.Bssid);

		// If WEP is enabled, add paiewise and shared key
#ifdef WPA_SUPPLICANT_SUPPORT
        if (((pAd->StaCfg.WpaSupplicantUP)&&
             (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)&&
             (pAd->StaCfg.PortSecured == WPA_802_1X_PORT_SECURED)) ||
            ((pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE)&&
              (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)))
#else
		if (pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled)
#endif // WPA_SUPPLICANT_SUPPORT //
		{
			PUCHAR	Key;
			UCHAR	CipherAlg;

			for (idx=0; idx < SHARE_KEY_NUM; idx++)
		{
				CipherAlg = pAd->SharedKey[BSS0][idx].CipherAlg;
			Key = pAd->SharedKey[BSS0][idx].Key;

				if (pAd->SharedKey[BSS0][idx].KeyLen > 0)
				{
					// Set key material and cipherAlg to Asic
				AsicAddSharedKeyEntry(pAd, BSS0, idx, CipherAlg, Key, NULL, NULL);

					if (idx == pAd->StaCfg.DefaultKeyId)
					{
						// Assign group key info
						RTMPAddWcidAttributeEntry(pAd, BSS0, idx, CipherAlg, NULL);

						pEntry->Aid = BSSID_WCID;
						// Assign pairwise key info
						RTMPAddWcidAttributeEntry(pAd, BSS0, idx, CipherAlg, pEntry);
					}
				}
			}
		}

		// only INFRASTRUCTURE mode need to indicate connectivity immediately; ADHOC mode
		// should wait until at least 2 active nodes in this BSSID.
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);

        // For GUI ++
		if (pAd->StaCfg.AuthMode < Ndis802_11AuthModeWPA)
		{
			pAd->IndicateMediaState = NdisMediaStateConnected;
			pAd->ExtraInfo = GENERAL_LINK_UP;
			RTMP_IndicateMediaState(pAd);
		}
		else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
				 (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK))
		{
#ifdef WPA_SUPPLICANT_SUPPORT
			if (pAd->StaCfg.WpaSupplicantUP == WPA_SUPPLICANT_DISABLE)
#endif // WPA_SUPPLICANT_SUPPORT //
				RTMPSetTimer(&pAd->Mlme.LinkDownTimer, LINK_DOWN_TIMEOUT);
		}
        // --

		// Add BSSID in my MAC Table.
        NdisAcquireSpinLock(&pAd->MacTabLock);
		// add this MAC entry into HASH table
		if (pEntry)
		{
			HashIdx = MAC_ADDR_HASH_INDEX(pAd->CommonCfg.Bssid);
			if (pAd->MacTab.Hash[HashIdx] == NULL)
			{
				pAd->MacTab.Hash[HashIdx] = pEntry;
			}
			else
			{
				pCurrEntry = pAd->MacTab.Hash[HashIdx];
				while (pCurrEntry->pNext != NULL)
				{
					pCurrEntry = pCurrEntry->pNext;
				}
				pCurrEntry->pNext = pEntry;
			}
		}
		RTMPMoveMemory(pEntry->Addr, pAd->CommonCfg.Bssid, MAC_ADDR_LEN);
		pEntry->Aid = BSSID_WCID;
		pEntry->pAd = pAd;
		pEntry->ValidAsCLI = TRUE;	//Although this is bssid..still set ValidAsCl
		pAd->MacTab.Size = 1;	// infra mode always set MACtab size =1.
		pEntry->Sst = SST_ASSOC;
		pEntry->AuthState = SST_ASSOC;
		pEntry->AuthMode = pAd->StaCfg.AuthMode;
		pEntry->WepStatus = pAd->StaCfg.WepStatus;
		if (pEntry->AuthMode < Ndis802_11AuthModeWPA)
		{
			pEntry->WpaState = AS_NOTUSE;
			pEntry->PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
		}
		else
		{
			pEntry->WpaState = AS_PTKSTART;
			pEntry->PrivacyFilter = Ndis802_11PrivFilter8021xWEP;
		}
		NdisReleaseSpinLock(&pAd->MacTabLock);

		DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK UP !!!  ClientStatusFlags=%lx)\n",
			pAd->MacTab.Content[BSSID_WCID].ClientStatusFlags));


		MlmeUpdateTxRates(pAd, TRUE, BSS0);
#ifdef DOT11_N_SUPPORT
		MlmeUpdateHtTxRates(pAd, BSS0);
		DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK UP !! (StaActive.bHtEnable =%d, )\n", pAd->StaActive.SupportedPhyInfo.bHtEnable));
#endif // DOT11_N_SUPPORT //


		if (pAd->CommonCfg.bAggregationCapable)
		{
			if ((pAd->CommonCfg.bPiggyBackCapable) && (pAd->MlmeAux.APRalinkIe & 0x00000003) == 3)
			{

				OPSTATUS_SET_FLAG(pAd, fOP_STATUS_PIGGYBACK_INUSED);
				OPSTATUS_SET_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);
				CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_AGGREGATION_CAPABLE);
				CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_PIGGYBACK_CAPABLE);
				RTMPSetPiggyBack(pAd, TRUE);
				DBGPRINT(RT_DEBUG_TRACE, ("Turn on Piggy-Back\n"));
			}
			else if (pAd->MlmeAux.APRalinkIe & 0x00000001)
			{
				CLIENT_STATUS_SET_FLAG(pEntry, fCLIENT_STATUS_AGGREGATION_CAPABLE);
				OPSTATUS_SET_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);
			}
		}

		if (pAd->MlmeAux.APRalinkIe != 0x0)
		{
#ifdef DOT11_N_SUPPORT
			if (CLIENT_STATUS_TEST_FLAG(&pAd->MacTab.Content[BSSID_WCID], fCLIENT_STATUS_RDG_CAPABLE))
			{
				AsicEnableRDG(pAd);
			}
#endif // DOT11_N_SUPPORT //
			OPSTATUS_SET_FLAG(pAd, fCLIENT_STATUS_RALINK_CHIPSET);
			CLIENT_STATUS_SET_FLAG(&pAd->MacTab.Content[BSSID_WCID], fCLIENT_STATUS_RALINK_CHIPSET);
		}
		else
		{
			OPSTATUS_CLEAR_FLAG(pAd, fCLIENT_STATUS_RALINK_CHIPSET);
			CLIENT_STATUS_CLEAR_FLAG(&pAd->MacTab.Content[BSSID_WCID], fCLIENT_STATUS_RALINK_CHIPSET);
		}
	}


#ifdef DOT11_N_SUPPORT
	DBGPRINT(RT_DEBUG_TRACE, ("NDIS_STATUS_MEDIA_CONNECT Event B!.BACapability = %x. ClientStatusFlags = %lx\n", pAd->CommonCfg.BACapability.word, pAd->MacTab.Content[BSSID_WCID].ClientStatusFlags));
#endif // DOT11_N_SUPPORT //

	// Set LED
	RTMPSetLED(pAd, LED_LINK_UP);

	pAd->Mlme.PeriodicRound = 0;
	pAd->Mlme.OneSecPeriodicRound = 0;
	pAd->bConfigChanged = FALSE;        // Reset config flag
	pAd->ExtraInfo = GENERAL_LINK_UP;   // Update extra information to link is up

	// Set asic auto fall back
	{
		PUCHAR					pTable;
		UCHAR					TableSize = 0;

		MlmeSelectTxRateTable(pAd, &pAd->MacTab.Content[BSSID_WCID], &pTable, &TableSize, &pAd->CommonCfg.TxRateIndex);
		AsicUpdateAutoFallBackTable(pAd, pTable);
	}

	NdisAcquireSpinLock(&pAd->MacTabLock);
    pEntry->HTPhyMode.word = pAd->StaCfg.HTPhyMode.word;
    pEntry->MaxHTPhyMode.word = pAd->StaCfg.HTPhyMode.word;
	if (pAd->StaCfg.bAutoTxRateSwitch == FALSE)
	{
		pEntry->bAutoTxRateSwitch = FALSE;
#ifdef DOT11_N_SUPPORT
		if (pEntry->HTPhyMode.field.MCS == 32)
			pEntry->HTPhyMode.field.ShortGI = GI_800;

		if ((pEntry->HTPhyMode.field.MCS > MCS_7) || (pEntry->HTPhyMode.field.MCS == 32))
			pEntry->HTPhyMode.field.STBC = STBC_NONE;
#endif // DOT11_N_SUPPORT //
		// If the legacy mode is set, overwrite the transmit setting of this entry.
		if (pEntry->HTPhyMode.field.MODE <= MODE_OFDM)
			RTMPUpdateLegacyTxSetting((UCHAR)pAd->StaCfg.DesiredTransmitSetting.field.FixedTxMode, pEntry);
	}
	else
		pEntry->bAutoTxRateSwitch = TRUE;
	NdisReleaseSpinLock(&pAd->MacTabLock);

	//  Let Link Status Page display first initial rate.
	pAd->LastTxRate = (USHORT)(pEntry->HTPhyMode.word);
	// Select DAC according to HT or Legacy
	if (pAd->StaActive.SupportedPhyInfo.MCSSet[0] != 0x00)
	{
		RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R1, &Value);
		Value &= (~0x18);
		if (pAd->Antenna.field.TxPath == 2)
		{
		    Value |= 0x10;
		}
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R1, Value);
	}
	else
	{
		RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R1, &Value);
		Value &= (~0x18);
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R1, Value);
	}

#ifdef DOT11_N_SUPPORT
	if (pAd->StaActive.SupportedPhyInfo.bHtEnable == FALSE)
	{
	}
	else if (pEntry->MaxRAmpduFactor == 0)
	{
	    // If HT AP doesn't support MaxRAmpduFactor = 1, we need to set max PSDU to 0.
	    // Because our Init value is 1 at MACRegTable.
		RTMP_IO_WRITE32(pAd, MAX_LEN_CFG, 0x0fff);
	}
#endif // DOT11_N_SUPPORT //

	// Patch for Marvel AP to gain high throughput
	// Need to set as following,
	// 1. Set txop in register-EDCA_AC0_CFG as 0x60
	// 2. Set EnTXWriteBackDDONE in register-WPDMA_GLO_CFG as zero
	// 3. PBF_MAX_PCNT as 0x1F3FBF9F
	// 4. kick per two packets when dequeue
	//
	// Txop can only be modified when RDG is off, WMM is disable and TxBurst is enable
	//
	// if 1. Legacy AP WMM on,  or 2. 11n AP, AMPDU disable.  Force turn off burst no matter what bEnableTxBurst is.
#ifdef DOT11_N_SUPPORT
	if (!((pAd->CommonCfg.RxStream == 1)&&(pAd->CommonCfg.TxStream == 1)) &&
		(pAd->StaCfg.bForceTxBurst == FALSE) &&
		(((pAd->StaActive.SupportedPhyInfo.bHtEnable == FALSE) && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
		|| ((pAd->StaActive.SupportedPhyInfo.bHtEnable == TRUE) && (pAd->CommonCfg.BACapability.field.Policy == BA_NOTUSE))))
	{
		RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &Data);
		Data  &= 0xFFFFFF00;
		RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, Data);

		RTMP_IO_WRITE32(pAd, PBF_MAX_PCNT, 0x1F3F7F9F);
		DBGPRINT(RT_DEBUG_TRACE, ("Txburst 1\n"));
	}
	else
#endif // DOT11_N_SUPPORT //
	if (pAd->CommonCfg.bEnableTxBurst)
	{
		RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &Data);
		Data  &= 0xFFFFFF00;
		Data  |= 0x60;
		RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, Data);
		pAd->CommonCfg.IOTestParm.bNowAtherosBurstOn = TRUE;

		RTMP_IO_WRITE32(pAd, PBF_MAX_PCNT, 0x1F3FBF9F);
		DBGPRINT(RT_DEBUG_TRACE, ("Txburst 2\n"));
	}
	else
	{
		RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &Data);
		Data  &= 0xFFFFFF00;
		RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, Data);

		RTMP_IO_WRITE32(pAd, PBF_MAX_PCNT, 0x1F3F7F9F);
		DBGPRINT(RT_DEBUG_TRACE, ("Txburst 3\n"));
	}

#ifdef DOT11_N_SUPPORT
	// Re-check to turn on TX burst or not.
	if ((pAd->CommonCfg.IOTestParm.bLastAtheros == TRUE) && ((STA_WEP_ON(pAd))||(STA_TKIP_ON(pAd))))
	{
		pAd->CommonCfg.IOTestParm.bNextDisableRxBA = TRUE;
		if (pAd->CommonCfg.bEnableTxBurst)
		{
		    UINT32 MACValue = 0;
			// Force disable  TXOP value in this case. The same action in MLMEUpdateProtect too.
			// I didn't change PBF_MAX_PCNT setting.
			RTMP_IO_READ32(pAd, EDCA_AC0_CFG, &MACValue);
			MACValue  &= 0xFFFFFF00;
			RTMP_IO_WRITE32(pAd, EDCA_AC0_CFG, MACValue);
			pAd->CommonCfg.IOTestParm.bNowAtherosBurstOn = FALSE;
		}
	}
	else
	{
		pAd->CommonCfg.IOTestParm.bNextDisableRxBA = FALSE;
	}
#endif // DOT11_N_SUPPORT //

	pAd->CommonCfg.IOTestParm.bLastAtheros = FALSE;
	COPY_MAC_ADDR(pAd->CommonCfg.LastBssid, pAd->CommonCfg.Bssid);
	DBGPRINT(RT_DEBUG_TRACE, ("!!!pAd->bNextDisableRxBA= %d \n", pAd->CommonCfg.IOTestParm.bNextDisableRxBA));
	// BSSID add in one MAC entry too.  Because in Tx, ASIC need to check Cipher and IV/EIV, BAbitmap
	// Pther information in MACTab.Content[BSSID_WCID] is not necessary for driver.
	// Note: As STA, The MACTab.Content[BSSID_WCID]. PairwiseKey and Shared Key for BSS0 are the same.

	if (pAd->StaCfg.WepStatus <= Ndis802_11WEPDisabled)
	{
#ifdef WPA_SUPPLICANT_SUPPORT
		if (pAd->StaCfg.WpaSupplicantUP &&
			(pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled) &&
			(pAd->StaCfg.IEEE8021X == TRUE))
			;
		else
#endif // WPA_SUPPLICANT_SUPPORT //
		{
			pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
			pAd->StaCfg.PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
		}
	}

	NdisAcquireSpinLock(&pAd->MacTabLock);
	pEntry->PortSecured = pAd->StaCfg.PortSecured;
	NdisReleaseSpinLock(&pAd->MacTabLock);

    //
	// Patch Atheros AP TX will breakdown issue.
	// AP Model: DLink DWL-8200AP
	//
	if (INFRA_ON(pAd) && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) && STA_TKIP_ON(pAd))
	{
		RTMP_IO_WRITE32(pAd, RX_PARSER_CFG, 0x01);
	}
	else
	{
		RTMP_IO_WRITE32(pAd, RX_PARSER_CFG, 0x00);
	}

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);
	RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_GO_TO_SLEEP_NOW);

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
	if ((pAd->CommonCfg.BACapability.field.b2040CoexistScanSup) && (pAd->CommonCfg.Channel <= 11))
	{
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_SCAN_2040);
		BuildEffectedChannelList(pAd);
	}
#endif // DOT11N_DRAFT3 //
#endif // DOT11_N_SUPPORT //
}

/*
	==========================================================================

	Routine	Description:
		Disconnect current BSSID

	Arguments:
		pAd				- Pointer to our adapter
		IsReqFromAP		- Request from AP

	Return Value:
		None

	IRQL = DISPATCH_LEVEL

	Note:
		We need more information to know it's this requst from AP.
		If yes! we need to do extra handling, for example, remove the WPA key.
		Otherwise on 4-way handshaking will faied, since the WPA key didn't be
		remove while auto reconnect.
		Disconnect request from AP, it means we will start afresh 4-way handshaking
		on WPA mode.

	==========================================================================
*/
VOID LinkDown(
	IN PRTMP_ADAPTER pAd,
	IN  BOOLEAN      IsReqFromAP)
{
	UCHAR			    i, ByteValue = 0;

	BOOLEAN		Cancelled;

	// Do nothing if monitor mode is on
	if (MONITOR_ON(pAd))
		return;

#ifdef RALINK_ATE
	// Nothing to do in ATE mode.
	if (ATE_ON(pAd))
		return;
#endif // RALINK_ATE //
	RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_GO_TO_SLEEP_NOW);
	//Comment the codes, beasue the line 2291 call the same function.
	//RTMPCancelTimer(&pAd->Mlme.PsPollTimer,		&Cancelled);
	// Not allow go to sleep within linkdown function.
	RTMP_CLEAR_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP);

    if (pAd->CommonCfg.bWirelessEvent)
	{
		RTMPSendWirelessEvent(pAd, IW_STA_LINKDOWN_EVENT_FLAG, pAd->MacTab.Content[BSSID_WCID].Addr, BSS0, 0);
	}

	DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK DOWN !!!\n"));
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);

#ifdef RTMP_MAC_PCI
    if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_PCIE_DEVICE))
    {
	    BOOLEAN Cancelled;
        pAd->Mlme.bPsPollTimerRunning = FALSE;
        RTMPCancelTimer(&pAd->Mlme.PsPollTimer,	&Cancelled);
    }

	pAd->bPCIclkOff = FALSE;
#endif // RTMP_MAC_PCI //

    if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE)
||	RTMP_TEST_PSFLAG(pAd, fRTMP_PS_SET_PCI_CLK_OFF_COMMAND)
		|| RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_IDLE_RADIO_OFF))
    {
        AUTO_WAKEUP_STRUC AutoWakeupCfg;
		AsicForceWakeup(pAd, TRUE);
        AutoWakeupCfg.word = 0;
	    RTMP_IO_WRITE32(pAd, AUTO_WAKEUP_CFG, AutoWakeupCfg.word);
        OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_DOZE);
    }
#ifdef RTMP_MAC_PCI
	pAd->bPCIclkOff = FALSE;
#endif // RTMP_MAC_PCI //
	if (ADHOC_ON(pAd))		// Adhoc mode link down
	{
		DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK DOWN 1!!!\n"));

		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_ADHOC_ON);
			OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
			pAd->IndicateMediaState = NdisMediaStateDisconnected;
		RTMP_IndicateMediaState(pAd);
            pAd->ExtraInfo = GENERAL_LINK_DOWN;
			BssTableDeleteEntry(&pAd->ScanTab, pAd->CommonCfg.Bssid, pAd->CommonCfg.Channel);
		DBGPRINT(RT_DEBUG_TRACE, ("!!! MacTab.Size=%d !!!\n", pAd->MacTab.Size));
	}
	else					// Infra structure mode
	{
		DBGPRINT(RT_DEBUG_TRACE, ("!!! LINK DOWN 2!!!\n"));

#ifdef QOS_DLS_SUPPORT
		// DLS tear down frame must be sent before link down
		// send DLS-TEAR_DOWN message
		if (pAd->CommonCfg.bDLSCapable)
		{
			// tear down local dls table entry
			for (i=0; i<MAX_NUM_OF_INIT_DLS_ENTRY; i++)
			{
				if (pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status == DLS_FINISH))
				{
					pAd->StaCfg.DLSEntry[i].Status = DLS_NONE;
					RTMPSendDLSTearDownFrame(pAd, pAd->StaCfg.DLSEntry[i].MacAddr);
				}
			}

			// tear down peer dls table entry
			for (i=MAX_NUM_OF_INIT_DLS_ENTRY; i<MAX_NUM_OF_DLS_ENTRY; i++)
			{
				if (pAd->StaCfg.DLSEntry[i].Valid && (pAd->StaCfg.DLSEntry[i].Status ==  DLS_FINISH))
				{
					pAd->StaCfg.DLSEntry[i].Status = DLS_NONE;
					RTMPSendDLSTearDownFrame(pAd, pAd->StaCfg.DLSEntry[i].MacAddr);
				}
			}
		}
#endif // QOS_DLS_SUPPORT //

		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_INFRA_ON);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);

		// Saved last SSID for linkup comparison
		pAd->CommonCfg.LastSsidLen = pAd->CommonCfg.SsidLen;
		NdisMoveMemory(pAd->CommonCfg.LastSsid, pAd->CommonCfg.Ssid, pAd->CommonCfg.LastSsidLen);
		COPY_MAC_ADDR(pAd->CommonCfg.LastBssid, pAd->CommonCfg.Bssid);
		if (pAd->MlmeAux.CurrReqIsFromNdis == TRUE)
		{
			pAd->IndicateMediaState = NdisMediaStateDisconnected;
			RTMP_IndicateMediaState(pAd);
            pAd->ExtraInfo = GENERAL_LINK_DOWN;
			DBGPRINT(RT_DEBUG_TRACE, ("NDIS_STATUS_MEDIA_DISCONNECT Event A!\n"));
			pAd->MlmeAux.CurrReqIsFromNdis = FALSE;
		}
		else
		{
            //
			// If disassociation request is from NDIS, then we don't need to delete BSSID from entry.
			// Otherwise lost beacon or receive De-Authentication from AP,
			// then we should delete BSSID from BssTable.
			// If we don't delete from entry, roaming will fail.
			//
			BssTableDeleteEntry(&pAd->ScanTab, pAd->CommonCfg.Bssid, pAd->CommonCfg.Channel);
		}

		// restore back to -
		//      1. long slot (20 us) or short slot (9 us) time
		//      2. turn on/off RTS/CTS and/or CTS-to-self protection
		//      3. short preamble
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED);

#ifdef EXT_BUILD_CHANNEL_LIST
		// Country IE of the AP will be evaluated and will be used.
		if (pAd->StaCfg.IEEE80211dClientMode != Rt802_11_D_None)
		{
			NdisMoveMemory(&pAd->CommonCfg.CountryCode[0], &pAd->StaCfg.StaOriCountryCode[0], 2);
			pAd->CommonCfg.Geography = pAd->StaCfg.StaOriGeography;
			BuildChannelListEx(pAd);
		}
#endif // EXT_BUILD_CHANNEL_LIST //

	}


	for (i=1; i<MAX_LEN_OF_MAC_TABLE; i++)
	{
		if (pAd->MacTab.Content[i].ValidAsCLI == TRUE)
			MacTableDeleteEntry(pAd, pAd->MacTab.Content[i].Aid, pAd->MacTab.Content[i].Addr);
	}

	AsicSetSlotTime(pAd, TRUE); //FALSE);
	AsicSetEdcaParm(pAd, NULL);

	// Set LED
	RTMPSetLED(pAd, LED_LINK_DOWN);
    pAd->LedIndicatorStrength = 0xF0;
    RTMPSetSignalLED(pAd, -100);	// Force signal strength Led to be turned off, firmware is not done it.

		AsicDisableSync(pAd);

	pAd->Mlme.PeriodicRound = 0;
	pAd->Mlme.OneSecPeriodicRound = 0;

	if (pAd->StaCfg.BssType == BSS_INFRA)
	{
	// Remove StaCfg Information after link down
	NdisZeroMemory(pAd->CommonCfg.Bssid, MAC_ADDR_LEN);
	NdisZeroMemory(pAd->CommonCfg.Ssid, MAX_LEN_OF_SSID);
		pAd->CommonCfg.SsidLen = 0;
	}
#ifdef DOT11_N_SUPPORT
	NdisZeroMemory(&pAd->MlmeAux.HtCapability, sizeof(HT_CAPABILITY_IE));
	NdisZeroMemory(&pAd->MlmeAux.AddHtInfo, sizeof(ADD_HT_INFO_IE));
	pAd->MlmeAux.HtCapabilityLen = 0;
	pAd->MlmeAux.NewExtChannelOffset = 0xff;
#endif // DOT11_N_SUPPORT //

	// Reset WPA-PSK state. Only reset when supplicant enabled
	if (pAd->StaCfg.WpaState != SS_NOTUSE)
	{
		pAd->StaCfg.WpaState = SS_START;
		// Clear Replay counter
		NdisZeroMemory(pAd->StaCfg.ReplayCounter, 8);

#ifdef QOS_DLS_SUPPORT
		if (pAd->CommonCfg.bDLSCapable)
			NdisZeroMemory(pAd->StaCfg.DlsReplayCounter, 8);
#endif // QOS_DLS_SUPPORT //
	}

	//
	// if link down come from AP, we need to remove all WPA keys on WPA mode.
	// otherwise will cause 4-way handshaking failed, since the WPA key not empty.
	//
	if ((IsReqFromAP) && (pAd->StaCfg.AuthMode >= Ndis802_11AuthModeWPA))
	{
		// Remove all WPA keys
		RTMPWPARemoveAllKeys(pAd);
	}

	// 802.1x port control
#ifdef WPA_SUPPLICANT_SUPPORT
	// Prevent clear PortSecured here with static WEP
	// NetworkManger set security policy first then set SSID to connect AP.
	if (pAd->StaCfg.WpaSupplicantUP &&
		(pAd->StaCfg.WepStatus == Ndis802_11WEPEnabled) &&
		(pAd->StaCfg.IEEE8021X == FALSE))
	{
		pAd->StaCfg.PortSecured = WPA_802_1X_PORT_SECURED;
	}
	else
#endif // WPA_SUPPLICANT_SUPPORT //
	{
		pAd->StaCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
		pAd->StaCfg.PrivacyFilter = Ndis802_11PrivFilter8021xWEP;
	}

	NdisAcquireSpinLock(&pAd->MacTabLock);
	NdisZeroMemory(&pAd->MacTab, sizeof(MAC_TABLE));
	pAd->MacTab.Content[BSSID_WCID].PortSecured = pAd->StaCfg.PortSecured;
	NdisReleaseSpinLock(&pAd->MacTabLock);

	pAd->StaCfg.MicErrCnt = 0;

    pAd->IndicateMediaState = NdisMediaStateDisconnected;
	// Update extra information to link is up
	pAd->ExtraInfo = GENERAL_LINK_DOWN;

    pAd->StaActive.SupportedPhyInfo.bHtEnable = FALSE;


	// Clean association information
	NdisZeroMemory(&pAd->StaCfg.AssocInfo, sizeof(NDIS_802_11_ASSOCIATION_INFORMATION));
	pAd->StaCfg.AssocInfo.Length = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);
	pAd->StaCfg.ReqVarIELen = 0;
	pAd->StaCfg.ResVarIELen = 0;

	//
	// Reset RSSI value after link down
	//
	pAd->StaCfg.RssiSample.AvgRssi0 = 0;
	pAd->StaCfg.RssiSample.AvgRssi0X8 = 0;
	pAd->StaCfg.RssiSample.AvgRssi1 = 0;
	pAd->StaCfg.RssiSample.AvgRssi1X8 = 0;
	pAd->StaCfg.RssiSample.AvgRssi2 = 0;
	pAd->StaCfg.RssiSample.AvgRssi2X8 = 0;

	// Restore MlmeRate
	pAd->CommonCfg.MlmeRate = pAd->CommonCfg.BasicMlmeRate;
	pAd->CommonCfg.RtsRate = pAd->CommonCfg.BasicMlmeRate;

#ifdef DOT11_N_SUPPORT
	//
	// After Link down, reset piggy-back setting in ASIC. Disable RDG.
	//
	if (pAd->CommonCfg.BBPCurrentBW == BW_40)
	{
		pAd->CommonCfg.BBPCurrentBW = BW_20;
		RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R4, &ByteValue);
		ByteValue &= (~0x18);
		RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R4, ByteValue);
	}
#endif // DOT11_N_SUPPORT //
	// Reset DAC
	RTMP_BBP_IO_READ8_BY_REG_ID(pAd, BBP_R1, &ByteValue);
	ByteValue &= (~0x18);
	if (pAd->Antenna.field.TxPath == 2)
	{
	ByteValue |= 0x10;
	}
	RTMP_BBP_IO_WRITE8_BY_REG_ID(pAd, BBP_R1, ByteValue);

		RTMPSetPiggyBack(pAd,FALSE);
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_PIGGYBACK_INUSED);

#ifdef DOT11_N_SUPPORT
	pAd->CommonCfg.BACapability.word = pAd->CommonCfg.REGBACapability.word;
#endif // DOT11_N_SUPPORT //

	// Restore all settings in the following.
	AsicUpdateProtect(pAd, 0, (ALLN_SETPROTECT|CCKSETPROTECT|OFDMSETPROTECT), TRUE, FALSE);
	AsicDisableRDG(pAd);
	pAd->CommonCfg.IOTestParm.bCurrentAtheros = FALSE;
	pAd->CommonCfg.IOTestParm.bNowAtherosBurstOn = FALSE;

#ifdef DOT11_N_SUPPORT
#ifdef DOT11N_DRAFT3
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_SCAN_2040);
	pAd->CommonCfg.BSSCoexist2040.word = 0;
	TriEventInit(pAd);
	for (i = 0; i < (pAd->ChannelListNum - 1); i++)
	{
		pAd->ChannelList[i].bEffectedChannel = FALSE;
	}
#endif // DOT11N_DRAFT3 //
#endif // DOT11_N_SUPPORT //

	RTMP_IO_WRITE32(pAd, MAX_LEN_CFG, 0x1fff);
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);
// Allow go to sleep after linkdown steps.
	RTMP_SET_PSFLAG(pAd, fRTMP_PS_CAN_GO_SLEEP);
#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
	if (pAd->StaCfg.WpaSupplicantUP) {
		//send disassociate event to wpa_supplicant
		RtmpOSWrielessEventSend(pAd, IWEVCUSTOM, RT_DISASSOC_EVENT_FLAG, NULL, NULL, 0);
	}
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //
#endif // WPA_SUPPLICANT_SUPPORT //

#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
	RtmpOSWrielessEventSend(pAd, SIOCGIWAP, -1, NULL, NULL, 0);
#endif // NATIVE_WPA_SUPPLICANT_SUPPORT //

#ifdef RT30xx
	if ((IS_RT30xx(pAd) || IS_RT3090(pAd)||IS_RT3390(pAd))
		&&(pAd->Antenna.field.RxPath>1||pAd->Antenna.field.TxPath>1))
	{
		RTMP_ASIC_MMPS_DISABLE(pAd);
	}
#endif // RT30xx //

}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID IterateOnBssTab(
	IN PRTMP_ADAPTER pAd)
{
	MLME_START_REQ_STRUCT   StartReq;
	MLME_JOIN_REQ_STRUCT    JoinReq;
	ULONG                   BssIdx;

	// Change the wepstatus to original wepstatus
	pAd->StaCfg.WepStatus   = pAd->StaCfg.OrigWepStatus;
	pAd->StaCfg.PairCipher  = pAd->StaCfg.OrigWepStatus;
	pAd->StaCfg.GroupCipher = pAd->StaCfg.OrigWepStatus;

	BssIdx = pAd->MlmeAux.BssIdx;
	if (BssIdx < pAd->MlmeAux.SsidBssTab.BssNr)
	{
		// Check cipher suite, AP must have more secured cipher than station setting
		// Set the Pairwise and Group cipher to match the intended AP setting
		// We can only connect to AP with less secured cipher setting
		if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA) || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPAPSK))
		{
			pAd->StaCfg.GroupCipher = pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA.GroupCipher;

			if (pAd->StaCfg.WepStatus == pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA.PairCipher)
				pAd->StaCfg.PairCipher = pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA.PairCipher;
			else if (pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA.PairCipherAux != Ndis802_11WEPDisabled)
				pAd->StaCfg.PairCipher = pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA.PairCipherAux;
			else	// There is no PairCipher Aux, downgrade our capability to TKIP
				pAd->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;
		}
		else if ((pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2) || (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPA2PSK))
		{
			pAd->StaCfg.GroupCipher = pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA2.GroupCipher;

			if (pAd->StaCfg.WepStatus == pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA2.PairCipher)
				pAd->StaCfg.PairCipher = pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA2.PairCipher;
			else if (pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA2.PairCipherAux != Ndis802_11WEPDisabled)
				pAd->StaCfg.PairCipher = pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA2.PairCipherAux;
			else	// There is no PairCipher Aux, downgrade our capability to TKIP
				pAd->StaCfg.PairCipher = Ndis802_11Encryption2Enabled;

			// RSN capability
			pAd->StaCfg.RsnCapability = pAd->MlmeAux.SsidBssTab.BssEntry[BssIdx].WPA2.RsnCapability;
		}

		// Set Mix cipher flag
		pAd->StaCfg.bMixCipher = (pAd->StaCfg.PairCipher == pAd->StaCfg.GroupCipher) ? FALSE : TRUE;
		/*if (pAd->StaCfg.bMixCipher == TRUE)
		{
			// If mix cipher, re-build RSNIE
			RTMPMakeRSNIE(pAd, pAd->StaCfg.AuthMode, pAd->StaCfg.WepStatus, 0);
		}*/

		DBGPRINT(RT_DEBUG_TRACE, ("CNTL - iterate BSS %ld of %d\n", BssIdx, pAd->MlmeAux.SsidBssTab.BssNr));
		JoinParmFill(pAd, &JoinReq, BssIdx);
		MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_JOIN_REQ, sizeof(MLME_JOIN_REQ_STRUCT),
					&JoinReq);
		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_JOIN;
	}
	else if (pAd->StaCfg.BssType == BSS_ADHOC)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CNTL - All BSS fail; start a new ADHOC (Ssid=%s)...\n",pAd->MlmeAux.Ssid));
		StartParmFill(pAd, &StartReq, (PCHAR)pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
		MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_START_REQ, sizeof(MLME_START_REQ_STRUCT), &StartReq);
		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_START;
	}
	else // no more BSS
	{

#ifdef DOT11_N_SUPPORT
#endif // DOT11_N_SUPPORT //
		{
			AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);
			AsicLockChannel(pAd, pAd->CommonCfg.Channel);
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - All roaming failed, restore to channel %d, Total BSS[%02d]\n",pAd->CommonCfg.Channel, pAd->ScanTab.BssNr));
		}

		pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
	}
}

// for re-association only
// IRQL = DISPATCH_LEVEL
VOID IterateOnBssTab2(
	IN PRTMP_ADAPTER pAd)
{
	MLME_REASSOC_REQ_STRUCT ReassocReq;
	ULONG                   BssIdx;
	BSS_ENTRY               *pBss;

	BssIdx = pAd->MlmeAux.RoamIdx;
	pBss = &pAd->MlmeAux.RoamTab.BssEntry[BssIdx];

	if (BssIdx < pAd->MlmeAux.RoamTab.BssNr)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CNTL - iterate BSS %ld of %d\n", BssIdx, pAd->MlmeAux.RoamTab.BssNr));

		AsicSwitchChannel(pAd, pBss->Channel, FALSE);
		AsicLockChannel(pAd, pBss->Channel);

		// reassociate message has the same structure as associate message
		AssocParmFill(pAd, &ReassocReq, pBss->Bssid, pBss->CapabilityInfo,
					  ASSOC_TIMEOUT, pAd->StaCfg.DefaultListenCount);
		MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_REASSOC_REQ,
					sizeof(MLME_REASSOC_REQ_STRUCT), &ReassocReq);

		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_REASSOC;
	}
	else // no more BSS
	{

#ifdef DOT11_N_SUPPORT
#endif // DOT11_N_SUPPORT //
		{
			AsicSwitchChannel(pAd, pAd->CommonCfg.Channel, FALSE);
			AsicLockChannel(pAd, pAd->CommonCfg.Channel);
			DBGPRINT(RT_DEBUG_TRACE, ("CNTL - All roaming failed, restore to channel %d, Total BSS[%02d]\n",pAd->CommonCfg.Channel, pAd->ScanTab.BssNr));
		}

		pAd->Mlme.CntlMachine.CurrState = CNTL_IDLE;
	}
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID JoinParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_JOIN_REQ_STRUCT *JoinReq,
	IN ULONG BssIdx)
{
	JoinReq->BssIdx = BssIdx;
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID ScanParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_SCAN_REQ_STRUCT *ScanReq,
	IN STRING Ssid[],
	IN UCHAR SsidLen,
	IN UCHAR BssType,
	IN UCHAR ScanType)
{
    NdisZeroMemory(ScanReq->Ssid, MAX_LEN_OF_SSID);
	ScanReq->SsidLen = SsidLen;
	NdisMoveMemory(ScanReq->Ssid, Ssid, SsidLen);
	ScanReq->BssType = BssType;
	ScanReq->ScanType = ScanType;
}

#ifdef QOS_DLS_SUPPORT
/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID DlsParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_DLS_REQ_STRUCT *pDlsReq,
	IN PRT_802_11_DLS pDls,
	IN USHORT reason)
{
	pDlsReq->pDLS = pDls;
	pDlsReq->Reason = reason;
}
#endif // QOS_DLS_SUPPORT //

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID StartParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_START_REQ_STRUCT *StartReq,
	IN CHAR Ssid[],
	IN UCHAR SsidLen)
{
	ASSERT(SsidLen <= MAX_LEN_OF_SSID);
	NdisMoveMemory(StartReq->Ssid, Ssid, SsidLen);
	StartReq->SsidLen = SsidLen;
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
VOID AuthParmFill(
	IN PRTMP_ADAPTER pAd,
	IN OUT MLME_AUTH_REQ_STRUCT *AuthReq,
	IN PUCHAR pAddr,
	IN USHORT Alg)
{
	COPY_MAC_ADDR(AuthReq->Addr, pAddr);
	AuthReq->Alg = Alg;
	AuthReq->Timeout = AUTH_TIMEOUT;
}

/*
	==========================================================================
	Description:

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
#ifdef RTMP_MAC_PCI
VOID ComposePsPoll(
	IN PRTMP_ADAPTER pAd)
{
	NdisZeroMemory(&pAd->PsPollFrame, sizeof(PSPOLL_FRAME));
	pAd->PsPollFrame.FC.Type = BTYPE_CNTL;
	pAd->PsPollFrame.FC.SubType = SUBTYPE_PS_POLL;
	pAd->PsPollFrame.Aid = pAd->StaActive.Aid | 0xC000;
	COPY_MAC_ADDR(pAd->PsPollFrame.Bssid, pAd->CommonCfg.Bssid);
	COPY_MAC_ADDR(pAd->PsPollFrame.Ta, pAd->CurrentAddress);
}

// IRQL = DISPATCH_LEVEL
VOID ComposeNullFrame(
	IN PRTMP_ADAPTER pAd)
{
	NdisZeroMemory(&pAd->NullFrame, sizeof(HEADER_802_11));
	pAd->NullFrame.FC.Type = BTYPE_DATA;
	pAd->NullFrame.FC.SubType = SUBTYPE_NULL_FUNC;
	pAd->NullFrame.FC.ToDs = 1;
	COPY_MAC_ADDR(pAd->NullFrame.Addr1, pAd->CommonCfg.Bssid);
	COPY_MAC_ADDR(pAd->NullFrame.Addr2, pAd->CurrentAddress);
	COPY_MAC_ADDR(pAd->NullFrame.Addr3, pAd->CommonCfg.Bssid);
}
#endif // RTMP_MAC_PCI //




/*
	==========================================================================
	Description:
		Pre-build a BEACON frame in the shared memory

	IRQL = PASSIVE_LEVEL
	IRQL = DISPATCH_LEVEL

	==========================================================================
*/
ULONG MakeIbssBeacon(
	IN PRTMP_ADAPTER pAd)
{
	UCHAR         DsLen = 1, IbssLen = 2;
	UCHAR         LocalErpIe[3] = {IE_ERP, 1, 0x04};
	HEADER_802_11 BcnHdr;
	USHORT        CapabilityInfo;
	LARGE_INTEGER FakeTimestamp;
	ULONG         FrameLen = 0;
	PTXWI_STRUC	  pTxWI = &pAd->BeaconTxWI;
	UCHAR         *pBeaconFrame = pAd->BeaconBuf;
	BOOLEAN       Privacy;
	UCHAR         SupRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR         SupRateLen = 0;
	UCHAR         ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR         ExtRateLen = 0;
	UCHAR         RSNIe = IE_WPA;

	if ((pAd->CommonCfg.PhyMode == PHY_11B) && (pAd->CommonCfg.Channel <= 14))
	{
		SupRate[0] = 0x82; // 1 mbps
		SupRate[1] = 0x84; // 2 mbps
		SupRate[2] = 0x8b; // 5.5 mbps
		SupRate[3] = 0x96; // 11 mbps
		SupRateLen = 4;
		ExtRateLen = 0;
	}
	else if (pAd->CommonCfg.Channel > 14)
	{
		SupRate[0]  = 0x8C;    // 6 mbps, in units of 0.5 Mbps, basic rate
		SupRate[1]  = 0x12;    // 9 mbps, in units of 0.5 Mbps
		SupRate[2]  = 0x98;    // 12 mbps, in units of 0.5 Mbps, basic rate
		SupRate[3]  = 0x24;    // 18 mbps, in units of 0.5 Mbps
		SupRate[4]  = 0xb0;    // 24 mbps, in units of 0.5 Mbps, basic rate
		SupRate[5]  = 0x48;    // 36 mbps, in units of 0.5 Mbps
		SupRate[6]  = 0x60;    // 48 mbps, in units of 0.5 Mbps
		SupRate[7]  = 0x6c;    // 54 mbps, in units of 0.5 Mbps
		SupRateLen  = 8;
		ExtRateLen  = 0;

		//
		// Also Update MlmeRate & RtsRate for G only & A only
		//
		pAd->CommonCfg.MlmeRate = RATE_6;
		pAd->CommonCfg.RtsRate = RATE_6;
		pAd->CommonCfg.MlmeTransmit.field.MODE = MODE_OFDM;
		pAd->CommonCfg.MlmeTransmit.field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.MlmeRate];
		pAd->MacTab.Content[BSS0Mcast_WCID].HTPhyMode.field.MODE = MODE_OFDM;
		pAd->MacTab.Content[BSS0Mcast_WCID].HTPhyMode.field.MCS = OfdmRateToRxwiMCS[pAd->CommonCfg.MlmeRate];
	}
	else
	{
		SupRate[0] = 0x82; // 1 mbps
		SupRate[1] = 0x84; // 2 mbps
		SupRate[2] = 0x8b; // 5.5 mbps
		SupRate[3] = 0x96; // 11 mbps
		SupRateLen = 4;

		ExtRate[0]  = 0x0C;    // 6 mbps, in units of 0.5 Mbps,
		ExtRate[1]  = 0x12;    // 9 mbps, in units of 0.5 Mbps
		ExtRate[2]  = 0x18;    // 12 mbps, in units of 0.5 Mbps,
		ExtRate[3]  = 0x24;    // 18 mbps, in units of 0.5 Mbps
		ExtRate[4]  = 0x30;    // 24 mbps, in units of 0.5 Mbps,
		ExtRate[5]  = 0x48;    // 36 mbps, in units of 0.5 Mbps
		ExtRate[6]  = 0x60;    // 48 mbps, in units of 0.5 Mbps
		ExtRate[7]  = 0x6c;    // 54 mbps, in units of 0.5 Mbps
		ExtRateLen  = 8;
	}

	pAd->StaActive.SupRateLen = SupRateLen;
	NdisMoveMemory(pAd->StaActive.SupRate, SupRate, SupRateLen);
	pAd->StaActive.ExtRateLen = ExtRateLen;
	NdisMoveMemory(pAd->StaActive.ExtRate, ExtRate, ExtRateLen);

	// compose IBSS beacon frame
	MgtMacHeaderInit(pAd, &BcnHdr, SUBTYPE_BEACON, 0, BROADCAST_ADDR, pAd->CommonCfg.Bssid);
	Privacy = (pAd->StaCfg.WepStatus == Ndis802_11Encryption1Enabled) ||
			  (pAd->StaCfg.WepStatus == Ndis802_11Encryption2Enabled) ||
			  (pAd->StaCfg.WepStatus == Ndis802_11Encryption3Enabled);
	CapabilityInfo = CAP_GENERATE(0, 1, Privacy, (pAd->CommonCfg.TxPreamble == Rt802_11PreambleShort), 0, 0);

	MakeOutgoingFrame(pBeaconFrame,                &FrameLen,
					  sizeof(HEADER_802_11),           &BcnHdr,
					  TIMESTAMP_LEN,                   &FakeTimestamp,
					  2,                               &pAd->CommonCfg.BeaconPeriod,
					  2,                               &CapabilityInfo,
					  1,                               &SsidIe,
					  1,                               &pAd->CommonCfg.SsidLen,
					  pAd->CommonCfg.SsidLen,          pAd->CommonCfg.Ssid,
					  1,                               &SupRateIe,
					  1,                               &SupRateLen,
					  SupRateLen,                      SupRate,
					  1,                               &DsIe,
					  1,                               &DsLen,
					  1,                               &pAd->CommonCfg.Channel,
					  1,                               &IbssIe,
					  1,                               &IbssLen,
					  2,                               &pAd->StaActive.AtimWin,
					  END_OF_ARGS);

	// add ERP_IE and EXT_RAE IE of in 802.11g
	if (ExtRateLen)
	{
		ULONG	tmp;

		MakeOutgoingFrame(pBeaconFrame + FrameLen,         &tmp,
						  3,                               LocalErpIe,
						  1,                               &ExtRateIe,
						  1,                               &ExtRateLen,
						  ExtRateLen,                      ExtRate,
						  END_OF_ARGS);
		FrameLen += tmp;
	}

	// If adhoc secruity is set for WPA-None, append the cipher suite IE
	if (pAd->StaCfg.AuthMode == Ndis802_11AuthModeWPANone)
	{
		ULONG tmp;
        RTMPMakeRSNIE(pAd, pAd->StaCfg.AuthMode, pAd->StaCfg.WepStatus, BSS0);

		MakeOutgoingFrame(pBeaconFrame + FrameLen,		&tmp,
						  1,					&RSNIe,
						  1,					&pAd->StaCfg.RSNIE_Len,
						  pAd->StaCfg.RSNIE_Len,		pAd->StaCfg.RSN_IE,
						  END_OF_ARGS);
		FrameLen += tmp;
	}

#ifdef DOT11_N_SUPPORT
	if ((pAd->CommonCfg.PhyMode >= PHY_11ABGN_MIXED))
	{
		ULONG TmpLen;
		UCHAR HtLen, HtLen1;

#ifdef RT_BIG_ENDIAN
		HT_CAPABILITY_IE HtCapabilityTmp;
		ADD_HT_INFO_IE	addHTInfoTmp;
		USHORT	b2lTmp, b2lTmp2;
#endif

		// add HT Capability IE
		HtLen = sizeof(pAd->CommonCfg.HtCapability);
		HtLen1 = sizeof(pAd->CommonCfg.AddHTInfo);
#ifndef RT_BIG_ENDIAN
		MakeOutgoingFrame(pBeaconFrame+FrameLen,	&TmpLen,
						  1,						&HtCapIe,
						  1,						&HtLen,
						  HtLen,					&pAd->CommonCfg.HtCapability,
						  1,						&AddHtInfoIe,
						  1,						&HtLen1,
						  HtLen1,					&pAd->CommonCfg.AddHTInfo,
						  END_OF_ARGS);
#else
		NdisMoveMemory(&HtCapabilityTmp, &pAd->CommonCfg.HtCapability, HtLen);
		*(USHORT *)(&HtCapabilityTmp.HtCapInfo) = SWAP16(*(USHORT *)(&HtCapabilityTmp.HtCapInfo));
		*(USHORT *)(&HtCapabilityTmp.ExtHtCapInfo) = SWAP16(*(USHORT *)(&HtCapabilityTmp.ExtHtCapInfo));

		NdisMoveMemory(&addHTInfoTmp, &pAd->CommonCfg.AddHTInfo, HtLen1);
		*(USHORT *)(&addHTInfoTmp.AddHtInfo2) = SWAP16(*(USHORT *)(&addHTInfoTmp.AddHtInfo2));
		*(USHORT *)(&addHTInfoTmp.AddHtInfo3) = SWAP16(*(USHORT *)(&addHTInfoTmp.AddHtInfo3));

		MakeOutgoingFrame(pBeaconFrame+FrameLen,	&TmpLen,
						  1,						&HtCapIe,
						  1,						&HtLen,
						  HtLen,					&HtCapabilityTmp,
						  1,						&AddHtInfoIe,
						  1,						&HtLen1,
						  HtLen1,					&addHTInfoTmp,
						  END_OF_ARGS);
#endif
		FrameLen += TmpLen;
	}
#endif // DOT11_N_SUPPORT //

	//beacon use reserved WCID 0xff
    if (pAd->CommonCfg.Channel > 14)
    {
	RTMPWriteTxWI(pAd, pTxWI, FALSE, FALSE,  TRUE, FALSE, FALSE, TRUE, 0, 0xff, FrameLen,
		PID_MGMT, PID_BEACON, RATE_1, IFS_HTTXOP, FALSE, &pAd->CommonCfg.MlmeTransmit);
    }
    else
    {
        // Set to use 1Mbps for Adhoc beacon.
		HTTRANSMIT_SETTING Transmit;
        Transmit.word = 0;
        RTMPWriteTxWI(pAd, pTxWI, FALSE, FALSE,  TRUE, FALSE, FALSE, TRUE, 0, 0xff, FrameLen,
		PID_MGMT, PID_BEACON, RATE_1, IFS_HTTXOP, FALSE, &Transmit);
    }

#ifdef RT_BIG_ENDIAN
	RTMPFrameEndianChange(pAd, pBeaconFrame, DIR_WRITE, FALSE);
	RTMPWIEndianChange((PUCHAR)pTxWI, TYPE_TXWI);
#endif

    DBGPRINT(RT_DEBUG_TRACE, ("MakeIbssBeacon (len=%ld), SupRateLen=%d, ExtRateLen=%d, Channel=%d, PhyMode=%d\n",
					FrameLen, SupRateLen, ExtRateLen, pAd->CommonCfg.Channel, pAd->CommonCfg.PhyMode));
	return FrameLen;
}
