#include "alarms.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <esp_log.h>

#include "application.h"
#include "assets/lang_config.h"
#include "board.h"
#include "launcher.h"
#include "prayer_times.h"
#include "settings.h"
#include "display.h"
#include "display/lvgl_display/lvgl_theme.h"

#define TAG "Alarms"

void Alarms::Initialize() {
    LoadSettings();
}

void Alarms::LoadSettings() {
    Settings s("alarms", false);
    daily_alarm_enabled_ = s.GetBool("daily_en", true);
    daily_alarm_hour_ = s.GetInt("daily_hr", 7);
    daily_alarm_minute_ = s.GetInt("daily_min", 0);
    prayer_alert_enabled_ = s.GetBool("prayer_en", true);
    ESP_LOGI(TAG, "Loaded alarms: daily=%s (%02d:%02d), prayer=%s",
             daily_alarm_enabled_ ? "ON" : "OFF",
             daily_alarm_hour_, daily_alarm_minute_,
             prayer_alert_enabled_ ? "ON" : "OFF");
}

void Alarms::SaveSettings() {
    Settings s("alarms", true);
    s.SetBool("daily_en", daily_alarm_enabled_);
    s.SetInt("daily_hr", daily_alarm_hour_);
    s.SetInt("daily_min", daily_alarm_minute_);
    s.SetBool("prayer_en", prayer_alert_enabled_);
    ESP_LOGI(TAG, "Saved alarms settings to NVS");
}

void Alarms::BuildUi() {
    auto* screen = lv_screen_active();

    root_ = lv_obj_create(screen);
    lv_obj_set_size(root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x0f172a), 0); // Slate 900
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    header_title_ = lv_label_create(root_);
    lv_obj_set_width(header_title_, LV_HOR_RES);
    lv_obj_set_pos(header_title_, 0, 8);
    lv_obj_set_style_text_align(header_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header_title_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_title_, lv_color_hex(0xfbbf24), 0); // Amber 400
    lv_label_set_text(header_title_, "[ ALARMS & ALERTS ]");

    // Subtitle
    header_sub_ = lv_label_create(root_);
    lv_obj_set_width(header_sub_, LV_HOR_RES);
    lv_obj_set_pos(header_sub_, 0, 24);
    lv_obj_set_style_text_align(header_sub_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header_sub_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_sub_, lv_color_hex(0x94a3b8), 0); // Slate 400
    lv_label_set_text(header_sub_, "Daily Alarms & Prayer Alerts");

    // 5 Rows
    static const char* const kLabels[5] = {
        "Daily Alarm",
        "Alarm Time",
        "Prayer Alert",
        "Test Sound",
        "< Back to Menu"
    };

    int start_y = 50;
    for (int i = 0; i < 5; i++) {
        rows_[i].row = lv_obj_create(root_);
        lv_obj_set_size(rows_[i].row, 216, 42);
        lv_obj_set_pos(rows_[i].row, 12, start_y + i * 48);
        lv_obj_set_style_radius(rows_[i].row, 6, 0);
        lv_obj_set_style_pad_left(rows_[i].row, 10, 0);
        lv_obj_set_style_pad_right(rows_[i].row, 10, 0);
        lv_obj_set_style_pad_top(rows_[i].row, 6, 0);
        lv_obj_set_style_pad_bottom(rows_[i].row, 6, 0);
        lv_obj_set_style_bg_color(rows_[i].row, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_border_width(rows_[i].row, 1, 0);
        lv_obj_set_style_border_color(rows_[i].row, lv_color_hex(0x334155), 0);
        lv_obj_remove_flag(rows_[i].row, LV_OBJ_FLAG_SCROLLABLE);

        rows_[i].label = lv_label_create(rows_[i].row);
        lv_obj_set_style_text_font(rows_[i].label, &lv_font_unscii_8, 0);
        lv_label_set_text(rows_[i].label, kLabels[i]);
        lv_obj_set_style_text_color(rows_[i].label, lv_color_hex(0xe2e8f0), 0);
        lv_obj_align(rows_[i].label, LV_ALIGN_LEFT_MID, 0, 0);

        rows_[i].value = lv_label_create(rows_[i].row);
        lv_obj_set_style_text_font(rows_[i].value, &lv_font_unscii_8, 0);
        lv_label_set_text(rows_[i].value, "");
        lv_obj_set_style_text_color(rows_[i].value, lv_color_hex(0x38bdf8), 0);
        lv_obj_align(rows_[i].value, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // Footer
    footer_label_ = lv_label_create(root_);
    lv_obj_set_width(footer_label_, LV_HOR_RES);
    lv_obj_set_pos(footer_label_, 0, 296);
    lv_obj_set_style_text_align(footer_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(footer_label_, lv_color_hex(0x64748b), 0);
    lv_label_set_text(footer_label_, "UP/DN:Move  OK:Toggle/Edit");

    BuildAlertModal();

    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    UpdateUi();
}

void Alarms::BuildAlertModal() {
    auto* screen = lv_screen_active();

    modal_root_ = lv_obj_create(screen);
    lv_obj_set_size(modal_root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(modal_root_, 0, 0);
    lv_obj_set_style_radius(modal_root_, 0, 0);
    lv_obj_set_style_border_width(modal_root_, 0, 0);
    lv_obj_set_style_bg_color(modal_root_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(modal_root_, LV_OPA_70, 0);
    lv_obj_remove_flag(modal_root_, LV_OBJ_FLAG_SCROLLABLE);

    modal_card_ = lv_obj_create(modal_root_);
    lv_obj_set_size(modal_card_, 216, 140);
    lv_obj_center(modal_card_);
    lv_obj_set_style_radius(modal_card_, 12, 0);
    lv_obj_set_style_bg_color(modal_card_, lv_color_hex(0x1e1b4b), 0); // Dark Indigo
    lv_obj_set_style_bg_opa(modal_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(modal_card_, lv_color_hex(0xfbbf24), 0); // Amber 400
    lv_obj_set_style_border_width(modal_card_, 2, 0);
    lv_obj_remove_flag(modal_card_, LV_OBJ_FLAG_SCROLLABLE);

    modal_title_ = lv_label_create(modal_card_);
    lv_obj_set_width(modal_title_, 200);
    lv_obj_align(modal_title_, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_align(modal_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(modal_title_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(modal_title_, lv_color_hex(0xfbbf24), 0);
    lv_label_set_text(modal_title_, "[ ALERT ]");

    modal_desc_ = lv_label_create(modal_card_);
    lv_obj_set_width(modal_desc_, 200);
    lv_obj_align(modal_desc_, LV_ALIGN_CENTER, 0, -2);
    lv_obj_set_style_text_align(modal_desc_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(modal_desc_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(modal_desc_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(modal_desc_, "--");

    modal_hint_ = lv_label_create(modal_card_);
    lv_obj_set_width(modal_hint_, 200);
    lv_obj_align(modal_hint_, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_align(modal_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(modal_hint_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(modal_hint_, lv_color_hex(0x94a3b8), 0);
    lv_label_set_text(modal_hint_, "Press any key to dismiss");

    lv_obj_add_flag(modal_root_, LV_OBJ_FLAG_HIDDEN);
}

void Alarms::Show(bool show) {
    if (root_ == nullptr) return;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (show) {
        selected_row_ = 0;
        edit_mode_ = kSelectRow;
        UpdateUi();
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(root_);
    } else {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    }
}

void Alarms::UpdateUi() {
    if (root_ == nullptr) return;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    // Row 0: Daily Alarm
    lv_label_set_text(rows_[0].value, daily_alarm_enabled_ ? "[ ON ]" : "[ OFF ]");
    lv_obj_set_style_text_color(rows_[0].value,
        daily_alarm_enabled_ ? lv_color_hex(0x10b981) : lv_color_hex(0xef4444), 0);

    // Row 1: Alarm Time
    char time_buf[32];
    if (edit_mode_ == kEditHour) {
        snprintf(time_buf, sizeof(time_buf), "[%02d] : %02d", daily_alarm_hour_, daily_alarm_minute_);
    } else if (edit_mode_ == kEditMinute) {
        snprintf(time_buf, sizeof(time_buf), "%02d : [%02d]", daily_alarm_hour_, daily_alarm_minute_);
    } else {
        snprintf(time_buf, sizeof(time_buf), "%02d:%02d", daily_alarm_hour_, daily_alarm_minute_);
    }
    lv_label_set_text(rows_[1].value, time_buf);
    lv_obj_set_style_text_color(rows_[1].value,
        (edit_mode_ != kSelectRow) ? lv_color_hex(0xfbbf24) : lv_color_hex(0x38bdf8), 0);

    // Row 2: Prayer Alert
    lv_label_set_text(rows_[2].value, prayer_alert_enabled_ ? "[ ON ]" : "[ OFF ]");
    lv_obj_set_style_text_color(rows_[2].value,
        prayer_alert_enabled_ ? lv_color_hex(0x10b981) : lv_color_hex(0xef4444), 0);

    // Row 3: Test Sound
    lv_label_set_text(rows_[3].value, "[ Play ]");

    // Row 4: Back
    lv_label_set_text(rows_[4].value, "");

    // Highlight selected row
    for (int i = 0; i < 5; i++) {
        if (i == selected_row_) {
            lv_obj_set_style_bg_color(rows_[i].row, lv_color_hex(0x164e63), 0); // Cyan dark tint
            lv_obj_set_style_border_color(rows_[i].row, lv_color_hex(0x06b6d4), 0); // Cyan 500
            lv_obj_set_style_border_width(rows_[i].row, 2, 0);
        } else {
            lv_obj_set_style_bg_color(rows_[i].row, lv_color_hex(0x1e293b), 0); // Slate 800
            lv_obj_set_style_border_color(rows_[i].row, lv_color_hex(0x334155), 0); // Slate 700
            lv_obj_set_style_border_width(rows_[i].row, 1, 0);
        }
    }

    if (edit_mode_ == kEditHour) {
        lv_label_set_text(footer_label_, "UP/DN:Hour  |  OK:Next");
    } else if (edit_mode_ == kEditMinute) {
        lv_label_set_text(footer_label_, "UP/DN:Min   |  OK:Save");
    } else {
        lv_label_set_text(footer_label_, "UP/DN:Move  OK:Toggle/Edit");
    }
}

bool Alarms::HandleButtonUp() {
    if (edit_mode_ == kEditHour) {
        daily_alarm_hour_ = (daily_alarm_hour_ + 1) % 24;
        UpdateUi();
        return true;
    }
    if (edit_mode_ == kEditMinute) {
        daily_alarm_minute_ = (daily_alarm_minute_ + 5) % 60;
        UpdateUi();
        return true;
    }
    selected_row_ = (selected_row_ - 1 + 5) % 5;
    UpdateUi();
    return true;
}

bool Alarms::HandleButtonDown() {
    if (edit_mode_ == kEditHour) {
        daily_alarm_hour_ = (daily_alarm_hour_ - 1 + 24) % 24;
        UpdateUi();
        return true;
    }
    if (edit_mode_ == kEditMinute) {
        daily_alarm_minute_ = (daily_alarm_minute_ - 5 + 60) % 60;
        UpdateUi();
        return true;
    }
    selected_row_ = (selected_row_ + 1) % 5;
    UpdateUi();
    return true;
}

bool Alarms::HandleButtonOk() {
    if (edit_mode_ == kEditHour) {
        edit_mode_ = kEditMinute;
        UpdateUi();
        return true;
    }
    if (edit_mode_ == kEditMinute) {
        edit_mode_ = kSelectRow;
        SaveSettings();
        UpdateUi();
        return true;
    }

    switch (selected_row_) {
        case 0:
            daily_alarm_enabled_ = !daily_alarm_enabled_;
            SaveSettings();
            UpdateUi();
            break;
        case 1:
            edit_mode_ = kEditHour;
            UpdateUi();
            break;
        case 2:
            prayer_alert_enabled_ = !prayer_alert_enabled_;
            SaveSettings();
            UpdateUi();
            break;
        case 3: {
            auto codec = Board::GetInstance().GetAudioCodec();
            if (codec != nullptr) {
                codec->SetOutputVolume(100);
                ESP_LOGI(TAG, "Testing alarm sound (OGG_SUCCESS), vol=100");
            }
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
            break;
        }
        case 4:
            Launcher::GetInstance().EnterMenu();
            break;
    }
    return true;
}

void Alarms::CheckAlarms() {
    if (alert_active_) {
        alert_ring_ticks_++;
        if (alert_ring_ticks_ <= 60 && (alert_ring_ticks_ % 4 == 0)) {
            auto codec = Board::GetInstance().GetAudioCodec();
            if (codec != nullptr) {
                codec->SetOutputVolume(100);
            }
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
        }
    }

    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);

    if (ti.tm_year < 120) {
        // Not synchronized yet
        return;
    }

    int current_min = ti.tm_hour * 60 + ti.tm_min;
    if (current_min == last_triggered_min_) {
        return;
    }

    // Check Daily Alarm
    if (daily_alarm_enabled_ && ti.tm_hour == daily_alarm_hour_ && ti.tm_min == daily_alarm_minute_) {
        last_triggered_min_ = current_min;
        char buf[64];
        snprintf(buf, sizeof(buf), "%02d:%02d - Wake Up!", daily_alarm_hour_, daily_alarm_minute_);
        TriggerAlert("DAILY ALARM", buf);
        return;
    }

    // Check Prayer Alert
    if (prayer_alert_enabled_) {
        std::string p = PrayerTimes::GetInstance().CheckPrayerAlert(ti.tm_hour, ti.tm_min);
        if (!p.empty()) {
            last_triggered_min_ = current_min;
            TriggerAlert("PRAYER TIME", "Time for " + p);
            return;
        }
    }
}

void Alarms::TriggerAlert(const std::string& title, const std::string& message) {
    alert_active_ = true;
    alert_ring_ticks_ = 0;
    ESP_LOGI(TAG, "ALERT TRIGGERED: %s - %s", title.c_str(), message.c_str());

    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec != nullptr) {
        codec->SetOutputVolume(100);
    }
    Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);

    if (modal_root_ != nullptr) {
        auto* display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        lv_label_set_text(modal_title_, title.c_str());
        lv_label_set_text(modal_desc_, message.c_str());
        lv_obj_clear_flag(modal_root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(modal_root_);
    }
}

void Alarms::DismissAlert() {
    if (alert_active_) {
        alert_active_ = false;
        alert_ring_ticks_ = 0;
        if (modal_root_ != nullptr) {
            auto* display = Board::GetInstance().GetDisplay();
            DisplayLockGuard lock(display);
            lv_obj_add_flag(modal_root_, LV_OBJ_FLAG_HIDDEN);
        }
        ESP_LOGI(TAG, "Alert dismissed");
    }
}
