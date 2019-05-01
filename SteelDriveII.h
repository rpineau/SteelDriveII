//
//  SteelDriveII.h
//
//  Created by Rodolphe Pineau on 2019/04/01.
//  SteelDrive II X2 plugin

#ifndef __SteelDriveII__
#define __SteelDriveII__
#include <math.h>
#include <string.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

#include "StopWatch.h"

#define BS_DEBUG 3

#define MAX_TIMEOUT 1000
#define SERIAL_BUFFER_SIZE 256
#define LOG_BUFFER_SIZE 256

enum BS_Errors		{BS_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, BS_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum TempSourses	{FOCUSER = 0, CONTROLLER, BOTH};

typedef struct {
    std::string     sName;
    int             nPos;
    int             nLimit;
} SteelDriveInfo;

class CSteelDriveII
{
public:
    CSteelDriveII();
    ~CSteelDriveII();

    int		Connect(const char *pszPort);
    void	Disconnect(void);
    bool	IsConnected(void) { return m_bIsConnected; }

    void	SetSerxPointer(SerXInterface *p) { m_pSerx = p; }
    void	SetSleeperPointer(SleeperInterface *pSleeper) {m_pSleeper = pSleeper;}
    // move commands
    int		haltFocuser();
    int		gotoPosition(int nPos);
    int		moveRelativeToPosision(int nSteps);

    // command complete functions
    int		isGoToComplete(bool &bComplete);

    // getter and setter
    void	setDebugLog(bool bEnable) {m_bDebugLog = bEnable; };

    int		getFirmwareVersion(char *pszVersion, int nStrMaxLen);
    int		getInfo(void);
    int		getDeviceName(char *pzName, int nStrMaxLen);

    int		getPosition(int &nPosition);
	int		setPosition(const int &nPosition);

    int		getMaxPosLimit(int &nLimit);
    int		setMaxPosLimit(const int &nLimit);
    int		setCurrentPosAsMax();

	int		getUseEndStop(bool &bEnable);
	int		setUseEndStop(const bool &bEnable);

	int		Zeroing();

	int		getJogSteps(int &nStep);
	int		setJogSteps(const int &nStep);

	int		getSingleStep(int &nStep);
	int		setSingleStep(const int &nStep);

	int		getCurrentMove(int &nValue);
	int		setCurrentMove(const int &nValue);

	int		getCurrentHold(int &nValue);
	int		setCurrentHold(const int &nValue);

	int		getRCX(const char cChannel, int &nValue);
	int		setRCX(const char cChannel,const int &nValue);

	int		getTemperatureFromSource(int nSource, double &dTemperature);
	int		getTemperature(int nSource, double &dTemperature);

	int		enableTempComp(const bool &bEnable);
	int		isTempCompEnable(bool &bEnable);

	int		getTempCompSensorSource(int &nSource);
	int		setTempCompSensorSource(const int &nSource);

    int     pauseTempComp(const bool &bPaused);
    int     isTempCompPaused(bool &bPaused);
    
    int     getTempCompFactor(double &dFactor);
    int     setTempCompFactor(const double &dFactor);

	int		getTemperatureOffsetFromSource(int nSource, double &dTemperatureOffset);
	int		setTemperatureOffsetForSource(int nSource, double &dTemperatureOffset);

	int		getPIDControl(bool bIsEnabled);
	int		setPIDControl(const bool bEnable);

	int     getPIDTarget(double &dTarget);
	int     setPIDTarget(const double &dTarget);


protected:

    int				SteelDriveIICommand(const char *pszCmd, char *pszResult, int nResultMaxLen);
    int				readResponse(char *pszRespBuffer, int nBufferLen);
    int				parseFields(const char *pszIn, std::vector<std::string> &svFields, char cSeparator);
    int				parseFields(std::string sIn, std::vector<std::string> &svFields, char cSeparator);
    std::string&	trim(std::string &str, const std::string &filter );
    std::string&	ltrim(std::string &str, const std::string &filter);
    std::string&	rtrim(std::string &str, const std::string &filter);
    int				disableCRC();


    SerXInterface   	*m_pSerx;
    SleeperInterface    *m_pSleeper;

    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];

    int             m_nTargetPos;
	bool			m_bAbborted;

    SteelDriveInfo	m_SteelDriveInfo;
    CStopWatch		cmdTimer;

#ifdef BS_DEBUG
    std::string		m_sLogfilePath;
    char			*timestamp;
    time_t			ltime;
    FILE			*Logfile;
#endif

};

#endif //__SteelDriveII__
