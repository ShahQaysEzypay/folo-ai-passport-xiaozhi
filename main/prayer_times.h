#ifndef PRAYER_TIMES_H
#define PRAYER_TIMES_H

#include <string>
#include <vector>
#include <ctime>
#include <lvgl.h>

struct PrayerInfo {
    std::string name;       // e.g. "Subuh (Fajr)"
    std::string time_str;   // e.g. "05:48"
    int hour = 0;
    int minute = 0;
    bool is_next = false;
};

class PrayerTimes {
public:
    static PrayerTimes& GetInstance() {
        static PrayerTimes instance;
        return instance;
    }
    PrayerTimes(const PrayerTimes&) = delete;
    PrayerTimes& operator=(const PrayerTimes&) = delete;

    void Initialize();
    void BuildUi();
    void Show(bool show);
    void UpdateUi();
    void RefreshAsync();
    void CalculateNextPrayer();

    // Check if current hour & minute matches any prayer time for alert (returns prayer name or empty)
    std::string CheckPrayerAlert(int hour, int minute);

    bool IsLoaded() const { return loaded_; }
    bool IsFetching() const { return fetching_; }
    int GetFetchedMday() const { return fetched_mday_; }
    const std::vector<PrayerInfo>& GetPrayers() const { return prayers_; }
    const std::string& GetHijriDate() const { return hijri_date_; }
    const std::string& GetNextPrayerName() const { return next_prayer_name_; }
    const std::string& GetNextPrayerCountdown() const { return next_prayer_countdown_; }

private:
    PrayerTimes() = default;
    ~PrayerTimes() = default;

    void LoadCache();
    void SaveCache();
    static void FetchTask(void* arg);
    bool DoFetch();

    bool loaded_ = false;
    bool fetching_ = false;
    int fetched_mday_ = -1;
    std::string zone_ = "SGR03";
    std::string location_name_ = "Banting (SGR03)";
    std::string hijri_date_ = "--";
    std::string gregorian_date_ = "--";
    std::string day_name_ = "--";
    std::string next_prayer_name_ = "--";
    std::string next_prayer_countdown_ = "--";

    std::vector<PrayerInfo> prayers_;

    // LVGL widgets
    lv_obj_t* root_ = nullptr;
    lv_obj_t* header_title_ = nullptr;
    lv_obj_t* header_sub_ = nullptr;
    lv_obj_t* date_label_ = nullptr;
    lv_obj_t* next_card_ = nullptr;
    lv_obj_t* next_title_ = nullptr;
    lv_obj_t* next_countdown_ = nullptr;
    struct RowWidget {
        lv_obj_t* row = nullptr;
        lv_obj_t* name_label = nullptr;
        lv_obj_t* time_label = nullptr;
    };
    std::vector<RowWidget> rows_;
    lv_obj_t* footer_label_ = nullptr;
};

#endif // PRAYER_TIMES_H
