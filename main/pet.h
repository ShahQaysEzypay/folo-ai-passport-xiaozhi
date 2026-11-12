#ifndef PET_H
#define PET_H

#include <lvgl.h>
#include <string>
#include <ctime>

enum class LauncherButton;
enum class LauncherButtonEvent;

enum PetMode {
    kPetModeRoom,
    kPetModeFeedSelect,
    kPetModePlaySelect,
    kPetModePlayCatch,
    kPetModePlayDance,
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
    void AddXp(int amount);
    void PlayChime();

    // Feed
    void OpenFeedMenu();
    void ConfirmFeed();

    // Play
    void OpenPlayMenu();
    void StartCatchGame();
    void HandleCatchButton();
    void StartDanceGame();
    void HandleDanceButton(LauncherButton button);

    // Sleep
    void StartSleep();
    void HandleSleepButton();

    // Bath
    void StartBath();
    void HandleBathButton();

    // Clean
    void DoClean();

    // UI elements
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_card_ = nullptr;
    lv_obj_t* header_title_ = nullptr;
    lv_obj_t* header_mood_ = nullptr;

    // Stat bars card
    lv_obj_t* stats_card_ = nullptr;
    lv_obj_t* bar_hunger_ = nullptr;
    lv_obj_t* bar_happy_ = nullptr;
    lv_obj_t* bar_clean_ = nullptr;
    lv_obj_t* bar_energy_ = nullptr;

    // Room stage
    lv_obj_t* stage_card_ = nullptr;
    lv_obj_t* floor_strip_ = nullptr;
    lv_obj_t* window_box_ = nullptr;
    lv_obj_t* window_decor_ = nullptr;
    lv_obj_t* wall_art_ = nullptr;
    lv_obj_t* wall_heart_ = nullptr;
    lv_obj_t* speech_box_ = nullptr;
    lv_obj_t* speech_bubble_ = nullptr;

    lv_obj_t* cat_img_ = nullptr;
    lv_obj_t* poop_img_ = nullptr;
    lv_obj_t* treat_img_ = nullptr;

    // Interactive Sub-mode Panel inside stage
    lv_obj_t* sub_panel_ = nullptr;
    lv_obj_t* sub_label_ = nullptr;
    lv_obj_t* catch_bar_bg_ = nullptr;
    lv_obj_t* catch_target_ = nullptr;
    lv_obj_t* catch_cursor_ = nullptr;
    lv_obj_t* rhythm_bar_ = nullptr;

    // Actions row
    lv_obj_t* actions_box_ = nullptr;
    lv_obj_t* action_btns_[5] = {nullptr};
    lv_obj_t* action_labels_[5] = {nullptr};

    // Footer
    lv_obj_t* footer_lbl_ = nullptr;

    // Pet Persistent State
    int hunger_ = 85;       // 0 - 100
    int happiness_ = 90;    // 0 - 100
    int cleanliness_ = 95;  // 0 - 100
    int energy_ = 85;       // 0 - 100
    int poop_count_ = 0;    // 0 - 3
    int feed_counter_ = 0;  // Spawns poop every 2 feeds
    int xp_ = 0;            // Friendship XP
    int level_ = 1;         // Friendship Level
    bool is_sleeping_ = false;
    time_t last_tick_time_ = 0;

    // Runtime UI State
    PetMode mode_ = kPetModeRoom;
    int selected_action_ = 0; // 0: Feed, 1: Play, 2: Sleep, 3: Clean, 4: Bath
    int menu_choice_ = 0;     // Sub-menu index for Feed / Play
    int anim_ticks_ = 0;      // Temporary animation countdown (eating/cleaning)
    int tick_counter_ = 0;

    // Catch minigame state
    int yarn_pos_ = 10;       // 0 .. 168
    int yarn_dir_ = 6;
    int play_score_ = 0;

    // Dance minigame state
    int dance_moves_[3] = {0, 1, 2}; // 0: UP, 1: DOWN, 2: OK
    int dance_step_ = 0;
    int dance_round_ = 0;

    // Sleep rhythm state
    int sleep_pats_ = 0;
    int rhythm_val_ = 0;      // 0 .. 100
    int rhythm_dir_ = 5;
    int64_t last_pat_time_ms_ = 0;

    // Bath state
    int bath_step_ = 0;

    lv_timer_t* tick_timer_ = nullptr;
};

#endif // PET_H
