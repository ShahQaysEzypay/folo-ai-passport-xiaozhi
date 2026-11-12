#ifndef PET_H
#define PET_H

#include <lvgl.h>
#include <string>
#include <ctime>

enum class LauncherButton;
enum class LauncherButtonEvent;

enum PetMode {
    kPetModeRoom,
    kPetModePlay,
    kPetModeSleep,
    kPetModeBath,
};

class Pet {
public:
    static Pet& GetInstance() {
        static Pet instance;
        return instance;
    }

    void Initialize();
    void BuildUi();
    void Show(bool show);
    void UpdateUi();
    void Tick();

    bool HandleButton(LauncherButton button, LauncherButtonEvent event);

private:
    Pet() = default;
    ~Pet() = default;

    void LoadState();
    void SaveState();
    void DecayStats();

    void DoFeed();
    void StartPlay();
    void UpdatePlay();
    void HandlePlayButton();

    void StartSleep();
    void HandleSleepButton();

    void StartBath();
    void HandleBathButton();

    void DoClean();

    // UI elements
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_title_ = nullptr;
    lv_obj_t* header_mood_ = nullptr;

    // Stat bars
    lv_obj_t* stats_card_ = nullptr;
    lv_obj_t* bar_hunger_ = nullptr;
    lv_obj_t* bar_happy_ = nullptr;
    lv_obj_t* bar_clean_ = nullptr;
    lv_obj_t* bar_energy_ = nullptr;
    lv_obj_t* lbl_hunger_ = nullptr;
    lv_obj_t* lbl_happy_ = nullptr;
    lv_obj_t* lbl_clean_ = nullptr;
    lv_obj_t* lbl_energy_ = nullptr;

    // Room stage
    lv_obj_t* stage_card_ = nullptr;
    lv_obj_t* cat_img_ = nullptr;
    lv_obj_t* poop_img_ = nullptr;
    lv_obj_t* item_img_ = nullptr;
    lv_obj_t* speech_bubble_ = nullptr;

    // Play minigame
    lv_obj_t* play_zone_box_ = nullptr;
    lv_obj_t* play_target_box_ = nullptr;
    lv_obj_t* play_yarn_img_ = nullptr;
    lv_obj_t* play_score_lbl_ = nullptr;

    // Sleep minigame
    lv_obj_t* sleep_heart_lbl_ = nullptr;
    lv_obj_t* sleep_progress_lbl_ = nullptr;

    // Actions row
    lv_obj_t* actions_box_ = nullptr;
    lv_obj_t* action_btns_[5] = {nullptr};
    lv_obj_t* action_labels_[5] = {nullptr};

    // Footer
    lv_obj_t* footer_lbl_ = nullptr;

    // Pet State
    int hunger_ = 85;       // 0 - 100
    int happiness_ = 90;    // 0 - 100
    int cleanliness_ = 95;  // 0 - 100
    int energy_ = 80;       // 0 - 100
    int poop_count_ = 0;    // 0 - 3
    bool is_sleeping_ = false;
    time_t last_tick_time_ = 0;
    int age_days_ = 1;

    PetMode mode_ = kPetModeRoom;
    int selected_action_ = 0; // 0: Feed, 1: Play, 2: Sleep, 3: Clean, 4: Bath

    // Play minigame state
    int yarn_pos_ = 50;
    int yarn_dir_ = 8;
    int play_round_ = 0;
    int play_score_ = 0;

    // Sleep rhythm state
    int sleep_pats_ = 0;
    int64_t last_pat_time_ms_ = 0;

    // Bath state
    int bath_step_ = 0;

    // LVGL timer
    lv_timer_t* tick_timer_ = nullptr;
};

#endif // PET_H
