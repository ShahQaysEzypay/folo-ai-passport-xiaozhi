#ifndef ALARMS_H
#define ALARMS_H

#include <string>
#include <ctime>
#include <lvgl.h>

class Alarms {
public:
    static Alarms& GetInstance() {
        static Alarms instance;
        return instance;
    }
    Alarms(const Alarms&) = delete;
    Alarms& operator=(const Alarms&) = delete;

    void Initialize();
    void BuildUi();
    void Show(bool show);
    void UpdateUi();

    // Button interactions inside Alarms page
    bool HandleButtonUp();
    bool HandleButtonDown();
    bool HandleButtonOk();

    // Check every second in tick
    void CheckAlarms();

    // Modal popup dismiss
    bool IsAlertActive() const { return alert_active_; }
    void DismissAlert();

    bool IsDailyAlarmEnabled() const { return daily_alarm_enabled_; }
    int GetDailyAlarmHour() const { return daily_alarm_hour_; }
    int GetDailyAlarmMinute() const { return daily_alarm_minute_; }
    bool IsPrayerAlertEnabled() const { return prayer_alert_enabled_; }

private:
    Alarms() = default;
    ~Alarms() = default;

    void LoadSettings();
    void SaveSettings();
    void TriggerAlert(const std::string& title, const std::string& message);
    void BuildAlertModal();

    bool daily_alarm_enabled_ = true;
    int daily_alarm_hour_ = 7;
    int daily_alarm_minute_ = 0;
    bool prayer_alert_enabled_ = true;

    int last_triggered_min_ = -1;
    bool alert_active_ = false;
    int alert_ring_ticks_ = 0;

    // UI state
    int selected_row_ = 0;
    enum EditMode { kSelectRow, kEditHour, kEditMinute } edit_mode_ = kSelectRow;

    // LVGL widgets
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_title_ = nullptr;
    lv_obj_t* header_sub_ = nullptr;
    struct RowWidget {
        lv_obj_t* row = nullptr;
        lv_obj_t* label = nullptr;
        lv_obj_t* value = nullptr;
    };
    RowWidget rows_[5]; // 0: Daily Alarm, 1: Time, 2: Prayer Alert, 3: Test Sound, 4: Back
    lv_obj_t* footer_label_ = nullptr;

    // Alert modal
    lv_obj_t* modal_root_ = nullptr;
    lv_obj_t* modal_card_ = nullptr;
    lv_obj_t* modal_title_ = nullptr;
    lv_obj_t* modal_desc_ = nullptr;
    lv_obj_t* modal_hint_ = nullptr;
};

#endif // ALARMS_H
