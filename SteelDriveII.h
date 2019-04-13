//
//  dmfc.h
//  NexDome
//
//  Created by Rodolphe Pineau on 2017/05/30.
//  NexDome X2 plugin

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

// #define SESTO_DEBUG 3

#define MAX_TIMEOUT 1000
#define SERIAL_BUFFER_SIZE 256
#define LOG_BUFFER_SIZE 256

enum SENSO_Errors    {SENSO_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, SENSO_BAD_CMD_RESPONSE, COMMAND_FAILED};
enum MotorStatus    {IDLE = 0, MOVING};
enum CalibrationSteps   {MANUAL_RACK_IN = 0, RACK_IN, SET_MIN, RACK_OUT, SET_MAX, CAL_DONE};

typedef struct {
    std::string     sName;
    int             nPos;
    int             nState;
    int             nLimit;
} SteelDriveInfo;

class CSteelDriveII
{
public:
    CSteelDriveII();
    ~CSteelDriveII();

    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; }

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; }
    void        SetSleeperPointer(SleeperInterface *pSleeper) {m_pSleeper = pSleeper;}
    // move commands
    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);

    // getter and setter
    void        setDebugLog(bool bEnable) {m_bDebugLog = bEnable; };

    int         getFirmwareVersion(char *pszVersion, int nStrMaxLen);
    int         getInfo(void);
    int         getDeviceName(char *pzName, int nStrMaxLen);

    int         getTemperature(double &dTemperature);

    int         getPosition(int &nPosition);

    int         syncMotorPosition(const int &nPos);

    int         getMinPosLimit(int &nLimit);
    int         setMinPosLimit(const int &nLimit);
    int         getMaxPosLimit(int &nLimit);
    int         setMaxPosLimit(const int &nLimit);
    int         setCurrentPosAsMax();

    void        getHoldCurrent(int &nValue);
    void        setHoldCurrent(const int &nValue);

    void        getRunCurrent(int &nValue);
    void        setRunCurrent(const int &nValue);

    void        getAccCurrent(int &nValue);
    void        setAccCurrent(const int &nValue);

    void        getDecCurrent(int &nValue);
    void        setDecCurrent(const int &nValue);

    void        getRunSpeed(int &nValue);
    void        setRunSpeed(const int &nValue);

    void        getAccSpeed(int &nValue);
    void        setAccSpeed(const int &nValue);

    void        getDecSpeed(int &nValue);
    void        setDecSpeed(const int &nValue);


    int         readParams(void);
    int         saveParams(void);
    int         saveParamsToMemory(void);
    int         resetToDefault(void);

    int         setLockMode(const bool &bLock);

protected:

    int             SteelDriveIICommand(const char *pszCmd, char *pszResult, int nResultMaxLen);
    int             readResponse(char *pszRespBuffer, int nBufferLen);
    int             parseFields(const char *pszIn, std::vector<std::string> &svFields, char cSeparator);

    int             disableCRC();


    SerXInterface   *m_pSerx;
    SleeperInterface    *m_pSleeper;

    bool            m_bDebugLog;
    bool            m_bIsConnected;
    char            m_szFirmwareVersion[SERIAL_BUFFER_SIZE];
    char            m_szLogBuffer[LOG_BUFFER_SIZE];

    int             m_nCurPos;
    int             m_nTargetPos;
    int             m_nMinPosLimit;
    int             m_nMaxPosLimit;
	bool			m_bAbborted;
    bool            m_bMoving;

    float           m_dTemperature;
    char            m_szDeviceName[SERIAL_BUFFER_SIZE];

    SteelDriveInfo     m_SteelDriveInfo;
    CStopWatch      cmdTimer;

#ifdef SESTO_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile
#endif

};

#endif //__SteelDriveII__
