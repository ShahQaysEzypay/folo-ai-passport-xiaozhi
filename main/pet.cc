#include "pet.h"
#include "pet_sprites.h"

#include <cstdio>
#include <algorithm>
#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#include "board.h"
#include "display.h"
#include "launcher.h"
#include "settings.h"
#include "application.h"
#include "assets/lang_config.h"

#define TAG "Pet"

namespace {
lv_obj_t* CreateStatBar(lv_obj_t* parent, int x, int y, const char* name, uint32_t color_hex) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_obj_set_pos(lbl, x, y);
    lv_obj_set_style_text_font(lbl, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xfdf2f8), 0);
    lv_label_set_text(lbl, name);

    lv_obj_t* bar = lv_bar_create(parent);
    lv_obj_set_size(bar, 58, 8);
    lv_obj_set_pos(bar, x + 38, y);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x3b1945), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x701a75), LV_PART_MAIN);

    lv_obj_set_style_radius(bar, 3, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(color_hex), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    return bar;
}
} // namespace

void Pet::Initialize() {
    ESP_LOGI(TAG, "Sanrio Kawaii Virtual Pet (Mochi) Initializing...");
    LoadState();
    DecayStats();
}

void Pet::LoadState() {
    Settings s("pet", false);
    hunger_ = s.GetInt("hunger", 85);
    happiness_ = s.GetInt("happiness", 90);
    cleanliness_ = s.GetInt("cleanliness", 95);
    energy_ = s.GetInt("energy", 85);
    poop_count_ = s.GetInt("poop", 0);
    feed_counter_ = s.GetInt("feeds", 0);
    xp_ = s.GetInt("xp", 0);
    level_ = s.GetInt("level", 1);
    is_sleeping_ = s.GetInt("sleeping", 0) != 0;
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
    s.SetInt("feeds", feed_counter_);
    s.SetInt("xp", xp_);
    s.SetInt("level", level_);
    s.SetInt("sleeping", is_sleeping_ ? 1 : 0);
    s.SetInt("last_time", static_cast<int>(time(nullptr)));
}

void Pet::AddXp(int amount) {
    xp_ += amount;
    if (xp_ >= 50) {
        xp_ -= 50;
        level_++;
    }
}

void Pet::PlayChime() {
    auto codec = Board::GetInstance().GetAudioCodec();
    if (codec != nullptr) {
        codec->SetOutputVolume(100);
        codec->EnableOutput(true);
    }
    Application::GetInstance().Schedule([]() {
        Application::GetInstance().PlaySound(Lang::Sounds::OGG_SUCCESS);
    });
}

void Pet::DecayStats() {
    time_t now = time(nullptr);
    if (last_tick_time_ > 0 && now > last_tick_time_) {
        int elapsed_hours = (now - last_tick_time_) / 3600;
        if (elapsed_hours > 0) {
            // Gentle school-friendly decay (~35-48h to drop low, never dies)
            hunger_ = std::max(20, hunger_ - elapsed_hours * 2);
            happiness_ = std::max(25, happiness_ - elapsed_hours * 2);
            cleanliness_ = std::max(25, cleanliness_ - elapsed_hours * 1);

            if (is_sleeping_) {
                energy_ = std::min(100, energy_ + elapsed_hours * 20);
                if (energy_ >= 95) {
                    is_sleeping_ = false;
                }
            } else {
                energy_ = std::max(20, energy_ - elapsed_hours * 2);
            }

            if (elapsed_hours >= 8 && poop_count_ < 2) {
                poop_count_++;
            }
        }
    }
    last_tick_time_ = now;
    SaveState();
}

void Pet::BuildUi() {
    auto* screen = lv_screen_active();

    // Kuromi / Sanrio Deep Berry-Plum Outer Shell
    root_ = lv_obj_create(screen);
    lv_obj_set_size(root_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(0x1f102b), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    // 1. Top Kawaii Header Card (Y=4..32)
    header_card_ = lv_obj_create(root_);
    lv_obj_set_size(header_card_, 224, 28);
    lv_obj_set_pos(header_card_, 8, 4);
    lv_obj_set_style_radius(header_card_, 6, 0);
    lv_obj_set_style_border_width(header_card_, 2, 0);
    lv_obj_set_style_border_color(header_card_, lv_color_hex(0xf472b6), 0); // Kawaii Pink
    lv_obj_set_style_bg_color(header_card_, lv_color_hex(0x3b1945), 0);
    lv_obj_set_style_pad_all(header_card_, 0, 0);
    lv_obj_remove_flag(header_card_, LV_OBJ_FLAG_SCROLLABLE);

    header_title_ = lv_label_create(header_card_);
    lv_obj_align(header_title_, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_text_font(header_title_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_title_, lv_color_hex(0xfde047), 0); // Star Gold
    lv_label_set_text(header_title_, "MOCHI LV.1");

    header_mood_ = lv_label_create(header_card_);
    lv_obj_align(header_mood_, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_text_font(header_mood_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(header_mood_, lv_color_hex(0xff9ebb), 0); // Soft Pink
    lv_label_set_text(header_mood_, "HAPPY <3");

    // 2. Graphical Stat Bars Card (Y=35..73)
    stats_card_ = lv_obj_create(root_);
    lv_obj_set_size(stats_card_, 224, 38);
    lv_obj_set_pos(stats_card_, 8, 35);
    lv_obj_set_style_radius(stats_card_, 6, 0);
    lv_obj_set_style_border_width(stats_card_, 1, 0);
    lv_obj_set_style_border_color(stats_card_, lv_color_hex(0xc084fc), 0); // Kuromi Lavender
    lv_obj_set_style_bg_color(stats_card_, lv_color_hex(0x2d143b), 0);
    lv_obj_set_style_pad_all(stats_card_, 0, 0);
    lv_obj_remove_flag(stats_card_, LV_OBJ_FLAG_SCROLLABLE);

    bar_hunger_ = CreateStatBar(stats_card_, 8,   6,  "FOOD", 0xfb923c); // Orange-Pink
    bar_happy_  = CreateStatBar(stats_card_, 116, 6,  "LOVE", 0xff69aa); // Ribbon Pink
    bar_clean_  = CreateStatBar(stats_card_, 8,   21, "BATH", 0x38bdf8); // Cinnamoroll Blue
    bar_energy_ = CreateStatBar(stats_card_, 116, 21, "REST", 0xa855f7); // Lavender Purple

    // 3. Cozy Sanrio Room Stage (Y=76..226, height=150)
    stage_card_ = lv_obj_create(root_);
    lv_obj_set_size(stage_card_, 224, 150);
    lv_obj_set_pos(stage_card_, 8, 76);
    lv_obj_set_style_radius(stage_card_, 8, 0);
    lv_obj_set_style_border_width(stage_card_, 2, 0);
    lv_obj_set_style_border_color(stage_card_, lv_color_hex(0xff85b4), 0);
    lv_obj_set_style_bg_color(stage_card_, lv_color_hex(0xfdf2f8), 0); // Matches sprite BG_DAY (#FDF2F8)
    lv_obj_set_style_pad_all(stage_card_, 0, 0);
    lv_obj_remove_flag(stage_card_, LV_OBJ_FLAG_SCROLLABLE);

    // Pastel Floor Wainscoting Strip at bottom of room
    floor_strip_ = lv_obj_create(stage_card_);
    lv_obj_set_size(floor_strip_, 220, 42);
    lv_obj_set_pos(floor_strip_, 0, 104);
    lv_obj_set_style_radius(floor_strip_, 0, 0);
    lv_obj_set_style_border_width(floor_strip_, 1, 0);
    lv_obj_set_style_border_side(floor_strip_, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(floor_strip_, lv_color_hex(0xfbcfe8), 0);
    lv_obj_set_style_bg_color(floor_strip_, lv_color_hex(0xfdf2f8), 0);
    lv_obj_remove_flag(floor_strip_, LV_OBJ_FLAG_SCROLLABLE);

    // Left Wall: Cute Window (Sky Blue by day, Starry Indigo at night)
    window_box_ = lv_obj_create(stage_card_);
    lv_obj_set_size(window_box_, 34, 34);
    lv_obj_set_pos(window_box_, 8, 30);
    lv_obj_set_style_radius(window_box_, 6, 0);
    lv_obj_set_style_border_width(window_box_, 2, 0);
    lv_obj_set_style_border_color(window_box_, lv_color_hex(0xf472b6), 0);
    lv_obj_set_style_bg_color(window_box_, lv_color_hex(0xbae6fd), 0);
    lv_obj_set_style_pad_all(window_box_, 0, 0);
    lv_obj_remove_flag(window_box_, LV_OBJ_FLAG_SCROLLABLE);

    window_decor_ = lv_label_create(window_box_);
    lv_obj_center(window_decor_);
    lv_obj_set_style_text_font(window_decor_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(window_decor_, lv_color_hex(0xffffff), 0);
    lv_label_set_text(window_decor_, "(*)");

    // Right Wall: Cute Framed Heart Art
    wall_art_ = lv_obj_create(stage_card_);
    lv_obj_set_size(wall_art_, 32, 28);
    lv_obj_set_pos(wall_art_, 180, 32);
    lv_obj_set_style_radius(wall_art_, 4, 0);
    lv_obj_set_style_border_width(wall_art_, 2, 0);
    lv_obj_set_style_border_color(wall_art_, lv_color_hex(0xc084fc), 0);
    lv_obj_set_style_bg_color(wall_art_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_pad_all(wall_art_, 0, 0);
    lv_obj_remove_flag(wall_art_, LV_OBJ_FLAG_SCROLLABLE);

    wall_heart_ = lv_label_create(wall_art_);
    lv_obj_center(wall_heart_);
    lv_obj_set_style_text_font(wall_heart_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(wall_heart_, lv_color_hex(0xff69aa), 0);
    lv_label_set_text(wall_heart_, "<3");

    // Top Speech Bubble Pill Banner inside Stage
    speech_box_ = lv_obj_create(stage_card_);
    lv_obj_set_size(speech_box_, 210, 20);
    lv_obj_set_pos(speech_box_, 5, 4);
    lv_obj_set_style_radius(speech_box_, 10, 0);
    lv_obj_set_style_border_width(speech_box_, 1, 0);
    lv_obj_set_style_border_color(speech_box_, lv_color_hex(0xf472b6), 0);
    lv_obj_set_style_bg_color(speech_box_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_pad_all(speech_box_, 0, 0);
    lv_obj_remove_flag(speech_box_, LV_OBJ_FLAG_SCROLLABLE);

    speech_bubble_ = lv_label_create(speech_box_);
    lv_obj_set_width(speech_bubble_, 204);
    lv_obj_center(speech_bubble_);
    lv_obj_set_style_text_align(speech_bubble_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(speech_bubble_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(speech_bubble_, lv_color_hex(0x3b1945), 0);
    lv_label_set_text(speech_bubble_, "Meow~ Hi Maryam! <3");

    // Mochi Kawaii Cat Sprite (Native 104x80)
    cat_img_ = lv_image_create(stage_card_);
    lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
    lv_obj_set_pos(cat_img_, 58, 28);

    // Cute Pastel Swirl Poop Sprite (Native 28x28)
    poop_img_ = lv_image_create(stage_card_);
    lv_image_set_src(poop_img_, &pet_poop_dsc);
    lv_obj_set_pos(poop_img_, 14, 78);
    lv_obj_add_flag(poop_img_, LV_OBJ_FLAG_HIDDEN);

    // Cute Food/Treat Icon on right side of rug (Native 28x28)
    treat_img_ = lv_image_create(stage_card_);
    lv_image_set_src(treat_img_, &pet_fish_dsc);
    lv_obj_set_pos(treat_img_, 174, 78);
    lv_obj_add_flag(treat_img_, LV_OBJ_FLAG_HIDDEN);

    // Interactive Sub-mode Panel at bottom of stage (Y=110..143)
    sub_panel_ = lv_obj_create(stage_card_);
    lv_obj_set_size(sub_panel_, 210, 32);
    lv_obj_set_pos(sub_panel_, 5, 110);
    lv_obj_set_style_radius(sub_panel_, 6, 0);
    lv_obj_set_style_border_width(sub_panel_, 2, 0);
    lv_obj_set_style_border_color(sub_panel_, lv_color_hex(0x9333ea), 0);
    lv_obj_set_style_bg_color(sub_panel_, lv_color_hex(0x3b1945), 0);
    lv_obj_set_style_pad_all(sub_panel_, 0, 0);
    lv_obj_remove_flag(sub_panel_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);

    sub_label_ = lv_label_create(sub_panel_);
    lv_obj_set_width(sub_label_, 202);
    lv_obj_center(sub_label_);
    lv_obj_set_style_text_align(sub_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(sub_label_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(sub_label_, lv_color_hex(0xfde047), 0);
    lv_label_set_text(sub_label_, "");

    // Catch Game Bar inside sub_panel_
    catch_bar_bg_ = lv_obj_create(sub_panel_);
    lv_obj_set_size(catch_bar_bg_, 192, 22);
    lv_obj_center(catch_bar_bg_);
    lv_obj_set_style_radius(catch_bar_bg_, 4, 0);
    lv_obj_set_style_border_width(catch_bar_bg_, 1, 0);
    lv_obj_set_style_border_color(catch_bar_bg_, lv_color_hex(0xf472b6), 0);
    lv_obj_set_style_bg_color(catch_bar_bg_, lv_color_hex(0x1f102b), 0);
    lv_obj_set_style_pad_all(catch_bar_bg_, 0, 0);
    lv_obj_remove_flag(catch_bar_bg_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(catch_bar_bg_, LV_OBJ_FLAG_HIDDEN);

    // Sweet Spot Target in center of Catch Bar (X=70..122)
    catch_target_ = lv_obj_create(catch_bar_bg_);
    lv_obj_set_size(catch_target_, 52, 18);
    lv_obj_set_pos(catch_target_, 70, 1);
    lv_obj_set_style_radius(catch_target_, 3, 0);
    lv_obj_set_style_border_width(catch_target_, 2, 0);
    lv_obj_set_style_border_color(catch_target_, lv_color_hex(0x4ade80), 0);
    lv_obj_set_style_bg_color(catch_target_, lv_color_hex(0x15803d), 0);

    // Moving Pink Yarn Cursor inside Catch Bar
    catch_cursor_ = lv_obj_create(catch_bar_bg_);
    lv_obj_set_size(catch_cursor_, 16, 16);
    lv_obj_set_pos(catch_cursor_, 10, 2);
    lv_obj_set_style_radius(catch_cursor_, 8, 0);
    lv_obj_set_style_border_width(catch_cursor_, 2, 0);
    lv_obj_set_style_border_color(catch_cursor_, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_color(catch_cursor_, lv_color_hex(0xff69aa), 0);

    // Rhythm / Progress Bar for Sleep & Bath inside sub_panel_
    rhythm_bar_ = lv_bar_create(sub_panel_);
    lv_obj_set_size(rhythm_bar_, 190, 16);
    lv_obj_center(rhythm_bar_);
    lv_bar_set_range(rhythm_bar_, 0, 100);
    lv_obj_set_style_radius(rhythm_bar_, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(rhythm_bar_, lv_color_hex(0x1f102b), LV_PART_MAIN);
    lv_obj_set_style_border_width(rhythm_bar_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(rhythm_bar_, lv_color_hex(0xc084fc), LV_PART_MAIN);
    lv_obj_set_style_radius(rhythm_bar_, 5, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(rhythm_bar_, lv_color_hex(0xf472b6), LV_PART_INDICATOR);
    lv_obj_add_flag(rhythm_bar_, LV_OBJ_FLAG_HIDDEN);

    // 4. Bottom Action Dock (Y=230..272)
    actions_box_ = lv_obj_create(root_);
    lv_obj_set_size(actions_box_, 224, 42);
    lv_obj_set_pos(actions_box_, 8, 230);
    lv_obj_set_style_radius(actions_box_, 6, 0);
    lv_obj_set_style_border_width(actions_box_, 1, 0);
    lv_obj_set_style_border_color(actions_box_, lv_color_hex(0xf472b6), 0);
    lv_obj_set_style_bg_color(actions_box_, lv_color_hex(0x2d143b), 0);
    lv_obj_set_style_pad_all(actions_box_, 2, 0);
    lv_obj_remove_flag(actions_box_, LV_OBJ_FLAG_SCROLLABLE);

    static const char* kActionNames[5] = {
        "FEED", "PLAY", "SLEEP", "CLEAN", "BATH"
    };

    for (int i = 0; i < 5; i++) {
        action_btns_[i] = lv_obj_create(actions_box_);
        lv_obj_set_size(action_btns_[i], 41, 34);
        lv_obj_set_pos(action_btns_[i], 2 + i * 44, 2);
        lv_obj_set_style_radius(action_btns_[i], 5, 0);
        lv_obj_set_style_pad_all(action_btns_[i], 0, 0);
        lv_obj_set_style_border_width(action_btns_[i], 1, 0);
        lv_obj_remove_flag(action_btns_[i], LV_OBJ_FLAG_SCROLLABLE);

        action_labels_[i] = lv_label_create(action_btns_[i]);
        lv_obj_set_style_text_font(action_labels_[i], &lv_font_unscii_8, 0);
        lv_obj_align(action_labels_[i], LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(action_labels_[i], kActionNames[i]);
    }

    // 5. Footer Navigation Hint (Y=278..314)
    footer_lbl_ = lv_label_create(root_);
    lv_obj_set_width(footer_lbl_, LV_HOR_RES);
    lv_obj_set_pos(footer_lbl_, 0, 280);
    lv_obj_set_style_text_align(footer_lbl_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(footer_lbl_, &lv_font_unscii_8, 0);
    lv_obj_set_style_text_color(footer_lbl_, lv_color_hex(0xe9d5ff), 0);
    lv_label_set_text(footer_lbl_, "UP/DN:Move  OK:Pick\nHOLD OK:Back to Menu");

    // 60ms Smooth Animation & Minigame Ticker (~16.6 FPS)
    tick_timer_ = lv_timer_create([](lv_timer_t* t) {
        Pet::GetInstance().Tick();
    }, 60, nullptr);

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
        anim_ticks_ = 0;
        lv_obj_add_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(treat_img_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(speech_bubble_, is_sleeping_ ? "Zzz... Sleeping on cloud..." : "Meow~ Hi Maryam! <3");
        lv_label_set_text(footer_lbl_, "UP/DN:Move  OK:Pick\nHOLD OK:Back to Menu");
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

    // Header Title + Friendship Level
    char hbuf[32];
    snprintf(hbuf, sizeof(hbuf), "MOCHI LV.%d", level_);
    lv_label_set_text(header_title_, hbuf);

    // Update graphical progress bars
    lv_bar_set_value(bar_hunger_, hunger_, LV_ANIM_OFF);
    lv_bar_set_value(bar_happy_,  happiness_, LV_ANIM_OFF);
    lv_bar_set_value(bar_clean_,  cleanliness_, LV_ANIM_OFF);
    lv_bar_set_value(bar_energy_, energy_, LV_ANIM_OFF);

    // Window Day/Night state
    if (is_sleeping_) {
        lv_obj_set_style_bg_color(window_box_, lv_color_hex(0x1e1b4b), 0);
        lv_obj_set_style_text_color(window_decor_, lv_color_hex(0xfde047), 0);
        lv_label_set_text(window_decor_, "C*");
    } else {
        lv_obj_set_style_bg_color(window_box_, lv_color_hex(0xbae6fd), 0);
        lv_obj_set_style_text_color(window_decor_, lv_color_hex(0xffffff), 0);
        lv_label_set_text(window_decor_, "(*)");
    }

    // Poop visibility
    if (poop_count_ > 0 && mode_ == kPetModeRoom) {
        lv_obj_clear_flag(poop_img_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(poop_img_, LV_OBJ_FLAG_HIDDEN);
    }

    // Cat Sprite & Mood Label when in Room Mode
    if (mode_ == kPetModeRoom && anim_ticks_ == 0) {
        if (is_sleeping_) {
            lv_image_set_src(cat_img_, &pet_cat_sleep_dsc);
            lv_label_set_text(header_mood_, "SLEEPING zZ");
        } else if (hunger_ < 40) {
            lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
            lv_label_set_text(header_mood_, "HUNGRY!");
        } else if (cleanliness_ < 40 || poop_count_ > 0) {
            lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
            lv_label_set_text(header_mood_, "MESSY!");
        } else if (happiness_ >= 85) {
            lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
            lv_label_set_text(header_mood_, "SUPER HAPPY");
        } else {
            lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
            lv_label_set_text(header_mood_, "CUTE <3");
        }
    }

    // Highlight selected action button
    for (int i = 0; i < 5; i++) {
        if (i == selected_action_) {
            lv_obj_set_style_bg_color(action_btns_[i], lv_color_hex(0xff69aa), 0); // Kawaii Ribbon Pink
            lv_obj_set_style_border_color(action_btns_[i], lv_color_hex(0xfde047), 0); // Gold border
            lv_obj_set_style_border_width(action_btns_[i], 2, 0);
            lv_obj_set_style_text_color(action_labels_[i], lv_color_hex(0xffffff), 0);
        } else {
            lv_obj_set_style_bg_color(action_btns_[i], lv_color_hex(0x3b1945), 0);
            lv_obj_set_style_border_color(action_btns_[i], lv_color_hex(0x701a75), 0);
            lv_obj_set_style_border_width(action_btns_[i], 1, 0);
            lv_obj_set_style_text_color(action_labels_[i], lv_color_hex(0xfbcfe8), 0);
        }
    }
}

void Pet::Tick() {
    if (root_ == nullptr || lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN)) return;

    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    tick_counter_++;

    // Handle timed animation countdown (e.g. Eating / Cleaning celebration)
    if (anim_ticks_ > 0) {
        anim_ticks_--;
        if (anim_ticks_ == 0) {
            lv_obj_add_flag(treat_img_, LV_OBJ_FLAG_HIDDEN);
            UpdateUi();
        }
        return;
    }

    // 1. In Yarn Catch Minigame: Smoothly glide the pink yarn cursor at 60ms ticks
    if (mode_ == kPetModePlayCatch) {
        yarn_pos_ += yarn_dir_;
        if (yarn_pos_ >= 170) {
            yarn_pos_ = 170;
            yarn_dir_ = - std::abs(yarn_dir_);
        } else if (yarn_pos_ <= 4) {
            yarn_pos_ = 4;
            yarn_dir_ = std::abs(yarn_dir_);
        }
        lv_obj_set_pos(catch_cursor_, yarn_pos_, 2);

        // Animate Mochi bouncing excitedly while playing!
        if ((tick_counter_ % 6) == 0) {
            lv_image_set_src(cat_img_, (tick_counter_ % 12 == 0) ? &pet_cat_idle2_dsc : &pet_cat_happy_dsc);
        }
        return;
    }

    // 2. In Sleep Lullaby Mode: Smoothly pulse the rhythm bar (0..100..0)
    if (mode_ == kPetModeSleep) {
        rhythm_val_ += rhythm_dir_;
        if (rhythm_val_ >= 100) {
            rhythm_val_ = 100;
            rhythm_dir_ = -4;
        } else if (rhythm_val_ <= 0) {
            rhythm_val_ = 0;
            rhythm_dir_ = 4;
        }
        lv_bar_set_value(rhythm_bar_, rhythm_val_, LV_ANIM_OFF);
        // Glow gold when inside sweet lullaby zone (55..95)
        if (rhythm_val_ >= 55) {
            lv_obj_set_style_bg_color(rhythm_bar_, lv_color_hex(0xfde047), LV_PART_INDICATOR);
        } else {
            lv_obj_set_style_bg_color(rhythm_bar_, lv_color_hex(0xc084fc), LV_PART_INDICATOR);
        }
        return;
    }

    // 3. In Main Room Mode: Cute Sanrio idle animations (Wave, Wink, Purr!)
    if (!is_sleeping_ && mode_ == kPetModeRoom) {
        int phase = tick_counter_ % 40;
        if (phase == 0) {
            lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
        } else if (phase == 14) {
            lv_image_set_src(cat_img_, &pet_cat_idle2_dsc); // Wave & Wink!
        } else if (phase == 22) {
            lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
        } else if (phase == 32 && happiness_ >= 75) {
            lv_image_set_src(cat_img_, &pet_cat_happy_dsc); // Happy Purr!
        }
    }
}

// ============================================================================
// 1. FEED MECHANIC (Treat Selector + Eating Animation)
// ============================================================================
static const char* kFoodMenuText[3] = {
    "< 1.Taiyaki (+35 Food) >",
    "< 2.Berry Milk (+Love) >",
    "< 3.Star Macaron (+30) >"
};

void Pet::OpenFeedMenu() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    mode_ = kPetModeFeedSelect;
    menu_choice_ = 0;

    lv_obj_clear_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sub_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(catch_bar_bg_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rhythm_bar_, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text(speech_bubble_, "Yum! Pick a Sanrio treat!");
    lv_label_set_text(sub_label_, kFoodMenuText[menu_choice_]);
    lv_label_set_text(footer_lbl_, "UP/DN:Pick Treat  OK:Feed\nHOLD OK:Cancel");
}

void Pet::ConfirmFeed() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    is_sleeping_ = false;
    mode_ = kPetModeRoom;
    lv_obj_add_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);

    if (menu_choice_ == 0) {
        hunger_ = std::min(100, hunger_ + 35);
        happiness_ = std::min(100, happiness_ + 10);
        lv_label_set_text(speech_bubble_, "Nom nom! Warm Taiyaki! <3");
    } else if (menu_choice_ == 1) {
        hunger_ = std::min(100, hunger_ + 20);
        happiness_ = std::min(100, happiness_ + 20);
        lv_label_set_text(speech_bubble_, "Slurp~ Sweet Berry Milk! <3");
    } else {
        hunger_ = std::min(100, hunger_ + 15);
        happiness_ = std::min(100, happiness_ + 30);
        lv_label_set_text(speech_bubble_, "Crunch! Star Macaron! *");
    }

    feed_counter_++;
    if (feed_counter_ >= 2) {
        feed_counter_ = 0;
        if (poop_count_ < 3) poop_count_++;
    }

    AddXp(10);
    anim_ticks_ = 30; // Show eating sprite for ~1.8 seconds
    lv_image_set_src(cat_img_, &pet_cat_eat_dsc);
    lv_obj_clear_flag(treat_img_, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(footer_lbl_, "UP/DN:Move  OK:Pick\nHOLD OK:Back to Menu");

    PlayChime();
    UpdateUi();
    SaveState();
}

// ============================================================================
// 2. PLAY MECHANICS (2 Mini-Games: Star/Yarn Catch & Copycat Dance!)
// ============================================================================
static const char* kPlayMenuText[2] = {
    "< Game 1: Yarn Catch! >",
    "< Game 2: Dance Copy! >"
};

void Pet::OpenPlayMenu() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    mode_ = kPetModePlaySelect;
    menu_choice_ = 0;

    lv_obj_clear_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sub_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(catch_bar_bg_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rhythm_bar_, LV_OBJ_FLAG_HIDDEN);

    lv_image_set_src(cat_img_, &pet_cat_idle2_dsc);
    lv_label_set_text(speech_bubble_, "Yay! Which game shall we play?");
    lv_label_set_text(sub_label_, kPlayMenuText[menu_choice_]);
    lv_label_set_text(footer_lbl_, "UP/DN:Pick Game  OK:Start\nHOLD OK:Cancel");
}

void Pet::StartCatchGame() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    is_sleeping_ = false;
    mode_ = kPetModePlayCatch;
    play_score_ = 0;
    yarn_pos_ = 10;
    yarn_dir_ = 7;

    lv_obj_clear_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sub_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rhythm_bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(catch_bar_bg_, LV_OBJ_FLAG_HIDDEN);

    lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
    lv_label_set_text(speech_bubble_, "Press OK in the GREEN zone!");
    lv_label_set_text(footer_lbl_, "OK:Catch Yarn! (0/3)\nHOLD OK:Exit Game");
}

void Pet::HandleCatchButton() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    // Target box is X=70..122 (width 52), cursor is width 16
    if (yarn_pos_ >= 58 && yarn_pos_ <= 122) {
        play_score_++;
        happiness_ = std::min(100, happiness_ + 15);
        energy_ = std::max(15, energy_ - 3);
        // Speed up slightly each round!
        yarn_dir_ = (yarn_dir_ > 0) ? (7 + play_score_ * 2) : -(7 + play_score_ * 2);

        PlayChime();

        if (play_score_ >= 3) {
            mode_ = kPetModeRoom;
            AddXp(20);
            lv_obj_add_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
            lv_label_set_text(speech_bubble_, "YOU WON! Mochi is so happy! <3");
            lv_label_set_text(footer_lbl_, "UP/DN:Move  OK:Pick\nHOLD OK:Back to Menu");
            UpdateUi();
            SaveState();
        } else {
            char buf[64];
            snprintf(buf, sizeof(buf), "GREAT CATCH! * (%d/3)", play_score_);
            lv_label_set_text(speech_bubble_, buf);
            snprintf(buf, sizeof(buf), "OK:Catch Yarn! (%d/3)\nHOLD OK:Exit Game", play_score_);
            lv_label_set_text(footer_lbl_, buf);
        }
    } else {
        lv_label_set_text(speech_bubble_, "Almost! Aim for the GREEN box!");
    }
}

static const char* MoveName(int m) {
    if (m == 0) return "UP";
    if (m == 1) return "DN";
    return "OK";
}

void Pet::StartDanceGame() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    is_sleeping_ = false;
    mode_ = kPetModePlayDance;
    dance_step_ = 0;

    for (int i = 0; i < 3; i++) {
        dance_moves_[i] = esp_random() % 3;
    }

    lv_obj_clear_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sub_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(catch_bar_bg_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(rhythm_bar_, LV_OBJ_FLAG_HIDDEN);

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "Dance: [%s] -> [%s] -> [%s]",
             MoveName(dance_moves_[0]), MoveName(dance_moves_[1]), MoveName(dance_moves_[2]));
    lv_label_set_text(sub_label_, pattern);
    lv_label_set_text(speech_bubble_, "Copy Mochi's 3 dance moves!");
    lv_label_set_text(footer_lbl_, "Press UP, DN, or OK in order!\nHOLD OK:Exit Game");
}

void Pet::HandleDanceButton(LauncherButton button) {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    int pressed = (button == LauncherButton::kUp) ? 0 : ((button == LauncherButton::kDown) ? 1 : 2);
    if (pressed == dance_moves_[dance_step_]) {
        dance_step_++;
        // Toggle cute dance pose on Mochi!
        lv_image_set_src(cat_img_, (dance_step_ % 2 == 1) ? &pet_cat_idle2_dsc : &pet_cat_happy_dsc);

        if (dance_step_ >= 3) {
            // Won the Dance Game!
            happiness_ = std::min(100, happiness_ + 30);
            energy_ = std::max(15, energy_ - 5);
            AddXp(25);
            mode_ = kPetModeRoom;
            lv_obj_add_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
            lv_label_set_text(speech_bubble_, "PERFECT DANCE! Best friends! <3");
            lv_label_set_text(footer_lbl_, "UP/DN:Move  OK:Pick\nHOLD OK:Back to Menu");
            PlayChime();
            UpdateUi();
            SaveState();
        } else {
            char buf[64];
            snprintf(buf, sizeof(buf), "Nice move! (%d/3) Keep going!", dance_step_);
            lv_label_set_text(speech_bubble_, buf);
        }
    } else {
        dance_step_ = 0;
        lv_image_set_src(cat_img_, &pet_cat_idle1_dsc);
        lv_label_set_text(speech_bubble_, "Oops! Try the sequence from #1!");
    }
}

// ============================================================================
// 3. SLEEP MECHANIC (Rhythmic Lullaby Patting with Visual Pulse Bar)
// ============================================================================
void Pet::StartSleep() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    mode_ = kPetModeSleep;
    sleep_pats_ = 0;
    rhythm_val_ = 20;
    rhythm_dir_ = 4;
    last_pat_time_ms_ = 0;

    lv_obj_clear_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sub_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(catch_bar_bg_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(rhythm_bar_, LV_OBJ_FLAG_HIDDEN);

    lv_image_set_src(cat_img_, &pet_cat_sleep_dsc);
    lv_label_set_text(speech_bubble_, "Tap OK when bar glows GOLD!");
    lv_label_set_text(footer_lbl_, "Tap OK gently [0/5 pats]\nHOLD OK:Cancel");
}

void Pet::HandleSleepButton() {
    int64_t now_ms = esp_timer_get_time() / 1000;
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    // Check that user isn't mashing rapidly (< 400ms)
    if (last_pat_time_ms_ > 0 && (now_ms - last_pat_time_ms_) < 400) {
        lv_label_set_text(speech_bubble_, "Shhh~ Slower, gentle pats...");
        return;
    }

    last_pat_time_ms_ = now_ms;
    sleep_pats_++;

    static const char* kSleepMsgs[5] = {
        "*pat pat* Mochi yawns... (~o~)",
        "*pat pat* Heavy eyelids... (-.-)",
        "*pat pat* Soft lullaby... (u.u)",
        "*pat pat* Curling on cloud...",
        "Zzz... Fast asleep! Sweet dreams!"
    };

    lv_label_set_text(speech_bubble_, kSleepMsgs[std::min(4, sleep_pats_ - 1)]);

    char fbuf[64];
    snprintf(fbuf, sizeof(fbuf), "Tap OK gently [%d/5 pats]\nHOLD OK:Cancel", sleep_pats_);
    lv_label_set_text(footer_lbl_, fbuf);

    if (sleep_pats_ >= 5) {
        is_sleeping_ = true;
        mode_ = kPetModeRoom;
        energy_ = 100;
        happiness_ = std::min(100, happiness_ + 15);
        AddXp(15);

        lv_obj_add_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(footer_lbl_, "UP/DN:Move  OK:Pick\nHOLD OK:Back to Menu");

        PlayChime();
        UpdateUi();
        SaveState();
    }
}

// ============================================================================
// 4. CLEAN POOP MECHANIC (Sparkle Sweep!)
// ============================================================================
void Pet::DoClean() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    if (poop_count_ > 0) {
        poop_count_ = 0;
        cleanliness_ = 100;
        happiness_ = std::min(100, happiness_ + 20);
        AddXp(15);
        lv_label_set_text(speech_bubble_, "*SWEEP!* All poop scooped! *");
        PlayChime();
    } else {
        cleanliness_ = 100;
        happiness_ = std::min(100, happiness_ + 5);
        lv_label_set_text(speech_bubble_, "*Sparkle!* Room is spotless! <3");
    }

    anim_ticks_ = 25;
    lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
    UpdateUi();
    SaveState();
}

// ============================================================================
// 5. BATH MECHANIC (Bubble Crown Scrub!)
// ============================================================================
void Pet::StartBath() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    is_sleeping_ = false;
    mode_ = kPetModeBath;
    bath_step_ = 0;

    lv_obj_clear_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sub_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(catch_bar_bg_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(rhythm_bar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(rhythm_bar_, lv_color_hex(0x38bdf8), LV_PART_INDICATOR);
    lv_bar_set_value(rhythm_bar_, 15, LV_ANIM_OFF);

    lv_image_set_src(cat_img_, &pet_cat_bath_dsc);
    lv_label_set_text(speech_bubble_, "Bath time! Press OK to scrub!");
    lv_label_set_text(footer_lbl_, "OK:Scrub Bubbles (1/3)\nHOLD OK:Cancel");
}

void Pet::HandleBathButton() {
    auto* display = Board::GetInstance().GetDisplay();
    DisplayLockGuard lock(display);

    bath_step_++;
    if (bath_step_ == 1) {
        lv_bar_set_value(rhythm_bar_, 55, LV_ANIM_OFF);
        lv_label_set_text(speech_bubble_, "Scrub-a-dub! Bubble crown! oOo");
        lv_label_set_text(footer_lbl_, "OK:Warm Rinse (2/3)\nHOLD OK:Cancel");
    } else if (bath_step_ == 2) {
        lv_bar_set_value(rhythm_bar_, 90, LV_ANIM_OFF);
        lv_label_set_text(speech_bubble_, "Splash~ Rinsing with ducky!");
        lv_label_set_text(footer_lbl_, "OK:Dry & Fluff! (3/3)\nHOLD OK:Cancel");
    } else {
        cleanliness_ = 100;
        happiness_ = std::min(100, happiness_ + 20);
        AddXp(15);
        mode_ = kPetModeRoom;

        lv_obj_add_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
        anim_ticks_ = 25;
        lv_image_set_src(cat_img_, &pet_cat_happy_dsc);
        lv_label_set_text(speech_bubble_, "So fluffy & smelling like berries!");
        lv_label_set_text(footer_lbl_, "UP/DN:Move  OK:Pick\nHOLD OK:Back to Menu");

        PlayChime();
        UpdateUi();
        SaveState();
    }
}

// ============================================================================
// BUTTON ROUTING
// ============================================================================
bool Pet::HandleButton(LauncherButton button, LauncherButtonEvent event) {
    if (root_ == nullptr || lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN)) {
        return false;
    }

    // Long-press OK exits sub-mode to Room, or exits Room to Main Menu
    if (button == LauncherButton::kOk && event == LauncherButtonEvent::kLongPress) {
        if (mode_ != kPetModeRoom) {
            auto* display = Board::GetInstance().GetDisplay();
            DisplayLockGuard lock(display);
            mode_ = kPetModeRoom;
            lv_obj_add_flag(sub_panel_, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(speech_bubble_, "Meow~ What should we do next?");
            lv_label_set_text(footer_lbl_, "UP/DN:Move  OK:Pick\nHOLD OK:Back to Menu");
            UpdateUi();
            return true;
        }
        Launcher::GetInstance().EnterMenu();
        return true;
    }

    if (event != LauncherButtonEvent::kClick) {
        return false;
    }

    // 1. Feed Treat Selector
    if (mode_ == kPetModeFeedSelect) {
        auto* display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        if (button == LauncherButton::kUp) {
            menu_choice_ = (menu_choice_ - 1 + 3) % 3;
            lv_label_set_text(sub_label_, kFoodMenuText[menu_choice_]);
        } else if (button == LauncherButton::kDown) {
            menu_choice_ = (menu_choice_ + 1) % 3;
            lv_label_set_text(sub_label_, kFoodMenuText[menu_choice_]);
        } else if (button == LauncherButton::kOk) {
            ConfirmFeed();
        }
        return true;
    }

    // 2. Play Game Selector
    if (mode_ == kPetModePlaySelect) {
        auto* display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        if (button == LauncherButton::kUp || button == LauncherButton::kDown) {
            menu_choice_ = (menu_choice_ + 1) % 2;
            lv_label_set_text(sub_label_, kPlayMenuText[menu_choice_]);
        } else if (button == LauncherButton::kOk) {
            if (menu_choice_ == 0) {
                StartCatchGame();
            } else {
                StartDanceGame();
            }
        }
        return true;
    }

    // 3. Active Mini-Games
    if (mode_ == kPetModePlayCatch) {
        if (button == LauncherButton::kOk) {
            HandleCatchButton();
        }
        return true;
    }

    if (mode_ == kPetModePlayDance) {
        HandleDanceButton(button);
        return true;
    }

    if (mode_ == kPetModeSleep) {
        if (button == LauncherButton::kOk) {
            HandleSleepButton();
        }
        return true;
    }

    if (mode_ == kPetModeBath) {
        HandleBathButton();
        return true;
    }

    // 4. Main Room Action Dock Navigation
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
            case 0: OpenFeedMenu(); break;
            case 1: OpenPlayMenu(); break;
            case 2: StartSleep();   break;
            case 3: DoClean();      break;
            case 4: StartBath();    break;
        }
        return true;
    }

    return false;
}
