#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_NONE
};

// Native global variables

extern int32_t get_var_setpoint();
extern void set_var_setpoint(int32_t value);
extern int32_t get_var_current_temp();
extern void set_var_current_temp(int32_t value);
extern int32_t get_var_humidity();
extern void set_var_humidity(int32_t value);
extern const char *get_var_pressure_string();
extern void set_var_pressure_string(const char *value);
extern int32_t get_var_mode();
extern void set_var_mode(int32_t value);
extern int32_t get_var_wifi_connected();
extern void set_var_wifi_connected(int32_t value);
extern const char *get_var_ip_address();
extern void set_var_ip_address(const char *value);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/