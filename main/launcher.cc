#include "launcher.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "alarms.h"
#include "application.h"
#include "board.h"
#include "calendar.h"
#include "display.h"
#include "display/lvgl_display/lvgl_theme.h"
#include <material_symbols.h>
#include "passport_page.h"
#include "pet.h"
#include "prayer_times.h"
#include <wifi_manager.h>

#define TAG "Launcher"

void Launcher::Initialize() {
    if (initialized_) {
        return;
    }
    initialized_ = true;

    // Initialize sub-modules
    PassportPage::GetInstance().Initialize();
    PrayerTimes::GetInstance().Initialize();
    Alarms::GetInstance().Initialize();
    Calendar::GetInstance().Initialize();
    Pet::GetInstance().Initialize();

    auto* display = Board::GetInstance().GetDisplay();
    {
        DisplayLockGuard lock(display);
        BuildMenuUi();
        PassportPage::GetInstance().BuildUi();
        PrayerTimes::GetInstance().BuildUi();
        Alarms::GetInstance().BuildUi();
        Calendar::GetInstance().BuildUi();
        Pet::GetInstance().BuildUi();
    }

    // Start in Passport Main Page mode
    EnterPassport();

    StartTickTimer();
    ESP_LOGI(TAG, "Launcher initialized successfully (Passport, AI Agent, Prayer Times, Alarms, Calendar)");
}

void Launcher::StartTickTimer() {
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = [](void* arg) {
        auto* self = static_cast<Launcher*>(arg);
        Application::GetInstance().Schedule([self]() {
            self->Tick();
        });
    };
    timer_args.arg = this;
    timer_args.name = "launcher_tick";
    esp_timer_create(&timer_args, &tick_timer_);
    esp_timer_start_periodic(tick_timer_, 1000000); // 1 second
}

void Launcher::BuildMenuUi() {
    auto* screen = lv_screen_active();
    auto* display = Board::GetInstance().GetDisplay();
    auto* theme = static_cast<LvglTheme*>(display->GetTheme());
    const lv_font_t* font_icon = theme ? theme->icon_font()->font() : nullptr;

    menu_root_ = lv_obj_create(screen);
    lv_obj_set_size(menu_root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(menu_root_, 0, 0);
    lv_obj_set_style_radius(menu_root_, 0, 0);
    lv_obj_set_style_border_width(menu_root_, 0, 0);
    lv_obj_set_style_pad_all(menu_root_, 0, 0);
    lv_obj_set_style_bg_color(menu_root_, lv_color_hex(0x0f172a), 0); // Slate 900
    lv_obj_set_style_bg_opa(menu_root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(menu_root_, LV_OBJ_FLAG_SCROLLABLE);

    // Top Status Bar
    status_bar_ = lv_obj_create(menu_root_);
    lv_obj_set_size(status_bar_, 216, 26);
    lv_obj_set_pos(status_bar_, 12, 6);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(status_bar_, LV_OBJ_FLAG_SCROLLABLE);

    clock_label_ = lv_label_create(status_bar_);
    lv_label_set_text(clock_label_, "--:--");
    lv_obj_set_style_text_font(clock_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(clock_label_, lv_color_hex(0x94a3b8), 0); // Slate 400
    lv_obj_align(clock_label_, LV_ALIGN_LEFT_MID, 0, 0);

    status_title_ = lv_label_create(status_bar_);
    lv_label_set_text(status_title_, "// PASSPORT //");
    lv_obj_set_style_text_font(status_title_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(status_title_, lv_color_hex(0xf8fafc), 0); // White
    lv_obj_align(status_title_, LV_ALIGN_CENTER, 0, 0);

    wifi_label_ = lv_label_create(status_bar_);
    if (font_icon) {
        lv_obj_set_style_text_font(wifi_label_, font_icon, 0);
    }
    lv_label_set_text(wifi_label_, MATERIAL_SYMBOLS_WIFI);
    lv_obj_set_style_text_color(wifi_label_, lv_color_hex(0x38bdf8), 0);
    lv_obj_align(wifi_label_, LV_ALIGN_RIGHT_MID, 0, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "--%");
    lv_obj_set_style_text_font(battery_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(battery_label_, lv_color_hex(0x94a3b8), 0);
    lv_obj_align(battery_label_, LV_ALIGN_RIGHT_MID, -22, 0);

    // Divider Line
    lv_obj_t* divider = lv_obj_create(menu_root_);
    lv_obj_set_size(divider, 216, 1);
    lv_obj_set_pos(divider, 12, 34);
    lv_obj_set_style_radius(divider, 0, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

    // 5 Menu Cards
    struct CardConfig {
        const char* icon;
        const char* title;
        const char* subtitle;
        uint32_t icon_color;
    };
    static const CardConfig kCardConfigs[5] = {
        {MATERIAL_SYMBOLS_ROBOT_2,         "1. AI AGENT",     "Voice Assistant & LLM",  0x38bdf8},
        {MATERIAL_SYMBOLS_SCHEDULE,        "2. PRAYER TIMES", "Banting, Selangor",      0x34d399},
        {MATERIAL_SYMBOLS_ALARM,           "3. ALARMS",       "Daily & Adhan Alerts",   0xfbbf24},
        {MATERIAL_SYMBOLS_CALENDAR_MONTH,  "4. CALENDAR",     "Gregorian & Hijri",      0xa78bfa},
        {MATERIAL_SYMBOLS_FAVORITE,        "5. VIRTUAL PET",  "Mochi the Kitten",       0xf472b6},
    };

    int card_y = 40;
    for (int i = 0; i < 5; i++) {
        cards_[i].card = lv_obj_create(menu_root_);
        lv_obj_set_size(cards_[i].card, 216, 44);
        lv_obj_set_pos(cards_[i].card, 12, card_y + i * 49);
        lv_obj_set_style_radius(cards_[i].card, 6, 0);
        lv_obj_set_style_pad_left(cards_[i].card, 8, 0);
        lv_obj_set_style_pad_right(cards_[i].card, 8, 0);
        lv_obj_set_style_pad_top(cards_[i].card, 2, 0);
        lv_obj_set_style_pad_bottom(cards_[i].card, 2, 0);
        lv_obj_set_style_bg_color(cards_[i].card, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_border_width(cards_[i].card, 1, 0);
        lv_obj_set_style_border_color(cards_[i].card, lv_color_hex(0x334155), 0);
        lv_obj_remove_flag(cards_[i].card, LV_OBJ_FLAG_SCROLLABLE);

        // Icon
        cards_[i].icon = lv_label_create(cards_[i].card);
        if (font_icon) {
            lv_obj_set_style_text_font(cards_[i].icon, font_icon, 0);
        }
        lv_label_set_text(cards_[i].icon, kCardConfigs[i].icon);
        lv_obj_set_style_text_color(cards_[i].icon, lv_color_hex(kCardConfigs[i].icon_color), 0);
        lv_obj_align(cards_[i].icon, LV_ALIGN_LEFT_MID, 4, 0);

        // Title
        cards_[i].title = lv_label_create(cards_[i].card);
        lv_obj_set_style_text_font(cards_[i].title, &lv_font_unscii_8, 0);
        lv_label_set_text(cards_[i].title, kCardConfigs[i].title);
        lv_obj_set_style_text_color(cards_[i].title, lv_color_hex(0xf8fafc), 0);
        lv_obj_align(cards_[i].title, LV_ALIGN_TOP_LEFT, 36, 6);

        // Subtitle
        cards_[i].subtitle = lv_label_create(cards_[i].card);
        lv_obj_set_style_text_font(cards_[i].subtitle, &lv_font_unscii_8, 0);
        lv_label_set_text(cards_[i].subtitle, kCardConfigs[i].subtitle);
        lv_obj_set_style_text_color(cards_[i].subtitle, lv_color_hex(0x94a3b8), 0);
        lv_obj_align(cards_[i].subtitle, LV_ALIGN_TOP_LEFT, 36, 22);
    }

    // Bottom Navigation Hint
    footer_hint_ = lv_label_create(menu_root_);
    lv_obj_set_width(footer_hint_, LV_HOR_RES);
    lv_obj_set_pos(footer_hint_, 0, 296);
    lv_obj_set_style_text_align(footer_hint_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer_hint_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(footer_hint_, lv_color_hex(0x64748b), 0);
    lv_label_set_text(footer_hint_, "UP/DN:Move  OK:Open  HOLD:ID");

    lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
    UpdateMenuUi();
}

void Launcher::StyleMenuCard(int index, bool selected) {
    if (cards_[index].card == nullptr) return;
    if (selected) {
        lv_obj_set_style_bg_color(cards_[index].card, lv_color_hex(0x164e63), 0); // Cyan dark tint
        lv_obj_set_style_border_color(cards_[index].card, lv_color_hex(0x06b6d4), 0); // Cyan 500
        lv_obj_set_style_border_width(cards_[index].card, 2, 0);
        lv_obj_set_style_text_color(cards_[index].title, lv_color_hex(0xffffff), 0);
    } else {
        lv_obj_set_style_bg_color(cards_[index].card, lv_color_hex(0x1e293b), 0);
        lv_obj_set_style_border_color(cards_[index].card, lv_color_hex(0x334155), 0);
        lv_obj_set_style_border_width(cards_[index].card, 1, 0);
        lv_obj_set_style_text_color(cards_[index].title, lv_color_hex(0xe2e8f0), 0);
    }
}

void Launcher::UpdateMenuUi() {
    for (int i = 0; i < 5; i++) {
        StyleMenuCard(i, i == selected_menu_item_);
    }
    UpdateStatusBar();
}

void Launcher::UpdateStatusBar() {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);

    if (ti.tm_year >= 120 && clock_label_ != nullptr) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        lv_label_set_text(clock_label_, buf);
    }

    if (battery_label_ != nullptr) {
        int level = 0;
        bool charging = false, discharging = false;
        if (Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
            char bbuf[16];
            snprintf(bbuf, sizeof(bbuf), "%d%%%s", level, charging ? "+" : "");
            lv_label_set_text(battery_label_, bbuf);
        }
    }

    if (wifi_label_ != nullptr) {
        lv_label_set_text(wifi_label_, Board::GetInstance().GetNetworkStateIcon());
    }
}

void Launcher::EnterPassport() {
    auto* display = Board::GetInstance().GetDisplay();
    {
        DisplayLockGuard lock(display);
        mode_ = LauncherMode::kPassport;
        if (menu_root_ != nullptr) {
            lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        }
        PrayerTimes::GetInstance().Show(false);
        Alarms::GetInstance().Show(false);
        Calendar::GetInstance().Show(false);
        Pet::GetInstance().Show(false);
        PassportPage::GetInstance().Show(true);
    }
    ESP_LOGI(TAG, "Entered Passport Main Page");
}

void Launcher::EnterMenu() {
    if (mode_ == LauncherMode::kAiAgent) {
        auto& app = Application::GetInstance();
        auto state = app.GetDeviceState();
        if (state == kDeviceStateSpeaking) {
            app.AbortSpeaking(kAbortReasonNone);
            app.StopListening();
        } else if (state == kDeviceStateListening) {
            app.ToggleChatState();
        }
    }

    auto* display = Board::GetInstance().GetDisplay();
    {
        DisplayLockGuard lock(display);
        mode_ = LauncherMode::kMenu;
        PassportPage::GetInstance().Show(false);
        PrayerTimes::GetInstance().Show(false);
        Alarms::GetInstance().Show(false);
        Calendar::GetInstance().Show(false);
        Pet::GetInstance().Show(false);

        if (menu_root_ != nullptr) {
            UpdateMenuUi();
            lv_obj_clear_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(menu_root_);
        }
    }
    ESP_LOGI(TAG, "Entered Main Menu");
}

void Launcher::EnterAiAgent() {
    auto* display = Board::GetInstance().GetDisplay();
    {
        DisplayLockGuard lock(display);
        mode_ = LauncherMode::kAiAgent;
        PassportPage::GetInstance().Show(false);
        PrayerTimes::GetInstance().Show(false);
        Alarms::GetInstance().Show(false);
        Calendar::GetInstance().Show(false);
        Pet::GetInstance().Show(false);

        if (menu_root_ != nullptr) {
            lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        }
    }
    display->ShowNotification("OK: Talk | Hold OK: Menu", 3000);
    ESP_LOGI(TAG, "Entered AI Agent mode");
}

void Launcher::EnterPrayerTimes() {
    auto* display = Board::GetInstance().GetDisplay();
    {
        DisplayLockGuard lock(display);
        mode_ = LauncherMode::kPrayerTimes;
        PassportPage::GetInstance().Show(false);
        if (menu_root_ != nullptr) {
            lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        }
        Alarms::GetInstance().Show(false);
        Calendar::GetInstance().Show(false);
        Pet::GetInstance().Show(false);
        PrayerTimes::GetInstance().Show(true);
    }
    ESP_LOGI(TAG, "Entered Prayer Times mode");
}

void Launcher::EnterAlarms() {
    auto* display = Board::GetInstance().GetDisplay();
    {
        DisplayLockGuard lock(display);
        mode_ = LauncherMode::kAlarms;
        PassportPage::GetInstance().Show(false);
        if (menu_root_ != nullptr) {
            lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        }
        PrayerTimes::GetInstance().Show(false);
        Calendar::GetInstance().Show(false);
        Pet::GetInstance().Show(false);
        Alarms::GetInstance().Show(true);
    }
    ESP_LOGI(TAG, "Entered Alarms mode");
}

void Launcher::EnterCalendar() {
    auto* display = Board::GetInstance().GetDisplay();
    {
        DisplayLockGuard lock(display);
        mode_ = LauncherMode::kCalendar;
        PassportPage::GetInstance().Show(false);
        if (menu_root_ != nullptr) {
            lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        }
        PrayerTimes::GetInstance().Show(false);
        Alarms::GetInstance().Show(false);
        Pet::GetInstance().Show(false);
        Calendar::GetInstance().Show(true);
    }
    ESP_LOGI(TAG, "Entered Calendar mode");
}

void Launcher::EnterPet() {
    auto* display = Board::GetInstance().GetDisplay();
    {
        DisplayLockGuard lock(display);
        mode_ = LauncherMode::kPet;
        PassportPage::GetInstance().Show(false);
        if (menu_root_ != nullptr) {
            lv_obj_add_flag(menu_root_, LV_OBJ_FLAG_HIDDEN);
        }
        PrayerTimes::GetInstance().Show(false);
        Alarms::GetInstance().Show(false);
        Calendar::GetInstance().Show(false);
        Pet::GetInstance().Show(true);
    }
    ESP_LOGI(TAG, "Entered Virtual Pet (Mochi) mode");
}

void Launcher::OnNetworkConnected() {
    ESP_LOGI(TAG, "Network connected");
    // Do NOT trigger immediate HTTPS fetch on Wi-Fi connect to avoid concurrent TLS with ActivationTask.
    // Tick() will sync prayer times once device is idle and heap is sufficient.
}

void Launcher::Tick() {
    tick_count_++;

    // Check alarms and prayer alerts
    Alarms::GetInstance().CheckAlarms();

    auto* display = Board::GetInstance().GetDisplay();
    if (mode_ == LauncherMode::kPassport && (tick_count_ % 60 == 0)) {
        DisplayLockGuard lock(display);
        PassportPage::GetInstance().UpdateUi();
    } else if (mode_ == LauncherMode::kMenu) {
        DisplayLockGuard lock(display);
        UpdateStatusBar();
    } else if (mode_ == LauncherMode::kPrayerTimes && (tick_count_ % 10 == 0)) {
        DisplayLockGuard lock(display);
        PrayerTimes::GetInstance().CalculateNextPrayer();
        PrayerTimes::GetInstance().UpdateUi();
    }


    // Auto-refresh prayer times once per day when connected and idle with plenty of heap
    if (WifiManager::GetInstance().IsConnected() &&
        Application::GetInstance().GetDeviceState() == kDeviceStateIdle &&
        esp_get_free_heap_size() > 45000) {
        time_t now = time(nullptr);
        struct tm ti;
        localtime_r(&now, &ti);
        if (ti.tm_year >= 120) {
            auto& pt = PrayerTimes::GetInstance();
            if (pt.GetFetchedMday() != ti.tm_mday && !pt.IsFetching() &&
                last_fetched_day_ != ti.tm_mday) {
                last_fetched_day_ = ti.tm_mday;
                pt.RefreshAsync();
            }
        }
    }
}

bool Launcher::HandleButton(LauncherButton button, LauncherButtonEvent event) {
    auto* display = Board::GetInstance().GetDisplay();

    // If an alert popup is ringing, any button click dismisses it
    if (Alarms::GetInstance().IsAlertActive()) {
        if (event == LauncherButtonEvent::kClick) {
            Alarms::GetInstance().DismissAlert();
            return true;
        }
    }

    // Long press OK:
    // In kMenu -> Return to Passport main page
    // In sub-modules -> Return to Main Menu
    if (button == LauncherButton::kOk && event == LauncherButtonEvent::kLongPress) {
        if (mode_ == LauncherMode::kMenu) {
            EnterPassport();
            return true;
        } else if (mode_ == LauncherMode::kPet) {
            return Pet::GetInstance().HandleButton(button, event);
        } else {
            EnterMenu();
            return true;
        }
    }

    switch (mode_) {
        case LauncherMode::kPassport:
            if (event == LauncherButtonEvent::kClick) {
                EnterMenu();
                return true;
            }
            return true;
        case LauncherMode::kMenu:
            if (event == LauncherButtonEvent::kClick) {
                if (button == LauncherButton::kUp) {
                    DisplayLockGuard lock(display);
                    selected_menu_item_ = (selected_menu_item_ - 1 + 5) % 5;
                    UpdateMenuUi();
                    return true;
                }
                if (button == LauncherButton::kDown) {
                    DisplayLockGuard lock(display);
                    selected_menu_item_ = (selected_menu_item_ + 1) % 5;
                    UpdateMenuUi();
                    return true;
                }
                if (button == LauncherButton::kOk) {
                    if (selected_menu_item_ == 0) {
                        EnterAiAgent();
                    } else if (selected_menu_item_ == 1) {
                        EnterPrayerTimes();
                    } else if (selected_menu_item_ == 2) {
                        EnterAlarms();
                    } else if (selected_menu_item_ == 3) {
                        EnterCalendar();
                    } else if (selected_menu_item_ == 4) {
                        EnterPet();
                    }
                    return true;
                }
            }
            return true;

        case LauncherMode::kPrayerTimes:
            if (event == LauncherButtonEvent::kClick) {
                if (button == LauncherButton::kUp || button == LauncherButton::kDown) {
                    PrayerTimes::GetInstance().RefreshAsync();
                    return true;
                }
                if (button == LauncherButton::kOk) {
                    EnterMenu();
                    return true;
                }
            }
            return true;

        case LauncherMode::kAlarms:
            if (event == LauncherButtonEvent::kClick) {
                if (button == LauncherButton::kUp) {
                    return Alarms::GetInstance().HandleButtonUp();
                }
                if (button == LauncherButton::kDown) {
                    return Alarms::GetInstance().HandleButtonDown();
                }
                if (button == LauncherButton::kOk) {
                    return Alarms::GetInstance().HandleButtonOk();
                }
            }
            return true;

        case LauncherMode::kCalendar:
            if (event == LauncherButtonEvent::kClick) {
                if (button == LauncherButton::kUp) {
                    return Calendar::GetInstance().HandleButtonUp();
                }
                if (button == LauncherButton::kDown) {
                    return Calendar::GetInstance().HandleButtonDown();
                }
                if (button == LauncherButton::kOk) {
                    return Calendar::GetInstance().HandleButtonOk();
                }
            }
            return true;

        case LauncherMode::kPet:
            return Pet::GetInstance().HandleButton(button, event);

        case LauncherMode::kAiAgent:
            // Let up/down adjust volume, ok click toggle voice chat
            return false;
    }

    return false;
}
