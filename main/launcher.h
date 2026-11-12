#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <string>
#include <esp_timer.h>
#include <lvgl.h>

enum class LauncherMode {
    kPassport,
    kMenu,
    kAiAgent,
    kPrayerTimes,
    kAlarms,
    kCalendar,
    kPet,
};

enum class LauncherButton {
    kUp,
    kDown,
    kOk,
};

enum class LauncherButtonEvent {
    kClick,
    kLongPress,
    kPressDown,
    kPressUp,
};

class Launcher {
public:
    static Launcher& GetInstance() {
        static Launcher instance;
        return instance;
    }
    Launcher(const Launcher&) = delete;
    Launcher& operator=(const Launcher&) = delete;

    void Initialize();
    bool HandleButton(LauncherButton button, LauncherButtonEvent event);

    void EnterPassport();
    void EnterMenu();
    void EnterAiAgent();
    void EnterPrayerTimes();
    void EnterAlarms();
    void EnterCalendar();
    void EnterPet();

    void OnNetworkConnected();
    void Tick();

    LauncherMode mode() const { return mode_; }

private:
    Launcher() = default;
    ~Launcher() = default;

    void BuildMenuUi();
    void UpdateMenuUi();
    void UpdateStatusBar();
    void StyleMenuCard(int index, bool selected);

    void StartTickTimer();

    bool initialized_ = false;
    LauncherMode mode_ = LauncherMode::kMenu;
    int selected_menu_item_ = 0; // 0: AI Agent, 1: Prayer times, 2: Alarms, 3: Calendar, 4: Pet

    // Status bar widgets
    lv_obj_t* menu_root_ = nullptr;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* clock_label_ = nullptr;
    lv_obj_t* status_title_ = nullptr;
    lv_obj_t* battery_label_ = nullptr;
    lv_obj_t* wifi_label_ = nullptr;

    // 5 Cards
    struct CardWidget {
        lv_obj_t* card = nullptr;
        lv_obj_t* icon = nullptr;
        lv_obj_t* title = nullptr;
        lv_obj_t* subtitle = nullptr;
    };
    CardWidget cards_[5];
    lv_obj_t* footer_hint_ = nullptr;

    esp_timer_handle_t tick_timer_ = nullptr;
    int tick_count_ = 0;
    int last_fetched_day_ = -1;
};

#endif // LAUNCHER_H
