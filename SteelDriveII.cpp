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
    m_SteelDriveInfo.sName = "";
    m_SteelDriveInfo.nPos = 0;
    m_SteelDriveInfo.nState = STOPPED;
    m_SteelDriveInfo.nLimit = 0;
    m_SteelDriveInfo.nHoldCurrent = 0;
    m_SteelDriveInfo.nMoveCurrent = 0;
    
    strncpy(m_szFirmwareVersion,"Not Available", SERIAL_BUFFER_SIZE);
    
    m_bAbborted = false;
    m_pSerx = NULL;
    m_dTemperature = -100.0;

    cmdTimer.Reset();

#ifdef BS_DEBUG
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

#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII] Constructor Called\n", timestamp);
    fflush(Logfile);
#endif

}

CSteelDriveII::~CSteelDriveII()
{
#ifdef	BS_DEBUG
	// Close LogFile
	if (Logfile)
        fclose(Logfile);
#endif
}

int CSteelDriveII::Connect(const char *pszPort)
{
    int nErr = BS_OK;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef BS_DEBUG
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
#ifdef BS_DEBUG
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CSteelDriveII::Connect] Error connection to %s : %d\n", timestamp, pszPort, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }
#ifdef BS_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::Connect] connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    disableCRC();

    // nErr = getFirmwareVersion(m_szFirmwareVersion, SERIAL_BUFFER_SIZE);
    nErr = getInfo();
    if(nErr)
        nErr = ERR_COMMNOLINK;

    cmdTimer.Reset();

    return nErr;
}

void CSteelDriveII::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();

	m_bIsConnected = false;
}


#pragma mark move commands
int CSteelDriveII::haltFocuser()
{
    int nErr;
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    // abort
    nErr = SteelDriveIICommand("$BS STOP\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        if(!strstr(szResp, "OK")) {
            return ERR_CMDFAILED;
        }

        m_bAbborted = true;
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


#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::gotoPosition] moving to %d\n", timestamp, nPos);
    fprintf(Logfile, "[%s] [CSteelDriveII::gotoPosition] m_nMaxPosLimit = %d\n", timestamp, m_SteelDriveInfo.nLimit);
    fflush(Logfile);
#endif

    if ( nPos > m_SteelDriveInfo.nLimit)
        return ERR_LIMITSEXCEEDED;

    sprintf(szCmd,"$BS GO %d\n", nPos);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;

    // goto return the current position
    if(strlen(szResp)) {
        m_nTargetPos = nPos;
    }

    return nErr;
}



int CSteelDriveII::moveRelativeToPosision(int nSteps)
{
    int nErr;

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;


#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::moveRelativeToPosision] moving by %d steps\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    m_nTargetPos = m_SteelDriveInfo.nPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);

    return nErr;
}

#pragma mark command complete functions

int CSteelDriveII::isGoToComplete(bool &bComplete)
{
    int nErr = BS_OK;


	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    bComplete = false;
    getInfo();
    // we ignore the errors
    
    if(m_bAbborted) {
        bComplete = true;
        m_nTargetPos = m_SteelDriveInfo.nPos;
        m_bAbborted = false;
    }

    if(m_SteelDriveInfo.nPos == m_nTargetPos)
        bComplete = true;
    else
        bComplete = false;

    return nErr;
}


#pragma mark getters and setters

int CSteelDriveII::getFirmwareVersion(char *pszVersion, int nStrMaxLen)
{
    int nErr = BS_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    strncpy(pszVersion, m_szFirmwareVersion, nStrMaxLen);
    return nErr;

    return nErr;
}

int CSteelDriveII::getInfo()
{

    int nErr = BS_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> svFields;
    std::vector<std::string> svField;
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getInfo]\n", timestamp);
    fflush(Logfile);
#endif

    nErr = SteelDriveIICommand("$BS INFO\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getInfo] szResp = '%s'\n", timestamp, szResp);
    fflush(Logfile);
#endif

    // parse info
    nErr = parseFields(szResp, svFields, ';');
    if(nErr)
        return nErr;
    if(svFields.size()<4)
        return ERR_CMDFAILED;

    nErr = parseFields(svFields[0], svField, ':');
    if(svField.size()>1)
        m_SteelDriveInfo.sName = svField[1];

    nErr = parseFields(svFields[1], svField, ':');
    if(svField.size()>1)
        m_SteelDriveInfo.nPos = std::stoi(svField[1]);

    //nErr = parseFields(svFields[2], svField, ':');
    //if(svField.size()>1)
    //    m_SteelDriveInfo.nState = svField[1];

    nErr = parseFields(svFields[3], svField, ':');
    if(svField.size()>1)
        m_SteelDriveInfo.nLimit = std::stoi(svField[1]);

    return nErr;
}

int CSteelDriveII::getDeviceName(char *pzName, int nStrMaxLen)
{
    int nErr = BS_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vNameField;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getDeviceName]\n", timestamp);
    fflush(Logfile);
#endif

    nErr = SteelDriveIICommand("$BS GET NAME\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;

    
#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getDeviceName] szResp = '%s'\n", timestamp, szResp);
    fflush(Logfile);
#endif

    if(strlen(szResp)) {
            nErr = parseFields(szResp, vNameField, ':');
            if(nErr)
                return nErr;
        if(vNameField.size()>1) {
            strncpy(pzName, vNameField[1].c_str(), nStrMaxLen);
            m_SteelDriveInfo.sName = vNameField[1];
        }

#if defined BS_DEBUG && BS_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CSteelDriveII::getDeviceName] pzName = '%s'\n", timestamp, pzName);
        fflush(Logfile);
#endif
    }
    return nErr;
}

int CSteelDriveII::getTemperature(double &dTemperature)
{
    int nErr = BS_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;
    std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = SteelDriveIICommand("$BS GET TEMP0\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;

    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        nErr = parseFields(szResp, vFieldsData, ':');
        if(nErr)
            return nErr;
        if(vFieldsData.size()>1) { // temp is in 2nd field
            dTemperature = std::stof(vFieldsData[1]);
            if(dTemperature == -128.0f) {
                m_dTemperature = -100.0f;
                dTemperature = -100.0f;
            }
            else
                m_dTemperature = dTemperature;
        }
    }
#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getTemperature] m_dTemperature = %3.2f\n", timestamp, m_dTemperature);
    fflush(Logfile);
#endif

    return nErr;
}

int CSteelDriveII::getPosition(int &nPosition)
{
	int nErr = BS_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


    // let's not hammer the controller
    if(cmdTimer.GetElapsedSeconds() < 0.1f) {
        nPosition = m_SteelDriveInfo.nPos;
        return nErr;
    }
    cmdTimer.Reset();

#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getPosition]\n", timestamp);
    fflush(Logfile);
#endif
    getInfo();
    nPosition = m_SteelDriveInfo.nPos;
    
#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getPosition]  m_nCurPos = %d \n", timestamp, m_SteelDriveInfo.nPos);
    fflush(Logfile);
#endif
    return nErr;
}


int CSteelDriveII::syncMotorPosition(const int &nPos)
{
    int nErr = BS_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    
    snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET POSITION:%d\n", nPos);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;

    m_SteelDriveInfo.nPos = nPos;
    return nErr;
}


int CSteelDriveII::getMaxPosLimit(int &nLimit)
{
    int nErr = BS_OK;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = getInfo();
    if(nErr)
        return nErr;
    
    nLimit = m_SteelDriveInfo.nLimit;

    return nErr;
}

int CSteelDriveII::setMaxPosLimit(const int &nLimit)
{
    int nErr = BS_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET LIMIT:%d!", nLimit);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;

    if(strlen(szResp)) {
            m_SteelDriveInfo.nLimit = nLimit;
    }

    return nErr;
}



int CSteelDriveII::setCurrentPosAsMax()
{
    int nErr = BS_OK;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    setMaxPosLimit(m_SteelDriveInfo.nPos);
    return nErr;
}

// Current
void CSteelDriveII::getHoldCurrent(int &nValue)
{
    nValue = m_SteelDriveInfo.nHoldCurrent;
}

void CSteelDriveII::setHoldCurrent(const int &nValue)
{
    m_SteelDriveInfo.nHoldCurrent = nValue;
}


#pragma mark command and response functions

int CSteelDriveII::disableCRC()
{
    int nErr = BS_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    SteelDriveIICommand("$BS CRC_DISABLE*00\n", szResp, SERIAL_BUFFER_SIZE);
    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;
    return nErr;
}

int CSteelDriveII::SteelDriveIICommand(const char *pszszCmd, char *pszResult, int nResultMaxLen)
{
    int nErr = BS_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    unsigned long  ulBytesWrite;
    std::string sTmp;
    std::string sEcho;
    std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszResult, 0, nResultMaxLen);

    m_pSerx->purgeTxRx();

#if defined BS_DEBUG && BS_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CSteelDriveII::SteelDriveIICommand Sending '%s'\n", timestamp, pszszCmd);
	fflush(Logfile);
#endif

    sTmp.assign(pszszCmd);
    sTmp = trim(sTmp," \n\r");

    nErr = m_pSerx->writeFile((void *)pszszCmd, strlen(pszszCmd), ulBytesWrite);
    m_pSerx->flushTx();

    if(nErr){
        return nErr;
    }
    // read command echo
    nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
    sEcho.assign(szResp);
    sEcho = trim(sEcho," \n\r");
    if(pszResult) {
        sResp = "";
        while(sResp.size()==0) {
            // read response
            nErr = readResponse(szResp, SERIAL_BUFFER_SIZE);
            sResp.assign(szResp);
            sResp = trim(sResp," \r\n");
            if(nErr){
#if defined BS_DEBUG && BS_DEBUG >= 2
                ltime = time(NULL);
                timestamp = asctime(localtime(&ltime));
                timestamp[strlen(timestamp) - 1] = 0;
                fprintf(Logfile, "[%s] CSteelDriveII::SteelDriveIICommand response Error : %d\n", timestamp, nErr);
                fflush(Logfile);
#endif
                return nErr;
            }
#if defined BS_DEBUG && BS_DEBUG >= 3
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] CSteelDriveII::SteelDriveIICommand response '%s'\n", timestamp, szResp);
            fflush(Logfile);
#endif
        }

        // copy response(s) to result string
        strncpy(pszResult, sResp.c_str(), nResultMaxLen);
    }

    m_pSerx->purgeTxRx();   // purge data we don't want.

    return nErr;
}

int CSteelDriveII::readResponse(char *pszRespBuffer, int nBufferLen)
{
    int nErr = BS_OK;
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
#if defined BS_DEBUG && BS_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CSteelDriveII::readResponse] readFile Error.\n\n", timestamp);
            fflush(Logfile);
#endif
            return nErr;
        }

        if (ulBytesRead !=1) {// timeout
#if defined BS_DEBUG && BS_DEBUG >= 2
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
    } while (*pszBufPtr++ != '\n' && ulTotalBytesRead < nBufferLen );

    if(ulTotalBytesRead)
        *(pszBufPtr-1) = 0; //remove the \n or the !

    return nErr;
}


int CSteelDriveII::parseFields(const char *pszIn, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = BS_OK;
    std::string sIn(pszIn);
    
    nErr = parseFields(sIn, svFields, cSeparator);
    
    return nErr;
}

int CSteelDriveII::parseFields(std::string sIn, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = BS_OK;
    std::string sSegment;
    std::stringstream ssTmp(sIn);
    
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


std::string& CSteelDriveII::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CSteelDriveII::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CSteelDriveII::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}

