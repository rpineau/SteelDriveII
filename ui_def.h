//
//  ui_def.h
//
//  Created by Rodolphe Pineau on 2019/05/19.
//  SteelDrive II X2 plugin UI  binding

// position UI
#pragma mark - position UI
#define INITIATE_ZEROING	"pushButton"
#define SET_MAX_POS			"pushButton_2"
#define SYNC_TO_POS			"pushButton_3"
#define USE_END_STOP		"checkBox"

// position Events
#pragma mark - position Events
#define INITIATE_ZEROING_CLICKED	"on_pushButton_clicked"
#define SET_MAX_POS_CLICKED			"on_pushButton_2_clicked"
#define SYNC_TO_POS_CLICKED			"on_pushButton_3_clicked"
#define USE_END_STOP_CLICKED		"on_checkBox_stateChanged"

// temperature settings UI
#pragma mark - temperature settings UI
#define ENABLE_TEMP_COMP		"checkBox_2"
#define SET_TEMP_SOURCE_FOC		"radioButton"
#define SET_TEMP_SOURCE_CTRL	"radioButton_2"
#define SET_TEMP_SOURCE_BOTH	"radioButton_3"
#define PAUSE_TEMP_COMP			"checkBox_3"

#define SET_TEMP_COMP_FACTOR	"pushButton_6"
#define SET_TEMP_COMP_PERIOD	"pushButton_7"
#define SET_TEMP_COMP_DELTA		"pushButton_8"
#define SET_FOC_TEMP_OFFSET		"pushButton_9"
#define SET_CTRL_TEMP_OFFSET	"pushButton_10"

#define ENABLE_TEMP_PID_COMP		"checkBox_4"
#define SET_PID_TEMP_SOURCE_FOC		"radioButton_4"
#define SET_PID_TEMP_SOURCE_CTRL	"radioButton_5"
#define SET_PID_TEMP_SOURCE_BOTH	"radioButton_6"

// dew control
#define SET_PWM_DEW_HEATER			"pushButton_4"
#define SET_DEW_TEMP_SOURCE_FOC		"radioButton_7"
#define SET_DEW_TEMP_SOURCE_CTRL	"radioButton_8"
#define SET_DEW_TEMP_OFFSET			"pushButton_5"
#define ENABLE_AUTO_DEW_COMP		"checkBox_5"

// temperature settings Events
#pragma mark - temperature settings Events
#define ENABLE_TEMP_COMP_CLICKED		"on_checkBox_2_stateChanged"
#define SET_TEMP_SOURCE_FOC_CLICKED		"on_radioButton_clicked"
#define SET_TEMP_SOURCE_CTRL_CLICKED	"on_radioButton_2_clicked"
#define SET_TEMP_SOURCE_BOTH_CLICKED	"on_radioButton_3_clicked"
#define PAUSE_TEMP_COMP_CLICKED			"on_checkBox_3_clicked"

#define SET_TEMP_COMP_FACTOR_CLICKED	"on_pushButton_6_clicked"
#define SET_TEMP_COMP_PERIOD_CLICKED	"on_pushButton_7_clicked"
#define SET_TEMP_COMP_DELTA_CLICKED		"on_pushButton_8_clicked"
#define SET_FOC_TEMP_OFFSET_CLICKED		"on_pushButton_9_clicked"
#define SET_CTRL_TEMP_OFFSET_CLICKED	"on_pushButton_10_clicked"


#define ENABLE_TEMP_PID_COMP_CLICKED		"on_checkBox_4_stateChanged"
#define SET_PID_TEMP_SOURCE_FOC_CLICKED		"on_radioButton_4_clicked"
#define SET_PID_TEMP_SOURCE_CTRL_CLICKED	"on_radioButton_5_clicked"
#define SET_PID_TEMP_SOURCE_BOTH_CLICKED	"on_radioButton_6_clicked"

// dew settings event
#define SET_PWM_DEW_HEATER_CLICKED			"on_pushButton_4_clicked"
#define SET_DEW_TEMP_SOURCE_FOC_CLICKED		"on_radioButton_7_clicked"
#define SET_DEW_TEMP_SOURCE_CTRL_CLICKED	"on_radioButton_8_clicked"
#define SET_DEW_TEMP_OFFSET_CLICKED			"on_pushButton_5_clicked"
#define ENABLE_AUTO_DEW_CLICKED				"on_checkBox_5_stateChanged"






