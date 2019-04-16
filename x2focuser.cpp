#include "x2focuser.h"

X2Focuser::X2Focuser(const char* pszDisplayName,
												const int& nInstanceIndex,
												SerXInterface						* pSerXIn,
												TheSkyXFacadeForDriversInterface	* pTheSkyXIn,
												SleeperInterface					* pSleeperIn,
												BasicIniUtilInterface				* pIniUtilIn,
												LoggerInterface						* pLoggerIn,
												MutexInterface						* pIOMutexIn,
												TickCountInterface					* pTickCountIn)

{
	m_pSerX							= pSerXIn;
	m_pTheSkyXForMounts				= pTheSkyXIn;
	m_pSleeper						= pSleeperIn;
	m_pIniUtil						= pIniUtilIn;
	m_pLogger						= pLoggerIn;
	m_pIOMutex						= pIOMutexIn;
	m_pTickCount					= pTickCountIn;

    m_nParity = SerXInterface::B_NOPARITY;
	m_bLinked = false;
	m_nPosition = 0;
    m_fLastTemp = -273.15f; // aboslute zero :)
	m_nTempSource = 0; // focuser

    // Read in settings
    if (m_pIniUtil) {
		m_nTempSource = m_pIniUtil->readInt(PARENT_KEY, TEMP_SOURCE, 0); // default to focuser
    }
	m_SteelDriveII.SetSerxPointer(m_pSerX);
    m_SteelDriveII.SetSleeperPointer(m_pSleeper);
}

X2Focuser::~X2Focuser()
{

	//Delete objects used through composition
	if (GetSerX())
		delete GetSerX();
	if (GetTheSkyXFacadeForDrivers())
		delete GetTheSkyXFacadeForDrivers();
	if (GetSleeper())
		delete GetSleeper();
	if (GetSimpleIniUtil())
		delete GetSimpleIniUtil();
	if (GetLogger())
		delete GetLogger();
	if (GetMutex())
		delete GetMutex();
}

#pragma mark - DriverRootInterface

int	X2Focuser::queryAbstraction(const char* pszName, void** ppVal)
{
    *ppVal = NULL;

    if (!strcmp(pszName, LinkInterface_Name))
        *ppVal = (LinkInterface*)this;

    else if (!strcmp(pszName, FocuserGotoInterface2_Name))
        *ppVal = (FocuserGotoInterface2*)this;

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, X2GUIEventInterface_Name))
        *ppVal = dynamic_cast<X2GUIEventInterface*>(this);

    else if (!strcmp(pszName, FocuserTemperatureInterface_Name))
        *ppVal = dynamic_cast<FocuserTemperatureInterface*>(this);

    else if (!strcmp(pszName, LoggerInterface_Name))
        *ppVal = GetLogger();

    else if (!strcmp(pszName, ModalSettingsDialogInterface_Name))
        *ppVal = dynamic_cast<ModalSettingsDialogInterface*>(this);

    else if (!strcmp(pszName, SerialPortParams2Interface_Name))
        *ppVal = dynamic_cast<SerialPortParams2Interface*>(this);

    return SB_OK;
}

#pragma mark - DriverInfoInterface
void X2Focuser::driverInfoDetailedInfo(BasicStringInterface& str) const
{
        str = "SteelDriveII X2 plugin by Rodolphe Pineau";
}

double X2Focuser::driverInfoVersion(void) const
{
	return DRIVER_VERSION;
}

void X2Focuser::deviceInfoNameShort(BasicStringInterface& str) const
{
    if(!m_bLinked) {
        str = "";
    }
    else {
        X2Focuser* pMe = (X2Focuser*)this;
        X2MutexLocker ml(pMe->GetMutex());
        char cName[SERIAL_BUFFER_SIZE];
        pMe->m_SteelDriveII.getDeviceName(cName, SERIAL_BUFFER_SIZE);
        str = cName;
    }
}

void X2Focuser::deviceInfoNameLong(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoDetailedDescription(BasicStringInterface& str) const
{
    deviceInfoNameShort(str);
}

void X2Focuser::deviceInfoFirmwareVersion(BasicStringInterface& str)
{

    if(!m_bLinked) {
        str="";
    }
    else {
    // get firmware version
        X2MutexLocker ml(GetMutex());
        char cFirmware[SERIAL_BUFFER_SIZE];
        m_SteelDriveII.getFirmwareVersion(cFirmware, SERIAL_BUFFER_SIZE);
        str = cFirmware;
    }
}

void X2Focuser::deviceInfoModel(BasicStringInterface& str)
{
    deviceInfoNameShort(str);
}

#pragma mark - LinkInterface
int	X2Focuser::establishLink(void)
{
    char szPort[DRIVER_MAX_STRING];
    int nErr;

    X2MutexLocker ml(GetMutex());
    // get serial port device name
    portNameOnToCharPtr(szPort,DRIVER_MAX_STRING);
    nErr = m_SteelDriveII.Connect(szPort);
    if(nErr)
        m_bLinked = false;
    else
        m_bLinked = true;


    return nErr;
}

int	X2Focuser::terminateLink(void)
{
    if(!m_bLinked)
        return SB_OK;

    X2MutexLocker ml(GetMutex());
    m_SteelDriveII.Disconnect();
    m_bLinked = false;
    return SB_OK;
}

bool X2Focuser::isLinked(void) const
{
	return m_bLinked;
}

#pragma mark - ModalSettingsDialogInterface
int	X2Focuser::initModalSettingsDialog(void)
{
    return SB_OK;
}

int	X2Focuser::execModalSettingsDialog(void)
{
    int nErr = SB_OK;
    X2ModalUIUtil uiutil(this, GetTheSkyXFacadeForDrivers());
    X2GUIInterface*					ui = uiutil.X2UI();
    X2GUIExchangeInterface*			dx = NULL;//Comes after ui is loaded
    bool bPressedOK = false;
    int nTmp;
    char szTmp[LOG_BUFFER_SIZE];

    if (NULL == ui)
        return ERR_POINTER;

    if ((nErr = ui->loadUserInterface("SteelDriveII.ui", deviceType(), m_nPrivateMulitInstanceIndex)))
        return nErr;

    if (NULL == (dx = uiutil.X2DX()))
        return ERR_POINTER;

    X2MutexLocker ml(GetMutex());

	// set controls values
    if(m_bLinked) {

        setMainDialogControlState(dx, true);

        m_SteelDriveII.getCurrentHold(nTmp);
        dx->setPropertyInt("holdCurrent", "value", nTmp);

        m_SteelDriveII.getPosition(m_nPosition);
        snprintf(szTmp, LOG_BUFFER_SIZE, "%d", m_nPosition);
        dx->setPropertyString("currentPos", "text", szTmp);

        m_SteelDriveII.getMaxPosLimit(nTmp);
        dx->setPropertyInt("maxPos", "value", nTmp);

        dx->setPropertyInt("newPos", "value", m_nPosition);

    }
    else {
        // disable unsued controls when not connected
        setMainDialogControlState(dx, false);
        dx->setPropertyInt("accelerationCurrent", "value", 0);
        dx->setPropertyInt("runCurrent", "value", 0);
        dx->setPropertyInt("decCurrent", "value", 0);
        dx->setPropertyInt("holdCurrent", "value", 0);
        dx->setPropertyInt("accelerationSpeed", "value", 0);
        dx->setPropertyInt("runSpeed", "value", 0);
        dx->setPropertyInt("decelerationSpeed", "value", 0);
        dx->setPropertyString("currentPos", "text", "");
        dx->setPropertyInt("minPos", "value", 0);
        dx->setPropertyInt("maxPos", "value", 0);
        dx->setPropertyInt("newPos", "value", 0);
    }

    //Display the user interface
    m_nCurrentDialog = MAIN;
    if ((nErr = ui->exec(bPressedOK)))
        return nErr;

    //Retreive values from the user interface
    if (bPressedOK) {
        // read current values
        dx->propertyInt("accelerationCurrent", "value", nTmp);
        dx->propertyInt("holdCurrent", "value", nTmp);
        m_SteelDriveII.setCurrentHold(nTmp);
		if(m_pIniUtil) {
			nErr = m_pIniUtil->writeInt(PARENT_KEY, TEMP_SOURCE, m_nTempSource);
		}
    }
    return nErr;
}

void X2Focuser::uiEvent(X2GUIExchangeInterface* uiex, const char* pszEvent)
{
    int nErr = SB_OK;
    int nTmp = 0;
    char szTmp[LOG_BUFFER_SIZE];

    if (!strcmp(pszEvent, "on_pushButton_6_clicked")) {
        uiex->propertyInt("maxPos", "value", nTmp);
        m_SteelDriveII.setMaxPosLimit(nTmp);
    }

    else if (!strcmp(pszEvent, "on_pushButton_7_clicked")) {
        uiex->propertyInt("newPos", "value", nTmp);
        m_SteelDriveII.setPosition(nTmp);
        snprintf(szTmp, LOG_BUFFER_SIZE, "%d", nTmp);
        uiex->setPropertyString("currentPos", "text", szTmp);
    }

    else if (!strcmp(pszEvent, "on_pushButton_8_clicked")) {
        uiex->propertyInt("holdCurrent", "value", nTmp);
        m_SteelDriveII.setCurrentHold(nTmp);
    }
}

void X2Focuser::setMainDialogControlState(X2GUIExchangeInterface* uiex, bool enabled)
{

    uiex->setEnabled("pushButton", enabled);
    uiex->setEnabled("pushButton_2", enabled);
    uiex->setEnabled("pushButton_3", enabled);
    uiex->setEnabled("pushButton_4", enabled);
    uiex->setEnabled("pushButton_5", enabled);
    uiex->setEnabled("pushButton_6", enabled);
    uiex->setEnabled("pushButton_7", enabled);
    uiex->setEnabled("pushButton_8", enabled);
    uiex->setEnabled("pushButton_9", enabled);
    uiex->setEnabled("pushButton_10", enabled);
    uiex->setEnabled("accelerationCurrent", enabled);
    uiex->setEnabled("runCurrent", enabled);
    uiex->setEnabled("decCurrent", enabled);
    uiex->setEnabled("holdCurrent", enabled);
    uiex->setEnabled("accelerationSpeed", enabled);
    uiex->setEnabled("runSpeed", enabled);
    uiex->setEnabled("decelerationSpeed", enabled);
    uiex->setEnabled("minPos", enabled);
    uiex->setEnabled("maxPos", enabled);
    uiex->setEnabled("newPos", enabled);
    uiex->setEnabled("pushButtonOK", enabled);
}



#pragma mark - FocuserGotoInterface2
int	X2Focuser::focPosition(int& nPosition)
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_SteelDriveII.getPosition(nPosition);
    m_nPosition = nPosition;

    return nErr;
}

int	X2Focuser::focMinimumLimit(int& nMinLimit)
{
    int nErr = SB_OK;

    X2MutexLocker ml(GetMutex());
    nMinLimit = 0;

    return nErr;
}

int	X2Focuser::focMaximumLimit(int& nPosLimit)
{
    int nErr = SB_OK;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_SteelDriveII.getMaxPosLimit(nPosLimit);

    return nErr;
}

int	X2Focuser::focAbort()
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_SteelDriveII.haltFocuser();

    return nErr;
}

int	X2Focuser::startFocGoto(const int& nRelativeOffset)
{
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    m_SteelDriveII.moveRelativeToPosision(nRelativeOffset);

    return SB_OK;
}

int	X2Focuser::isCompleteFocGoto(bool& bComplete) const
{
    int nErr;

    if(!m_bLinked)
        return NOT_CONNECTED;

    X2Focuser* pMe = (X2Focuser*)this;
    X2MutexLocker ml(pMe->GetMutex());
    nErr = pMe->m_SteelDriveII.isGoToComplete(bComplete);

    return nErr;
}

int	X2Focuser::endFocGoto(void)
{
    int nErr;
    if(!m_bLinked)
        return NOT_CONNECTED;

    X2MutexLocker ml(GetMutex());
    nErr = m_SteelDriveII.getPosition(m_nPosition);

    return nErr;
}

int X2Focuser::amountCountFocGoto(void) const
{
	return 6;
}

int	X2Focuser::amountNameFromIndexFocGoto(const int& nZeroBasedIndex, BasicStringInterface& strDisplayName, int& nAmount)
{
	switch (nZeroBasedIndex)
	{
		default:
		case 0: strDisplayName="10 steps"; nAmount=10;break;
		case 1: strDisplayName="100 steps"; nAmount=100;break;
		case 2: strDisplayName="1000 steps"; nAmount=1000;break;
        case 3: strDisplayName="5000 steps"; nAmount=5000;break;
        case 4: strDisplayName="7500 steps"; nAmount=7500;break;
        case 5: strDisplayName="10000 steps"; nAmount=10000;break;
	}
	return SB_OK;
}

int	X2Focuser::amountIndexFocGoto(void)
{
	return 0;
}

#pragma mark - FocuserTemperatureInterface
int X2Focuser::focTemperature(double &dTemperature)
{
    int nErr = SB_OK;

    if(!m_bLinked) {
        dTemperature = -100.0;
        return NOT_CONNECTED;
    }

    X2MutexLocker ml(GetMutex());

    // Taken from Richard's Robofocus plugin code.
    // this prevent us from asking the temperature too often
    static CStopWatch timer;

    if(timer.GetElapsedSeconds() > 30.0f || m_fLastTemp < -99.0f) {
        X2MutexLocker ml(GetMutex());
        nErr = m_SteelDriveII.getTemperature(m_nTempSource, m_fLastTemp);
        timer.Reset();
    }

    dTemperature = m_fLastTemp;

    return nErr;
}

#pragma mark - SerialPortParams2Interface

void X2Focuser::portName(BasicStringInterface& str) const
{
    char szPortName[DRIVER_MAX_STRING];

    portNameOnToCharPtr(szPortName, DRIVER_MAX_STRING);

    str = szPortName;

}

void X2Focuser::setPortName(const char* pszPort)
{
    if (m_pIniUtil)
        m_pIniUtil->writeString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort);

}


void X2Focuser::portNameOnToCharPtr(char* pszPort, const int& nMaxSize) const
{
    if (NULL == pszPort)
        return;

    snprintf(pszPort, nMaxSize, DEF_PORT_NAME);

    if (m_pIniUtil)
        m_pIniUtil->readString(PARENT_KEY, CHILD_KEY_PORTNAME, pszPort, pszPort, nMaxSize);

}




