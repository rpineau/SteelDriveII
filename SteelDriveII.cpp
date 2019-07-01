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
	m_bCrcEnabled = false;
	m_nTargetPos = 0;
    m_SteelDriveInfo.sName = "";
    m_SteelDriveInfo.nPos = 0;
    m_SteelDriveInfo.nLimit = 0;
    m_fFirmware = 0.0;

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
    std::string sDummy;
    
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

    // read hello world
    m_pSleeper->sleep(1000);
    readResponse(sDummy);
#ifdef BS_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::Connect] sDummy = '%s'\n", timestamp, sDummy.c_str());
    fflush(Logfile);
#endif
    if(m_bCrcEnabled)
        enableCRC();
    else
        disableCRC();

	nErr = getFirmwareVersion(m_sFirmwareVersion);
    nErr |= getInfo();
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
	std::string sResp;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    // abort
    nErr = SteelDriveIICommand("$BS STOP", sResp);
    if(nErr)
        return nErr;
    if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
        if(sResp.find("OK") == -1) {
            return ERR_CMDFAILED;
        }

        m_bAbborted = true;
    }
    return nErr;
}

int CSteelDriveII::gotoPosition(int nPos)
{
    int nErr;
	std::string sResp;
	std::string sCmd;

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

	sCmd = "$BS GO " + std::to_string(nPos);
	nErr = SteelDriveIICommand(sCmd, sResp);
    if(nErr)
        return nErr;

    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;

    // goto return the current position
    if(sResp.size()) {
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
int CSteelDriveII::getFirmwareVersion(std::string &sVersion)
{
    int nErr = BS_OK;
	std::string sResp;
    std::vector<std::string> svField;
    std::vector<std::string> svFirmwareFields;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = SteelDriveIICommand("$BS GET VERSION", sResp);
    if(nErr) {
		sVersion.assign("Unknown");
        m_fFirmware = 0.0;
        return BS_OK;
    }
    if(sResp.find("ERROR") != -1) {
		sVersion.assign("Unknown");
        m_fFirmware = 0.0;
        return BS_OK;
    }
    
    if(sResp.size()) {
        nErr = parseFields(sResp, svField, ':');
        if(nErr)
            return nErr;
        if(svField.size()>1) {
			sVersion.assign(svField[1]);
            parseFields(svField[1], svFirmwareFields, '(');
            if(svFirmwareFields.size()) {
                m_fFirmware = std::stof(svFirmwareFields[0]);
            }
            
        }
        
#if defined BS_DEBUG && BS_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CSteelDriveII::getFirmwareVersion] pszVersion = '%s'\n", timestamp, sVersion.c_str());
        fflush(Logfile);
#endif
    }
    return nErr;
}

int CSteelDriveII::getFirmwareVersion(float &fVersion)
{
    int nErr = BS_OK;
	std::string sVersion;
    fVersion = 0.0;

	nErr = getFirmwareVersion(sVersion);
    if(nErr)
        return nErr;
    fVersion = m_fFirmware;
    return nErr;
}

int CSteelDriveII::getInfo()
{
    int nErr = BS_OK;
	std::string sResp;
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

    nErr = SteelDriveIICommand("$BS INFO", sResp);
    if(nErr)
        return nErr;

#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getInfo] sResp = '%s'\n", timestamp, sResp.c_str());
    fflush(Logfile);
#endif

    // parse info
    nErr = parseFields(sResp, svFields, ';');
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

int CSteelDriveII::getSummary()
{
	int nErr = BS_OK;
	std::string sResp;
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

	nErr = SteelDriveIICommand("$BS SUMMARY", sResp);
	if(nErr)
		return nErr;

#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getSummary] sResp = '%s'\n", timestamp, sResp.c_str());
	fflush(Logfile);
#endif

	// parse summary
	nErr = parseFields(sResp, svFields, ';');
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

	nErr = parseFields(svFields[2], svField, ':');
	if(svField.size()>1) {
		if(svField[1] == "STOPPED")
	    	m_SteelDriveInfo.nState = STOPPED;
	}

	nErr = parseFields(svFields[3], svField, ':');
	if(svField.size()>1)
		m_SteelDriveInfo.nLimit = std::stoi(svField[1]);

	nErr = parseFields(svFields[4], svField, ':');
	if(svField.size()>1)
		m_SteelDriveInfo.nFocus = std::stoi(svField[1]);

	nErr = parseFields(svFields[5], svField, ':');
	if(svField.size()>1)
		m_SteelDriveInfo.dTemp0 = std::stod(svField[1]);

	nErr = parseFields(svFields[6], svField, ':');
	if(svField.size()>1)
		m_SteelDriveInfo.dTemp1 = std::stod(svField[1]);

	nErr = parseFields(svFields[7], svField, ':');
	if(svField.size()>1)
		m_SteelDriveInfo.dTempAve = std::stod(svField[1]);

	nErr = parseFields(svFields[8], svField, ':');
	if(svField.size()>1)
		m_SteelDriveInfo.bTcomp = (svField[1] == "0"?false:true);

	nErr = parseFields(svFields[9], svField, ':');
	if(svField.size()>1)
		m_SteelDriveInfo.nPwm = std::stoi(svField[1]);

	return nErr;

}

int CSteelDriveII::getDeviceName(std::string &sName)
{
    int nErr = BS_OK;
	std::string sResp;
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

    nErr = SteelDriveIICommand("$BS GET NAME", sResp);
    if(nErr)
        return nErr;

    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;

    
#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getDeviceName] sResp = '%s'\n", timestamp, sResp.c_str());
    fflush(Logfile);
#endif

    if(sResp.size()) {
            nErr = parseFields(sResp, vNameField, ':');
            if(nErr)
                return nErr;
        if(vNameField.size()>1) {
			sName.assign(vNameField[1]);
            m_SteelDriveInfo.sName = vNameField[1];
        }

#if defined BS_DEBUG && BS_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CSteelDriveII::getDeviceName] pzName = '%s'\n", timestamp, sName.c_str());
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
    fprintf(Logfile, "[%s] [CSteelDriveII::getPosition]  m_nCurPos = %d\n", timestamp, m_SteelDriveInfo.nPos);
    fflush(Logfile);
#endif
    return nErr;
}

int CSteelDriveII::setPosition(const int &nPosition)
{

    int nErr = BS_OK;
	std::string sCmd;
	std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;


	sCmd = "$BS SET POS:" + std::to_string(nPosition);

    nErr = SteelDriveIICommand(sCmd, sResp);
    if(nErr)
        return nErr;

    if(sResp.find("ERROR") != -1)
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
	std::string sCmd;
	std::string sResp;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

	sCmd = "$BS SET LIMIT:" + std::to_string(nLimit);
    nErr = SteelDriveIICommand(sCmd, sResp);
    if(nErr)
        return nErr;

    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;

    if(sResp.size()) {
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
	std::string sResp;

	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bEnable = false;
	nErr = SteelDriveIICommand("$BS GET USE_ENDSTOP", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sCmd;
	std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET USE_ENDSTOP:" + std::to_string(bEnable?1:0);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::Zeroing()
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS ZEROING", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}


#pragma mark - steps commands

int CSteelDriveII::getJogSteps(int &nStep)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET JOGSTEPS", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sCmd;
	std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET JOGSTEPS:" + std::to_string(nStep);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::getSingleStep(int &nStep)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET SINGLESTEPS", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET SINGLESTEPS:" + std::to_string(nStep);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}


#pragma mark - current commands

int CSteelDriveII::getCurrentMove(int &nValue)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET CURRENT_MOVE", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET CURRENT_MOVE:" + std::to_string(nValue);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}



int CSteelDriveII::getCurrentHold(int &nValue)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET CURRENT_HOLD", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET CURRENT_HOLD:" + std::to_string(nValue);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::getRCX(const char cChannel, int &nValue)
{
	int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS GET RC";
	sCmd.push_back(cChannel);

	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET RC";
	sCmd.push_back(cChannel);
	sCmd.push_back(':');
	sCmd += std::to_string(nValue);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
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
	std::string sResp;
	std::string sCmd;
	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	// 0 = focuser, 1 = controller
	sCmd = "$BS GET TEMP" + std::to_string(nSource);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET TCOMP:" + std::to_string(bEnable?1:0);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::isTempCompEnable(bool &bEnable)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bEnable = false;
	nErr = SteelDriveIICommand("$BS GET TCOMP", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::vector<std::string> vFieldsData;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET TCOMP_SENSOR", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::string sCmd;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

	sCmd = "$BS SET TCOMP_SENSOR:" + std::to_string(nSource);
    nErr = SteelDriveIICommand(sCmd, sResp);
    if(nErr)
        return nErr;
    
    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;
    
    return nErr;
}

int CSteelDriveII::getTempCompPeriod(int &nTimeMs)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET TCOMP_PERIOD", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nTimeMs = std::stoi(vFieldsData[1]);
		}
	}
	return nErr;
}

int CSteelDriveII::setTempCompPeriod(const int &nTimeMs)
{
	int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET TCOMP_PERIOD:" + std::to_string(nTimeMs);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::pauseTempComp(const bool &bPaused)
{
    int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

	sCmd = "$BS SET TCOMP_PAUSE:" + std::to_string(bPaused?1:0);
    nErr = SteelDriveIICommand(sCmd, sResp);
    if(nErr)
        return nErr;
    
    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;
    
    return nErr;
}

int CSteelDriveII::isTempCompPaused(bool &bPaused)
{
    int nErr = BS_OK;
	std::string sResp;
    std::vector<std::string> vFieldsData;
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    bPaused = false;
    nErr = SteelDriveIICommand("$BS GET TCOMP_PAUSE", sResp);
    if(nErr)
        return nErr;
    
    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;
    
    if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
        nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::string sCmd;
	std::ostringstream floatFormater;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

	floatFormater << std::setprecision(2) << dFactor;
	sCmd = "$BS SET TCOMP_FACTOR:" + floatFormater.str();

    nErr = SteelDriveIICommand(sCmd, sResp);
    if(nErr)
        return nErr;
    
    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;
    
    return nErr;
}

int CSteelDriveII::getTempCompFactor(double &dFactor)
{
    int nErr = SB_OK;
    
	std::string sResp;
    std::vector<std::string> vFieldsData;
    std::vector<std::string> vNameField;
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    // 0 = focuser, 1 = controller
    nErr = SteelDriveIICommand("$BS GET TCOMP_FACTOR", sResp);
    if(nErr)
        return nErr;
    
    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;
    
    if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
        nErr = parseFields(sResp, vFieldsData, ':');
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
    fprintf(Logfile, "[%s] [CSteelDriveII::getTempCompFactor] temp comp factor = %3.2f\n", timestamp, dFactor);
    fflush(Logfile);
#endif
    
    return nErr;
}

int CSteelDriveII::setTempCompDelta(const double &dDelta)
{
    int nErr = BS_OK;
    std::string sResp;
    std::string sCmd;
    std::ostringstream floatFormater;
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    floatFormater << std::setprecision(2) << dDelta;
    sCmd = "$BS SET TCOMP_DELTA:" + floatFormater.str();
    
    nErr = SteelDriveIICommand(sCmd, sResp);
    if(nErr)
        return nErr;
    
    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;
    
    return nErr;
}

int CSteelDriveII::getTempCompDelta(double &dDelta)
{
    int nErr = SB_OK;
    
    std::string sResp;
    std::vector<std::string> vFieldsData;
    std::vector<std::string> vNameField;
    
    if(!m_bIsConnected)
        return ERR_COMMNOLINK;
    
    // 0 = focuser, 1 = controller
    nErr = SteelDriveIICommand("$BS GET TCOMP_DELTA", sResp);
    if(nErr)
        return nErr;
    
    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;
    
    if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
        nErr = parseFields(sResp, vFieldsData, ':');
        if(nErr)
            return nErr;
        if(vFieldsData.size()>1) { // temp is in 2nd field
            dDelta = std::stof(vFieldsData[1]);
        }
    }
#if defined BS_DEBUG && BS_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CSteelDriveII::getTempCompDelta] temp comp delta = %3.2f\n", timestamp, dDelta);
    fflush(Logfile);
#endif
    
    return nErr;
}


int CSteelDriveII::getTemperatureOffsetFromSource(int nSource, double &dTemperatureOffset)
{
	int nErr = SB_OK;
	std::string sResp;
	std::string sCmd;
	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	// 0 = focuser, 1 = controller
	sCmd = "$BS GET TEMP" + std::to_string(nSource) + "_OFS";

	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	fprintf(Logfile, "[%s] [CSteelDriveII::getTemperatureOffsetFromSource] source TEMP%d_OFS dTemperatureOffset = %3.2f\n", timestamp, nSource, dTemperatureOffset);
	fflush(Logfile);
#endif

	return nErr;
}

int CSteelDriveII::setTemperatureOffsetForSource(int nSource, double &dTemperatureOffset)
{
	int nErr = SB_OK;
	std::string sResp;
	std::string sCmd;
	std::ostringstream floatFormater;
	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	// 0 = focuser, 1 = controller
	floatFormater << std::setprecision(2) << dTemperatureOffset;
	sCmd = "$BS SET TEMP" + std::to_string(nSource) + "_OFS:" + floatFormater.str();
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}


int CSteelDriveII::getPIDControl(bool &bIsEnabled)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bIsEnabled = false;
	nErr = SteelDriveIICommand("$BS GET PID_CTRL", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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

int CSteelDriveII::setPIDControl(const bool &bEnable)
{
	int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET PID_CTRL:" + std::to_string(bEnable?1:0);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::getPIDTarget(double &dTarget)
{
	int nErr = SB_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET PID_TARGET", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	std::string sResp;
	std::string sCmd;
	std::ostringstream floatFormater;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	floatFormater << std::setprecision(2) << dTarget;
	sCmd = "$BS SET PID_TARGET:" + floatFormater.str();
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}


int CSteelDriveII::getPIDSensorSource(int &nSource)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET PID_SENSOR", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nSource = std::stoi(vFieldsData[1]);
		}
	}
	return nErr;
}


int CSteelDriveII::setPiDSensorSource(const int &nSource)
{
	int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET PID_SENSOR:" + std::to_string(nSource);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}


#pragma mark - Dew control

int CSteelDriveII::getPWM(int &nValue)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET PWM", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nValue = std::stoi(vFieldsData[1]);
		}
	}
	return nErr;
}

int CSteelDriveII::setPWM(const int &nValue)
{
	int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET PWM:" + std::to_string(nValue);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}



int CSteelDriveII::getTempAmbienSensorSource(int &nSource)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET AMBIENT_SENSOR", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // value is in 2nd field
			nSource = std::stoi(vFieldsData[1]);
		}
	}
	return nErr;
}

int CSteelDriveII::setTempAmbienSensorSource(const int &nSource)
{
	int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET AMBIENT_SENSOR:" + std::to_string(nSource);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::getPidDewTemperatureOffset(double &dOffset)
{
	int nErr = SB_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;
	std::vector<std::string> vNameField;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	nErr = SteelDriveIICommand("$BS GET PID_DEW_OFS", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
		if(nErr)
			return nErr;
		if(vFieldsData.size()>1) { // temp is in 2nd field
			dOffset = std::stof(vFieldsData[1]);
		}
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::getPidDewTemperatureOffset] PID dew offset = %3.2f\n", timestamp, dOffset);
	fflush(Logfile);
#endif

	return nErr;
}


int CSteelDriveII::setPidDewTemperatureOffset(const double &dOffset)
{
	int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;
	std::ostringstream floatFormater;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	floatFormater << std::setprecision(2) << dOffset;
	sCmd = "$BS SET PID_DEW_OFS:" + floatFormater.str();
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}


int CSteelDriveII::enableAutoDew(const bool &bEnable)
{
	int nErr = BS_OK;
	std::string sResp;
	std::string sCmd;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	sCmd = "$BS SET AUTO_DEW:" + std::to_string(bEnable?1:0);
	nErr = SteelDriveIICommand(sCmd, sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	return nErr;
}

int CSteelDriveII::isAutoDewEnable(bool &bEnable)
{
	int nErr = BS_OK;
	std::string sResp;
	std::vector<std::string> vFieldsData;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	bEnable = false;
	nErr = SteelDriveIICommand("$BS GET AUTO_DEW", sResp);
	if(nErr)
		return nErr;

	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;

	if(sResp.size()) { // sometimes we don't get the reply but "\r" with no data
		nErr = parseFields(sResp, vFieldsData, ':');
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
	fprintf(Logfile, "[%s] [CSteelDriveII::isAutoDewEnable] bEnable = %s\n", timestamp, bEnable?"Yes":"No");
	fflush(Logfile);
#endif

	return nErr;
}



#pragma mark - command and response functions

int CSteelDriveII::disableCRC()
{
    int nErr = BS_OK;
    std::string sResp;

	// this command doesn't need CRC so set the flag to false before sending it.
	m_bCrcEnabled = false;
	SteelDriveIICommand("$BS CRC_DISABLE", sResp);
    if(sResp.find("ERROR") != -1)
        return ERR_CMDFAILED;
	return nErr;
}

int CSteelDriveII::enableCRC()
{
	int nErr = BS_OK;
	std::string sResp;

    m_bCrcEnabled = false;
	SteelDriveIICommand("$BS CRC_ENABLE", sResp);
	if(sResp.find("ERROR") != -1)
		return ERR_CMDFAILED;
	m_bCrcEnabled = true;
	return nErr;
}

int CSteelDriveII::SteelDriveIICommand(std::string sCmd, std::string &sResult)
{
	int nErr = BS_OK;
	unsigned long  ulBytesWrite;
	std::string sEcho;
	std::string sResp;
	uint8_t nRespCRC = 0;
	std::vector<std::string> svField;
    
	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

	m_pSerx->purgeTxRx();

	if(m_bCrcEnabled) {
		//compute and add CRC
		std::stringstream ss;
		ss << std::hex << +crc8((uint8_t *)sCmd.c_str(), (uint8_t)sCmd.size());
		sCmd += "*" + ss.str();
	}

	// add \r\n
	sCmd+="\r\n";

#if defined BS_DEBUG && BS_DEBUG >= 3
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::SteelDriveIICommand] Sending '%s'\n", timestamp, sCmd.c_str());
	fflush(Logfile);
#endif

	nErr = m_pSerx->writeFile((void *)sCmd.c_str(), sCmd.size(), ulBytesWrite);
	m_pSerx->flushTx();

	if(nErr){
		return nErr;
	}

	// read command echo
	nErr = readResponse(sEcho);
	if(nErr)
		return nErr;
	// check echo
	sEcho = trim(sEcho," \n\r");
    sCmd = trim(sCmd," \n\r");
    if(sCmd != sEcho) {
#if defined BS_DEBUG && BS_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CSteelDriveII::SteelDriveIICommand] ECHO Error : '%s'\n", timestamp, sEcho.c_str());
        fprintf(Logfile, "[%s] [CSteelDriveII::SteelDriveIICommand] sCmd       : '%s'\n", timestamp, sCmd.c_str());
        fflush(Logfile);
#endif
        m_pSerx->purgeTxRx();
		return ECHO_ERR;
    }
	// read response
	nErr = readResponse(sResp);
	sResp = trim(sResp," \r\n");
	if(nErr){
#if defined BS_DEBUG && BS_DEBUG >= 2
		ltime = time(NULL);
		timestamp = asctime(localtime(&ltime));
		timestamp[strlen(timestamp) - 1] = 0;
		fprintf(Logfile, "[%s] [CSteelDriveII::SteelDriveIICommand] response Error : %d\n", timestamp, nErr);
		fflush(Logfile);
#endif
		return nErr;
	}
#if defined BS_DEBUG && BS_DEBUG >= 2
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] [CSteelDriveII::SteelDriveIICommand] response  : %s\n", timestamp, sResp.c_str());
	fflush(Logfile);
#endif

	// check CRC
	if(m_bCrcEnabled) {
		nErr = parseFields(sResp, svField, '*');
		if(nErr)
			return nErr;
		if(svField.size()>1) {
			nRespCRC = crc8((uint8_t *)svField[0].c_str(), (uint8_t)svField[0].size());
#if defined BS_DEBUG && BS_DEBUG >= 2
			ltime = time(NULL);
			timestamp = asctime(localtime(&ltime));
			timestamp[strlen(timestamp) - 1] = 0;
			fprintf(Logfile, "[%s] [CSteelDriveII::SteelDriveIICommand]  response CRC : %s\n", timestamp, svField[1].c_str());
			fprintf(Logfile, "[%s] [CSteelDriveII::SteelDriveIICommand] converted CRC : %x\n", timestamp, (uint8_t)std::stoul(svField[1], 0, 16));
			fprintf(Logfile, "[%s] [CSteelDriveII::SteelDriveIICommand]  computed CRC : %x\n", timestamp, nRespCRC);
			fflush(Logfile);
#endif
			if(nRespCRC != (uint8_t)std::stoul(svField[1], nullptr, 16)) {
				return CRC_ERROR;
			}
			// CRC is ok, let's just return the response
			sResp.assign(svField[0]);
		}
		else {
			// there was no CRC !!
			return NO_CRC;
		}
	}

    // copy response(s) to result string
	sResult.assign(sResp);

	m_pSerx->purgeTxRx();   // purge data we don't want.

	return nErr;
}

int CSteelDriveII::readResponse(std::string &RespBuffer)
{
    int nErr = BS_OK;
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
	char pszRespBuffer[SERIAL_BUFFER_SIZE];
	char *pszBufPtr;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    memset(pszRespBuffer, 0, (size_t) SERIAL_BUFFER_SIZE);
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
			fprintf(Logfile, "[%s] [CSteelDriveII::readResponse] timeout\n", timestamp);
			fflush(Logfile);
#endif
            nErr = ERR_NORESPONSE;
            break;
        }
        ulTotalBytesRead += ulBytesRead;
    } while (*pszBufPtr++ != '\n' && ulTotalBytesRead < SERIAL_BUFFER_SIZE );

	RespBuffer.assign(pszRespBuffer);
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

/**
 CRC8 (Maxim/Dallas) calculation
 \brief Calculate CRC8 checksum based on lookup table
 \param data pointer to data array
 \param size size of data array
 \returns CRC8 checksum
*/

uint8_t CSteelDriveII::crc8(uint8_t * data, uint8_t size)
{
	uint8_t crc = 0;
	for ( uint8_t i = 0; i < size; ++i ){
		crc = crc_array[data[i] ^ crc ];
	}
	return crc;
}

