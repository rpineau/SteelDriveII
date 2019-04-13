//
//  nexdome.cpp
//  NexDome X2 plugin
//
//  Created by Rodolphe Pineau on 6/11/2016.


#include "SteelDriveII.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <string.h>
#include <time.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif


CSteelDriveII::CSteelDriveII()
{
    m_nTargetPos = 0;
    m_nMinPosLimit = 0;
    m_nMaxPosLimit = 2097152;
    m_bAbborted = false;
    m_pSerx = NULL;
    m_bMoving = false;
    m_dTemperature = -100.0;

    cmdTimer.Reset();

#ifdef SESTO_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\SteelDriveIILog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/SteelDriveIILog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/SteelDriveIILog.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if defined SESTO_DEBUG && SESTO_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII] Constructor Called\n", timestamp);
    fflush(Logfile);
#endif

}

CSteelDriveII::~CSteelDriveII()
{
#ifdef	SESTO_DEBUG
	// Close LogFile
	if (Logfile)
        fclose(Logfile);
#endif
}

int CSteelDriveII::Connect(const char *pszPort)
{
    int nErr = SENSO_OK;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef SESTO_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::Connect] Connecting to  %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    nErr = m_pSerx->open(pszPort, 19200, SerXInterface::B_NOPARITY);
    if(nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected) {
#ifdef SESTO_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CSteelDriveII::Connect] Error connection to %s : %d\n", timestamp, pszPort, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
#ifdef SESTO_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::Connect] connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    disableCRC();

    nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
    if(nErr)
        nErr = ERR_COMMNOLINK;

    nErr = getCurrentValues();
    nErr |= getSpeedValues();

    cmdTimer.Reset();

    return nErr;
}

void CSteelDriveII::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();

	m_bIsConnected = false;
    m_bMoving = false;
}


#pragma mark move commands
int CSteelDriveII::haltFocuser()
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    // abort
    nErr = SteelDriveIICommand("STOP", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK")) {
            return ERR_CMDFAILED;
        }

        m_bAbborted = true;
        m_bMoving = false;
    }
    return nErr;
}

int CSteelDriveII::gotoPosition(int nPos)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

#if defined SESTO_DEBUG && SESTO_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::gotoPosition] moving to %d\n", timestamp, nPos);
    fprintf(Logfile, "[%s] [CSteelDriveII::gotoPosition] m_nMinPosLimit =  %d\n", timestamp, m_nMinPosLimit);
    fprintf(Logfile, "[%s] [CSteelDriveII::gotoPosition] m_nMaxPosLimit = %d\n", timestamp, m_nMaxPosLimit);
    fflush(Logfile);
#endif

    if ( nPos < m_nMinPosLimit || nPos > m_nMaxPosLimit)
        return ERR_LIMITSEXCEEDED;

    sprintf(szCmd,"GO %d", nPos);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    // goto return the current position
    m_bMoving = true;
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK")) {
            m_nCurPos = atoi(szResp);
#if defined SESTO_DEBUG && SESTO_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CSteelDriveII::gotoPosition] m_nCurPos = %d\n", timestamp, m_nCurPos);
            fflush(Logfile);
#endif
        }

        m_nTargetPos = nPos;
    }

    return nErr;
}



int CSteelDriveII::moveRelativeToPosision(int nSteps)
{
    int nErr;

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

#if defined SESTO_DEBUG && SESTO_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::moveRelativeToPosision] moving by %d steps\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);

    return nErr;
}

#pragma mark command complete functions

int CSteelDriveII::isGoToComplete(bool &bComplete)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];


	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    bComplete = false;

    if(m_bAbborted) {
        bComplete = true;
        m_nTargetPos = m_nCurPos;
        m_bAbborted = false;
    }
    else if(m_bMoving) {
        nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
        if(nErr)
            return nErr;
        if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
            if(strstr(szResp, "OK")) {
                m_bMoving = false;
                bComplete = true;
                getPosition(m_nCurPos);
            }
            else {
                if(strlen(szResp))
                    m_nCurPos = atoi(szResp);
            }
        }
    }
    else {
        getPosition(m_nCurPos);

        if(m_nCurPos == m_nTargetPos)
            bComplete = true;
        else
            bComplete = false;
    }
    return nErr;
}


#pragma mark getters and setters

int CSteelDriveII::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
    int nErr = SENSO_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(m_bMoving) {
        strncpy(pszVersion, m_szFirmwareVersion, nStrMaxLen);
        return nErr;
    }

    nErr = getInfo();

    return nErr;
}

int CSteelDriveII::getInfo()
{

    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> svFields;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = SteelDriveIICommand("INFO", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // parse info
    nErr = parseFields(szResp, svFields, ';');
    if(nErr)
        return nErr;
    if(svFields.size()<4)
        return ERR_CMDFAILED;

    m_SteelDriveInfo.sName = svFields[0];
    m_SteelDriveInfo.nPos = std::stoi(svFields[1]);
    m_SteelDriveInfo.nState = std::stoi(svFields[2]);
    m_SteelDriveInfo.nLimit = std::stoi(svFields[3]);

    return nErr;
}

int CSteelDriveII::getDeviceName(char *pzName, int nStrMaxLen)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;
    std::vector<std::string> vNameField;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(m_bMoving)
        return nErr;

    if(m_bMoving) {
        strncpy(pzName, m_szDeviceName, SERIAL_BUFFER_SIZE);
        return nErr;
    }

    nErr = SteelDriveIICommand("#QN!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        nErr = parseFields(szResp, vFieldsData, ';');
        if(nErr)
            return nErr;
        if(vFieldsData.size()==2) { // name is in 2nd field
            nErr = parseFields(vFieldsData[1].c_str(), vNameField, '!');
            if(nErr)
                return nErr;
        }
        strncpy(pzName, vNameField[0].c_str(), nStrMaxLen);
        strncpy(m_szDeviceName, pzName, SERIAL_BUFFER_SIZE);
    }
    return nErr;
}

int CSteelDriveII::getTemperature(double &dTemperature)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(m_bMoving) {
        dTemperature = m_dTemperature;
        return nErr;
    }
    nErr = SteelDriveIICommand("#QT!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        // convert response
        dTemperature = atof(szResp);
        m_dTemperature = dTemperature;
    }
    return nErr;
}

int CSteelDriveII::getPosition(int &nPosition)
{
	int nErr = SENSO_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> svFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    // let's not hammer the controller
    if(cmdTimer.GetElapsedSeconds() < 0.1f) {
        nPosition = m_nCurPos;
        return nErr;
    }
    cmdTimer.Reset();

    nErr = SteelDriveIICommand("GET POSITION", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    // convert response
	nErr = parseFields(szResp, svFieldsData, ';');
	if(nErr)
		return nErr;
	

#if defined SESTO_DEBUG && SESTO_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getPosition] [NOT Moving] m_nCurPos = %d \n", timestamp, m_nCurPos);
    fflush(Logfile);
#endif
    return nErr;
}


int CSteelDriveII::syncMotorPosition(const int &nPos)
{
    int nErr = SENSO_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "#SP%d!", nPos);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK"))
            nErr = ERR_CMDFAILED;

        m_nCurPos = nPos;
    }
    return nErr;
}


int CSteelDriveII::getMaxPosLimit(int &nLimit)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nLimit = m_nMaxPosLimit;

    if(m_bMoving) {
        return nErr;
    }

    nErr = SteelDriveIICommand("#QM!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        // convert response
        nErr = parseFields(szResp, vFieldsData, ';');
        if(nErr)
            return nErr;
        if(vFieldsData.size()>=2) {
            nLimit = atoi(vFieldsData[1].c_str());
            m_nMaxPosLimit = nLimit;
        }
        else
            nErr = ERR_CMDFAILED;
    }

    return nErr;
}

int CSteelDriveII::setMaxPosLimit(const int &nLimit)
{
    int nErr = SENSO_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "#SM;%d!", nLimit);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK"))
            nErr = ERR_CMDFAILED;
        else
            m_nMaxPosLimit = nLimit;
    }

    return nErr;
}

int CSteelDriveII::getMinPosLimit(int &nLimit)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nLimit = m_nMinPosLimit;

    if(m_bMoving) {
        return nErr;
    }

    nErr = SteelDriveIICommand("#Qm!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        // convert response
        nErr = parseFields(szResp, vFieldsData, ';');
        if(nErr)
            return nErr;
        if(vFieldsData.size()>=2) {
            nLimit = atoi(vFieldsData[1].c_str());
            m_nMinPosLimit = nLimit;
        }
        else
            nErr = ERR_CMDFAILED;
    }

    return nErr;
}

int CSteelDriveII::setMinPosLimit(const int &nLimit)
{
    int nErr = SENSO_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];


    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "#Sm;%d!", nLimit);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK"))
            nErr = ERR_CMDFAILED;
        else
            m_nMinPosLimit = nLimit;
    }
    return nErr;
}

int CSteelDriveII::setCurrentPosAsMax()
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];


    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    nErr = SteelDriveIICommand("#SM!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK"))
            nErr = ERR_CMDFAILED;
    }
    return nErr;
}

// Current
void CSteelDriveII::getHoldCurrent(int &nValue)
{
    nValue = m_SensoParams.nHoldCurrent;
}

void CSteelDriveII::setHoldCurrent(const int &nValue)
{
    m_SensoParams.nHoldCurrent = nValue;
}

void CSteelDriveII::getRunCurrent(int &nValue)
{
    nValue = m_SensoParams.nRunCurrent;
}

void CSteelDriveII::setRunCurrent(const int &nValue)
{
    m_SensoParams.nRunCurrent = nValue;
}

void CSteelDriveII::getAccCurrent(int &nValue)
{
    nValue = m_SensoParams.nAccCurrent;
}

void CSteelDriveII::setAccCurrent(const int &nValue)
{
    m_SensoParams.nAccCurrent = nValue;
}

void CSteelDriveII::getDecCurrent(int &nValue)
{
    nValue = m_SensoParams.nDecCurrent;
}

void CSteelDriveII::setDecCurrent(const int &nValue)
{
    m_SensoParams.nDecCurrent = nValue;
}

// Sppeds
void CSteelDriveII::getRunSpeed(int &nValue)
{
    nValue = m_SensoParams.nRunSpeed;
}

void CSteelDriveII::setRunSpeed(const int &nValue)
{
    m_SensoParams.nRunSpeed = nValue;
}

void CSteelDriveII::getAccSpeed(int &nValue)
{
    nValue = m_SensoParams.nAccSpeed;
}

void CSteelDriveII::setAccSpeed(const int &nValue)
{
    m_SensoParams.nAccSpeed = nValue;
}

void CSteelDriveII::getDecSpeed(int &nValue)
{
    nValue = m_SensoParams.nDecSpeed;
}

void CSteelDriveII::setDecSpeed(const int &nValue)
{
    m_SensoParams.nDecSpeed = nValue;
}

int CSteelDriveII::readParams(void)
{
    int nErr = SENSO_OK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    nErr = getCurrentValues();
    nErr |= getSpeedValues();

    return nErr;
}

int CSteelDriveII::saveParams(void)
{
    int nErr = SENSO_OK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    nErr = setCurrentValues();
    nErr |= setSpeedValues();

    return nErr;
}

int CSteelDriveII::saveParamsToMemory(void)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    // save params to memory
    m_pSleeper->sleep(250); // make sure the controller is ready for us as it tends to fail otherwize
    nErr = SteelDriveIICommand("#PS!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK")) {
            return ERR_CMDFAILED;
        }
    }
    return nErr;
}

int CSteelDriveII::resetToDefault(void)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    // reset to default
    nErr = SteelDriveIICommand("#PD!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK")) {
            return ERR_CMDFAILED;
        }
    }
    nErr = getCurrentValues();
    nErr |= getSpeedValues();
    return nErr;
}

int CSteelDriveII::setLockMode(const bool &bLock)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    if(bLock){
        // set to free as we're no longer controlling the focuser.
        nErr = SteelDriveIICommand("#MF!", szResp, SERIAL_BUFFER_SIZE);

    }
    else {
        // set to free as we're no longer controlling the focuser.
        nErr = SteelDriveIICommand("#MF!", szResp, SERIAL_BUFFER_SIZE);
    }

    return nErr;
}


#pragma mark command and response functions

int CSteelDriveII::disableCRC()
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];

    SteelDriveIICommand("CRC_DISABLE*00", szResp, SERIAL_BUFFER_SIZE);
    return nErr;
}

int CSteelDriveII::SteelDriveIICommand(const char *pszszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszResult, 0, nResultMaxLen);

    m_pSerx->purgeTxRx();

#if defined SESTO_DEBUG && SESTO_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CSteelDriveII::SteelDriveIICommand Sending %s\n", timestamp, pszszCmd);
	fflush(Logfile);
#endif

    nErr = m_pSerx->writeFile((void *)pszszCmd, strlen(pszszCmd), ulBytesWrite);
    m_pSerx->flushTx();

    if(nErr){
        return nErr;
    }

    if(pszResult) {
            // read response
            nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
            if(nErr){
#if defined SESTO_DEBUG && SESTO_DEBUG >= 2
                ltime = time(NULL);
                timestamp = asctime(localtime(&ltime));
                timestamp[strlen(timestamp) - 1] = 0;
                fprintf(Logfile, "[%s] CSteelDriveII::SteelDriveIICommand response Error : %d\n", timestamp, nErr);
                fflush(Logfile);
#endif
                return nErr;
            }
#if defined SESTO_DEBUG && SESTO_DEBUG >= 3
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] CSteelDriveII::SteelDriveIICommand response \"%s\"\n", timestamp, szResp);
            fflush(Logfile);
#endif

        // copy response(s) to result string
        strncpy(pszResult, szResp, nResultMaxLen);
    }

    return nErr;
}

int CSteelDriveII::readResponse(char *pszRespBuffer, int nBufferLen)
{
    int nErr = SENSO_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszRespBuffer, 0, (size_t) nBufferLen);
    pszBufPtr = pszRespBuffer;

    do {
        nErr = m_pSerx->readFile(pszBufPtr, 1, ulBytesRead, MAX_TIMEOUT);
        if(nErr) {
#if defined SESTO_DEBUG && SESTO_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CSteelDriveII::readResponse] readFile Error.\n\n", timestamp);
            fflush(Logfile);
#endif
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
#if defined SESTO_DEBUG && SESTO_DEBUG >= 2
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] CSteelDriveII::readResponse timeout\n", timestamp);
			fflush(Logfile);
#endif
            nErr = ERR_NORESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
        // special case
        if (*pszBufPtr == '!') {
            pszBufPtr++;
            break;
        }
    } while (*pszBufPtr++ != '\r' && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the \n or the !

    return nErr;
}


int CSteelDriveII::parseFields(const char *pszIn, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = SENSO_OK;
    std::string sSegment;
    std::stringstream ssTmp(pszIn);

    svFields.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
        svFields.push_back(sSegment);
    }

    if(svFields.size()==0) {
        nErr = ERR_BADFORMAT;
    }
    return nErr;
}

#pragma mark parameters command

int CSteelDriveII::getCurrentValues(void)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    nErr = SteelDriveIICommand("#GC!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    nErr = parseFields(szResp, vFieldsData, ';');
    if(nErr)
        return nErr;
    if(vFieldsData.size()>=5) {
        m_SensoParams.nHoldCurrent = atoi(vFieldsData[1].c_str());
        m_SensoParams.nRunCurrent = atoi(vFieldsData[2].c_str());
        m_SensoParams.nAccCurrent = atoi(vFieldsData[3].c_str());
        m_SensoParams.nDecCurrent = atoi(vFieldsData[4].c_str());
    }
    return nErr;
}

int CSteelDriveII::getSpeedValues(void)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }


    nErr = SteelDriveIICommand("#GS!", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    nErr = parseFields(szResp, vFieldsData, ';');
    if(nErr)
        return nErr;
    if(vFieldsData.size()>=4) {
        m_SensoParams.nAccSpeed = atoi(vFieldsData[1].c_str());
        m_SensoParams.nRunSpeed = atoi(vFieldsData[2].c_str());
        m_SensoParams.nDecSpeed = atoi(vFieldsData[3].c_str());
    }
    return nErr;
}

int CSteelDriveII::setCurrentValues(void)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    m_pSleeper->sleep(250); // make sure the controller is read as sometimes it can fails if to much traffic is sent.
    snprintf(szCmd, SERIAL_BUFFER_SIZE, "#SC;%d;%d;%d;%d!", m_SensoParams.nHoldCurrent,
                                                            m_SensoParams.nRunCurrent,
                                                            m_SensoParams.nAccCurrent,
                                                            m_SensoParams.nDecCurrent );

    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp, "OK"))
        nErr = ERR_CMDFAILED;
    return nErr;
}

int CSteelDriveII::setSpeedValues(void)
{
    int nErr = SENSO_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];

    if(m_bMoving) {
        return ERR_CMD_IN_PROGRESS_FOC;
    }

    m_pSleeper->sleep(250);// make sure the controller is read as sometimes it can fails if to much traffic is sent.
    snprintf(szCmd, SERIAL_BUFFER_SIZE, "#SS;%d;%d;%d!",  m_SensoParams.nAccSpeed, m_SensoParams.nRunSpeed, m_SensoParams.nDecSpeed );
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(!strstr(szResp, "OK"))
        nErr = ERR_CMDFAILED;

    return nErr;
}
