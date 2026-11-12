#include "pet.h"
#include "pet_sprites.h"

#include <cstdio>
#include <algorithm>
#include <esp_log.h>
#include <esp_timer.h>

#include "board.h"
#include "display.h"
#include "launcher.h"
#include "settings.h"
#include "application.h"
#include "assets/lang_config.h"

#define TAG "Pet"

void Pet::Initialize() {
    ESP_LOGI(TAG, "Virtual Pet (Mochi) Initializing...");
    LoadState();
    DecayStats();
}

void Pet::LoadState() {
    Settings s("pet", false);
    hunger_ = s.GetInt("hunger", 85);
    happiness_ = s.GetInt("happiness", 90);
    cleanliness_ = s.GetInt("cleanliness", 95);
    energy_ = s.GetInt("energy", 80);
    poop_count_ = s.GetInt("poop", 0);
    is_sleeping_ = s.GetInt("sleeping", 0) != 0;
    age_days_ = s.GetInt("age_days", 1);
    last_tick_time_ = static_cast<time_t>(s.GetInt("last_time", 0));

    if (last_tick_time_ == 0) {
        last_tick_time_ = time(nullptr);
    }
}

void Pet::SaveState() {
    Settings s("pet", true);
    s.SetInt("hunger", hunger_);
    s.SetInt("happiness", happiness_);
    s.SetInt("cleanliness", cleanliness_);
    s.SetInt("energy", energy_);
    s.SetInt("poop", poop_count_);
    s.SetInt("sleeping", is_sleeping_ ? 1 : 0);
    s.SetInt("age_days", age_days_);
    s.SetInt("last_time", static_cast<int>(time(nullptr)));
}

void Pet::DecayStats() {
    time_t now = time(nullptr);
    if (last_tick_time_ > 0 && now > last_tick_time_) {
        int elapsed_hours = (now - last_tick_time_) / 3600;
        if (elapsed_hours > 0) {
            // Gentle decay for school schedule
            hunger_ = std::max(15, hunger_ - elapsed_hours * 2);
            happiness_ = std::max(20, happiness_ - elapsed_hours * 2);
            cleanliness_ = std::max(20, cleanliness_ - elapsed_hours * 1);

            if (is_sleeping_) {
                energy_ = std::min(100, energy_ + elapsed_hours * 15);
                if (energy_ >= 95) {
                    is_sleeping_ = false; // Wakes up refreshed
                }
            } else {
                energy_ = std::max(20, energy_ - elapsed_hours * 2);
            }

            if (elapsed_hours >= 12 && hunger_ < 60 && poop_count_ < 2) {
                poop_count_++;
            }

            int days_passed = elapsed_hours / 24;
            if (days_passed > 0) {
                age_days_ += days_passed;
            }
        }
    }
    last_tick_time_ = now;
    SaveState();
}

void Pet::BuildUi() {
    auto* screen = lv_screen_active();

    root_ = lv_obj_create(screen);
    lv_obj_set_size(root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x0a0f1d), 0); // Deep cyber navy
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    // Header Title
    header_title_ = lv_label_create(root_);
    lv_obj_set_width(header_title_, LV_HOR_RES);
    lv_obj_set_pos(header_title_, 0, 6);
    lv_obj_set_style_text_align(header_title_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header_title_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_title_, lv_color_hex(0xfbbf24), 0); // Amber 400
    lv_label_set_text(header_title_, "[ MOCHI THE KITTEN ]");

    // Header Mood
    header_mood_ = lv_label_create(root_);
    lv_obj_set_width(header_mood_, LV_HOR_RES);
    lv_obj_set_pos(header_mood_, 0, 20);
    lv_obj_set_style_text_align(header_mood_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(header_mood_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_mood_, lv_color_hex(0xf472b6), 0); // Pink 400
    lv_label_set_text(header_mood_, "(=^･ω･^=) Purring happily <3");

    // Stat Bars Card
    stats_card_ = lv_obj_create(root_);
    lv_obj_set_size(stats_card_, 224, 46);
    lv_obj_set_pos(stats_card_, 8, 36);
    lv_obj_set_style_radius(stats_card_, 4, 0);
    lv_obj_set_style_border_width(stats_card_, 1, 0);
    lv_obj_set_style_border_color(stats_card_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(stats_card_, lv_color_hex(0x131d31), 0);
    lv_obj_set_style_pad_all(stats_card_, 4, 0);
    lv_obj_remove_flag(stats_card_, LV_OBJ_FLAG_SCROLLABLE);

    // Row 1: Hunger & Happy
    lbl_hunger_ = lv_label_create(stats_card_);
    lv_obj_set_pos(lbl_hunger_, 6, 4);
    lv_obj_set_style_text_font(lbl_hunger_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(lbl_hunger_, lv_color_hex(0xf97316), 0);
    lv_label_set_text(lbl_hunger_, "HNG:100%");

    lbl_happy_ = lv_label_create(stats_card_);
    lv_obj_set_pos(lbl_happy_, 116, 4);
    lv_obj_set_style_text_font(lbl_happy_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(lbl_happy_, lv_color_hex(0xf472b6), 0);
    lv_label_set_text(lbl_happy_, "HAP:100%");

    // Row 2: Clean & Energy
    lbl_clean_ = lv_label_create(stats_card_);
    lv_obj_set_pos(lbl_clean_, 6, 22);
    lv_obj_set_style_text_font(lbl_clean_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(lbl_clean_, lv_color_hex(0x38bdf8), 0);
    lv_label_set_text(lbl_clean_, "CLN:100%");

    lbl_energy_ = lv_label_create(stats_card_);
    lv_obj_set_pos(lbl_energy_, 116, 22);
    lv_obj_set_style_text_font(lbl_energy_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(lbl_energy_, lv_color_hex(0x4ade80), 0);
    lv_label_set_text(lbl_energy_, "NRG:100%");

    // Stage Card (Living Room)
    stage_card_ = lv_obj_create(root_);
    lv_obj_set_size(stage_card_, 224, 134);
    lv_obj_set_pos(stage_card_, 8, 86);
    lv_obj_set_style_radius(stage_card_, 6, 0);
    lv_obj_set_style_border_width(stage_card_, 2, 0);
    lv_obj_set_style_border_color(stage_card_, lv_color_hex(0x38bdf8), 0); // Cyan border
    lv_obj_set_style_bg_color(stage_card_, lv_color_hex(0x0f172a), 0);
    lv_obj_set_style_pad_all(stage_card_, 2, 0);
    lv_obj_remove_flag(stage_card_, LV_OBJ_FLAG_SCROLLABLE);

    // Speech bubble / status message on stage
    speech_bubble_ = lv_label_create(stage_card_);
    lv_obj_set_width(speech_bubble_, 216);
    lv_obj_set_pos(speech_bubble_, 4, 8);
    lv_obj_set_style_text_align(speech_bubble_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(speech_bubble_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(speech_bubble_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(speech_bubble_, "Meow! Welcome home Maryam!");

    // Cat image (Scale 2x = 64x64)
    cat_img_ = lv_image_create(stage_card_);
    lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
    lv_image_set_scale(cat_img_, 512); // 256 * 2 = 2x scale
    lv_obj_align(cat_img_, LV_ALIGN_CENTER, 0, 12);

    // Poop image
    poop_img_ = lv_image_create(stage_card_);
    lv_image_set_src(poop_img_, &pet_poop_dsc);
    lv_image_set_scale(poop_img_, 512);
    lv_obj_set_pos(poop_img_, 16, 84);
    lv_obj_add_flag(poop_img_, LV_OBJ_FLAG_HIDDEN);

    // Minigame element: Play Yarn / Food
    item_img_ = lv_image_create(stage_card_);
    lv_image_set_src(item_img_, &pet_yarn_dsc);
    lv_image_set_scale(item_img_, 512);
    lv_obj_set_pos(item_img_, 100, 70);
    lv_obj_add_flag(item_img_, LV_OBJ_FLAG_HIDDEN);

    // Minigame: Catch Zone Box
    play_zone_box_ = lv_obj_create(stage_card_);
    lv_obj_set_size(play_zone_box_, 190, 24);
    lv_obj_set_pos(play_zone_box_, 17, 98);
    lv_obj_set_style_radius(play_zone_box_, 2, 0);
    lv_obj_set_style_border_width(play_zone_box_, 1, 0);
    lv_obj_set_style_border_color(play_zone_box_, lv_color_hex(0x475569), 0);
    lv_obj_set_style_bg_color(play_zone_box_, lv_color_hex(0x1e293b), 0);
    lv_obj_set_style_pad_all(play_zone_box_, 0, 0);
    lv_obj_add_flag(play_zone_box_, LV_OBJ_FLAG_HIDDEN);

    // Target Sweet Spot inside Catch Zone Box
    play_target_box_ = lv_obj_create(play_zone_box_);
    lv_obj_set_size(play_target_box_, 50, 20);
    lv_obj_set_pos(play_target_box_, 70, 1);
    lv_obj_set_style_radius(play_target_box_, 2, 0);
    lv_obj_set_style_border_width(play_target_box_, 2, 0);
    lv_obj_set_style_border_color(play_target_box_, lv_color_hex(0x22c55e), 0); // Green target
    lv_obj_set_style_bg_color(play_target_box_, lv_color_hex(0x15803d), 0);

    // Actions Row (5 buttons: Feed, Play, Sleep, Clean, Bath)
    actions_box_ = lv_obj_create(root_);
    lv_obj_set_size(actions_box_, 224, 46);
    lv_obj_set_pos(actions_box_, 8, 226);
    lv_obj_set_style_radius(actions_box_, 4, 0);
    lv_obj_set_style_border_width(actions_box_, 1, 0);
    lv_obj_set_style_border_color(actions_box_, lv_color_hex(0x334155), 0);
    lv_obj_set_style_bg_color(actions_box_, lv_color_hex(0x131d31), 0);
    lv_obj_set_style_pad_all(actions_box_, 2, 0);
    lv_obj_remove_flag(actions_box_, LV_OBJ_FLAG_SCROLLABLE);

    static const char* kActionNames[5] = {
        "FEED", "PLAY", "SLEEP", "CLEAN", "BATH"
    };

    int btn_w = 40;
    int btn_spacing = 44;
    for (int i = 0; i < 5; i++) {
        action_btns_[i] = lv_obj_create(actions_box_);
        lv_obj_set_size(action_btns_[i], btn_w, 38);
        lv_obj_set_pos(action_btns_[i], 2 + i * btn_spacing, 2);
        lv_obj_set_style_radius(action_btns_[i], 4, 0);
        lv_obj_set_style_pad_all(action_btns_[i], 0, 0);
        lv_obj_set_style_border_width(action_btns_[i], 1, 0);
        lv_obj_remove_flag(action_btns_[i], LV_OBJ_FLAG_SCROLLABLE);

        action_labels_[i] = lv_label_create(action_btns_[i]);
        lv_obj_set_style_text_font(action_labels_[i], &lv_font_unscii_8, 0);
        lv_obj_align(action_labels_[i], LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(action_labels_[i], kActionNames[i]);
    }

    // Footer Hint
    footer_lbl_ = lv_label_create(root_);
    lv_obj_set_width(footer_lbl_, LV_HOR_RES);
    lv_obj_set_pos(footer_lbl_, 0, 282);
    lv_obj_set_style_text_align(footer_lbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer_lbl_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(footer_lbl_, lv_color_hex(0x94a3b8), 0);
    lv_label_set_text(footer_lbl_, "UP/DN:Action  OK:Choose  HOLD:Menu");

    // Create 150ms Animation / Game Ticker
    tick_timer_ = lv_timer_create([](lv_timer_t* t) {
        Pet::GetInstance().Tick();
    }, 150, nullptr);

    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    UpdateUi();
}

void Pet::Show(bool show) {
    if (root_ == nullptr) return;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (show) {
        DecayStats();
        mode_ = kPetModeRoom;
        selected_action_ = 0;
        lv_label_set_text(speech_bubble_, is_sleeping_ ? "Zzz... Mochi is resting..." : "Meow! Welcome home Maryam!");
        UpdateUi();
        lv_obj_clear_flag(root_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(root_);
    } else {
        SaveState();
        lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    }
}

void Pet::UpdateUi() {
    if (root_ == nullptr) return;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    // Update stat labels
    char buf[32];
    snprintf(buf, sizeof(buf), "HNG:%d%%", hunger_);
    lv_label_set_text(lbl_hunger_, buf);

    snprintf(buf, sizeof(buf), "HAP:%d%%", happiness_);
    lv_label_set_text(lbl_happy_, buf);

    snprintf(buf, sizeof(buf), "CLN:%d%%", cleanliness_);
    lv_label_set_text(lbl_clean_, buf);

    snprintf(buf, sizeof(buf), "NRG:%d%%", energy_);
    lv_label_set_text(lbl_energy_, buf);

    // Poop display
    if (poop_count_ > 0) {
        lv_obj_clear_flag(poop_img_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(poop_img_, LV_OBJ_FLAG_HIDDEN);
    }

    // Cat Sprite based on state
    if (mode_ == kPetModeRoom) {
        if (is_sleeping_) {
            lv_image_set_src(cat_img_, &pet_cat_sleep_dsc);
            lv_label_set_text(header_mood_, "(-.-)zZ Fast asleep...");
        } else if (happiness_ >= 80) {
            lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
            lv_label_set_text(header_mood_, "(=^･ω･^=) Purring happily <3");
        } else {
            lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
            lv_label_set_text(header_mood_, "(=^.^=) Waiting for pets!");
        }
    }

    // Highlight selected action button
    for (int i = 0; i < 5; i++) {
        if (i == selected_action_) {
            lv_obj_set_style_bg_color(action_btns_[i], lv_color_hex(0x0284c7), 0); // Bright sky cyan
            lv_obj_set_style_border_color(action_btns_[i], lv_color_hex(0x38bdf8), 0);
            lv_obj_set_style_border_width(action_btns_[i], 2, 0);
            lv_obj_set_style_text_color(action_labels_[i], lv_color_hex(0xffffff), 0);
        } else {
            lv_obj_set_style_bg_color(action_btns_[i], lv_color_hex(0x1e293b), 0);
            lv_obj_set_style_border_color(action_btns_[i], lv_color_hex(0x475569), 0);
            lv_obj_set_style_border_width(action_btns_[i], 1, 0);
            lv_obj_set_style_text_color(action_labels_[i], lv_color_hex(0x94a3b8), 0);
        }
    }
}

void Pet::Tick() {
    if (root_ == nullptr || lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN)) return;

    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    static int frame_count = 0;
    frame_count++;

    // In Play Minigame mode: animate the bouncing yarn ball
    if (mode_ == kPetModePlay) {
        yarn_pos_ += yarn_dir_;
        if (yarn_pos_ >= 170) {
            yarn_pos_ = 170;
            yarn_dir_ = -8;
        } else if (yarn_pos_ <= 20) {
            yarn_pos_ = 20;
            yarn_dir_ = 8;
        }
        lv_obj_set_pos(item_img_, yarn_pos_, 82);
        return;
    }

    // In Sleep Lullaby mode: pulse the gentle heart rhythm
    if (mode_ == kPetModeSleep) {
        if ((frame_count % 6) == 0) {
            lv_image_set_scale(cat_img_, 512 + (frame_count % 12 ? 30 : 0));
        }
        return;
    }

    // In normal room mode: subtle breathing / tail flick every 2 seconds
    if (!is_sleeping_ && mode_ == kPetModeRoom) {
        if ((frame_count % 16) == 0) {
            lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
        } else if ((frame_count % 16) == 4) {
            lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
        }
    }
}

void Pet::DoFeed() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (hunger_ >= 100) {
        lv_label_set_text(speech_bubble_, "Mochi is already full! *burp* <3");
        return;
    }

    if (is_sleeping_) {
        is_sleeping_ = false;
    }

    hunger_ = std::min(100, hunger_ + 35);
    happiness_ = std::min(100, happiness_ + 10);

    lv_image_set_src(cat_img_, &pet_cat_eat_dsc);
    lv_label_set_text(speech_bubble_, "Crunch crunch! Tasty fish! <3");

    // Play happy chime at 100% volume
    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec != nullptr) {
        codec->SetOutputVolume(100);
        codec->EnableOutput(true);
    }
    Application::GetInstance().Schedule([]() {
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
    });

    UpdateUi();
    SaveState();
}

void Pet::StartPlay() {
    mode_ = kPetModePlay;
    play_score_ = 0;
    play_round_ = 0;
    yarn_pos_ = 30;
    yarn_dir_ = 10;

    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (is_sleeping_) {
        is_sleeping_ = false;
    }

    lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
    lv_image_set_src(item_img_, &pet_yarn_dsc);
    lv_obj_clear_flag(item_img_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(play_zone_box_, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(speech_bubble_, "CATCH THE YARN! Press OK in GREEN zone!");
    lv_label_set_text(footer_lbl_, "Press OK to Catch!  HOLD: Back");
}

void Pet::HandlePlayButton() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    // Target box is from X=80 to X=130 in stage coords
    if (yarn_pos_ >= 70 && yarn_pos_ <= 125) {
        // Success catch!
        play_score_++;
        happiness_ = std::min(100, happiness_ + 25);
        energy_ = std::max(10, energy_ - 5);

        char buf[64];
        snprintf(buf, sizeof(buf), "PERFECT CATCH! +25 HAPPY! (%d/3)", play_score_);
        lv_label_set_text(speech_bubble_, buf);

        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec != nullptr) {
            codec->SetOutputVolume(100);
            codec->EnableOutput(true);
        }
        Application::GetInstance().Schedule([]() {
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
        });

        if (play_score_ >= 3) {
            // Finished minigame!
            mode_ = kPetModeRoom;
            lv_obj_add_flag(item_img_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(play_zone_box_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(speech_bubble_, "Maryam won! Mochi loves playing! <3");
            lv_label_set_text(footer_lbl_, "UP/DN:Action  OK:Choose  HOLD:Menu");
            UpdateUi();
            SaveState();
        }
    } else {
        lv_label_set_text(speech_bubble_, "Too fast! Watch the green zone! :3");
    }
}

void Pet::StartSleep() {
    mode_ = kPetModeSleep;
    sleep_pats_ = 0;
    last_pat_time_ms_ = 0;

    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    lv_image_set_src(cat_img_, &pet_cat_sleep_dsc);
    lv_label_set_text(speech_bubble_, "Gently tap OK to pat Mochi to sleep (1s beat)");
    lv_label_set_text(footer_lbl_, "Tap OK in rhythm [ 0 / 5 ]  HOLD: Back");
}

void Pet::HandleSleepButton() {
    int64_t now_ms = esp_timer_get_time() / 1000;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (last_pat_time_ms_ > 0) {
        int64_t diff = now_ms - last_pat_time_ms_;
        // Good gentle rhythm is between 600ms and 1500ms
        if (diff < 500) {
            lv_label_set_text(speech_bubble_, "Too fast! Gently... soft lullaby...");
            return;
        }
    }
    last_pat_time_ms_ = now_ms;
    sleep_pats_++;

    char buf[64];
    snprintf(buf, sizeof(buf), "*pat pat* ... (purrrrr) [ %d / 5 ]", sleep_pats_);
    lv_label_set_text(speech_bubble_, buf);

    snprintf(buf, sizeof(buf), "Tap OK in rhythm [ %d / 5 ]  HOLD: Back", sleep_pats_);
    lv_label_set_text(footer_lbl_, buf);

    if (sleep_pats_ >= 5) {
        // Asleep!
        is_sleeping_ = true;
        mode_ = kPetModeRoom;
        energy_ = std::min(100, energy_ + 30);
        happiness_ = std::min(100, happiness_ + 10);

        lv_label_set_text(speech_bubble_, "Mochi is fast asleep! Zzz... Sweet dreams!");
        lv_label_set_text(footer_lbl_, "UP/DN:Action  OK:Choose  HOLD:Menu");

        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec != nullptr) {
            codec->SetOutputVolume(100);
            codec->EnableOutput(true);
        }
        Application::GetInstance().Schedule([]() {
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
        });

        UpdateUi();
        SaveState();
    }
}

void Pet::DoClean() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (poop_count_ > 0) {
        poop_count_ = 0;
        cleanliness_ = 100;
        happiness_ = std::min(100, happiness_ + 15);
        lv_label_set_text(speech_bubble_, "*SWEEP!* Litter is spotless clean! *");
        lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
    } else {
        cleanliness_ = 100;
        lv_label_set_text(speech_bubble_, "The room is already sparkling clean! <3");
    }

    UpdateUi();
    SaveState();
}

void Pet::StartBath() {
    mode_ = kPetModeBath;
    bath_step_ = 0;

    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (is_sleeping_) is_sleeping_ = false;

    lv_image_set_src(cat_img_, &pet_cat_bath_dsc);
    lv_label_set_text(speech_bubble_, "Press OK to add warm water & soap suds!");
    lv_label_set_text(footer_lbl_, "Press OK: Step 1 / 3  HOLD: Back");
}

void Pet::HandleBathButton() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    bath_step_++;
    if (bath_step_ == 1) {
        lv_label_set_text(speech_bubble_, "Scrub scrub! Fluffy bubbles everywhere! <3");
        lv_label_set_text(footer_lbl_, "Press OK: Step 2 / 3 (Rinse!)");
    } else if (bath_step_ >= 2) {
        cleanliness_ = 100;
        happiness_ = std::min(100, happiness_ + 20);
        mode_ = kPetModeRoom;

        lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
        lv_label_set_text(speech_bubble_, "*SHAKE!* Mochi is squeaky clean! * * *");
        lv_label_set_text(footer_lbl_, "UP/DN:Action  OK:Choose  HOLD:Menu");

        auto codec = Board::GetInstance().GetAudioCodec();
        if (codec != nullptr) {
            codec->SetOutputVolume(100);
            codec->EnableOutput(true);
        }
        Application::GetInstance().Schedule([]() {
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
        });

        UpdateUi();
        SaveState();
    }
}

bool Pet::HandleButton(LauncherButton button, LauncherButtonEvent event) {
    if (root_ == nullptr || lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }

    // Long press OK returns to Menu
    if (button == LauncherButton::kOk && event == LauncherButtonEvent::kLongPress) {
        if (mode_ != kPetModeRoom) {
            // Return to room first
            mode_ = kPetModeRoom;
            auto* display = Board::GetInstance().GetDisplay();
            DisplayLockGuard lock(display);
            lv_obj_add_flag(item_img_, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(play_zone_box_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(footer_lbl_, "UP/DN:Action  OK:Choose  HOLD:Menu");
            UpdateUi();
            return true;
        } else {
            Launcher::GetInstance().EnterMenu();
            return true;
        }
    }

    if (event != LauncherButtonEvent::kClick) {
        return false;
    }

    // In Minigame modes:
    if (mode_ == kPetModePlay) {
        if (button == LauncherButton::kOk) {
            HandlePlayButton();
        }
        return true;
    }

    if (mode_ == kPetModeSleep) {
        if (button == LauncherButton::kOk) {
            HandleSleepButton();
        }
        return true;
    }

    if (mode_ == kPetModeBath) {
        if (button == LauncherButton::kOk) {
            HandleBathButton();
        }
        return true;
    }

    // In Main Room mode: navigate 5 actions
    if (button == LauncherButton::kUp) {
        selected_action_ = (selected_action_ - 1 + 5) % 5;
        UpdateUi();
        return true;
    }

    if (button == LauncherButton::kDown) {
        selected_action_ = (selected_action_ + 1) % 5;
        UpdateUi();
        return true;
    }

    if (button == LauncherButton::kOk) {
        switch (selected_action_) {
            case 0: DoFeed(); break;
            case 1: StartPlay(); break;
            case 2: StartSleep(); break;
            case 3: DoClean(); break;
            case 4: StartBath(); break;
        }
        return true;
    }

    return false;
}
