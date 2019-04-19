//
//  ui_def.h
//
//  Created by Rodolphe Pineau on 2019/05/19.
//  SteelDrive II X2 plugin UI  binding

// position UI
#pragma mark - position UI
#define SET_CURRENT_AS_MAX	"pushButton_4"
#define INITIATE_ZEROING	"pushButton_5"
#define SET_MAX_POS			"pushButton_6"
#define SYNC_TO_POS			"pushButton_7"
#define USE_END_STOP		"checkBox"

// position Events
#pragma mark - position Events
#define SET_CURRENT_AS_MAX_CLICKED	"on_pushButton_4_clicked"
#define INITIATE_ZEROING_CLICKED	"on_pushButton_5_clicked"
#define SET_MAX_POS_CLICKED			"on_pushButton_6_clicked"
#define SYNC_TO_POS_CLICKED			"on_pushButton_7_clicked"
#define USE_END_STOP_CLICKED		"on_checkBox_stateChanged"

// expert settings UI
#pragma mark - expert settings UI
#define SET_HOLD_CURRENT	"pushButton_8"
#define SET_MOVE_CURRENT	"pushButton_9"
#define SET_RCA_TIMING		"pushButton_10"
#define SET_RCB_TIMING		"pushButton_11"

// expert settings Events
#pragma mark - expert settings Events
#define SET_HOLD_CURRENT_CLICKED	"on_pushButton_8_clicked"
#define SET_MOVE_CURRENT_CLICKED	"on_pushButton_9_clicked"
#define SET_RCA_TIMING_CLICKED		"on_pushButton_10_clicked"
#define SET_RCB_TIMING_CLICKED		"on_pushButton_11_clicked"

// temperature settings UI
#pragma mark - temperature settings UI
#define ENABLE_TEMP_COMP		"checkBox_2"
#define SET_TEMP_SOURCE_FOC		"radioButton"
#define SET_TEMP_SOURCE_CTRL	"radioButton_2"
#define SET_TEMP_SOURCE_BOTH	"radioButton_3"
#define PAUSE_TEMP_COMP			"checkBox_3"

#define SET_TEMP_COMP_RATIO		"pushButton_13"
#define SET_TEMP_COMP_PERIOD	"pushButton_14"
#define SET_TEMP_COMP_DELTA		"pushButton_15"
#define SET_FOC_TEMP_OFFSET		"pushButton_16"
#define SET_CTRL_TEMP_OFFSET	"pushButton_17"


#define ENABLE_TEMP_PID_COMP		"checkBox_4"
#define SET_PID_TEMP_SOURCE_FOC		"radioButton_7"
#define SET_PID_TEMP_SOURCE_CTRL	"radioButton_8"
#define SET_PID_TEMP_SOURCE_BOTH	"radioButton_9"
#define SET_PWM_DEW_HEATER			"pushButton_12"

// temperature settings Events
#pragma mark - temperature settings Events
#define ENABLE_TEMP_COMP_CLICKED		"on_checkBox_2_stateChanged"
#define SET_TEMP_SOURCE_FOC_CLICKED		"on_radioButton_clicked"
#define SET_TEMP_SOURCE_CTRL_CLICKED	"on_radioButton_2_clicked"
#define SET_TEMP_SOURCE_BOTH_CLICKED	"on_radioButton_3_clicked"
#define PAUSE_TEMP_COMP_CLICKED			"on_checkBox_3_clicked"

#define SET_TEMP_COMP_RATIO_CLICKED		"on_pushButton_13_clicked"
#define SET_TEMP_COMP_PERIOD_CLICKED	"on_pushButton_14_clicked"
#define SET_TEMP_COMP_DELTA_CLICKED		"on_pushButton_15_clicked"
#define SET_FOC_TEMP_OFFSET_CLICKED		"on_pushButton_16_clicked"
#define SET_CTRL_TEMP_OFFSET_CLICKED	"on_pushButton_17_clicked"


#define ENABLE_TEMP_PID_COMP_CLICKED		"on_checkBox_4_stateChanged"
#define SET_PID_TEMP_SOURCE_FOC_CLICKED		"on_radioButton_7_clicked"
#define SET_PID_TEMP_SOURCE_CTRL_CLICKED	"on_radioButton_8_clicked"
#define SET_PID_TEMP_SOURCE_BOTH_CLICKED	"on_radioButton_9_clicked"
#define SET_PWM_DEW_HEATER_CLICKED			"on_pushButton_12_clicked"






