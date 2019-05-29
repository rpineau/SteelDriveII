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

enum BS_Errors		{BS_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, BS_BAD_CMD_RESPONSE, COMMAND_FAILED, NO_CRC, CRC_ERROR};
enum TempSourses	{FOCUSER = 0, CONTROLLER, BOTH};
enum FocState		{STOPPED = 0};

typedef struct {
    std::string     sName;
    int             nPos;
    int             nLimit;
	int				nState;
	int				nFocus;
	double			dTemp0;
	double			dTemp1;
	double			dTempAve;
	bool			bTcomp;
	int				nPwm;
} SteelDriveInfo;

/** CRC lookup table */
const uint8_t crc_array[256] = {
	0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83,
	0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
	0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e,
	0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
	0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0,
	0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62,
	0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d,
	0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
	0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5,
	0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07,
	0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58,
	0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
	0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6,
	0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
	0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b,
	0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9,
	0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f,
	0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
	0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92,
	0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50,
	0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c,
	0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee,
	0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1,
	0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
	0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49,
	0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
	0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4,
	0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
	0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a,
	0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
	0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7,
	0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35,
};

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

	int		getTempCompPeriod(int &nTimeMs);
	int		setTempCompPeriod(const int &nTimeMs);

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

	int		getPIDSensorSource(int &nSource);
	int		setPiDSensorSource(const int &nSource);

	int		getPWM(int &nValue);
	int		setPWM(const int &nValue);

	int		getTempAmbienSensorSource(int &nSource);
	int		setTempAmbienSensorSource(const int &nSource);

	int		getPidDewTemperatureOffset(double &dOffset);
	int		setPidDewTemperatureOffset(const double &dOffset);

	int		enableAutoDew(const bool &bEnable);
	int		isAutoDewEnable(bool &bEnable);

protected:

    int				SteelDriveIICommand(const char *pszCmd, char *pszResult, int nResultMaxLen);
    int				readResponse(char *pszRespBuffer, int nBufferLen);
    int				parseFields(const char *pszIn, std::vector<std::string> &svFields, char cSeparator);
    int				parseFields(std::string sIn, std::vector<std::string> &svFields, char cSeparator);
    std::string&	trim(std::string &str, const std::string &filter );
    std::string&	ltrim(std::string &str, const std::string &filter);
    std::string&	rtrim(std::string &str, const std::string &filter);
    int				disableCRC();
	int				enableCRC();
	int				getInfo(void);
	int				getSummary(void);

	uint8_t 		crc8(uint8_t * data, uint8_t size);


    SerXInterface   	*m_pSerx;
    SleeperInterface    *m_pSleeper;

	bool			m_bCrcEnabled;
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
