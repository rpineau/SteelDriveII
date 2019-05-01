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
    m_SteelDriveInfo.nLimit = 0;

	strncpy(m_szFirmwareVersion,"Not Available", SERIAL_BUFFER_SIZE);
    
    m_bAbborted = false;
    m_pSerx = NULL;

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


#pragma mark - move commands
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

#pragma mark - command complete functions

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


#pragma mark - device info
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



#pragma mark - position commands
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

int CSteelDriveII::setPosition(const int &nPosition)
{

    int nErr = BS_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    
    snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET POSITION:%d\n", nPosition);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;

    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;

    m_SteelDriveInfo.nPos = nPosition;
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

    snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET LIMIT:%d\n", nLimit);
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

int CSteelDriveII::getUseEndStop(bool &bEnable)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bEnable = false;
	nErr = SteelDriveIICommand("$BS GET USE_ENDSTOP\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			bEnable = (vFieldsData[1] == "1");
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getUseEndStop] bEnable = %s\n", timestamp, bEnable?"Yes":"No");
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setUseEndStop(const bool &bEnable)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET USE_ENDSTOP:%s\n", bEnable?"1":"0");
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::Zeroing()
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS ZEROING\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}


#pragma mark - steps commands

int CSteelDriveII::getJogSteps(int &nStep)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET JOGSTEPS\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nStep = std::stoi(vFieldsData[1]);
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getJogSteps] nStep = %d\n", timestamp, nStep);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setJogSteps(const int &nStep)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET JOGSTEPS:%d\n", nStep);
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::getSingleStep(int &nStep)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET SINGLESTEPS\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nStep = std::stoi(vFieldsData[1]);
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getSingleStep] nStep = %d\n", timestamp, nStep);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setSingleStep(const int &nStep)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET SINGLESTEPS:%d\n", nStep);
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}


#pragma mark - current commands

int CSteelDriveII::getCurrentMove(int &nValue)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET CURRENT_MOVE\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nValue = std::stoi(vFieldsData[1]);
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getCurrentMove] nValue = %d\n", timestamp, nValue);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setCurrentMove(const int &nValue)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET CURRENT_MOVE:%d\n", nValue);
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}



int CSteelDriveII::getCurrentHold(int &nValue)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET CURRENT_HOLD\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nValue = std::stoi(vFieldsData[1]);
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getCurrentHold] nValue = %d\n", timestamp, nValue);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setCurrentHold(const int &nValue)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET CURRENT_HOLD:%d\n", nValue);
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::getRCX(const char cChannel, int &nValue)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	char szCmd[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS GET RC%c\n", cChannel);
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nValue = std::stoi(vFieldsData[1]);
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getRCX] RC%c nValue = %d\n", timestamp, cChannel, nValue);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setRCX(const char cChannel, const int &nValue)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET RC%c:%d\n", cChannel, nValue);
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}

#pragma mark - temperature commands

int CSteelDriveII::getTemperature(int nSource, double &dTemperature)
{
	int nErr = SB_OK;
	double dT1,dT2;
	int nbValidSource = 0;
	dTemperature = 0;
	switch(nSource) {
		case FOCUSER:
		case CONTROLLER:
			nErr = getTemperatureFromSource(nSource, dTemperature);
			break;
		case BOTH:
			nErr = getTemperatureFromSource(FOCUSER, dT1);
			nErr = getTemperatureFromSource(CONTROLLER, dT2);
			if(dT1 != -100.0f) {
				dTemperature += dT1;
				nbValidSource ++;
			}
			if(dT2 != -100.0f) {
				dTemperature += dT2;
				nbValidSource ++;
			}
			if(nbValidSource)
				dTemperature /= nbValidSource;
			else
				dTemperature = -100.0f;
			break;
		default :
			dTemperature = -100;
			nErr = ERR_UNKNOWNCMD;
	}

#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getTemperature] source TEMP%d dTemperature = %3.2f\n", timestamp, nSource, dTemperature);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::getTemperatureFromSource(int nSource, double &dTemperature)
{
	int nErr = SB_OK;

	char szResp[SERIAL_BUFFER_SIZE];
	char szCmd[SERIAL_BUFFER_SIZE];

	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	// 0 = focuser, 1 = controller
	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS GET TEMP%d\n", nSource);

	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
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
				dTemperature = -100.0f;
			}
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getTemperatureFromSource] source TEMP%d dTemperature = %3.2f\n", timestamp, nSource, dTemperature);
	fflush(Logfile);
#endif

	return nErr;
}


int CSteelDriveII::enableTempComp(const bool &bEnable)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET TCOMP:%s\n", bEnable?"1":"0");
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::isTempCompEnable(bool &bEnable)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bEnable = false;
	nErr = SteelDriveIICommand("$BS GET TCOMP\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			bEnable = (vFieldsData[1] == "1");
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::isTempCompEnable] bEnable = %s\n", timestamp, bEnable?"Yes":"No");
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::getTempCompSensorSource(int &nSource)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET TCOMP_SENSOR\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nSource = std::stoi(vFieldsData[1]);
		}
	}
	return nErr;
}

int CSteelDriveII::setTempCompSensorSource(const int &nSource)
{
    int nErr = BS_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET TCOMP_SENSOR:%d\n", nSource);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    
    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;
    
    return nErr;
}

int CSteelDriveII::pauseTempComp(const bool &bPaused)
{
    int nErr = BS_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET TCOMP_PAUSE:%s\n", bPaused?"1":"0");
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    
    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;
    
    return nErr;
}

int CSteelDriveII::isTempCompPaused(bool &bPaused)
{
    int nErr = BS_OK;
    char szResp[SERIAL_BUFFER_SIZE];
    std::vector<std::string> vFieldsData;
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    bPaused = false;
    nErr = SteelDriveIICommand("$BS GET TCOMP_PAUSE\n", szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    
    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;
    
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        nErr = parseFields(szResp, vFieldsData, ':');
        if(nErr)
            return nErr;
        if(vFieldsData.size()>1) { // value is in 2nd field
            bPaused = (vFieldsData[1] == "1");
        }
    }
#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getUseEndStop] bPaused = %s\n", timestamp, bPaused?"Yes":"No");
    fflush(Logfile);
#endif
    
    return nErr;
}

int CSteelDriveII::setTempCompFactor(const double &dFactor)
{
    int nErr = BS_OK;
    char szCmd[SERIAL_BUFFER_SIZE];
    char szResp[SERIAL_BUFFER_SIZE];
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET TCOMP_FACTOR:%3.2f\n", dFactor);
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    
    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;
    
    return nErr;
}

int CSteelDriveII::getTempCompFactor(double &dFactor)
{
    int nErr = SB_OK;
    
    char szResp[SERIAL_BUFFER_SIZE];
    char szCmd[SERIAL_BUFFER_SIZE];
    
    std::vector<std::string> vFieldsData;
    std::vector<std::string> vNameField;
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    // 0 = focuser, 1 = controller
    snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS GET TCOMP_FACTOR\n");
    
    nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
    if(nErr)
        return nErr;
    
    if(strstr(szResp, "ERROR"))
        return ERR_CMDFAILED;
    
    if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
        nErr = parseFields(szResp, vFieldsData, ':');
        if(nErr)
            return nErr;
        if(vFieldsData.size()>1) { // temp is in 2nd field
            dFactor = std::stof(vFieldsData[1]);
        }
    }
#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getTemperatureFromSource] temp comp factor = %3.2f\n", timestamp, dFactor);
    fflush(Logfile);
#endif
    
    return nErr;
}



int CSteelDriveII::getTemperatureOffsetFromSource(int nSource, double &dTemperatureOffset)
{
	int nErr = SB_OK;

	char szResp[SERIAL_BUFFER_SIZE];
	char szCmd[SERIAL_BUFFER_SIZE];

	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	// 0 = focuser, 1 = controller
	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS GET TEMP%d_OFS\n", nSource);

	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // temp is in 2nd field
			dTemperatureOffset = std::stof(vFieldsData[1]);
			if(dTemperatureOffset == -128.0f) {
				dTemperatureOffset = -100.0f;
			}
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getTemperatureFromSource] source TEMP%d_OFS dTemperatureOffset = %3.2f\n", timestamp, nSource, dTemperatureOffset);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setTemperatureOffsetForSource(int nSource, double &dTemperatureOffset)
{
	int nErr = SB_OK;

	char szResp[SERIAL_BUFFER_SIZE];
	char szCmd[SERIAL_BUFFER_SIZE];

	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	// 0 = focuser, 1 = controller
	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET TEMP%d_OFS:%3.2f\n", nSource, dTemperatureOffset);

	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}


int CSteelDriveII::getPIDControl(bool bIsEnabled)
{
	int nErr = BS_OK;
	char szResp[SERIAL_BUFFER_SIZE];
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bIsEnabled = false;
	nErr = SteelDriveIICommand("$BS GET PID_CTRL\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			bIsEnabled = (vFieldsData[1] == "1");
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getPIDControl] bEnable = %s\n", timestamp, bIsEnabled?"Yes":"No");
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setPIDControl(const bool bEnable)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET PID_CTRL:%s\n", bEnable?"1":"0");
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::getPIDTarget(double &dTarget)
{
	int nErr = SB_OK;

	char szResp[SERIAL_BUFFER_SIZE];

	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET PID_TARGET\n", szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	if(strlen(szResp)) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(szResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // temp is in 2nd field
			dTarget = std::stof(vFieldsData[1]);
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getPIDTarget] PID Target = %3.2f\n", timestamp, dTarget);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setPIDTarget(const double &dTarget)
{
	int nErr = BS_OK;
	char szCmd[SERIAL_BUFFER_SIZE];
	char szResp[SERIAL_BUFFER_SIZE];

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	snprintf(szCmd, SERIAL_BUFFER_SIZE, "$BS SET PID_TARGET:%3.2f\n", dTarget);
	nErr = SteelDriveIICommand(szCmd, szResp, SERIAL_BUFFER_SIZE);
	if(nErr)
		return nErr;

	if(strstr(szResp, "ERROR"))
		return ERR_CMDFAILED;

	return nErr;
}


#pragma mark - command and response functions

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

