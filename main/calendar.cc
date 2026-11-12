#include "calendar.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <esp_log.h>

#include "board.h"
#include "display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include "launcher.h"
#include "prayer_times.h"

#define TAG "Calendar"

namespace {

int DaysInMonth(int year, int month) {
    if (month == 2) {
        bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        return leap ? 29 : 28;
    }
    if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
    return 31;
}

int FirstDayOfWeek(int year, int month) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = 1;
    mktime(&t);
    return t.tm_wday; // 0=Sunday, 1=Monday, ..., 6=Saturday
}

const char* MonthName(int month) {
    static const char* const kNames[] = {
        "", "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
        "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
    };
    if (month >= 1 && month <= 12) return kNames[month];
    return "";
}

const char* ShortMonthName(int month) {
    static const char* const kShort[] = {
        "", "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    if (month >= 1 && month <= 12) return kShort[month];
    return "";
}

const char* DayName(int wday) {
    static const char* const kDays[] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };
    if (wday >= 0 && wday < 7) return kDays[wday];
    return "";
}

} // namespace

void Calendar::Initialize() {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);

    if (ti.tm_year >= 120) {
        today_year_ = ti.tm_year + 1900;
        today_month_ = ti.tm_mon + 1;
        today_day_ = ti.tm_mday;
    } else {
        today_year_ = 2026;
        today_month_ = 11;
        today_day_ = 11;
    }

    current_year_ = today_year_;
    current_month_ = today_month_;
}

void Calendar::ResetToToday() {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    if (ti.tm_year >= 120) {
        today_year_ = ti.tm_year + 1900;
        today_month_ = ti.tm_mon + 1;
        today_day_ = ti.tm_mday;
    }
    current_year_ = today_year_;
    current_month_ = today_month_;
}

void Calendar::BuildUi() {
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
    lv_obj_set_pos(header_title_, 0, 6);
    lv_obj_set_style_text_align(header_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header_title_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_title_, lv_color_hex(0xa78bfa), 0); // Violet 400
    lv_label_set_text(header_title_, "[ CALENDAR ]");

    // Month & Year Label
    month_year_label_ = lv_label_create(root_);
    lv_obj_set_width(month_year_label_, LV_HOR_RES);
    lv_obj_set_pos(month_year_label_, 0, 22);
    lv_obj_set_style_text_align(month_year_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(month_year_label_, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(month_year_label_, lv_color_hex(0xf8fafc), 0); // White
    lv_label_set_text(month_year_label_, "NOVEMBER 2026");

    // Hijri date subtitle
    hijri_label_ = lv_label_create(root_);
    lv_obj_set_width(hijri_label_, LV_HOR_RES);
    lv_obj_set_pos(hijri_label_, 0, 44);
    lv_obj_set_style_text_align(hijri_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(hijri_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(hijri_label_, lv_color_hex(0xfbbf24), 0); // Amber 400
    lv_label_set_text(hijri_label_, "--");

    // Days of Week Header
    static const char* const kDayHeaders[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
    int start_x = 12;
    int col_w = 31;
    for (int c = 0; c < 7; c++) {
        day_headers_[c] = lv_label_create(root_);
        lv_obj_set_width(day_headers_[c], col_w);
        lv_obj_set_pos(day_headers_[c], start_x + c * col_w, 64);
        lv_obj_set_style_text_align(day_headers_[c], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(day_headers_[c], &lv_font_unscii_8, 0);
        lv_obj_set_style_text_color(day_headers_[c], (c == 0 || c == 6) ? lv_color_hex(0xf87171) : lv_color_hex(0x94a3b8), 0);
        lv_label_set_text(day_headers_[c], kDayHeaders[c]);
    }

    // Today highlight indicator (single container positioned behind today's label)
    today_box_ = lv_obj_create(root_);
    lv_obj_set_size(today_box_, 28, 20);
    lv_obj_set_style_radius(today_box_, 4, 0);
    lv_obj_set_style_bg_color(today_box_, lv_color_hex(0x06b6d4), 0); // Cyan 500
    lv_obj_set_style_bg_opa(today_box_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(today_box_, lv_color_hex(0x38bdf8), 0);
    lv_obj_set_style_border_width(today_box_, 1, 0);
    lv_obj_add_flag(today_box_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(today_box_, LV_OBJ_FLAG_SCROLLABLE);

    // 42 Day Labels (6 rows x 7 columns) directly on root_
    int grid_start_y = 82;
    int row_h = 22;
    for (int i = 0; i < 42; i++) {
        int r = i / 7;
        int c = i % 7;

        day_labels_[i] = lv_label_create(root_);
        lv_obj_set_size(day_labels_[i], 28, 20);
        lv_obj_set_pos(day_labels_[i], start_x + c * col_w + 1, grid_start_y + r * row_h + 3);
        lv_obj_set_style_text_align(day_labels_[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_font(day_labels_[i], &lv_font_unscii_8, 0);
        lv_obj_set_style_text_color(day_labels_[i], lv_color_hex(0xe2e8f0), 0);
        lv_label_set_text(day_labels_[i], "");
    }

    // Info Card
    info_card_ = lv_obj_create(root_);
    lv_obj_set_size(info_card_, 216, 48);
    lv_obj_set_pos(info_card_, 12, 226);
    lv_obj_set_style_radius(info_card_, 6, 0);
    lv_obj_set_style_bg_color(info_card_, lv_color_hex(0x1e293b), 0); // Slate 800
    lv_obj_set_style_bg_opa(info_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(info_card_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(info_card_, 1, 0);
    lv_obj_set_style_pad_all(info_card_, 4, 0);
    lv_obj_remove_flag(info_card_, LV_OBJ_FLAG_SCROLLABLE);

    date_full_label_ = lv_label_create(info_card_);
    lv_obj_set_width(date_full_label_, 208);
    lv_obj_align(date_full_label_, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_text_align(date_full_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(date_full_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(date_full_label_, lv_color_hex(0xf8fafc), 0);
    lv_label_set_text(date_full_label_, "--");

    date_sub_label_ = lv_label_create(info_card_);
    lv_obj_set_width(date_sub_label_, 208);
    lv_obj_align(date_sub_label_, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_text_align(date_sub_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(date_sub_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(date_sub_label_, lv_color_hex(0x34d399), 0); // Emerald 400
    lv_label_set_text(date_sub_label_, "--");

    // Footer Hint
    footer_label_ = lv_label_create(root_);
    lv_obj_set_width(footer_label_, LV_HOR_RES);
    lv_obj_set_pos(footer_label_, 0, 296);
    lv_obj_set_style_text_align(footer_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(footer_label_, lv_color_hex(0x64748b), 0);
    lv_label_set_text(footer_label_, "UP/DN:Month  OK:Return");

    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    UpdateUi();
}

void Calendar::Show(bool show) {
    if (root_ == nullptr) return;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (show) {
        ResetToToday();
        UpdateUi();
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(root_);
    } else {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    }
}

void Calendar::UpdateUi() {
    if (root_ == nullptr) return;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    // Month & Year header
    char my_buf[64];
    snprintf(my_buf, sizeof(my_buf), "%s %d", MonthName(current_month_), current_year_);
    lv_label_set_text(month_year_label_, my_buf);

    // Hijri subtitle
    std::string hijri = PrayerTimes::GetInstance().GetHijriDate();
    if (!hijri.empty() && hijri != "--") {
        lv_label_set_text(hijri_label_, hijri.c_str());
    } else {
        lv_label_set_text(hijri_label_, "Islamic & Solar Calendar");
    }

    int first_day = FirstDayOfWeek(current_year_, current_month_);
    int total_days = DaysInMonth(current_year_, current_month_);

    bool found_today = false;
    for (int i = 0; i < 42; i++) {
        int day_num = i - first_day + 1;
        if (day_num >= 1 && day_num <= total_days) {
            char dbuf[8];
            snprintf(dbuf, sizeof(dbuf), "%d", day_num);
            lv_label_set_text(day_labels_[i], dbuf);

            bool is_today = (current_year_ == today_year_ &&
                             current_month_ == today_month_ &&
                             day_num == today_day_);

            if (is_today) {
                int r = i / 7;
                int c = i % 7;
                int start_x = 12;
                int col_w = 31;
                int grid_start_y = 82;
                int row_h = 22;
                if (today_box_ != nullptr) {
                    lv_obj_set_pos(today_box_, start_x + c * col_w + 1, grid_start_y + r * row_h);
                    lv_obj_clear_flag(today_box_, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_move_to_index(today_box_, 0); // behind labels
                }
                lv_obj_set_style_text_color(day_labels_[i], lv_color_hex(0xffffff), 0);
                found_today = true;
            } else {
                int col = i % 7;
                if (col == 0 || col == 6) {
                    lv_obj_set_style_text_color(day_labels_[i], lv_color_hex(0xf87171), 0); // Weekend
                } else {
                    lv_obj_set_style_text_color(day_labels_[i], lv_color_hex(0xe2e8f0), 0);
                }
            }
        } else {
            lv_label_set_text(day_labels_[i], "");
        }
    }
    if (!found_today && today_box_ != nullptr) {
        lv_obj_add_flag(today_box_, LV_OBJ_FLAG_HIDDEN);
    }

    // Info card: today's full date (compute weekday from today_year_/today_month_/today_day_)
    struct tm t_today = {};
    t_today.tm_year = today_year_ - 1900;
    t_today.tm_mon = today_month_ - 1;
    t_today.tm_mday = today_day_;
    mktime(&t_today);

    char full_buf[64];
    snprintf(full_buf, sizeof(full_buf), "%s, %d %s %d",
             DayName(t_today.tm_wday), today_day_, ShortMonthName(today_month_), today_year_);
    lv_label_set_text(date_full_label_, full_buf);

    std::string next_p = PrayerTimes::GetInstance().GetNextPrayerName();
    if (!next_p.empty() && next_p != "--") {
        std::string sub = "Next: " + next_p;
        lv_label_set_text(date_sub_label_, sub.c_str());
    } else {
        lv_label_set_text(date_sub_label_, "Banting, Selangor (SGR03)");
    }
}

bool Calendar::HandleButtonUp() {
    current_month_--;
    if (current_month_ < 1) {
        current_month_ = 12;
        current_year_--;
    }
    UpdateUi();
    return true;
}

bool Calendar::HandleButtonDown() {
    current_month_++;
    if (current_month_ > 12) {
        current_month_ = 1;
        current_year_++;
    }
    UpdateUi();
    return true;
}

bool Calendar::HandleButtonOk() {
    Launcher::GetInstance().EnterMenu();
    return true;
}
