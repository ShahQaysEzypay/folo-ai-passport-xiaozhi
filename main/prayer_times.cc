#include "prayer_times.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/time.h>
#include <cJSON.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "application.h"
#include "board.h"
#include <http.h>
#include "settings.h"
#include "display.h"
#include "display/lvgl_display/lvgl_theme.h"

#define TAG "PrayerTimes"

namespace {

const char* HijriMonthName(int m) {
    static const char* const kMonths[] = {
        "",
        "Muharram", "Safar", "Rabi I", "Rabi II",
        "Jumada I", "Jumada II", "Rajab", "Sha'ban",
        "Ramadan", "Shawwal", "Dhul-Qadah", "Dhul-Hijjah"
    };
    if (m >= 1 && m <= 12) return kMonths[m];
    return "";
}

std::string FormatHijri(const std::string& raw) {
    // Format: "YYYY-MM-DD" e.g. "1448-06-01" -> "1 Jumada II 1448"
    int y = 0, m = 0, d = 0;
    if (sscanf(raw.c_str(), "%d-%d-%d", &y, &m, &d) == 3) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%d %s %d", d, HijriMonthName(m), y);
        return buf;
    }
    return raw;
}

void ParseTimeStr(const std::string& str, int& h, int& m) {
    h = 0;
    m = 0;
    sscanf(str.c_str(), "%d:%d", &h, &m);
}

} // namespace

void PrayerTimes::Initialize() {
    // Default fallback prayer times for Banting, Selangor (SGR03)
    prayers_ = {
        {"Subuh (Fajr)",   "05:48", 5, 48, false},
        {"Syuruk",         "06:57", 6, 57, false},
        {"Zohor (Dhuhr)",  "13:01", 13, 1, false},
        {"Asar (Asr)",     "16:22", 16, 22, false},
        {"Maghrib",        "18:59", 18, 59, false},
        {"Isyak (Isha)",   "20:11", 20, 11, false},
    };

    LoadCache();
    CalculateNextPrayer();
}

void PrayerTimes::LoadCache() {
    Settings s("prayer", false);
    std::string date = s.GetString("date", "");
    if (!date.empty()) {
        gregorian_date_ = date;
        hijri_date_ = s.GetString("hijri", "--");
        day_name_ = s.GetString("day", "--");

        auto load_p = [&](size_t idx, const std::string& key) {
            std::string t = s.GetString(key, "");
            if (!t.empty() && idx < prayers_.size()) {
                prayers_[idx].time_str = t;
                ParseTimeStr(t, prayers_[idx].hour, prayers_[idx].minute);
            }
        };

        load_p(0, "fajr");
        load_p(1, "syuruk");
        load_p(2, "dhuhr");
        load_p(3, "asr");
        load_p(4, "maghrib");
        load_p(5, "isha");

        loaded_ = true;
        ESP_LOGI(TAG, "Loaded cached prayer times for %s (%s)", gregorian_date_.c_str(), hijri_date_.c_str());
    }
}

void PrayerTimes::SaveCache() {
    Settings s("prayer", true);
    s.SetString("date", gregorian_date_);
    s.SetString("hijri", hijri_date_);
    s.SetString("day", day_name_);

    if (prayers_.size() >= 6) {
        s.SetString("fajr", prayers_[0].time_str);
        s.SetString("syuruk", prayers_[1].time_str);
        s.SetString("dhuhr", prayers_[2].time_str);
        s.SetString("asr", prayers_[3].time_str);
        s.SetString("maghrib", prayers_[4].time_str);
        s.SetString("isha", prayers_[5].time_str);
    }
    ESP_LOGI(TAG, "Saved prayer times cache to NVS");
}

void PrayerTimes::CalculateNextPrayer() {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);

    int now_m = ti.tm_hour * 60 + ti.tm_min;

    // Reset all flags
    for (auto& p : prayers_) {
        p.is_next = false;
    }

    int next_idx = -1;
    for (size_t i = 0; i < prayers_.size(); i++) {
        int pm = prayers_[i].hour * 60 + prayers_[i].minute;
        if (pm > now_m) {
            next_idx = i;
            break;
        }
    }

    if (next_idx != -1) {
        prayers_[next_idx].is_next = true;
        next_prayer_name_ = prayers_[next_idx].name + " " + prayers_[next_idx].time_str;
        int diff = (prayers_[next_idx].hour * 60 + prayers_[next_idx].minute) - now_m;
        int dh = diff / 60;
        int dm = diff % 60;
        char buf[64];
        if (dh > 0) {
            snprintf(buf, sizeof(buf), "IN %dH %02dM", dh, dm);
        } else {
            snprintf(buf, sizeof(buf), "IN %d MIN", dm);
        }
        next_prayer_countdown_ = buf;
    } else {
        // Next is Subuh tomorrow
        prayers_[0].is_next = true;
        next_prayer_name_ = prayers_[0].name + " " + prayers_[0].time_str;
        int diff = (24 * 60 - now_m) + (prayers_[0].hour * 60 + prayers_[0].minute);
        int dh = diff / 60;
        int dm = diff % 60;
        char buf[64];
        snprintf(buf, sizeof(buf), "IN %dH %02dM", dh, dm);
        next_prayer_countdown_ = buf;
    }
}

std::string PrayerTimes::CheckPrayerAlert(int hour, int minute) {
    for (const auto& p : prayers_) {
        // Only Subuh, Zohor, Asar, Maghrib, Isyak (skip Syuruk)
        if (p.name.find("Syuruk") != std::string::npos) continue;
        if (p.hour == hour && p.minute == minute) {
            return p.name + " (" + p.time_str + ")";
        }
    }
    return "";
}

void PrayerTimes::RefreshAsync() {
    if (fetching_) {
        ESP_LOGW(TAG, "Fetch already in progress");
        return;
    }
    fetching_ = true;
    xTaskCreate(FetchTask, "prayer_fetch", 6144, this, 3, nullptr);
}

void PrayerTimes::FetchTask(void* arg) {
    auto* self = static_cast<PrayerTimes*>(arg);
    ESP_LOGI(TAG, "Starting prayer times fetch task...");
    self->DoFetch();
    self->fetching_ = false;
    vTaskDelete(nullptr);
}

bool PrayerTimes::DoFetch() {
    if (esp_get_free_heap_size() < 40000) {
        ESP_LOGW(TAG, "Low heap (%lu bytes), skipping prayer times HTTPS fetch", (unsigned long)esp_get_free_heap_size());
        return false;
    }

    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);

    std::string url = "https://api.waktusolat.app/solat/SGR03";
    int requested_mday = -1;
    if (ti.tm_year >= 120 && ti.tm_mday >= 1 && ti.tm_mday <= 31) {
        requested_mday = ti.tm_mday;
        url += "/" + std::to_string(requested_mday);
    }

    ESP_LOGI(TAG, "Fetching prayer times from %s", url.c_str());

    auto network = Board::GetInstance().GetNetwork();
    if (network == nullptr) {
        ESP_LOGE(TAG, "Network interface unavailable");
        return false;
    }

    auto http = network->CreateHttp(0);
    if (!http) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return false;
    }

    http->SetHeader("User-Agent", "AI-Passport/1.0");
    http->SetTimeout(10000);

    if (auto opened = http->Open("GET", url); !opened) {
        ESP_LOGE(TAG, "Failed to connect to %s", url.c_str());
        http->Close();
        return false;
    }

    auto status = http->GetStatusCode();
    if (!status || *status != 200) {
        ESP_LOGE(TAG, "HTTP GET failed with code: %d", status ? *status : -1);
        http->Close();
        return false;
    }

    std::string response = http->ReadAll();
    http->Close();

    if (response.empty()) {
        ESP_LOGE(TAG, "Empty response received");
        return false;
    }

    cJSON* root = cJSON_Parse(response.c_str());
    if (root == nullptr) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return false;
    }

    // Parse Malaysia serverTime ("YYYY-MM-DD HH:MM:SS") to sync clock if needed
    // and select the exact day of the month when fetching the monthly array.
    cJSON* srv_time_item = cJSON_GetObjectItem(root, "serverTime");
    if (cJSON_IsString(srv_time_item) && srv_time_item->valuestring != nullptr) {
        int sy = 0, sm = 0, sd = 0, sh = 0, smin = 0, ssec = 0;
        if (sscanf(srv_time_item->valuestring, "%d-%d-%d %d:%d:%d",
                   &sy, &sm, &sd, &sh, &smin, &ssec) == 6 && sy >= 2024) {
            if (requested_mday < 1 && sd >= 1 && sd <= 31) {
                requested_mday = sd;
            }
            if (ti.tm_year < 120) {
                struct tm srv_tm = {};
                srv_tm.tm_year = sy - 1900;
                srv_tm.tm_mon = sm - 1;
                srv_tm.tm_mday = sd;
                srv_tm.tm_hour = sh;
                srv_tm.tm_min = smin;
                srv_tm.tm_sec = ssec;
                time_t srv_epoch = mktime(&srv_tm);
                if (srv_epoch > 0) {
                    struct timeval tv = {.tv_sec = srv_epoch, .tv_usec = 0};
                    settimeofday(&tv, nullptr);
                    ESP_LOGI(TAG, "Synced system clock from WaktuSolat serverTime: %s",
                             srv_time_item->valuestring);
                }
            }
        }
    }

    cJSON* pt_item = cJSON_GetObjectItem(root, "prayerTime");
    cJSON* target_day = nullptr;

    if (cJSON_IsArray(pt_item)) {
        int day_idx = (requested_mday >= 1) ? (requested_mday - 1) : 0;
        int arr_size = cJSON_GetArrayItem(pt_item, day_idx) ? day_idx : 0;
        target_day = cJSON_GetArrayItem(pt_item, arr_size);
    } else if (cJSON_IsObject(pt_item)) {
        target_day = pt_item;
    }

    if (target_day != nullptr) {
        auto get_str = [](cJSON* obj, const char* key) -> std::string {
            cJSON* item = cJSON_GetObjectItem(obj, key);
            return (item && item->valuestring) ? item->valuestring : "";
        };

        std::string new_hijri = FormatHijri(get_str(target_day, "hijri"));
        std::string new_greg = get_str(target_day, "date");
        std::string new_day = get_str(target_day, "day");
        std::string times[6] = {
            get_str(target_day, "fajr"),
            get_str(target_day, "syuruk"),
            get_str(target_day, "dhuhr"),
            get_str(target_day, "asr"),
            get_str(target_day, "maghrib"),
            get_str(target_day, "isha"),
        };
        int resolved_mday = (requested_mday >= 1) ? requested_mday : ti.tm_mday;
        cJSON_Delete(root);

        // Apply state updates on the main thread for thread safety
        Application::GetInstance().Schedule([this, new_hijri, new_greg, new_day, times, resolved_mday]() {
            hijri_date_ = new_hijri;
            gregorian_date_ = new_greg;
            day_name_ = new_day;
            for (size_t idx = 0; idx < 6 && idx < prayers_.size(); idx++) {
                std::string t = times[idx];
                if (!t.empty()) {
                    if (t.size() >= 5) t = t.substr(0, 5);
                    prayers_[idx].time_str = t;
                    ParseTimeStr(t, prayers_[idx].hour, prayers_[idx].minute);
                }
            }
            loaded_ = true;
            if (resolved_mday >= 1) {
                fetched_mday_ = resolved_mday;
            }
            SaveCache();
            CalculateNextPrayer();
            UpdateUi();
            ESP_LOGI(TAG, "Prayer times updated: Hijri=%s, Date=%s", hijri_date_.c_str(), gregorian_date_.c_str());
        });
        return true;
    }

    cJSON_Delete(root);
    return false;
}

void PrayerTimes::BuildUi() {
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
    lv_label_set_text(header_title_, "[ PRAYER TIMES ]");

    // Location Subtitle
    header_sub_ = lv_label_create(root_);
    lv_obj_set_width(header_sub_, LV_HOR_RES);
    lv_obj_set_pos(header_sub_, 0, 24);
    lv_obj_set_style_text_align(header_sub_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header_sub_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_sub_, lv_color_hex(0x94a3b8), 0); // Slate 400
    lv_label_set_text(header_sub_, "Banting, Selangor (SGR03)");

    // Date Label
    date_label_ = lv_label_create(root_);
    lv_obj_set_width(date_label_, LV_HOR_RES);
    lv_obj_set_pos(date_label_, 0, 40);
    lv_obj_set_style_text_align(date_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(date_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(date_label_, lv_color_hex(0x38bdf8), 0); // Sky 400
    lv_label_set_text(date_label_, "--");

    // Next Prayer Hero Card
    next_card_ = lv_obj_create(root_);
    lv_obj_set_size(next_card_, 216, 44);
    lv_obj_set_pos(next_card_, 12, 58);
    lv_obj_set_style_radius(next_card_, 6, 0);
    lv_obj_set_style_bg_color(next_card_, lv_color_hex(0x064e3b), 0); // Emerald 900
    lv_obj_set_style_bg_opa(next_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(next_card_, lv_color_hex(0x10b981), 0); // Emerald 500
    lv_obj_set_style_border_width(next_card_, 1, 0);
    lv_obj_set_style_pad_all(next_card_, 4, 0);
    lv_obj_remove_flag(next_card_, LV_OBJ_FLAG_SCROLLABLE);

    next_title_ = lv_label_create(next_card_);
    lv_obj_set_width(next_title_, 208);
    lv_obj_align(next_title_, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_text_align(next_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(next_title_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(next_title_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(next_title_, "NEXT: --");

    next_countdown_ = lv_label_create(next_card_);
    lv_obj_set_width(next_countdown_, 208);
    lv_obj_align(next_countdown_, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_text_align(next_countdown_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(next_countdown_, &lv_font_unscii_16, 0);
    lv_obj_set_style_text_color(next_countdown_, lv_color_hex(0x6ee7b7), 0); // Emerald 300
    lv_label_set_text(next_countdown_, "--");

    // 6 Prayer Rows
    rows_.clear();
    int start_y = 108;
    for (size_t i = 0; i < prayers_.size(); i++) {
        RowWidget rw;
        rw.row = lv_obj_create(root_);
        lv_obj_set_size(rw.row, 216, 28);
        lv_obj_set_pos(rw.row, 12, start_y + i * 30);
        lv_obj_set_style_radius(rw.row, 4, 0);
        lv_obj_set_style_pad_left(rw.row, 10, 0);
        lv_obj_set_style_pad_right(rw.row, 10, 0);
        lv_obj_set_style_pad_top(rw.row, 4, 0);
        lv_obj_set_style_pad_bottom(rw.row, 4, 0);
        lv_obj_set_style_bg_color(rw.row, lv_color_hex(0x1e293b), 0); // Slate 800
        lv_obj_set_style_border_width(rw.row, 0, 0);
        lv_obj_remove_flag(rw.row, LV_OBJ_FLAG_SCROLLABLE);

        rw.name_label = lv_label_create(rw.row);
        lv_obj_set_style_text_font(rw.name_label, &lv_font_unscii_8, 0);
        lv_label_set_text(rw.name_label, prayers_[i].name.c_str());
        lv_obj_set_style_text_color(rw.name_label, lv_color_hex(0xe2e8f0), 0);
        lv_obj_align(rw.name_label, LV_ALIGN_LEFT_MID, 0, 0);

        rw.time_label = lv_label_create(rw.row);
        lv_obj_set_style_text_font(rw.time_label, &lv_font_unscii_16, 0);
        lv_label_set_text(rw.time_label, prayers_[i].time_str.c_str());
        lv_obj_set_style_text_color(rw.time_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(rw.time_label, LV_ALIGN_RIGHT_MID, 0, 0);

        rows_.push_back(rw);
    }

    // Footer
    footer_label_ = lv_label_create(root_);
    lv_obj_set_width(footer_label_, LV_HOR_RES);
    lv_obj_set_pos(footer_label_, 0, 296);
    lv_obj_set_style_text_align(footer_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(footer_label_, lv_color_hex(0x64748b), 0); // Slate 500
    lv_label_set_text(footer_label_, "UP/DN:Refresh  OK:Return");

    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    UpdateUi();
}

void PrayerTimes::Show(bool show) {
    if (root_ == nullptr) return;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);
    if (show) {
        CalculateNextPrayer();
        UpdateUi();
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(root_);
    } else {
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    }
}

void PrayerTimes::UpdateUi() {
    if (root_ == nullptr) return;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    // Date (pure ASCII for unscii_8; shorten "11-Nov-2026" -> "11 Nov" when paired with Hijri)
    std::string dtext = gregorian_date_;
    if (!hijri_date_.empty() && hijri_date_ != "--") {
        if (dtext.size() >= 6 && dtext[2] == '-') {
            dtext = dtext.substr(0, 2) + " " + dtext.substr(3, 3);
        }
        dtext += " | " + hijri_date_;
    }
    lv_label_set_text(date_label_, dtext.c_str());

    // Next prayer hero card
    std::string ntitle = "NEXT: " + next_prayer_name_;
    lv_label_set_text(next_title_, ntitle.c_str());
    lv_label_set_text(next_countdown_, next_prayer_countdown_.c_str());

    // Update 6 rows
    for (size_t i = 0; i < prayers_.size() && i < rows_.size(); i++) {
        lv_label_set_text(rows_[i].name_label, prayers_[i].name.c_str());
        lv_label_set_text(rows_[i].time_label, prayers_[i].time_str.c_str());

        if (prayers_[i].is_next) {
            lv_obj_set_style_bg_color(rows_[i].row, lv_color_hex(0x0f766e), 0); // Teal 700
            lv_obj_set_style_border_color(rows_[i].row, lv_color_hex(0x2dd4bf), 0); // Teal 400
            lv_obj_set_style_border_width(rows_[i].row, 1, 0);
            lv_obj_set_style_text_color(rows_[i].name_label, lv_color_hex(0xf0fdfa), 0);
            lv_obj_set_style_text_color(rows_[i].time_label, lv_color_hex(0x2dd4bf), 0);
        } else {
            lv_obj_set_style_bg_color(rows_[i].row, lv_color_hex(0x1e293b), 0); // Slate 800
            lv_obj_set_style_border_width(rows_[i].row, 0, 0);
            lv_obj_set_style_text_color(rows_[i].name_label, lv_color_hex(0xe2e8f0), 0);
            lv_obj_set_style_text_color(rows_[i].time_label, lv_color_hex(0xffffff), 0);
        }
    }
}
